#define INPUT1 8
#define INPUT2 9
#define INPUT3 12
#define OUTPUT1 10
#define OUTPUT2 11
#define OUTPUT3 13

void setup() {
  // put your setup code here, to run once:
pinMode(INPUT1, INPUT);
pinMode(INPUT2, INPUT);
pinMode(INPUT3, INPUT);
pinMode(OUTPUT1, OUTPUT);
pinMode(OUTPUT2, OUTPUT);
pinMode(OUTPUT3, OUTPUT);
digitalWrite(INPUT1, LOW);
digitalWrite(INPUT2, LOW);
digitalWrite(INPUT3, LOW);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(OUTPUT1, digitalRead(INPUT1));
  digitalWrite(OUTPUT2, digitalRead(INPUT2));
  digitalWrite(OUTPUT3, digitalRead(INPUT3));
  delay(1);
}
