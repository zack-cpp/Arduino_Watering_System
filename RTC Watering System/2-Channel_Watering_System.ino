/*
   EEPROM Mapping
   address 0                            : amount of pattern
   address 1 =< (i*5) - 1 --> (i*5) + 3 : watering time (schedule number, duration in minutes, duration in seconds, hour, minute)
*/
#include <EEPROM.h>
#include <Keypad.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"

#define pump0 10
#define pump1 11
#define AMOUNT_PATTERN 0
#define DEFAULT_DURATION 10
#define LENGTH 6

const byte ROWS = 4;
const byte COLS = 4;

byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};

bool state[170];
bool stateDurasi = false;
bool buttonPressed = false;
bool channelState = true;
bool wateringState = false;
bool wateringChannelState[2];

String input = "";
String jam, menit;

int iJam, iMenit;
int i = 0;
int j = 0;

unsigned int durasiInSecond[2];
unsigned int currentWatering = 0;
unsigned int tmpSecond[2];
unsigned int scheduleIndex = 1;
unsigned int durasiChannel[2] = {0, 0};

char KeysID[] = {'1',  '2', '3', 'A', '4', '5', '6', 'B', '7', '8', '9', 'C', '*', '0', '#', 'D'};
char keys[ROWS][COLS] = {
  {KeysID[0],  KeysID[1], KeysID[2], KeysID[3]},
  {KeysID[4],  KeysID[5], KeysID[6], KeysID[7]},
  {KeysID[8],  KeysID[9], KeysID[10], KeysID[11]},
  {KeysID[12],  KeysID[13], KeysID[14], KeysID[15]}
};

Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS);
LiquidCrystal_I2C lcd(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);
RTC_DS1307 rtc;

void reset();
void checkTime();
void getClock(int schedule);
void decodeClock(int schedule);
void startWatering(int durasi, bool channel);
void addSchedule(int schedule);
void setDurasi(int durasi);
void setPinPump(int schedule);
void sort(int a[], int d[], int tChannel[], int size);

void setup() {
  Serial.begin(115200);
  //rtc setup
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  } else {
    Serial.println("RTC Started");
  }
  if (!rtc.isrunning()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  //
  pinMode(pump0, OUTPUT);
  pinMode(pump1, OUTPUT);
  digitalWrite(pump0, HIGH);
  digitalWrite(pump1, HIGH);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  reset();
}

void loop() {
  DateTime now = rtc.now();
  int hh = checkTime(true);
  int mm = checkTime(false);
  lcd.setCursor(0, 0);
  // show jadwal
  if(!wateringState){
    lcd.print("Jadwal: ");
    if(hh < 10){
      lcd.print("0");
    }
    lcd.print(hh);
    lcd.print(":");
    if(mm < 10){
      lcd.print("0");
    }
    lcd.print(mm);
    //
    //show realtime clock
    lcd.setCursor(0, 1);
    if(now.hour() < 10) {
      lcd.print("0");
    }
    lcd.print(now.hour());
    lcd.print(":");
    if (now.minute() < 10) {
      lcd.print("0");
    }
    lcd.print(now.minute());
    lcd.print(":");
    if (now.second() < 10) {
      lcd.print("0");
    }
    lcd.print(now.second());
  }else{
    if(wateringChannelState[0]){
      startWatering(durasiChannel[0], true);
    }else{
      
    }
    if(wateringChannelState[1]){
      startWatering(durasiChannel[1], false);
    }else{
      
    }
  }
  //
  char key = keypad.getKey();
  if (key) {
    if (key == KeysID[12]) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Watering Time!!!");
      buttonPressed = true;
      startWatering(30, true);
    } else if (key == KeysID[14]) {
      addSchedule(EEPROM.read(AMOUNT_PATTERN) + 1);
    } else if (key == KeysID[3]) {
      for (unsigned int i = 0; i < EEPROM.length(); i++) {
        EEPROM.write(i, 0);
      }
      reset(); // reset EEPROM
      lcd.clear();
      lcd.setCursor(0, 0);
    } else if (key == KeysID[7]) {
      if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        abort();
      }
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // adjust clock
    } else if (key == KeysID[11]) {
      addSchedule(EEPROM.read(AMOUNT_PATTERN) + 1);
      setDurasi(EEPROM.read(AMOUNT_PATTERN) + 1);
      setChannel(EEPROM.read(AMOUNT_PATTERN) + 1);
    }
  }
}

void reset() {
  if (EEPROM.read(AMOUNT_PATTERN) == 0) {
    addSchedule(1);
    setDurasi(1);
    setChannel(1);
    for (unsigned short int i = 0; i < EEPROM.length(); i++) {
      state[i] = false;
    }
  }
}

void addSchedule(int schedule) {
  lcd.clear();
  lcd.print("Add Schedule");
  input = "";
  stateDurasi = false;
  getClock(schedule);
}

void setDurasi(int schedule) {
  lcd.clear();
  lcd.print("Set Durasi");
  input = "";
  stateDurasi = true;
  getClock(schedule);
}

void getClock(int schedule) {
  lcd.setCursor(2, 2);
  lcd.print(":");
  unsigned int jml = 0;
  while (true) {
    char key = keypad.getKey();
    if(key){
      if (key != KeysID[3] && key != KeysID[7] && key != KeysID[11] && key != KeysID[12] && key != KeysID[14] && key != KeysID[15]) {
        if(jml == 2){
          input += ":";
          jml++;
        }
        input += key;
        jml++;
        for (unsigned int x = 0; x < input.length(); x++) {
          lcd.setCursor(x, 1);
          lcd.print(input[x]);
        }
        if(jml == 5){
          Serial.println("break");
          break;
        }
      }
    }
  }
  decodeClock(schedule);
}
void decodeClock(int schedule) {
  jam = "";
  menit = "";
  jam += input[0];
  jam += input[1];
  iJam = jam.toInt();
  menit += input[3];
  menit += input[4];
  iMenit = menit.toInt();
  Serial.print(iJam);
  Serial.print(":");
  Serial.println(iMenit);
  if(stateDurasi){
    if(iJam > 59 || iMenit > 59){
      lcd.clear();
      lcd.setCursor(0, 0);
      Serial.print("INPUT SALAH");
      addSchedule(schedule);
    }
    EEPROM.write(schedule * LENGTH - 5, schedule);
    EEPROM.write(schedule * LENGTH - 4, iJam);
    EEPROM.write(schedule * LENGTH - 3, iMenit);
    stateDurasi = false;
  }else{
    if(iJam > 23 || iMenit > 59){
      lcd.clear();
      lcd.setCursor(0, 0);
      Serial.print("INPUT SALAH");
      addSchedule(schedule);
    }
    EEPROM.write(schedule * LENGTH - 2, iJam);
    EEPROM.write(schedule * LENGTH - 1, iMenit);
  }
  lcd.clear();
}

void sort(int a[], int d[], int tChannel[], int size){
  for(int i=0; i<(size-1); i++) {
    for(int o=0; o<(size-(i+1)); o++) {
      if(a[o] > a[o+1]){
        int t = a[o];
        int x = d[o];
        int y = tChannel[o];
        a[o] = a[o+1];
        d[o] = d[o+1];
        tChannel[o] = tChannel[o+1];
        a[o+1] = t;
        d[o+1] = x;
        tChannel[o+1] = y;
      }else if(a[o] == a[o+1]){
        if(d[o] > d[o+1]){
          int t = a[o];
          int x = d[o];
          int y = tChannel[o];
          a[o] = a[o+1];
          d[o] = d[o+1];
          tChannel[o] = tChannel[o+1];
          a[o+1] = t;
          d[o+1] = x;
          tChannel[o+1] = y;
        }
      }
    }
  }
}

int checkTime(bool req){
  DateTime now = rtc.now();
  int checkSchedule = EEPROM.read(AMOUNT_PATTERN);
  int amountHour[checkSchedule];
  int amountMinute[checkSchedule];
  int checkDurasiMinute[checkSchedule];
  int checkDurasiSecond[checkSchedule];
  int totalChannel[checkSchedule];
  int channel1[checkSchedule];
  int channel2[checkSchedule];
  unsigned int ANDstate = 0;
  unsigned int channel1Index = 0;
  unsigned int channel2Index = 0;
  unsigned int realIndexCh1 = 0;
  unsigned int realIndexCh2 = 0;
  scheduleIndex = 0;

  for (int i = 0; i < checkSchedule; i++){
    checkDurasiMinute[i] = EEPROM.read((i+1) * LENGTH - 4);
    checkDurasiSecond[i] = EEPROM.read((i+1) * LENGTH - 3);
    amountHour[i] = EEPROM.read((i+1) * LENGTH - 2);
    amountMinute[i] = EEPROM.read((i+1) * LENGTH - 1);
    totalChannel[i] = EEPROM.read((i+1) * LENGTH);
  }

  sort(amountHour, amountMinute, totalChannel, sizeof(amountHour) / sizeof(int));
  
  //grouping by channel
  for(int i = 0; i < checkSchedule; i++){
    if(totalChannel[i] == 1){
      channel1[channel1Index] = i;
      channel1Index++;
    }else if(totalChannel[i] == 2){
      channel2[channel2Index] = i;
      channel2Index++;
    }
  }

  for(int i = channel1Index; i < checkSchedule; i++){
    channel1[i] = 999;
  }
  for(int i = channel2Index; i < checkSchedule; i++){
    channel2[i] = 999;
  }
  
  // getting index
  // for channel 1
  if(now.hour() < amountHour[channel1[i]]){
    realIndexCh1 = channel1[i];
  }else if(now.hour() == amountHour[channel1[i]]){
    if(now.minute() <= amountMinute[channel1[i]]){
      realIndexCh1 = channel1[i];
    }else if(now.minute() > amountMinute[channel1[i]]){
      state[channel1[i]] = true;
      i++;
    }
  }else if(now.hour() > amountHour[channel1[i]]){
    state[channel1[i]] = true;
    i++;
    if(i == checkSchedule){
      i = 0;
    }
  }
  // for channel 2
  if(now.hour() < amountHour[channel2[j]]){
    realIndexCh2 = channel2[j];
  }else if(now.hour() == amountHour[channel2[j]]){
    if(now.minute() <= amountMinute[channel2[j]]){
      realIndexCh2 = channel2[j];
    }else if(now.minute() > amountMinute[channel2[j]]){
      state[channel2[j]] = true;
      j++;
    }
  }else if(now.hour() > amountHour[channel2[j]]){
    state[channel2[j]] = true;
    j++;
    if(j == checkSchedule){
      j = 0;
    }
  }
  
  // getting duration
  durasiInSecond[0] = (checkDurasiMinute[realIndexCh1] * 60) + checkDurasiSecond[realIndexCh1];
  durasiInSecond[1] = (checkDurasiMinute[realIndexCh2] * 60) + checkDurasiSecond[realIndexCh2];
  
  //check time
    //channel 1
  if(now.hour() == amountHour[realIndexCh1] && now.minute() == amountMinute[realIndexCh1] || buttonPressed == true){
    if(state[realIndexCh1] == false) { // in case duration is lower then 60 seconds, prevent watering more than once in a minute.
      lcd.clear();
      state[realIndexCh1] = true;
      durasiChannel[0] = durasiInSecond[0];
      wateringState = true;
      wateringChannelState[0] = true;
      tmpSecond[0] = (3600 * now.hour()) + (60 * now.minute()) + now.second() + durasiChannel[0];
      i++;
    }
  }
    // channel 2
  if(now.hour() == amountHour[realIndexCh2] && now.minute() == amountMinute[realIndexCh2] || buttonPressed == true){
    if(state[realIndexCh2] == false) { // in case duration is lower then 60 seconds, prevent watering more than once in a minute.
      lcd.clear();
      state[realIndexCh2] = true;
      durasiChannel[1] = durasiInSecond[1];
      wateringState = true;
      wateringChannelState[1] = true;
      tmpSecond[1] = (3600 * now.hour()) + (60 * now.minute()) + now.second() + durasiChannel[1];
      j++;
    }
  }
  for(int i = 0; i < checkSchedule; i++){
    if(state[i] == true) {
      ANDstate++;
    }
  }
  //reset method
  if(now.hour() == 0 && now.minute() == 0){
    if(ANDstate == checkSchedule){
      for(int i = 0; i < checkSchedule; i++) {
        state[i] = false;
      }
    }
    if(i + j == checkSchedule){ // reset index
      i = 0;
      j = 0;
    }
  }
  //debug
  /*
  Serial.print("\nHour\t\t: ");
  for(int i = 0; i < checkSchedule; i++){
    Serial.print(amountHour[i]);
    if(i != checkSchedule - 1){
      Serial.print(", ");
    }
  }
  Serial.print("\nMinute\t\t: ");
  for(int i = 0; i < checkSchedule; i++){
    Serial.print(amountMinute[i]);
    if(i != checkSchedule - 1){
      Serial.print(", ");
    }
  }Serial.print("\nChannel\t\t: ");
  for(int i = 0; i < checkSchedule; i++){
    Serial.print(totalChannel[i]);
    if(i != checkSchedule - 1){
      Serial.print(", ");
    }
  }
  Serial.print("\nChannel 1\t: ");
  for(int i = 0; i < checkSchedule; i++){
    Serial.print(channel1[i]);
    if(i != checkSchedule - 1){
      Serial.print(", ");
    }
  }
  Serial.print("\nChannel 2\t: ");
  for(int i = 0; i < checkSchedule; i++){
    Serial.print(channel2[i]);
    if(i != checkSchedule - 1){
      Serial.print(", ");
    }
  }
  Serial.print("\nDurasi Ch\t: ");
  for(int i = 0; i < 2; i++){
    Serial.print(durasiInSecond[i]);
    if(i != 2 - 1){
      Serial.print(", ");
    }
  }
  Serial.print("\nState\t\t: ");
  for(int i = 0; i < checkSchedule; i++){
    Serial.print(state[i]);
    if(i != checkSchedule - 1){
      Serial.print(", ");
    }
  }
  Serial.println("");
//  Serial.print("\nCheck Schedule: ");
//  Serial.println(checkSchedule);
//  Serial.print("i: ");
//  Serial.println(i);
//  Serial.print("Schedule Index: ");
//  Serial.println(scheduleIndex);
  */
  if(req == true){
    if(ANDstate == checkSchedule){
      return amountHour[0];
    }else{
      return amountHour[i];
    }
  }else{
    if(ANDstate == checkSchedule){
      return amountMinute[0];
    }else{
      return amountMinute[i];
    }
  }
}

void startWatering(int durasi, bool channel) {
  Serial.print("Durasi: ");
  Serial.println(durasi);
  DateTime now = rtc.now();
  //new
  if(channel){
    digitalWrite(pump0, LOW);   //ACTIVE LOW
    lcd.setCursor(0,0);
    lcd.print("Channel 1: ON");
  }else{
    digitalWrite(pump1, LOW);   //ACTIVE LOW
    lcd.setCursor(0,1);
    lcd.print("Channel 2: ON");
  }
  if(tmpSecond[0] == (3600 * now.hour()) + (60 * now.minute()) + now.second()){
    durasiInSecond[0] = 0;
    tmpSecond[0] = 0;
    digitalWrite(pump0, HIGH);
    if (buttonPressed) {
      buttonPressed = !buttonPressed;
    }
    wateringChannelState[0] = false;
    lcd.setCursor(0,0);
    lcd.print("Channel 1: OFF");
  }
  if(tmpSecond[1] == (3600 * now.hour()) + (60 * now.minute()) + now.second()){
    durasiInSecond[1] = 0;
    tmpSecond[1] = 0;
    digitalWrite(pump1, HIGH);
    if (buttonPressed) {
      buttonPressed = !buttonPressed;
    }
    wateringChannelState[1] = false;
    lcd.setCursor(0,1);
    lcd.print("Channel 2: OFF");
  }
  if(wateringChannelState[0] == false && wateringChannelState[1] == false){
    wateringState = false;
    lcd.clear();
  }
}

void setChannel(int schedule){
  while(true){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Set Channel: ");
    if(channelState == true){
      lcd.print("1");
    }else if(channelState == false){
      lcd.print("2");
    }
    char key = keypad.getKey();
    while(!key){
       key = keypad.getKey();
    }
    if(key){
      if(key == KeysID[14]){
        channelState = !channelState;
      }else if(key == KeysID[15]){
        EEPROM.write(AMOUNT_PATTERN, EEPROM.read(AMOUNT_PATTERN) + 1);
        if(channelState){
          EEPROM.write(schedule * LENGTH, 1);
        }else{
          EEPROM.write(schedule * LENGTH, 2);
        }
        break;
      }
    }
  }
  lcd.clear();
}
