#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

//---------COMUNICATION

#define CLOSE_ERR '0'
#define OPEN_ERR '1'
#define BOTH_SEN_ERR '2'
#define DATES_ERR '3'
#define CLOSE_SUCCESS '4'
#define OPEN_SUCCESS '5'
#define FIX_OPEN '6'
#define FIX_CLOSE '7'
#define ALREADY_OPENED 'Y'
#define ALREADY_CLOSED 'Z'
#define NOT_IDENTIFIED '8'
#define ERROR_MSG '9'
#define INFO_MSG 'A'
#define PROTOCOL_MSG 'B'
#define END_CHAR '_'

#define OPEN_DOOR 'C'
#define CLOSE_DOOR 'D'
#define CALIBRATE 'E'
#define RING_BELL 'F'
#define ASK_TIME 'H'
#define ASK_MOVING_TIMES 'I'
#define GET_BOOL_INFO 'J'
#define GET_DELAY_INFO 'K'
#define UPDATE_TIME 'L'
#define RESET_SENS_ERR 'M'
#define SET_LIGHT_DEBUG 'N'
#define SET_BELL 'O'
#define SET_REST_DOOR 'P'
#define SET_IGN_DOOR_ERR 'Q'
#define SET_OPEN_DELAY 'R'
#define SET_CLOSE_DELAY 'S'
#define SET_BELL_DELAY 'T'

//--------CUSTOM CHARACTERS
#define CURSOR_ON 0
#define CURSOR_OFF 1
#define ARROW_UP 3
#define ARROW_DOWN 4

#define WAIT_TIME 2000  //ms
#define BLINK_INTERVAL 500

#define ENTER_BUTTON 4
#define UP_BUTTON 5
#define DOWN_BUTTON 6
#define BACK_BUTTON 3

//----------------------PINS
#define LED 13
#define BUTTONS A6

const byte CursorOn[] = {
  B00000,
  B01110,
  B10001,
  B10101,
  B10001,
  B01110,
  B00000,
  B00000,
};
const byte CursorOff[] = {
  B00000,
  B01110,
  B10001,
  B10001,
  B10001,
  B01110,
  B00000,
  B00000,
};
const byte ArrowUp[] = {
  B00000,
  B00100,
  B01110,
  B10101,
  B00100,
  B00100,
  B00100,
  B00000
};
const byte ArrowDown[] = {
  B00000,
  B00100,
  B00100,
  B00100,
  B10101,
  B01110,
  B00100,
  B00000
};

RTC_DS3231 rtc;
DateTime date;
LiquidCrystal_I2C lcd(0x27, 20, 4);
SoftwareSerial HC12(A1, A0);

typedef void (*Functions)();
void openDoor();
void closeDoor();
void calibrate();
void askMovingTimes();
void askTime();
void resetSensorError();
void getBoolInfo();
void getDelayInfo();
void updateTime();
void setOpenDelay();
void setCloseDelay();
void setBellDelay();
void setLightDebug();
void setIgnSensErr();
void setBell();
const Functions functArray[] PROGMEM = {
  openDoor,
  closeDoor,
  calibrate,
  askTime,
  askMovingTimes,
  getBoolInfo,
  getDelayInfo,
  updateTime,
  setOpenDelay,
  setCloseDelay,
  setBellDelay,
  setLightDebug,
  setIgnSensErr,
  setBell,
  resetSensorError,
};

byte activeMenu = 0;
byte prevMenu = 0;
byte cursorPos = 1;
byte scrollRange = 0;
byte pageAmount = 1;
byte pageCurrent = 1;
bool blinkState = false;
bool disableCursor = true;
bool doorState = 1;

const char msg[][20] PROGMEM = {
  "Open error",
  "Close error",
  "Both sensor error",
  "Dates error",
  "Fix door, can't open",
  "Fix door, cant close",
  "Door has opened",
  "Door has closed",
  "",
};

char errMsg[4][1];
char errDate[4][12];
byte numErrMsg = 0;
byte unseenErrMsg = 0;
byte numInfoMsg = 0;
byte unseenInfoMsg = 0;

unsigned long prevMillis = 0;

void setup() {
  Serial.begin(9600);
  HC12.begin(9600);
  //rtc.adjust(DateTime(__DATE__, __TIME__));
  pinMode(BUTTONS, INPUT_PULLUP);
  lcd.init();
  lockScreen();

  lcd.createChar(0, CursorOn);
  lcd.createChar(1, CursorOff);
  lcd.createChar(3, ArrowUp);
  lcd.createChar(4, ArrowDown);
}

String getMessage() {
  String s = "";
  if (HC12.available()) {
    unsigned long startTime = millis();  // Start a timeout counter
    // Read while data is available or until timeout
    while (HC12.available()) {
      char c = HC12.read();
      s += c;
      if (c == '_')
        break;
      // Reset timeout if data is received
      startTime = millis();
      // Optional: Implement a simple timeout (e.g., 1000 ms)
      if (millis() - startTime > 2000) {
        Serial.println("time out");
        break;  // Exit if no data is received within the timeout period
      }
    }
    Serial.println(s);
  }
  return s;
}
bool blink() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevMillis >= BLINK_INTERVAL) {
    prevMillis = currentMillis;
    blinkState = !blinkState;
  }
  return blinkState;
}
void openDoor() {
  HC12.write(OPEN_DOOR);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == OPEN_DOOR) {
    lcd.print(" Door opening...");
    doorState = 0;
  } else if (s != "" && s.charAt(1) == ALREADY_OPENED)
    lcd.print(" Door already opened");
  else if (s != "" && s.charAt(1) == FIX_OPEN)
    lcd.print("   Door blocked!");
  else if (s != "")
    lcd.print("Err unexpected char");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void closeDoor() {
  HC12.print(CLOSE_DOOR);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == CLOSE_DOOR) {
    lcd.print(" Door closing...");
    doorState = 1;
  } else if (s != "" && s.charAt(1) == ALREADY_CLOSED)
    lcd.print(" Door already closed");
  else if (s != "" && s.charAt(1) == FIX_CLOSE)
    lcd.print("   Door blocked!");
  else if (s != "")
    lcd.print("Err unexpected char");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void calibrate() {
  HC12.print("E");
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == CALIBRATE)
    lcd.print(" Door calibrating...");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void askMovingTimes() {
  HC12.write(ASK_MOVING_TIMES);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(2000);
  String s = getMessage();
  if (s != "" && s.charAt(1) == ASK_MOVING_TIMES) {
    lcd.print(" OPEN TIME: " + s.substring(3, 5) + ":" + s.substring(5, 7));
    lcd.setCursor(0, 2);
    lcd.print(" CLOSE TIME: " + s.substring(7, 9) + ":" + s.substring(9, 11));
    while (1) {
      if (didPress(ENTER_BUTTON)) {
        delay(200);
        return;
      }
    }
  } else {
    lcd.print(" Error comunicating");
    delay(2000);
  }
}
void askTime() {
  HC12.write(ASK_TIME);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == ASK_TIME) {
    lcd.print(" TIME: ");
    lcd.print(s.substring(7, 9) + ":" + s.substring(9, 11));
    lcd.setCursor(0, 2);
    lcd.print(" DATE: ");
    lcd.print(s.substring(3, 5) + "/" + s.substring(5, 7));
    delay(1000);
    while (1)
      if (didPress(ENTER_BUTTON))
        break;
  } else {
    lcd.print(" Error comunicating");
    delay(2000);
  }
}
void resetSensorError() {
  //check if sensor error is up
  HC12.write(RESET_SENS_ERR);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == RESET_SENS_ERR)
    lcd.print(" Sensor error reset");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void getBoolInfo() {
  HC12.write(GET_BOOL_INFO);
  lcd.clear();
  lcd.setCursor(0, 0);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == GET_BOOL_INFO) {
    lcd.print("LightDeb:  Blocked:");
    lcd.setCursor(9, 0);
    lcd.print(s.charAt(3));
    lcd.setCursor(19, 0);
    lcd.print(s.charAt(7));
    lcd.setCursor(0, 1);
    lcd.print("Bell:  RestDoor:");
    lcd.setCursor(5, 1);
    lcd.print(s.charAt(4));
    lcd.setCursor(16, 1);
    lcd.print(s.charAt(6));
    lcd.setCursor(0, 3);
    lcd.print("IgnoreDoorErr:");
    lcd.print(s.charAt(5));
    while (1) {
      if (didPress(ENTER_BUTTON)) {
        delay(200);
        return;
      }
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
  }
}
void getDelayInfo() {
  HC12.write(GET_DELAY_INFO);
  lcd.clear();
  lcd.setCursor(0, 0);
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == GET_DELAY_INFO) {
    lcd.print("DELAY TIMES");
    lcd.setCursor(0, 1);
    lcd.print("Open delay:    min");
    lcd.setCursor(12, 1);
    lcd.print(s.substring(3, 5));
    lcd.setCursor(0, 2);
    lcd.print("Close delay:    min");
    lcd.setCursor(13, 2);
    lcd.print(s.substring(5, 7));
    lcd.setCursor(0, 3);
    lcd.print("Bell delay:    min");
    lcd.setCursor(12, 3);
    lcd.print(s.substring(7, 9));
    while (1) {
      if (didPress(ENTER_BUTTON)) {
        delay(200);
        return;
      }
    }
  } else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
  }
}
void updateTime() {
  byte minute;
  byte hour;
  byte day;
  byte month;

  HC12.write(ASK_TIME);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "") {
    minute = s.substring(9, 11).toInt();
    hour = s.substring(7, 9).toInt();
    day = s.substring(5, 7).toInt();
    month = s.substring(3, 5).toInt();
  } else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
    return;
  }
  lcd.print("NEW TIME AND DATE");
  lcd.setCursor(0, 1);
  lcd.print("TIME: m:    h:   ");
  lcd.setCursor(9, 1);
  lcd.print(minute);
  lcd.setCursor(15, 1);
  lcd.print(hour);
  lcd.setCursor(0, 2);
  lcd.print("DATE: d:    M:   ");
  lcd.setCursor(9, 2);
  lcd.print(day);
  lcd.setCursor(15, 2);
  lcd.print(month);
  while (1) {  //---edit minutes
    lcd.setCursor(9, 1);
    if (blink())
      lcd.print(minute);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      minute++;  //might need a delay
    else if (didPress(DOWN_BUTTON))
      minute--;  //might need a delay
    if (minute == 255)
      minute = 59;
    else if (minute == 60)
      minute = 0;
    delay(200);
  }
  delay(500);
  while (1) {  //---edit hours
    lcd.setCursor(15, 1);
    if (blink())
      lcd.print(hour);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      hour++;  //might need a delay
    else if (didPress(DOWN_BUTTON))
      hour--;  //might need a delay
    if (hour == 255)
      hour = 23;
    else if (hour == 24)
      hour = 0;
    delay(200);
  }
  delay(500);
  while (1) {  //---edit day
    lcd.setCursor(9, 2);
    if (blink())
      lcd.print(day);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      day++;  //might need a delay
    else if (didPress(DOWN_BUTTON))
      day--;  //might need a delay
    if (day < 1)
      day = 31;
    else if (day > 31)
      day = 1;
    delay(200);
  }
  delay(500);
  while (1) {  //---edit month
    lcd.setCursor(15, 2);
    if (blink())
      lcd.print(month);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      month++;  //might need a delay
    else if (didPress(DOWN_BUTTON))
      month--;  //might need a delay
    if (month == 255)
      month = 12;
    else if (month == 13)
      month = 1;
    delay(200);
  }
  char buffer[10];
  sprintf(buffer, "%c%02d%02d%02d%02d", UPDATE_TIME, month, day, hour, minute);
  HC12.print(buffer);
  lcd.clear();
  delay(WAIT_TIME);
  lcd.setCursor(0, 1);
  s = getMessage();
  if (s != "" && s.charAt(1) == UPDATE_TIME)
    lcd.print("    TIME UPDATED");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void setOpenDelay() {
  byte delayTime;
  HC12.write(GET_DELAY_INFO);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "")
    if (s.charAt(0) == GET_DELAY_INFO)
      delayTime = s.substring(3, 5).toInt();
    else {
      delayTime = 0;
    }
  else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
    return;
  }
  lcd.print("SET OPENING DELAY");
  lcd.setCursor(0, 1);
  lcd.print("Delay (minutes): ");
  while (1) {
    lcd.setCursor(16, 1);
    if (blink())
      lcd.print(delayTime);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      delayTime++;
    else if (didPress(DOWN_BUTTON))
      delayTime--;
    if (delayTime == 255)
      delayTime = 99;
    else if (delayTime == 100)
      delayTime = 0;
    delay(200);
  }
  char buffer[4];
  sprintf(buffer, "%c%02d", SET_OPEN_DELAY, delayTime);
  HC12.print(buffer);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  s = getMessage();
  if (s != "" && s.charAt(1) == SET_OPEN_DELAY)
    lcd.print(" New open delay set");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void setCloseDelay() {
  byte delayTime;
  HC12.write(GET_DELAY_INFO);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "")
    if (s.charAt(0) == GET_DELAY_INFO)
      delayTime = s.substring(5, 7).toInt();
    else {
      delayTime = 0;
    }
  else {
    lcd.print(" Error comunicating");
    delay(3000);
    return;
  }
  lcd.print("SET CLOSING DELAY");
  lcd.setCursor(0, 1);
  lcd.print("Delay (minutes): ");
  while (1) {
    lcd.setCursor(16, 1);
    if (blink())
      lcd.print(delayTime);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      delayTime++;
    else if (didPress(DOWN_BUTTON))
      delayTime--;
    if (delayTime == 255)
      delayTime = 99;
    else if (delayTime == 100)
      delayTime = 0;
    delay(200);
  }
  char buffer[4];
  sprintf(buffer, "%c%02d", SET_CLOSE_DELAY, delayTime);
  HC12.print(buffer);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  s = getMessage();
  if (s != "" && s.charAt(1) == SET_CLOSE_DELAY)
    lcd.print(" New close delay set");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void setBellDelay() {
  byte delayTime;
  HC12.write(GET_DELAY_INFO);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "")
    if (s.charAt(1) == GET_DELAY_INFO)
      delayTime = s.substring(7, 9).toInt();
    else {
      delayTime = 0;
    }
  else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
    return;
  }
  lcd.print("SET BELL DELAY");
  lcd.setCursor(0, 1);
  lcd.print("Delay (minutes): ");
  while (1) {
    lcd.setCursor(16, 1);
    if (blink())
      lcd.print(delayTime);
    else
      lcd.print("  ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON))
      delayTime++;
    else if (didPress(DOWN_BUTTON))
      delayTime--;
    if (delayTime == 255)
      delayTime = 99;
    else if (delayTime == 100)
      delayTime = 0;
    delay(200);
  }
  char buffer[4];
  sprintf(buffer, "%c%02d", SET_BELL_DELAY, delayTime);
  HC12.print(buffer);
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  s = getMessage();
  if (s != "" && s.charAt(1) == SET_BELL_DELAY)
    lcd.print(" New bell delay set");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void setLightDebug() {
  bool state = 0;
  HC12.write(GET_BOOL_INFO);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == GET_BOOL_INFO)
    state = s.substring(3, 4).toInt();
  else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print("  Set Light Debug");
  while (1) {
    lcd.setCursor(7, 2);
    if (blink()) {
      if (state)
        lcd.print("true");
      else
        lcd.print("false");
    } else
      lcd.print("     ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON) || didPress(DOWN_BUTTON))
      state = !state;
  }
  HC12.print(String(SET_LIGHT_DEBUG) + String(state));
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  s = getMessage();
  if (s != "" && s.charAt(1) == SET_LIGHT_DEBUG)
    lcd.print("      Updated");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void setIgnSensErr() {
  bool state = 0;
  HC12.write(GET_BOOL_INFO);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == GET_BOOL_INFO)
    state = s.substring(5, 6).toInt();
  else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print("Ignore Sensor Error");
  while (1) {
    lcd.setCursor(7, 2);
    if (blink()) {
      if (state)
        lcd.print("true");
      else
        lcd.print("false");
    } else
      lcd.print("     ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON) == LOW || didPress(DOWN_BUTTON))
      state = !state;
  }
  HC12.print(String(SET_IGN_DOOR_ERR) + String(state));
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  s = getMessage();
  if (s != "" && s.charAt(1) == SET_IGN_DOOR_ERR)
    lcd.print("      Updated");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void setBell() {
  bool state = 0;
  HC12.write(GET_BOOL_INFO);
  lcd.clear();
  delay(WAIT_TIME);
  String s = getMessage();
  if (s != "" && s.charAt(1) == GET_BOOL_INFO)
    state = s.substring(4, 5).toInt();
  else {
    lcd.setCursor(0, 1);
    lcd.print(" Error comunicating");
    delay(2000);
    return;
  }
  lcd.setCursor(0, 1);
  lcd.print("Set Warning Bell");
  while (1) {
    lcd.setCursor(7, 2);
    if (blink()) {
      if (state)
        lcd.print("true");
      else
        lcd.print("false");
    } else
      lcd.print("     ");
    if (didPress(ENTER_BUTTON))
      break;
    else if (didPress(UP_BUTTON) || didPress(DOWN_BUTTON))
      state = !state;
  }
  HC12.print(String(SET_BELL) + String(state));
  lcd.clear();
  lcd.setCursor(0, 1);
  delay(WAIT_TIME);
  s = getMessage();
  if (s != "" && s.charAt(1) == SET_BELL)
    lcd.print("      Updated");
  else
    lcd.print(" Error comunicating");
  delay(2000);
}
void lockScreen() {
  lcd.clear();
  lcd.noBacklight();
  lcd.setCursor(2, 1);
  lcd.print("CONTROL STATION");
  disableCursor = true;
}

void mainScreen() {
  cursorPos = 1;
  lcd.clear();
  lcd.print("--CONTROL STATION---");
  lcd.setCursor(1, 1);
  lcd.print("Chicken Door");
  lcd.setCursor(1, 2);
  lcd.print("Greenhouse");
  scrollRange = 3;
  pageAmount = 1;
}

void doorScreen() {
  cursorPos = 1;
  lcd.clear();
  lcd.print("----CHICKEN DOOR----");
  lcd.setCursor(1, 1);
  lcd.print("Info  {");
  lcd.print(unseenErrMsg);
  lcd.print("}");
  lcd.setCursor(1, 2);
  lcd.print("Control");
  lcd.setCursor(0, 3);
  lcd.print("Door state: ");
  if (doorState == 0)
    lcd.print("OPEN");
  else
    lcd.print("CLOSE");
  scrollRange = 3;
  pageAmount = 1;
}

void ctrlDoorScreen() {
  pageAmount = 4;
  scrollRange = 4;  //only applies for the last page
  lcd.clear();
  if (pageCurrent == 1) {
    lcd.print("------CONTROL-------");
    lcd.setCursor(1, 1);
    lcd.print("Open door");
    lcd.setCursor(1, 2);
    lcd.print("Close door");
    lcd.setCursor(1, 3);
    lcd.print("Calibrate");
    lcd.setCursor(19, 3);
    lcd.write(byte(ARROW_DOWN));
  }
  if (pageCurrent == 2) {
    lcd.setCursor(1, 0);
    lcd.print("Ask time");
    lcd.setCursor(1, 1);
    lcd.print("Ask moving times");
    lcd.setCursor(1, 2);
    lcd.print("Get bool info");
    lcd.setCursor(1, 3);
    lcd.print("Get delay info");
    lcd.setCursor(19, 3);
    lcd.write(byte(ARROW_DOWN));
    lcd.setCursor(19, 0);
    lcd.write(byte(ARROW_UP));
  }
  if (pageCurrent == 3) {
    lcd.setCursor(1, 0);
    lcd.print("Update time");
    lcd.setCursor(1, 1);
    lcd.print("Set open delay");
    lcd.setCursor(1, 2);
    lcd.print("Set close delay");
    lcd.setCursor(1, 3);
    lcd.print("Set bell delay");
    lcd.setCursor(19, 3);
    lcd.write(byte(ARROW_DOWN));
    lcd.setCursor(19, 0);
    lcd.write(byte(ARROW_UP));
  }
  if (pageCurrent == 4) {
    lcd.setCursor(1, 0);
    lcd.print("Set light debug");
    lcd.setCursor(1, 1);
    lcd.print("Set ignore sens err");
    lcd.setCursor(1, 2);
    lcd.print("Set bell");
    lcd.setCursor(1, 3);
    lcd.print("Reset sensor error");
    lcd.setCursor(19, 0);
    lcd.write(byte(ARROW_UP));
  }
}
/*
void infoDoorScreen() {
  cursorPos = 1;
  pageAmount = 1;
  scrollRange = 2;
  lcd.clear();
  lcd.print("-------INFO-------");
  lcd.setCursor(1, 1);
  lcd.print("Error messages {");
  lcd.print(unseenErrMsg);
  lcd.print("}");
}*/

void greenHouseScreen() {
  lcd.clear();
  lcd.print("coming soon");
  disableCursor = true;
}

int translate(char c) {
  if (c == OPEN_ERR)
    return (0);
  else if (c == CLOSE_ERR)
    return (1);
  else if (c == BOTH_SEN_ERR)
    return (2);
  else if (c == DATES_ERR)
    return (3);
  else if (c == FIX_OPEN)
    return (4);
  else if (c == FIX_CLOSE)
    return (5);
  else if (c == OPEN_SUCCESS)
    return (6);
  else if (c == CLOSE_SUCCESS)
    return (7);
  else
    return (8);
}

void infoEDoorScreen() {
  const char* str;
  unseenErrMsg = 0;
  pageAmount = (numErrMsg + 1) / 2;
  digitalWrite(LED, LOW);
  lcd.clear();
  lcd.setCursor(1, 0);
  str = (const char*)pgm_read_word(&(msg[translate(errMsg[pageCurrent * 2 - 2])]));
  lcd.print(str);
  lcd.setCursor(1, 1);
  lcd.print("Date: ");
  lcd.print(translate(errDate[pageCurrent * 2 - 2]));
  lcd.setCursor(1, 2);
  if (errMsg[pageCurrent * 2 - 1] != "") {
    str = (const char*)pgm_read_word(&(msg[translate(errMsg[pageCurrent * 2 - 1])]));
    lcd.print(str);
    lcd.setCursor(1, 3);
    lcd.print("Date: ");
    lcd.print(errDate[pageCurrent * 2 - 1]);
  }
  if (pageCurrent < pageAmount) {
    lcd.setCursor(19, 3);
    lcd.write(byte(ARROW_DOWN));
  }
  if (pageCurrent > 1) {
    lcd.setCursor(19, 0);
    lcd.write(byte(ARROW_UP));
  }
}
/*
void infoIDoorScreen() {
  const char* str;
  unseenInfoMsg = 0;
  pageAmount = numInfoMsg / 2;
  if (numInfoMsg % 2 != 0)
    pageAmount++;
  lcd.clear();
  lcd.setCursor(1, 0);
  str = (const char*)pgm_read_word(&(msg[translate(infoMsg[pageCurrent * 2 - 2])]));
  lcd.print(str);
  lcd.setCursor(1, 1);
  lcd.print("Date: ");
  lcd.print(infoDate[pageCurrent * 2 - 2]);
  lcd.setCursor(1, 2);
  if (infoMsg[pageCurrent * 2 - 1] != "") {
    str = (const char*)pgm_read_word(&(msg[translate(infoMsg[pageCurrent * 2 - 1])]));
    lcd.print(str);
    lcd.setCursor(1, 3);
    lcd.print("Date: ");
    lcd.print(infoDate[pageCurrent * 2 - 1]);
  }
  if (pageCurrent < pageAmount) {
    lcd.setCursor(19, 3);
    lcd.write(byte(ARROW_DOWN));
  }
  if (pageCurrent > 1) {
    lcd.setCursor(19, 0);
    lcd.write(byte(ARROW_UP));
  }
}*/

void updateMenu() {
  pageAmount = 1;
  scrollRange = 4;
  if (activeMenu == 0)
    lockScreen();
  else if (activeMenu == 1)
    mainScreen();
  else if (activeMenu == 2)
    doorScreen();
  else if (activeMenu == 3)
    ctrlDoorScreen();
  else if (activeMenu == 4)
    infoEDoorScreen();
  else if (activeMenu == 5)
    greenHouseScreen();
}

void addErrMsg(const char* newMessage) {
  digitalWrite(LED, HIGH);
  unseenErrMsg++;  // Increment unseen error messages counter

  // Shift all messages down by one, regardless of how many are in the array
  for (int i = min(numErrMsg, 3); i > 0; i--) {
    strcpy(errMsg[i], errMsg[i - 1]);    // Copy previous message to the next index
    strcpy(errDate[i], errDate[i - 1]);  // Copy previous date to the next index
  }

  // Copy the new message to errMsg[0]
  strncpy(errMsg[0], newMessage, sizeof(errMsg[0]) - 1);
  errMsg[0][sizeof(errMsg[0]) - 1] = '\0';  // Ensure null termination

  // Create the date string and store it in errDate[0]
  char dateString[12];  // Assuming this size can accommodate the date format
  snprintf(dateString, sizeof(dateString), "%02d:%02d %02d/%02d", date.hour(), date.minute(), date.day(), date.month());
  strncpy(errDate[0], dateString, sizeof(errDate[0]) - 1);
  errDate[0][sizeof(errDate[0]) - 1] = '\0';  // Ensure null termination

  if (numErrMsg < 4) {
    numErrMsg++;
  }
}
/*void addInfoMsg(const char* newMessage) {

  unseenInfoMsg++;  // Increment unseen info messages counter
  // Shift all messages down by one, regardless of how many are in the array
  for (int i = min(numInfoMsg, 9); i > 0; i--) {
    strcpy(infoMsg[i], infoMsg[i - 1]);  // Copy previous message to the next index
    strcpy(infoDate[i], infoDate[i - 1]); // Copy previous date to the next index
  }

  // Copy the new message to infoMsg[0]
  strncpy(infoMsg[0], newMessage, sizeof(infoMsg[0]) - 1);
  infoMsg[0][sizeof(infoMsg[0]) - 1] = '\0';  // Ensure null termination

  // Create the date string and store it in infoDate[0]
  char dateString[12];  // Assuming this size can accommodate the date format
  snprintf(dateString, sizeof(dateString), "%02d:%02d %02d/%02d", date.hour(), date.minute(), date.day(), date.month());
  strncpy(infoDate[0], dateString, sizeof(infoDate[0]) - 1);
  infoDate[0][sizeof(infoDate[0]) - 1] = '\0';  // Ensure null termination

  // Cap numInfoMsg at 10
  if (numInfoMsg < 10) {
    numInfoMsg++;
  }
}*/


void cursor() {
  if (disableCursor == true)
    return;
  lcd.setCursor(0, cursorPos);
  if (blink())
    lcd.write(byte(CURSOR_ON));
  else
    lcd.write(byte(CURSOR_OFF));
}

void enter() {
  delay(100);
  if (activeMenu == 0) {
    activeMenu = 1;
    lcd.backlight();
    disableCursor = false;
  } 
  else if (activeMenu == 1) {
    if (cursorPos == 1)
      activeMenu = 2;
    else
      activeMenu = 5;
  } 
  else if (activeMenu == 2) {
    if (cursorPos == 1)
      activeMenu = 4;
    else
      activeMenu = 3;
    cursorPos = 1;
  }
  else if (activeMenu == 3) {
    Functions func = (Functions)pgm_read_ptr(&(functArray[(cursorPos - 1) + ((pageCurrent - 1) * 4)]));
    func();
  }
  updateMenu();
}

void moveUp() {
  delay(100);
  lcd.setCursor(0, cursorPos);
  lcd.print(" ");
  if (cursorPos > 0 && !(pageCurrent == 1 && cursorPos == 1))
    cursorPos--;
  else if (pageCurrent > 1) {
    pageCurrent--;
    cursorPos = 3;
    updateMenu();
  }
}

void moveDown() {
  delay(100);
  lcd.setCursor(0, cursorPos);
  lcd.print(" ");
  if (cursorPos < 3 && !(cursorPos == scrollRange - 1 && pageCurrent == pageAmount))
    cursorPos++;
  else if (cursorPos == 3 && pageCurrent < pageAmount) {
    pageCurrent++;
    cursorPos = 0;
    updateMenu();
  }
}

void back() {
  delay(100);
  if (activeMenu == 4)
    activeMenu = 2;
  if (activeMenu == 5)
    activeMenu = 1;
  else if (activeMenu > 0)
    activeMenu--;
  disableCursor = false;
  updateMenu();
}

void processMessage(String s) {
  if (s.charAt(1) == OPEN_ERR)
    addErrMsg(OPEN_ERR);
  else if (s.charAt(1) == CLOSE_ERR)
    addErrMsg(CLOSE_ERR);
  else if (s.charAt(1) == BOTH_SEN_ERR)
    addErrMsg(BOTH_SEN_ERR);
  else if (s.charAt(1) == DATES_ERR)
    addErrMsg(DATES_ERR);
  else if (s.charAt(1) == FIX_OPEN)
    addErrMsg(FIX_OPEN);
  else if (s.charAt(1) == FIX_CLOSE)
    addErrMsg(FIX_CLOSE);
  else if (s.charAt(1) == OPEN_SUCCESS)
    doorState = 0;
  else if (s.charAt(1) == CLOSE_SUCCESS)
    doorState = 1;
}
bool didPress(byte n) {
  int read;
  read = analogRead(BUTTONS);
  if (n == BACK_BUTTON && read > 140 && read < 240)
    return (true);
  if (n == UP_BUTTON && read > 240 && read < 330)
    return (true);
  if (n == ENTER_BUTTON && read > 480 && read < 540)
    return (true);
  if (n == DOWN_BUTTON && read > 650 && read < 690)
    return (true);
  return (false);
}

void loop() {
  if (didPress(ENTER_BUTTON))
    enter();
  else if (didPress(UP_BUTTON))
    moveUp();
  else if (didPress(DOWN_BUTTON))
    moveDown();
  else if (didPress(BACK_BUTTON))
    back();
  String s = getMessage();
  if (s != "")
    processMessage(s);
  cursor();
  delay(100);
}
