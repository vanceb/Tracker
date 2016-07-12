// MQTT over GPRS
// LinkIt One sketch for MQTT Demo

#include <LGPRS.h>
#include <LGPRSClient.h>
#include <LGPS.h>   // for GPS Positioning
#include <LBattery.h>  // Battery status
#include <PubSubClient.h>

/*
Modify to your WIFI Access Credentials.
*/
#define GPRS_APN "giffgaff.com"
#define GPRS_PASSWORD "password"
#define GPRS_USER "giffgaff"

#define UPDATE_PERIOD 10000
#define GPS_POWERUP_DELAY 30000

#define IMMOBILISE_PIN 7  // Immobilise the car
#define IGNITION_PIN 12   // Sense whether the ignition is on

// Global Variables
//GPS
bool power_down_gps = false;
gpsSentenceInfoStruct gps_info;
double latitude;
double longitude;
int lat_deg;
int lon_deg;
double lat_min;
double lon_min;
char lat_hemi;
char lon_hemi;
int hour, minute, second, num_satelites ;

// Power
int batt_level;
int last_batt_level;
bool batt_charging;
bool ignition;


/*
Modify to your MQTT broker - Select only one
*/
char mqttBroker[] = "geo-fun.org";
// byte mqttBroker[] = {192,168,1,220}; // modify to your local broker

LGPRSClient GPRSClient;

PubSubClient client( GPRSClient );

unsigned long lastSend;

void startGPRS() {
    while(!LGPRS.attachGPRS(GPRS_APN, GPRS_USER, GPRS_PASSWORD)) {
        Serial.println("wait for SIM card ready");
        delay(1000);
    }
    Serial.println("GPRS Connected!");
}

void reconnect() {
    startGPRS();
    // Loop until we're reconnected
    while (!client.connected()) {
        Serial.print("Connecting to MQTT broker ...");
        // Attempt to connect
        if ( client.connect("LandyTrack") ) {	// Better use some random name
            Serial.println( "[DONE]" );
            // Publish a message on topic "outTopic"
            client.publish( "outTopic","Hello, This is LinkIt One" );
            // Subscribe to topic "inTopic"
            client.subscribe( "inTopic" );
        } else {
            Serial.print( "[FAILED] [ rc = " );
            Serial.print( client.state() );
            Serial.println( " : retrying in 5 seconds]" );
            // Wait 5 seconds before retrying
            delay( 5000 );
        }
    }
}


void setup()
{
    delay( 5000 );
    Serial.begin( 115200 );
    startGPRS();

    client.setServer( mqttBroker, 1883 );
    client.setCallback( callback );

    lastSend = 0;
}

void loop()
{

    if( !client.connected() ) {
        reconnect();
    }

    if( millis()-lastSend > UPDATE_PERIOD ) {
        update_location();
        send_location();
        //sendAnalogData();
        lastSend = millis();
    }

    client.loop();
}

void callback( char* topic, byte* payload, unsigned int length ) {
    Serial.print( "Recived message on Topic:" );
    Serial.print( topic );
    Serial.print( "    Message:");
    for (int i=0;i<length;i++) {
        Serial.print( (char)payload[i] );
    }
    Serial.println();
}

void sendAnalogData() {
    // Read data to send
    int data_A0 = analogRead( A0 );
    int data_A1 = analogRead( A1 );
    int data_A2 = analogRead( A2 );

    // Just debug messages
    Serial.print( "Sending analog data : [" );
    Serial.print( data_A0 ); Serial.print( data_A1 ); Serial.print( data_A2 );
    Serial.print( "]   -> " );

    // Prepare a JSON payload string
    String payload = "{";
    payload += "\"A0\":\""; payload += int(data_A0); payload += "\", ";
    payload += "\"A1\":\""; payload += int(data_A1); payload += "\", ";
    payload += "\"A2\":\""; payload += int(data_A2); payload += "\"";
    payload += "}";

    // Send payload
    char analogData[100];
    payload.toCharArray( analogData, 100 );
    client.publish( "analogData", analogData );
    Serial.println( analogData );
}

void send_location() {
    // Prepare a JSON payload string
    String payload = "{";
    payload += "\"Lat\":\""; payload += lat_deg; payload += " "; payload += lat_min; payload += " "; payload += lat_hemi; payload += "\", ";
    payload += "\"Lon\":\""; payload += lon_deg; payload += " "; payload += lon_min; payload += " "; payload += lon_hemi; payload += "\", ";
    payload += "\"Satellites\":\""; payload += num_satelites; payload += "\"";
    payload += "}";

    // Send payload
    char data[100];
    payload.toCharArray( data, 100 );
    client.publish( "location", data );
    Serial.println( data );
}
// Get location from GPS
// Helper functions first

// Get the position of the nth comma in the NMEA GPS String
static unsigned char getComma(unsigned char n,const char *str)
{
    unsigned char i,j = 0;
    int len=strlen(str);
    for(i = 0;i < len;i ++)
    {
        if(str[i] == ',')
        j++;
        if(j == n)
        return i + 1;
    }
    return 0;
}

// Parse a double from the NMEA GPS String
static double getDoubleNumber(const char *s)
{
    char buf[10];
    unsigned char i;
    double rev;

    i=getComma(1, s);
    i = i - 1;
    strncpy(buf, s, i);
    buf[i] = 0;
    rev=atof(buf);
    return rev;
}

// Parse and integer from the NMEA GPS String
static double getIntNumber(const char *s)
{
    char buf[10];
    unsigned char i;
    double rev;

    i=getComma(1, s);
    i = i - 1;
    strncpy(buf, s, i);
    buf[i] = 0;
    rev=atoi(buf);
    return rev;
}

// Parse out a location message from the NMEA GPS String into buffers
void parse_GPGGA(const char* GPGGAstr)
{
    /* Refer to http://www.gpsinformation.org/dale/nmea.htm#GGA
    * Sample data: $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47
    * Where:
    *  GGA          Global Positioning System Fix Data
    *  123519       Fix taken at 12:35:19 UTC
    *  4807.038,N   Latitude 48 deg 07.038' N
    *  01131.000,E  Longitude 11 deg 31.000' E
    *  1            Fix quality: 0 = invalid
    *                            1 = GPS fix (SPS)
    *                            2 = DGPS fix
    *                            3 = PPS fix
    *                            4 = Real Time Kinematic
    *                            5 = Float RTK
    *                            6 = estimated (dead reckoning) (2.3 feature)
    *                            7 = Manual input mode
    *                            8 = Simulation mode
    *  08           Number of satellites being tracked
    *  0.9          Horizontal dilution of position
    *  545.4,M      Altitude, Meters, above mean sea level
    *  46.9,M       Height of geoid (mean sea level) above WGS84
    *                   ellipsoid
    *  (empty field) time in seconds since last DGPS update
    *  (empty field) DGPS station ID number
    *  *47          the checksum data, always begins with *
    */

    int tmp;
    if(GPGGAstr[0] == '$')
    {
        tmp = getComma(1, GPGGAstr);
        hour     = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
        minute   = (GPGGAstr[tmp + 2] - '0') * 10 + (GPGGAstr[tmp + 3] - '0');
        second    = (GPGGAstr[tmp + 4] - '0') * 10 + (GPGGAstr[tmp + 5] - '0');
        tmp = getComma(2, GPGGAstr);
        latitude = getDoubleNumber(&GPGGAstr[tmp]);
        lat_deg = (GPGGAstr[tmp + 0] - '0') * 10 + (GPGGAstr[tmp + 1] - '0');
        lat_min = getDoubleNumber(&GPGGAstr[tmp + 2]);
        tmp = getComma(3, GPGGAstr);
        lat_hemi = GPGGAstr[tmp];
        tmp = getComma(4, GPGGAstr);
        longitude = getDoubleNumber(&GPGGAstr[tmp]);
        lon_deg = (GPGGAstr[tmp] - '0') * 100 + (GPGGAstr[tmp + 1] - '0') * 10 + (GPGGAstr[tmp + 2] - '0');
        lon_min = getDoubleNumber(&GPGGAstr[tmp + 3]);
        tmp = getComma(5, GPGGAstr);
        lon_hemi = GPGGAstr[tmp];
        tmp = getComma(7, GPGGAstr);
        num_satelites = getIntNumber(&GPGGAstr[tmp]);
    }
    else
    {
        Serial.println("Corrupt GPS Data");
    }
}

void update_location() {
    // Make sure the GPS is turned on
    LGPS.powerOn();

    // If we are powering down the GPS subsystem
    if (power_down_gps) {
        // Wait for a period to give the GPS chance to get a fix
        delay(GPS_POWERUP_DELAY);
    }

    //Get the data from the GPS
    LGPS.getData(&gps_info);
    // Parse the data into the location messages
    parse_GPGGA((const char *) gps_info.GPGGA);

    if (power_down_gps) {
        LGPS.powerOff();
    }

}


// Power related monitoring
void check_power() {
    batt_level = LBattery.level();
    batt_charging = LBattery.isCharging();
    ignition = digitalRead(IGNITION_PIN);
    if (batt_level < last_batt_level) {
        Serial.print("Battery Level Change: " );
    }
    last_batt_level = batt_level;
}
