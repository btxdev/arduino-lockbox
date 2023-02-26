#define BTN_PREV 9
#define BTN_OK 10
#define BTN_NEXT 11

void setup() {
  Serial.begin(9600);
  pinMode(BTN_PREV, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_NEXT, INPUT_PULLUP);
}

void loop() {
  Serial.print(digitalRead(BTN_PREV));
  Serial.print(", ");
  Serial.print(digitalRead(BTN_OK));
  Serial.print(", ");
  Serial.print(digitalRead(BTN_NEXT));
  Serial.println();
  delay(200);
}
