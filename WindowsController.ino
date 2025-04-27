#include <EEPROM.h>
#include <SoftwareSerial.h>

#define PWMA 6
#define PWMB 10
#define AIN1 8
#define AIN2 7
#define BIN1 11
#define BIN2 12
#define STBY 9
#define ENC_A0 2
#define ENC_A1 4
#define ENC_B0 3
#define ENC_B1 5

#define DOOR1_ADDR 0
#define DOOR2_ADDR 4

// RX, TX
SoftwareSerial com(A3, A4);

#define DELAY_MOTOR 50

volatile long motorPositionA = 0;
volatile long motorPositionB = 0;

long POS_A;   
long POS_B;

void updateMotorPositionA() {
  if (digitalRead(ENC_A0) != digitalRead(ENC_A1))
    motorPositionA++;
  else
    motorPositionA--;
}

void updateMotorPositionB() {
  if (digitalRead(ENC_B0) != digitalRead(ENC_B1))
    motorPositionB++;
  else
    motorPositionB--;
}

void setup() {
  com.begin(9600);
  pinMode(PWMA, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(A0, OUTPUT);
  pinMode(B0, OUTPUT);
  pinMode(A1, OUTPUT);
  pinMode(B1, OUTPUT);
  pinMode(STBY, OUTPUT);
  pinMode(13, OUTPUT);
  pinMode(ENC_A0, INPUT);
  pinMode(ENC_B0, INPUT);
  pinMode(ENC_A1, INPUT);
  pinMode(ENC_B1, INPUT);

  attachInterrupt(digitalPinToInterrupt(ENC_A0), updateMotorPositionA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B0), updateMotorPositionB, CHANGE);

  EEPROM.get(DOOR1_ADDR, POS_A);
  EEPROM.get(DOOR2_ADDR, POS_B);

  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, 255);
  analogWrite(PWMB, 255);

  delay(1000);

  analogWrite(PWMA, 0);
  analogWrite(PWMB, 0);

  digitalWrite(STBY, LOW);
  digitalWrite(13, HIGH);
  delay(400);
  digitalWrite(13, LOW);
  delay(400);
  digitalWrite(13, HIGH);
  delay(400);
  digitalWrite(13, LOW);
  char c = 'L';
  com.print(c);
}

char readCom() {
  if (com.available()) {
    char c = com.read();
    
    com.print(c);
    return (c);
  }
  return '.';
}

void flushCom() {
  while (com.available())
    char t = com.read();
}

void calibrateDoors() {
  digitalWrite(STBY, HIGH);
  long time = millis();

  com.print('C');
  // Calibration for Door 1
  while (true) {
    char c = readCom();
    if (c == 'U') { // Up
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      analogWrite(PWMA, 255);
      time = millis();
    }
    else if (c == 'D') { // Down
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      analogWrite(PWMA, 255);
      time = millis();
    }
    else if (c == 'E') // Enter
      break;
    else {// No input, stop motor
      if (time + DELAY_MOTOR < millis()) {
        analogWrite(PWMA, 0);
        time = millis();
      }
    }
    delay(5);
  }

  com.print('1');
  delay(1000);
  flushCom();
  motorPositionA = 0;
  // Door 1 closed position set. Move to open position...
  time = millis();

  while (true) {
    char c = readCom();
    if (c == 'U') { // Up
      digitalWrite(AIN1, HIGH);
      digitalWrite(AIN2, LOW);
      analogWrite(PWMA, 255);
      time = millis();
    }
    else if (c == 'D') { // Down
      digitalWrite(AIN1, LOW);
      digitalWrite(AIN2, HIGH);
      analogWrite(PWMA, 255);
      time = millis();
    }
    else if (c == 'E') // Enter
      break;
    else {// No input, stop motor
      if (time + DELAY_MOTOR < millis()) {
        analogWrite(PWMA, 0);
        time = millis();
      }
    }
    delay(5);
  }

  EEPROM.put(DOOR1_ADDR, motorPositionA);
  com.print('2');
  delay(1000);
  flushCom();
  time = millis();

  // Calibration for Door 2
  while (true) {
    char c = readCom();
    if (c == 'U') { // Up
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      analogWrite(PWMB, 255);
      time = millis();
    }
    else if (c == 'D') { // Down
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      analogWrite(PWMB, 255);
      time = millis();
    }
    else if (c == 'E') // Enter
      break;
    else {// No input, stop motor
      if (time + DELAY_MOTOR < millis()) {
        analogWrite(PWMB, 0);
        time = millis();
      }
    }
    delay(5);
  }

  com.print('3');
  delay(1000);
  flushCom();
  motorPositionB = 0;
  // Door 2 closed position set. Move to open position...
  time = millis();

  while (true) {
    char c = readCom();
    if (c == 'U') { // Up
      digitalWrite(BIN1, HIGH);
      digitalWrite(BIN2, LOW);
      analogWrite(PWMB, 255);
      time = millis();
    }
    else if (c == 'D') { // Down
      digitalWrite(BIN1, LOW);
      digitalWrite(BIN2, HIGH);
      analogWrite(PWMB, 255);
      time = millis();
    }
    else if (c == 'E') // Enter
      break;
    else {// No input, stop motor
      if (time + DELAY_MOTOR < millis()) {
        analogWrite(PWMB, 0);
        time = millis();
      }
    }
    delay(5);
  }

  com.print('x');
  delay(1000);
  flushCom();
  EEPROM.put(DOOR2_ADDR, motorPositionB);
  digitalWrite(STBY, LOW);
}

void closeDoors() {
  com.print('C');
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  digitalWrite(BIN1, LOW);
  digitalWrite(BIN2, HIGH);
  analogWrite(PWMA, 255);
  analogWrite(PWMB, 255);

  int a = 0, b = 0;
  while (a + b != 2) {
    if (motorPositionA >= 0) {
      analogWrite(PWMA, 0);
      a = 1;
    }
    if (motorPositionB >= 0) {
      analogWrite(PWMB, 0);
      b = 1;
    }
  }
  digitalWrite(STBY, LOW);
  flushCom();
}

void openDoors() {
  com.print('O');
  digitalWrite(STBY, HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
  digitalWrite(BIN1, HIGH);
  digitalWrite(BIN2, LOW);
  analogWrite(PWMA, 255);
  analogWrite(PWMB, 255);

  int a = 0, b = 0;
  while (a + b != 2) {
    if (motorPositionA <= POS_A) {
      analogWrite(PWMA, 0);
      a = 1;
    }
    if (motorPositionB <= POS_B) {
      analogWrite(PWMB, 0);
      b = 1;
    }
  }
  digitalWrite(STBY, LOW);
  flushCom();
}

void loop() {
  char c = readCom();
  if (c == 'U') {
    openDoors();
  }
  else if (c == 'D') {
    closeDoors();
  }
  else if (c == 'S') {
    calibrateDoors();
  }
  delay(100);
}
