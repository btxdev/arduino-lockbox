#define BATTERY_ADC A0

uint8_t previousBatteryCharge = 100;

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

void setup() {
  Serial.begin(9600);
  pinMode(BATTERY_ADC, INPUT);
}   
void loop() {
  Serial.println(getBatteryCharge());
  delay(1000);
}
