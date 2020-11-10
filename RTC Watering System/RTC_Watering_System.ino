/*
 * EEPROM Mapping
 * address 0 --> 2                      : realtime clock (hh:mm:ss)
 * address 3                            : amount of pattern
 * address 4 =< (i*5) - 1 --> (i*5) + 3 : watering time (schedule number, duration in minutes, duration in seconds, hour, minute)
 */
#include <EEPROM.h>
#include <Keypad.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"

#define pump 10
#define AMOUNT_PATTERN 3
#define SCHEDULE AMOUNT_PATTERN + 1

const byte ROWS = 4;
const byte COLS = 4;

byte rowPins[ROWS] = {9,8,7,6};
byte colPins[COLS] = {5,4,3,2};

bool state[340];
bool stateDurasi = false;
bool watering = false;
bool wateringDone = false;
bool buttonPressed = false;

String input = "";
String jam[2],menit[2];

int iJam,iMenit,counter = 0, wateringCounter = 0;

unsigned int durasiInSecond = 0;
unsigned int currentWatering = 0;
unsigned int tmpSecond = 0;
unsigned int tick = 0;

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
void setWateringTime(int schedule);
void decodeClock(int schedule);
void startWatering(int durasi);
void addSchedule(int schedule);
void setDurasi(int durasi);

void setup() {
  Serial.begin(115200);
  //rtc setup
  if(!rtc.begin()){
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }else{
    Serial.println("RTC Started");
  }
  if(!rtc.isrunning()){
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  //
  pinMode(pump,OUTPUT);
  digitalWrite(pump,HIGH);
  lcd.begin(16,2);
  lcd.backlight();
  lcd.setCursor(0,0);
  reset();
}

void loop() {
  DateTime now = rtc.now();
  checkTime();
  EEPROM.write(0, now.hour());
  EEPROM.write(1, now.minute());
  EEPROM.write(2, now.second());
  lcd.setCursor(0,0);
  lcd.print(EEPROM.read(0));
  lcd.print(":");
  lcd.print(EEPROM.read(1));
  lcd.print(":");
  lcd.print(EEPROM.read(2));
  char key = keypad.getKey();
  if(key){
    if(key == KeysID[12]){
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("Watering Time!!!");
      buttonPressed = true;
      startWatering(30);
    }else if(key == KeysID[14]){
      addSchedule(EEPROM.read(AMOUNT_PATTERN) + 1);
    }else if(key == KeysID[3]){
      for(unsigned int i = 0; i < EEPROM.length(); i++){
        EEPROM.write(i, 0);
      }
      reset(); // reset EEPROM
      lcd.clear();
      lcd.setCursor(0,0);
    }else if(key == KeysID[7]){
      if(!rtc.begin()){
        Serial.println("Couldn't find RTC");
        Serial.flush();
        abort();
      }
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); // adjust clock
    }else if(key == KeysID[11]){
      addSchedule(EEPROM.read(AMOUNT_PATTERN) + 1);
      setDurasi(EEPROM.read(AMOUNT_PATTERN) + 1);
    }
  }
}

void reset(){
  if(EEPROM.read(AMOUNT_PATTERN) == 0){
    addSchedule(1);
    setDurasi(1);
    for(unsigned short int i = 0; i < EEPROM.length(); i++){
      state[i] = false;
    }
  }
}

void addSchedule(int schedule){
  lcd.clear();
  lcd.print("Add Schedule");
  input = "";
  stateDurasi = false;
  getClock(schedule);
}

void setDurasi(int schedule){
  lcd.clear();
  lcd.print("Set Durasi");
  input = "";
  stateDurasi = true;
  getClock(schedule);
}

void setWateringTime(int schedule){
  lcd.clear();
  lcd.print("Watering Time");
  input = "";
  getClock(schedule);
}

void getClock(int schedule){
  lcd.setCursor(2,2);
  lcd.print(":");
  unsigned int jml = 0;
  while(true){
    char key = keypad.getKey();
    if(key){
      if(key != KeysID[3] && key != KeysID[7] && key != KeysID[11] && key != KeysID[12] && key != KeysID[14] && key != KeysID[15]){
        if(jml == 2){
          input += ":";
          jml++;
        }
        input += key;
        jml++;
        for(unsigned int x = 0; x < input.length(); x++){
          lcd.setCursor(x,1);
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
void decodeClock(int schedule){
  jam[1] = "";
  menit[1] = "";
  jam[1] += input[0];
  jam[1] += input[1];
  iJam = jam[1].toInt();
  menit[1] += input[3];
  menit[1] += input[4];
  iMenit = menit[1].toInt();
  Serial.print(iJam);
  Serial.print(":");
  Serial.println(iMenit);
  if(iJam > 23 || iMenit > 59){
    lcd.clear();
    lcd.setCursor(0,0);
    Serial.print("INPUT SALAH");
   addSchedule(schedule);
  }
  if(stateDurasi){
    EEPROM.write(AMOUNT_PATTERN, EEPROM.read(AMOUNT_PATTERN) + 1);
    EEPROM.write(schedule*5 - 1, schedule);
    EEPROM.write(schedule*5, iJam);
    EEPROM.write(schedule*5 + 1, iMenit);
    stateDurasi = false;
  }else{
    EEPROM.write(schedule*5 + 2, iJam);
    EEPROM.write(schedule*5 + 3, iMenit);
  }
  lcd.clear();
}
void checkTime(){ //gurung bener
  DateTime now = rtc.now();
  int checkSchedule = EEPROM.read(AMOUNT_PATTERN);
  if(checkSchedule != 0){
    for(int schedule = 0; schedule < checkSchedule; schedule++){
      int checkHour = EEPROM.read(((schedule + 1) * 5) + 2);
      int checkMinute = EEPROM.read(((schedule + 1) *5) + 3);
      int checkDurasiMinute = EEPROM.read((schedule + 1) * 5);
      int checkDurasiSecond = EEPROM.read(((schedule + 1) * 5) + 1);
      durasiInSecond = (checkDurasiMinute * 60) + checkDurasiSecond;
      if(now.hour() == checkHour && now.minute() == checkMinute && wateringDone == false || buttonPressed == true){
        if(state[schedule] == false){
          state[schedule] = true;
          startWatering(durasiInSecond);
          wateringDone = true;
        }
      }else if(now.hour() != checkHour){
        if(now.minute() != checkMinute){
          wateringDone = false;
        }
      }
    }
  }
}

void startWatering(int durasi){
  DateTime now = rtc.now();
  watering = true;
  digitalWrite(pump,LOW);    //ACTIVE LOW
  lcd.clear();
  tmpSecond = (3600*now.hour()) + (60*now.minute()) + now.second() + durasi;
  while(true){
    DateTime now = rtc.now();
    delay(10);
    lcd.setCursor(0,0);
    lcd.print("Watering Time!");
    if(tmpSecond == (3600*now.hour()) + (60*now.minute()) + now.second()){
      watering = false;
      durasiInSecond = 0;
      tmpSecond = 0;
      digitalWrite(pump, HIGH);
      if(buttonPressed){
        buttonPressed = !buttonPressed;
      }
      lcd.clear();
      break;
    }
  }
}