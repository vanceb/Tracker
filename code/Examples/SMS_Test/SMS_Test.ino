#include <LGSM.h>

LSMSClass sms;

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);

  while(!sms.ready()){
    Serial.println("Waiting for SMS to be ready");
  }
  Serial.println("SMS Ready");

  if (sms.beginSMS("07799768111")) {
    Serial.println("Started SMS");
  } else {
    Serial.println("Start SMS Failed");
  }

  sms.print("Power Up!\n");
  sms.write('T');
  sms.write('e');
  sms.write('s');
  sms.write('t');

  if (sms.endSMS()) {
    Serial.println("SMS Sent");
  } else {
    Serial.println("SMS Send Failed");
  }

}

void loop() {
  // put your main code here, to run repeatedly:
  delay(100);
}
