#define BATTERY_ADC A0

void setup() {
  Serial.begin(9600);
  pinMode(BATTERY_ADC, INPUT);
}   
void loop() {
  // tone(BUZZER, 2000, 100);
  // delay(900);
  // analogWrite(BUZZER, 127);
  Serial.println(analogRead(BATTERY_ADC));
  delay(1000);
}
