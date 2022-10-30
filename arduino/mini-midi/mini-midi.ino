#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <MIDI.h>

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, MIDI);

#define PIN_MODE D2
#define PIN_SHIFT D3

#define PIN_VOLUME1 0
#define PIN_VOLUME2 1
#define RED 17
#define GREEN 16
#define BLUE 25

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

float rawPrevF[2] = {-1.0,-1.0};
float rawTrigF[2] = {-1.0,-1.0};
boolean isCCDisp = false;
int ccDispCnt = 0;
#define CCDISP_CNT_START 100

int sw_prev = LOW;
int onNote = 0;
uint8_t vol[2] = {0,0};
int initCnt = 0;

#define HC4051_PIN D10
uint8_t hc4051_cnt = 0;
int ht4051_sio[3] = {D7,D8,D9};
int ht4051_status[8] = {LOW,LOW,LOW,LOW,LOW,LOW,LOW,LOW};

uint8_t noteNums[16] = {
  0x60,
  0x61,
  0x62,
  0x63,
  0x64,
  0x65,
  0x66,
  0x67,
  0x68,
  0x69,
  0x6A,
  0x6B,
  0x6C,
  0x6D,
  0x6E,
  0x6F};

int initHc4051(){
  int cnt;
  int length = sizeof(ht4051_sio) / sizeof(int);
  for(cnt = 0;cnt < length;cnt++){
    pinMode(ht4051_sio[cnt],OUTPUT);
    digitalWrite(ht4051_sio[cnt],LOW);
  }
  pinMode(HC4051_PIN,INPUT);
}

boolean readHc4051(){
  int cnt;
  int data;
  uint8_t s[3];
  s[0] = hc4051_cnt & 0x01;
  s[1] = (hc4051_cnt >> 1) & 0x01;
  s[2] = (hc4051_cnt >> 2) & 0x01;

  for(cnt=0;cnt<3;cnt++){
    if(s[cnt] == (uint8_t)0x01){
      digitalWrite(ht4051_sio[cnt],HIGH);
    }else{
      digitalWrite(ht4051_sio[cnt],LOW);
    }
  }
  delay(1);
  data = digitalRead(HC4051_PIN);
  if(ht4051_status[hc4051_cnt] != data){
    ht4051_status[hc4051_cnt] = data;
    SerialTinyUSB.print("push sw=");
    SerialTinyUSB.print(hc4051_cnt);
    SerialTinyUSB.print(" status=");
    SerialTinyUSB.println(data);
    return true;
  }else{
    return false;
  }
}

int modeKey = LOW;
int shiftKey = LOW;

void readMode(){
  int data;
  data = digitalRead(PIN_MODE);
  if(modeKey != data){
    modeKey = data;
    if(modeKey == HIGH){
      MIDI.sendNoteOn(20,127,1);
      drawNote(20,1);
    }else{
      MIDI.sendNoteOff(20,0,1);
      drawNote(20,0);
    }
    delay(100);
  }
}

void readShift(){
  int data;
  data = digitalRead(PIN_SHIFT);
  if(shiftKey != data){
    shiftKey = data;
    delay(100);
  }
}

void drawStartUp(){
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(F("TripArts  Music"));
  display.display();
  delay(3000);
  display.clearDisplay();
  display.display();
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print(F("Mini Midi Controller"));
  display.display();
  delay(3000);
  display.clearDisplay();
  display.display();
}

void setup(){
#if defined(ARDUINO_ARCH_MBED) && defined(ARDUINO_ARCH_RP2040)
  TinyUSB_Device_Init(0);
#endif
  pinMode(RED,OUTPUT);
  pinMode(GREEN,OUTPUT);
  pinMode(BLUE,OUTPUT);
  digitalWrite(RED,HIGH);
  digitalWrite(GREEN,HIGH);
  digitalWrite(BLUE,HIGH); 
  pinMode(PIN_MODE, INPUT);
  pinMode(PIN_SHIFT, INPUT);
  MIDI.begin(MIDI_CHANNEL_OMNI);
  SerialTinyUSB.begin(1152000);

  initHc4051();

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    SerialTinyUSB.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  display.display();

  while( !TinyUSBDevice.mounted() ) delay(1);
  int raw = analogRead(PIN_VOLUME1); // ADCを起動しておく（起動直後バタつくため）
  raw = analogRead(PIN_VOLUME2);
  delay(1000);
  raw = analogRead(PIN_VOLUME1); // ADCを起動しておく
  raw = analogRead(PIN_VOLUME2);
  drawStartUp();
}

boolean checkChangeVolume(int volume, int index){
  float volF = (float)volume;
  float nowVolF;
  boolean res = false;
  if(rawPrevF[index] >= 0){
    nowVolF = (rawPrevF[index] * 0.9) + (volF * 0.1);
    float divVol = fabsf(rawTrigF[index] - nowVolF);
    float divPrev = fabsf(rawPrevF[index] - nowVolF);
    if((divVol > 8.5)&&(divPrev > 0.5)){
      SerialTinyUSB.print("divVol: ");
      SerialTinyUSB.print(divVol);
      SerialTinyUSB.print(" divPrev: ");
      SerialTinyUSB.println(divPrev);
      res = true;
      rawTrigF[index] = nowVolF;
    }
    rawPrevF[index] = nowVolF;
  }else{
    nowVolF = volF;
    rawPrevF[index] = nowVolF;
    rawTrigF[index] = nowVolF;
  }
  return res;
}


void drawNote(int note, int mode) {
  isCCDisp = false;
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  if(mode == 1){
    display.print(F("Note:"));
    if((note <= 99)&&(note >= 10)){
      display.print(F("0"));
    }else if(note <= 9){
      display.print(F("00"));
    }
    display.print(note);
    display.display();
  }else{
    display.clearDisplay();
    display.display();
  }
}

void drawCC(int num, int value, int mode) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  if(mode == 1){
    isCCDisp = true;
    ccDispCnt = CCDISP_CNT_START;
    display.print(F("CC:"));
    if((num <= 99)&&(num >= 10)){
      display.print(F("0"));
    }else if(num <= 9){
      display.print(F("00"));
    }
    display.print(num);
    display.print(F(" "));
    if((value <= 99)&&(value >= 10)){
      display.print(F("0"));
    }else if(value <= 9){
      display.print(F("00"));
    }
    display.print(value);
    display.display();
  }else{
    display.clearDisplay();
    display.display();    
  }
}

void checkClearCC(){
  if(ccDispCnt > 0){
    ccDispCnt --;
    if((ccDispCnt == 0)&&(isCCDisp == true)){
      drawCC(0,0,0);
    }
  }
}

void loop(){
  int index;
  TinyUSB_Device_FlushCDC();

  int raw1 = analogRead(PIN_VOLUME1);
  if(checkChangeVolume(raw1, 0)){
    vol[0] = (uint8_t)(raw1 >> 3);
    SerialTinyUSB.print("vol:");
    SerialTinyUSB.println(vol[0]);
    drawCC(0x42,vol[0],1);
    MIDI.sendControlChange(0x42,vol[0],1);
  }
  int raw2 = analogRead(PIN_VOLUME2);
  if(checkChangeVolume(raw2, 1)){
    vol[1] = (uint8_t)(raw2 >> 3);
    SerialTinyUSB.print("vol:");
    SerialTinyUSB.println(vol[1]);
    drawCC(0x43,vol[1],1);
    MIDI.sendControlChange(0x43,vol[1],1);
  }
  
  int sw = digitalRead(PIN_MODE);
//  SerialTinyUSB.println(sw);
  if(sw == HIGH){
    if(sw_prev == LOW){
      onNote = 20;
      SerialTinyUSB.print("NoteON:");
      SerialTinyUSB.println(onNote);
      MIDI.sendNoteOn(onNote,127,1);
    }
  }else{
    if(sw_prev == HIGH){
      SerialTinyUSB.print("NoteOFF:");
      SerialTinyUSB.println(onNote);
      MIDI.sendNoteOff(onNote,0,1);   
    }
  }

  if(readHc4051() == true){
    SerialTinyUSB.println("push key");
    if(shiftKey == HIGH){
      index = hc4051_cnt + 8;
    }else{
      index = hc4051_cnt;
    }
    if(ht4051_status[hc4051_cnt] == HIGH){
      MIDI.sendNoteOn(noteNums[index],127,1);
      drawNote(noteNums[index],1);
    }else{
      MIDI.sendNoteOff(noteNums[index],0,1);   
      drawNote(noteNums[index],0);
    }
  }
  hc4051_cnt ++;
  if(hc4051_cnt >= 8){
    hc4051_cnt = 0;
  }

  readMode();
  readShift();
  checkClearCC();
  
  MIDI.read();
  sw_prev = sw;
  delay(5);
}
