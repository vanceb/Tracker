#include <LGPRS.h> 
#include <LGPRSClient.h>

#define SITE_HOST "arduino.cc"
#define SITE_LOCATION "/asciilogo.txt" 

LGPRSClient client;

void setup() { 
  Serial.begin(115200);
  while(!LGPRS.attachGPRS("giffgaff.com", "giffgaff", "password")) {
    Serial.println("wait for SIM card ready");
    delay(1000);
  }

  Serial.print("Connecting to : " SITE_HOST SITE_LOCATION "..."); 
  if(!client.connect(SITE_HOST, 80)) {
    Serial.println("FAIL!");
    return;
  }
  Serial.println("Connected to host: " SITE_HOST);
  Serial.println("Sending GET request..."); 
  client.println("GET "  SITE_LOCATION " HTTP/1.1"); 
  client.println("Host: " SITE_HOST ":80"); 
  client.println(); 
  Serial.println("done");
}
void loop() {
  int v;
  while(client.available()) {
    v = client.read();
    if (v < 0)
      break;
    Serial.write(v);
  }
  delay(500);
}

