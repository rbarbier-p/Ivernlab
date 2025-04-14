#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <RTClib.h>
#include <avr/wdt.h>

// PINS
#define RX 5
#define TX 4
#define FANS 6
#define ENC_A 8
#define ENC_B 7
#define ENC_BUTTON 9
#define ONE_WIRE_BUS A3

// SERIAL COM COMUNICATION
#define CALIBRATE 'C'
#define CLOSE_WIN 'D'
#define OPEN_WIN 'U'
#define MOVE_DOWN 'D'
#define MOVE_UP 'U'
#define SAVE_VALUE 'S'
#define ENTER 'E'
#define RESET 'R'

#define TEMP_READ_DELAY 5000
#define AFK_TIMEOUT 30000
#define MARGIN_ACTIVE_VENT 5

#define CURSOR true
#define NO_CURSOR false

#define AUTO_WINDOWS_ADDR 0
#define TEMP_PASSIVE_VENT_ADDR 1
#define ACTIVE_VENT_ADDR 2

LiquidCrystal_I2C lcd(0x27, 20, 4);
SoftwareSerial com(RX, TX);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress sensor_inside, sensor_outside;

RTC_DS3231 rtc;
DateTime date;

volatile int ENC_POS = 0;
int ENC_PREV = 0;
bool wasAFK = false;
bool windows_opened_f = false;

// SETTINGS
bool automatic_windows_f = true;
bool active_vent_f = true;

int8_t temp_passive_vent = 30;
int8_t temp_inside = 0; //provisional
int8_t temp_outside = 0; //provisional

typedef void (*SelectCallback)(int);
typedef void (*UpdateScreen)(int);

/*===============CUSTOM CHAR================*/
const uint8_t degree[8] = {0x1C, 0x14, 0x1C, 0x0, 0x0, 0x0, 0x0, 0x0};    // 0
const uint8_t up_arrow[8] = {0x4, 0xE, 0x1F, 0x4, 0x4, 0x4, 0x4, 0x0};    // 1
const uint8_t down_arrow[8] = {0x0, 0x4, 0x4, 0x4, 0x4, 0x1F, 0xE, 0x4};  // 2
const uint8_t clock[8] = {0x0,0xe,0x15,0x17,0x11,0xe,0x0};                // 3
const uint8_t bell[8]  = {0x4,0xe,0xe,0xe,0x1f,0x0,0x4};                  // 4

/*===============CLASS DEFINITION================*/
class Screen {
  public:
  SelectCallback onSelect;
  UpdateScreen update;
  const char **options;
  int8_t option_count = 0;
  bool hasCursor;

  int8_t page = 0;   // Current page (0-based)
  int8_t cursor = 0; // Cursor within the current page (0-3)

  Screen(SelectCallback onSelect, const char **options, UpdateScreen update, bool hasCursor)
    : onSelect(onSelect), options(options), update(update), hasCursor(hasCursor) {
    uint8_t i = 0;
    while (options[i])
      i++;
    option_count = i;
  }

  void render() {
    lcd.clear();
    lcd.setCursor(0, 0);

    uint8_t start = page * 4; // starting index for the page (4 options per page)
    uint8_t index;
    for (uint8_t i = 0; i < 4; i++) {
      index = start + i;
      lcd.setCursor(hasCursor, i);
      if (index < option_count)
        lcd.print(options[index]);
    }
    if (index + 1 < option_count) {
      lcd.setCursor(19, 3);
      lcd.write(byte(2));
    }
    if (page > 0) {
      lcd.setCursor(19, 0);
      lcd.write(byte(1));
    }
    if (hasCursor)
      updateCursor(cursor);
    if (update != NULL)
      update(page);
  }

  void updateCursor(uint8_t new_cursor_pos) {
    lcd.setCursor(0, cursor);
    lcd.print(F(" "));
    lcd.setCursor(0, new_cursor_pos);
    lcd.print(F(">"));
    cursor = new_cursor_pos;
  }

  // Navigation: When moving down, if you pass the bottom line, go to the next page.
  void moveDown() {
    if (hasCursor) {
      uint8_t globalIndex = page * 4 + cursor;
      if (globalIndex + 1 < option_count) {
        // If we've reached the end of the page (and thers more options), move to the next page.
        if (cursor >= 3 && (page * 4 + cursor - 1) < option_count) {
          page++;
          cursor = 0;
          render();
        }
        else
          updateCursor(cursor + 1);
      }
    } else if ((page + 1) * 4 < option_count) {
      page++;
      render();
    }
  }

  // Navigation: When moving up, if you're at the top and press up, go to previous page.
  void moveUp() {
    if (hasCursor) {
      uint8_t globalIndex = page * 4 + cursor;
      if (globalIndex > 0) {
        if (cursor < 1) {
          page--;
          cursor = 3;
          render();
        }
        else
          updateCursor(cursor - 1);
      }
    } else if (page > 0) {
      page--;
      render();
    }
  }

  // Return the current option's global index.
  int getSelectedIndex() const {
    return page * 4 + cursor;
  }
};

Screen *current_screen;

void mainMenuSelect(int);
void defaultSelect(int);
int sendToCom(char);
void updateInfoScreen(int);
void settingsSelect(int);
void updateSettings(int);

/*===============OPTIONS================*/
const char* optionsMainMenu[] = {
  "Open windows",
  "Close windows",
  "Calibrate windows",
  "Set temperature",
  "Info sensors",
  "Settings",
  "mes",
  "i meeees",
  "i fins aqui",
  nullptr
};
const char* optionsInfoScreen[] = {
  "Inside Temp: 00 C",
  "Outside Temp: 00 C",
  "Date: 12/04/2025",
  "Time: 16:07",
  "hola",
  nullptr
};
const char* optionsSettings[] = {
  "[Settings]",
  "Auto. windows <",
  "Active vent. <",
  nullptr
};

/*===============SCREENS CONSTRUCTION================*/
Screen mainScreen(mainMenuSelect, optionsMainMenu, NULL, CURSOR);
Screen infoScreen(defaultSelect, optionsInfoScreen, updateInfoScreen, NO_CURSOR);
Screen settingsScreen(settingsSelect, optionsSettings, updateSettings, CURSOR);

/*===============ON SELECTs================*/
void mainMenuSelect(int index) {
  Serial.println(index);
  switch(index) {
    case 0:
      lcd.clear();
      lcd.setCursor(1, 1);
      if (!sendToCom(OPEN_WIN)) {
        lcd.print(F("Communication error"));
        delay(1000);
      }
      else {
        lcd.print(F("Opening windows..."));
        if (windows_opened_f == true) {
          lcd.setCursor(2, 2);
          lcd.print(F("(already opened)"));
        }
        else {
          automatic_windows_f = false;
          EEPROM.put(AUTO_WINDOWS_ADDR, automatic_windows_f);
        }
        windows_opened_f = true;
        delay(2000);
      }
      current_screen->render();
      break;
    case 1: 
      lcd.clear();
      lcd.setCursor(1, 1);
      if (!sendToCom(CLOSE_WIN)) {
        lcd.print(F("Communication error"));
        delay(1000);
      }
      else {
        lcd.print(F("Closing windows..."));
        if (windows_opened_f == false) {
          lcd.setCursor(2, 2);
          lcd.print(F("(already closed)"));
        }
        else {
          automatic_windows_f = false;
          EEPROM.put(AUTO_WINDOWS_ADDR, automatic_windows_f);
        }
        windows_opened_f = false;
        delay(2000);
      }
      current_screen->render();
      break;
    case 2: calibrateWindows(); break;
    case 3: setTemperature(); break;
    case 4: goToScreen(&infoScreen); break;
    case 5: goToScreen(&settingsScreen); break;
  }
}

void settingsSelect(int index) {
  switch(index) {
    case 0:
      EEPROM.put(AUTO_WINDOWS_ADDR, automatic_windows_f); 
      EEPROM.put(ACTIVE_VENT_ADDR, active_vent_f); 
      goToScreen(&mainScreen);
      break;
    case 1: 
      automatic_windows_f = !automatic_windows_f;
      current_screen->update(0);
      break;
    case 2:
      active_vent_f = !active_vent_f;
      current_screen->update(0);
      break;
  }
}

void defaultSelect(int index) {
  goToScreen(&mainScreen);
}

/*===============UPDATE FUNCTIONS================*/
void updateInfoScreen(int page) {
  if (page == 0) {
    lcd.setCursor(13, 0);
    lcd.print(temp_inside);
    lcd.setCursor(15, 0);
    lcd.write(byte(0));
    lcd.setCursor(14, 1);
    lcd.print(temp_outside);
    lcd.setCursor(16, 1);
    lcd.write(byte(0));
  }
}

void updateSettings(int page) {
  if (page == 0) {
    lcd.setCursor(16, 1);
    if (automatic_windows_f == false)
      lcd.print(F("OFF>"));
    else
      lcd.print(F("ON> "));
    lcd.setCursor(15, 1);
    if (active_vent_f == false)
      lcd.print(F("OFF>"));
    else
      lcd.print(F("ON> "));
  }
}

/*===============HELPER FUNCTIONS================*/

void goToScreen(Screen *new_screen) {
  current_screen = new_screen;
  new_screen->render();
}

int sendToCom(char c) {
  com.listen();
  while (com.available())
   char t = com.read();
  com.print(c);
  delay(100);
  unsigned long wait = millis();
  while (millis() - wait < 1000) {
    com.listen();
    if (com.available()) {
      char t = com.read();
      if (t == c)
        return (1); // success
      else {
        com.print(c);
      }
    }
    delay(5);
  }
  com.print(RESET);
  return (0); // fail
}

/*===============SPECIAL SCREENS================*/
void calibrateWindows() {
  //render
  lcd.clear();
  if (!sendToCom(CALIBRATE)) {
    lcd.setCursor(0, 1);
    lcd.print(F("Communication error"));
    delay(1000);
    current_screen->render();
    return ;
  }
  lcd.setCursor(4, 0);
  lcd.print(F("Calibration"));
  lcd.setCursor(1, 1);
  lcd.print(F("<= DOWN      UP =>"));
  lcd.setCursor(0, 2);
  lcd.print(F("set closed position"));
  lcd.setCursor(4, 3);
  lcd.print(F("Door 1 (1/2)"));

  int8_t mode = 0;
  int8_t stage = 0;
  bool changed_stage = false;
  bool click_f = false;
  while (1) {
    if (ENC_PREV < ENC_POS) {
      mode++;
      ENC_PREV = ENC_POS;
    }
    else if (ENC_PREV > ENC_POS) {
      mode--;
      ENC_PREV = ENC_POS;
    }

    mode = min(1, max(-1, mode));

    if (mode < 0)
      com.print(MOVE_DOWN);
    if (mode > 0)
      com.print(MOVE_UP);
    
    if (click_f && digitalRead(ENC_BUTTON) == LOW) {
      changed_stage = true;
      stage++;
      click_f = false;
    }
    if (!click_f && digitalRead(ENC_BUTTON) == HIGH)
      click_f = true;
    
    if (changed_stage) {
      if (stage == 1) {
        com.print('E');
        lcd.setCursor(0, 2);
        lcd.print(F("set opened position"));
        lcd.setCursor(12, 3);
        lcd.print(F("2"));
        mode = 0;
      }
      if (stage == 2) {
        com.print('E');
        lcd.setCursor(0, 2);
        lcd.print(F("set closed position"));
        lcd.setCursor(9, 3);
        lcd.print(F("2 (1"));
        mode = 0;
      }
      if (stage == 3) {
        com.print('E');
        lcd.setCursor(0, 2);
        lcd.print(F("set opened position"));
        lcd.setCursor(12, 3);
        lcd.print(F("2"));
        mode = 0;
      }
      if (stage == 4) {
        lcd.clear();
        lcd.setCursor(1, 1);
        if (!sendToCom(SAVE_VALUE)) {
          lcd.print(F("Communication error"));
        }
        else
          lcd.print(F("Calibration done !"));
        delay(1000);
        break;
      }
      changed_stage = false;
    }
    windows_opened_f = true;
    wdt_reset();
    delay(50);
  }
  current_screen->render();
}

void setTemperature() {
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print(F("Temperature for"));
  lcd.setCursor(0, 1);
  lcd.print(F("passive ventilation"));
  lcd.setCursor(6, 2);
  lcd.print(F("<    > "));
  lcd.write(byte(0));
  lcd.print(F("C"));

  delay(200);

  bool modification_f = false;
  unsigned long time;
  bool blink = true;
  lcd.setCursor(8, 2);
  lcd.print(temp_passive_vent);
  while (1) {
    if (blink && millis() - time >= 800) {
      lcd.setCursor(6, 2);
      lcd.print(F(" "));
      lcd.setCursor(11, 2);
      lcd.print(F(" "));
      time = millis();
      blink = false;
    }
    if (!blink && millis() - time >= 500) {
      lcd.setCursor(6, 2);
      lcd.print(F("<"));
      lcd.setCursor(11, 2);
      lcd.print(F(">"));
      time = millis();
      blink = true;
    }
    if (ENC_PREV > ENC_POS) {
      temp_passive_vent--;
      lcd.setCursor(8, 2);
      lcd.print(temp_passive_vent);
      ENC_PREV = ENC_POS;
      modification_f = true;
    }
    if (ENC_PREV < ENC_POS) {
      temp_passive_vent++;
      lcd.setCursor(8, 2);
      lcd.print(temp_passive_vent);
      ENC_PREV = ENC_POS;
      modification_f = true;
    }
    if (digitalRead(ENC_BUTTON) == LOW) {
      if (modification_f) {
        EEPROM.put(TEMP_PASSIVE_VENT_ADDR, temp_passive_vent);
        lcd.clear();
        lcd.setCursor(1, 1);
        lcd.print(F("Temperature saved !"));
        delay(1500);
      }
      break;
    }
    wdt_reset();
    delay(20);
  }
  current_screen->render();
}

void setup() {
  pinMode(FANS, OUTPUT);
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BUTTON, INPUT_PULLUP);

  Serial.begin(9600);
  com.begin(9600);
  Wire.begin();

  if (! rtc.begin()) {
    Serial.println("Module RTC not found");
  }
  //rtc.adjust(DateTime(__DATE__, __TIME__));

  if (sensors.getDeviceCount() < 2) {
    Serial.println("Error: Less than two DS18B20 sensors detected!");
  }

  sensors.getAddress(sensor_inside, 1);
  sensors.getAddress(sensor_outside, 0);

  wdt_enable(WDT_PERIOD_4KCLK_gc);

  attachInterrupt(digitalPinToInterrupt(ENC_A), read_encoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), read_encoder, CHANGE);

  EEPROM.get(AUTO_WINDOWS_ADDR, automatic_windows_f);
  EEPROM.get(TEMP_PASSIVE_VENT_ADDR, temp_passive_vent);
  EEPROM.get(ACTIVE_VENT_ADDR, active_vent_f);
  
  Wire.setClock(50000);
  lcd.init();

  lcd.createChar(0, degree);
  lcd.createChar(1, up_arrow);
  lcd.createChar(2, down_arrow);
  lcd.createChar(3, clock);
  lcd.createChar(4, bell);
  
  lcd.backlight();
  current_screen = &mainScreen;
  current_screen->render();

  Serial.println("Listo");
}

bool click_f = true;
int i = 0;

bool reset_enc_val_f = true;
volatile int8_t enc_val = 0; // encoder value
void read_encoder() {
  static uint8_t old_AB = 3; //lookup table index
  static const int8_t enc_states[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0}; // lookup table

  old_AB <<=2; //Remember previous state

  if (digitalRead(ENC_A)) old_AB |= 0x02; //add current state of pin A
  if (digitalRead(ENC_B)) old_AB |= 0x01; //add current state of pin B

  enc_val += enc_states[( old_AB & 0x0f )];

  //Update counter if encoder has rotated a full indent
  if (enc_val > 3 or enc_val < -3) {
    int change_val = (enc_val > 0) - (enc_val < 0);
    ENC_POS += change_val;
    ENC_POS = min(100, max(-100, ENC_POS));
    enc_val = 0;
  }
}

unsigned long last_user_input = 0;
void handleInput() {
  if (ENC_PREV > ENC_POS) {
    if (!wasAFK)
      current_screen->moveDown();
    ENC_PREV = ENC_POS;
    last_user_input = millis();
    reset_enc_val_f = true;
  }

  if (ENC_PREV < ENC_POS) {
    if (!wasAFK)
      current_screen->moveUp();
    ENC_PREV = ENC_POS;
    last_user_input = millis();
    reset_enc_val_f = true;
  }

  if (click_f && digitalRead(ENC_BUTTON) == LOW) {
    if (!wasAFK) 
      current_screen->onSelect(current_screen->getSelectedIndex());
    click_f = false;
    last_user_input = millis();
    reset_enc_val_f = true;
  }
  if (!click_f && digitalRead(ENC_BUTTON) == HIGH)
    click_f = true;
}

void saveScreen(bool goneAFK) {
  if (goneAFK != wasAFK) {
    if (goneAFK == true) {
      lcd.clear();
      lcd.setCursor(5, 1);
      lcd.print(F("Ivernlab"));
      lcd.noBacklight();
    }
    else {
      lcd.backlight();
      current_screen->render();
    }
    wasAFK = goneAFK;
  }
}

void readTemperatures() {
  sensors.requestTemperatures(); // Request temperature from both sensors
  temp_inside = sensors.getTempC(sensor_inside);
  temp_outside = sensors.getTempC(sensor_outside);
}

unsigned long time = 0;
void loop() {
  handleInput();
  if (millis() - time >= TEMP_READ_DELAY) {
    //Serial.println(millis());
    readTemperatures();
    if (active_vent_f && temp_inside > temp_passive_vent + MARGIN_ACTIVE_VENT)
      digitalWrite(FANS, HIGH);
    else
      digitalWrite(FANS, LOW);
    if (automatic_windows_f && !windows_opened_f && temp_inside > temp_passive_vent) {
      com.print(OPEN_WIN);
      windows_opened_f = true;
    }
    if (automatic_windows_f && windows_opened_f && temp_inside < temp_passive_vent) {
      com.print(CLOSE_WIN);
      windows_opened_f = false;
    }
    if (current_screen->update != NULL && !wasAFK)
      current_screen->update(current_screen->page);
    time = millis();
  }
  if (millis() - last_user_input >= AFK_TIMEOUT) {
    saveScreen(true);
  }
  else if (wasAFK) {
    saveScreen(false);
  if (!reset_enc_val_f && millis() - last_user_input >= 5000) {
    enc_val = 0;
    reset_enc_val_f = false;
  }
  wdt_reset();
  }
  delay(5);
}

