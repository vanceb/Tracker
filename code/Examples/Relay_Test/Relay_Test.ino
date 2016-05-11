#define IMMOBILISE_PIN 7

void setup() {
  // initialize digital pin 13 as an output.
  pinMode(IMMOBILISE_PIN, OUTPUT);
}

// the loop function runs over and over again forever
void loop() {
  digitalWrite(IMMOBILISE_PIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  delay(4000);              // wait for a second
  digitalWrite(IMMOBILISE_PIN, LOW);    // turn the LED off by making the voltage LOW
  delay(4000);              // wait for a second
}
