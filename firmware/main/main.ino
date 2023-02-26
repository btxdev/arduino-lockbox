#include <Wire.h>
#include <Servo.h>
#include <EEPROM.h>
#include <EncButton2.h>
#include <iarduino_RTC.h>
#include <LiquidCrystal_I2C.h>

#define PROGRAM_VERSION "1.0.0"

#define CLOCK_RST     8
#define CLOCK_DAT     7
#define CLOCK_CLK     6

#define BATTERY_ADC   A0
#define BUZZER        3
#define SERVO         5

#define BTN_PREV      9
#define BTN_OK        10
#define BTN_NEXT      11

#define MENU_NONE     0
#define MENU_MAIN     1
#define MENU_L_TIMER  2
#define MENU_L_RANDOM 3
#define MENU_SETTINGS 4
#define MENU_MELODY   5
#define MENU_SLEEP    6

#define CHAR_BAT0     0
#define CHAR_BAT1     1
#define CHAR_BAT2     2
#define CHAR_CLOCK    3
#define CHAR_LOCK     4
#define CHAR_MELODY   5

#define B_TRUE(bp,bb)      bp |= bb
#define B_FALSE(bp,bb)     bp &= ~(bb)
#define B_READ(bp,bb)      bool(bp & bb)
#define B_WRITE(bp,bb,val) if(val) { B_TRUE(bp, bb); } else { B_FALSE(bp, bb); }

Servo servo;
LiquidCrystal_I2C lcd(0x27, 16, 2);
EncButton2<EB_BTN> btnPrev(INPUT, BTN_PREV);
EncButton2<EB_BTN> btnOk(INPUT, BTN_OK);
EncButton2<EB_BTN> btnNext(INPUT, BTN_NEXT);
iarduino_RTC clock(RTC_DS1302, CLOCK_RST, CLOCK_CLK, CLOCK_DAT);

String EEPROMvalidationCode = String("LockBox v") + String(PROGRAM_VERSION) + String(" ");

byte symbolBattery0[] = {
  B00110,
  B01111,
  B01001,
  B01001,
  B01001,
  B01111,
  B01111,
  B00000
};

byte symbolBattery1[] = {
  B00110,
  B01111,
  B01001,
  B01111,
  B01111,
  B01111,
  B01111,
  B00000
};

byte symbolBattery2[] = {
  B00110,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
  B01111,
  B00000
};

byte symbolClock[] = {
  B00000,
  B01110,
  B10101,
  B10101,
  B10111,
  B10001,
  B01110,
  B00000
};

byte symbolLock[] = {
  B00000,
  B01110,
  B10001,
  B10001,
  B11111,
  B11011,
  B11011,
  B11111
};

byte symbolMelody[] = {
  B00100,
  B00110,
  B00101,
  B00101,
  B00100,
  B11100,
  B11100,
  B11000
};

uint8_t menu = MENU_NONE;
uint8_t menuPos = 0;
bool playMelody = true;
bool doSleep = true;
uint32_t renderMillis = millis();
uint32_t targetTime = 1677049863;
int8_t interfaceSeconds = 0;
int8_t interfaceMinutes = 0;
int16_t interfaceHours = 0;
bool interfaceClockMode = false;
bool doNotResetMenu = false;
uint8_t previousBatteryCharge = 100;
bool melodyPlayed = true;

uint8_t getBatteryCharge() {
  uint8_t charge = map(analogRead(BATTERY_ADC), 327, 430, 0, 100);
  if(abs(charge - previousBatteryCharge) >= 10) {
    previousBatteryCharge = charge;
    return charge;
  }
  else {
    return previousBatteryCharge;
  }
}

bool isLocked() {
  return targetTime > clock.gettimeUnix();
}

void renderClock() {
  lcd.setCursor(0, 0);
  lcd.write(CHAR_CLOCK);
  lcd.print(" ");
  lcd.print(clock.gettime("H:i:s"));
}

void renderCountdown() {
  uint32_t currentTime = clock.gettimeUnix();
  lcd.setCursor(0, 1);
  lcd.write(CHAR_LOCK);
  if(!isLocked()) {
    lcd.print(" Unlocked   ");
    return;
  }
  uint32_t diffUnix = targetTime - clock.gettimeUnix(); // 210073
  uint16_t days = diffUnix / 60 / 60 / 24; // 2
  uint8_t hours = diffUnix / 60 / 60 % 24; // 10
  uint8_t minutes = diffUnix / 60 % 60; // 21
  uint8_t seconds = diffUnix % 60; // 13
  lcd.print(" ");
  if(days > 0) {
    lcd.print(days);
    lcd.print(" days ");
  }
  else {
    lcdPrintInterfaceClock(hours, minutes, seconds);
  }
}

void renderBattery() {
  lcd.setCursor(14, 0);
  if(playMelody) lcd.write(CHAR_MELODY);
  lcd.setCursor(15, 0);
  uint8_t charge = getBatteryCharge();
  if(charge > 75) lcd.write(CHAR_BAT2);
  if(charge > 45) lcd.write(CHAR_BAT1);
  else lcd.write(CHAR_BAT0);
}

uint8_t getMenuMaxPos(uint8_t menu) {
  switch(menu) {
    case MENU_NONE:
      return 0;
    case MENU_MAIN:
      return 3;
    case MENU_SETTINGS:
      return 2;
    case MENU_MELODY:
      return 2;
    case MENU_SLEEP:
      return 2;
    case MENU_L_TIMER:
      return 4;
  }
}

void lcdPrintInterfaceClock(uint8_t h, uint8_t m, uint8_t s) {
  if(h < 10) lcd.print("0");
  lcd.print(h);
  lcd.print(":");
  if(m < 10) lcd.print("0");
  lcd.print(m);
  lcd.print(":");
  if(s < 10) lcd.print("0");
  lcd.print(s);
}

void lcdPrintInterfaceClock() {
  lcdPrintInterfaceClock(interfaceHours, interfaceMinutes, interfaceSeconds);
}

uint32_t convertToUnix(uint8_t h, uint8_t m, uint8_t s) {
  return h * 60 * 60 + m * 60 + s;
}

bool isEEPROMValid() {
  Serial.println("Check if eepom data is valid");
  Serial.print("Code: ");
  Serial.println(EEPROMvalidationCode);
  for(int addr = 10; addr < EEPROMvalidationCode.length() + 10; addr++) {
    bool code = EEPROM.read(addr);
    Serial.print(code);
    Serial.print(" ");
    if(code != EEPROMvalidationCode[addr]) {
      Serial.println();
      Serial.println("INVALID CODE");
      return false;
    }
  }
  Serial.println("CODE IS VALID");
  return true;
}

void saveSettingsToEEPROM() {
  for(int addr = 10; addr < EEPROMvalidationCode.length() + 10; addr++) {
    EEPROM.write(addr, EEPROMvalidationCode[addr]);
  }
  // bool: playMelody, doSleep, melodyPlayed
  bool packed = false;
  B_WRITE(packed, 1, playMelody);
  B_WRITE(packed, 2, doSleep);
  B_WRITE(packed, 4, melodyPlayed);
  EEPROM.write(15 + 10, packed);
  // uint32_t: targetTime
  EEPROM.put(16 + 10, targetTime);
  for(int i = 0; i < 3; i++) {
    tone(BUZZER, 440, 50);
    delay(50);
  }
}

void loadSettingsFromEEPROM() {
  Serial.println("Reading data from EEPROM:");
  // bool: playMelody, doSleep, melodyPlayed
  bool packed = EEPROM.read(15 + 10);
  Serial.print("packed byte: ");
  Serial.println(packed);
  playMelody = B_READ(packed, 1);
  doSleep = B_READ(packed, 2);
  melodyPlayed = B_READ(packed, 4);
  Serial.println(playMelody);
  Serial.println(doSleep);
  Serial.println(melodyPlayed);
  // uint32_t: targetTime
  EEPROM.get(16 + 10, targetTime);
  Serial.print("targetTime: ");
  Serial.println(targetTime);
}

void setup() {

  Serial.begin(9600);

  if(isEEPROMValid()) {
    loadSettingsFromEEPROM();
  }
  else {
    tone(BUZZER, 2000, 2000);
    delay(500);
    saveSettingsToEEPROM();
  }

  servo.attach(SERVO);

  pinMode(BUZZER, OUTPUT);

  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);

  pinMode(BATTERY_ADC, INPUT);

  clock.begin();

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.createChar(CHAR_BAT0, symbolBattery0);
  lcd.createChar(CHAR_BAT1, symbolBattery1);
  lcd.createChar(CHAR_BAT2, symbolBattery2);
  lcd.createChar(CHAR_CLOCK, symbolClock);
  lcd.createChar(CHAR_LOCK, symbolLock);
  lcd.createChar(CHAR_MELODY, symbolMelody);
}

void loop() {

  // servo
  if(isLocked()) servo.write(90);
  else servo.write(-90);

  // melody
  if(!isLocked() && !melodyPlayed) {
    if(playMelody) {
      tone(BUZZER, 622, 500);
      delay(70);
      tone(BUZZER, 932, 500);
      delay(70);
      tone(BUZZER, 1244, 500);
      delay(70);
    }
    melodyPlayed = true;
  }

  // menu button logic
  btnPrev.tick();
  btnOk.tick();
  btnNext.tick();
  if(btnOk.click()) {

    doNotResetMenu = false;

    switch(menu) {

      case MENU_NONE:
        menu = MENU_MAIN;
        break;
      
      case MENU_MAIN:
        switch(menuPos) {
          case 0:
            menu = MENU_NONE;
            break;
          case 1:
            interfaceHours = 0;
            interfaceMinutes = 0;
            interfaceSeconds = 0;
            menu = MENU_L_TIMER;
            break;
          case 2:
            menu = MENU_L_RANDOM;
            break;
          case 3:
            menu = MENU_SETTINGS;
            break;
        }
        break;

      case MENU_SETTINGS:
        switch(menuPos) {
          case 0:
            menu = MENU_MAIN;
            break;
          case 1:
            menu = MENU_MELODY;
            break;
          case 2:
            menu = MENU_SLEEP;
            break;
        }
        break;

      case MENU_MELODY:
        menu = MENU_SETTINGS;
        switch(menuPos) {
          case 1:
            playMelody = false;
            saveSettingsToEEPROM();
            break;
          case 2:
            playMelody = true;
            saveSettingsToEEPROM();
            break;
        }
        break;

      case MENU_SLEEP:
        menu = MENU_SETTINGS;
        switch(menuPos) {
          case 1:
            doSleep = false;
            saveSettingsToEEPROM();
            break;
          case 2:
            doSleep = true;
            saveSettingsToEEPROM();
            break;
        }
        break;

      case MENU_L_TIMER:
        switch(menuPos) {
          case 0:
            menu = MENU_MAIN;
            break;
          case 1:
          case 2:
          case 3:
            interfaceClockMode = !interfaceClockMode;
            doNotResetMenu = true;
            break;
          case 4:
            targetTime = clock.gettimeUnix() + convertToUnix(interfaceHours, interfaceMinutes, interfaceSeconds);
            interfaceClockMode = false;
            menu = MENU_NONE;
            menuPos = 0;
            melodyPlayed = false;
            saveSettingsToEEPROM();
            break;
        }
        break;

    }
    if(interfaceClockMode) {}
    else if(!doNotResetMenu) {
      menuPos = 0;
      lcd.clear();
      renderMillis = millis() - 1000;
      doNotResetMenu = true;
    }
  }
  if(btnPrev.click()) {
    if(interfaceClockMode) {
      switch(menuPos) {
        case 1:
          interfaceHours--;
          break;
        case 2:
          interfaceMinutes--;
          break;
        case 3:
          interfaceSeconds--;
          break;
      }
    }
    else {
      menuPos--;
      lcd.clear();
      renderMillis = millis() - 1000;
    }
  }
  if(btnNext.click()) {
    if(interfaceClockMode) {
      switch(menuPos) {
        case 1:
          interfaceHours++;
          break;
        case 2:
          interfaceMinutes++;
          break;
        case 3:
          interfaceSeconds++;
          break;
      }
    }
    else {
      menuPos++;
      lcd.clear();
      renderMillis = millis() - 1000;
    }
  }

  // menu limiter
  uint8_t maxPos = getMenuMaxPos(menu);
  if(menuPos == 255) menuPos = 0;
  if(menuPos > maxPos) menuPos = maxPos;

  // interface clock limiter
  if(interfaceSeconds > 59) interfaceSeconds = 0;
  if(interfaceMinutes > 59) interfaceMinutes = 0;
  if(interfaceHours > 32767) interfaceHours = 0;
  if(interfaceSeconds < 0) interfaceSeconds = 59;
  if(interfaceMinutes < 0) interfaceMinutes = 59;
  if(interfaceHours < 0) interfaceHours = 32767;

  // render
  if(menu == MENU_NONE) {
    if((millis() - renderMillis) > 1000) {
      renderMillis = millis();
      renderClock();
      renderCountdown();
      renderBattery();
      delay(10);
    }
  }
  else if(menu == MENU_MAIN) {
    switch(menuPos) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print(">Back              ");
        lcd.setCursor(0, 1);
        lcd.print(" L: timer   ");
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(" Back     ");
        lcd.setCursor(0, 1);
        lcd.print(">L: timer   ");
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print(" L: timer   ");
        lcd.setCursor(0, 1);
        lcd.print(">L: random   ");
        break;
      case 3:
        lcd.setCursor(0, 0);
        lcd.print(" L: random    ");
        lcd.setCursor(0, 1);
        lcd.print(">Settings");
        break;
    }
  }
  else if(menu == MENU_SETTINGS) {
    switch(menuPos) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print(">Back     ");
        lcd.setCursor(0, 1);
        lcd.print(" Melody   ");
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(" Back    ");
        lcd.setCursor(0, 1);
        lcd.print(">Melody    ");
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print(" Melody   ");
        lcd.setCursor(0, 1);
        lcd.print(">Sleep   ");
        break;
    }
  }
  else if(menu == MENU_MELODY) {
    switch(menuPos) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print(">Back     ");
        lcd.setCursor(0, 1);
        if(playMelody) lcd.print(" ");
        else lcd.print(".");
        lcd.print("None     ");
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(" Back     ");
        lcd.setCursor(0, 1);
        lcd.print(">None   ");
        break;
      case 2:
        lcd.setCursor(0, 0);
        if(playMelody) lcd.print(" ");
        else lcd.print(".");
        lcd.print("None    ");
        lcd.setCursor(0, 1);
        lcd.print(">Default   ");
        break;
    }
  }
  else if(menu == MENU_SLEEP) {
    switch(menuPos) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print(">Back     ");
        lcd.setCursor(0, 1);
        if(doSleep) lcd.print(" ");
        else lcd.print(".");
        lcd.print("Off     ");
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(" Back     ");
        lcd.setCursor(0, 1);
        lcd.print(">Off   ");
        break;
      case 2:
        lcd.setCursor(0, 0);
        if(doSleep) lcd.print(" ");
        else lcd.print(".");
        lcd.print("Off    ");
        lcd.setCursor(0, 1);
        lcd.print(">On   ");
        break;
    }
  }
  else if(menu == MENU_L_TIMER) {
    switch(menuPos) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print(">Back");
        lcd.setCursor(0, 1);
        lcd.print(" ");
        lcdPrintInterfaceClock();
        break;
      case 1:
        lcd.setCursor(0, 0);
        lcd.print(" ");
        lcdPrintInterfaceClock();
        if(interfaceClockMode) lcd.print(" T");
        else lcd.print("  ");
        lcd.setCursor(0, 1);
        lcd.print(" --         ");
        break;
      case 2:
        lcd.setCursor(0, 0);
        lcd.print(" ");
        lcdPrintInterfaceClock();
        if(interfaceClockMode) lcd.print(" T");
        else lcd.print("  ");
        lcd.setCursor(0, 1);
        lcd.print("    --     ");
        break;
      case 3:
        lcd.setCursor(0, 0);
        lcd.print(" ");
        lcdPrintInterfaceClock();
        if(interfaceClockMode) lcd.print(" T");
        else lcd.print("  ");
        lcd.setCursor(0, 1);
        lcd.print("       --  ");
        break;
      case 4:
        lcd.setCursor(0, 0);
        lcd.print(" ");
        lcdPrintInterfaceClock();
        lcd.setCursor(0, 1);
        lcd.print(">Apply");
        break;
    }
  }
  else {
    delay(10);
  }
}
