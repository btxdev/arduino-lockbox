#include <iarduino_RTC.h>

#define CLOCK_RST 8
#define CLOCK_DAT 7
#define CLOCK_CLK 6

iarduino_RTC clock(RTC_DS1302, CLOCK_RST, CLOCK_CLK, CLOCK_DAT);

uint8_t parsedData[8];

void setup() {
  clock.begin();
  // delay(300);
  Serial.begin(9600);

  
}

void loop() {
  Serial.println("Ready!");
  // wait for serial
  while(!Serial.available()) {}
  // read string
  String inputBuffer = Serial.readString();
  inputBuffer.trim();

  // parse string
  char *s = strtok(inputBuffer.c_str(), ",");
  uint8_t i = 0;
  parsedData[i++] = atoi(s);
  while(i < 7) {
    parsedData[i++] = atoi(strtok(NULL, ","));
  }

  // apply time (10 сек, 10 мин, 10 час, 4 день, 10 октябрь, 17 год, 3 среда)
  clock.settime(parsedData[0], parsedData[1], parsedData[2], parsedData[3], parsedData[4], parsedData[5], parsedData[6]);
  Serial.println("Time updated!");

  Serial.println("Current time:");
  while(true) {
    if(millis() % 1000 == 0) {
      Serial.println(clock.gettime("d-m-Y, H:i:s, D"));
      delay(2);      
    }
  }
}
