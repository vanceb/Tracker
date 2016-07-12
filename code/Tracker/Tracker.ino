#include <LGPS.h>   // for GPS Positioning
#include <LGPRS.h>  // for GPRS Data
#include <LGSM.h>   // for SMS Functionality
#include <LBattery.h>  // Battery status
#include "private.h"  // Phone numbers and other details

#define IMMOBILISE_PIN 7  // Immobilise the car
#define IGNITION_PIN 12   // Sense whether the ignition is on

// Comment out the following line for production
#define DEBUG_SERIAL 1    // Use the serial port for debug messages

#define GPS_POWERUP_DELAY 30000  // Allow time for the GPS to get a fix

#define SMS_RETRIES 3 // The number of time we attempt to send SMS before fail

#define GPRS_RETRY_DELAY 2
#define GPRS_APN "giffgaff.com"
#define GPRS_USER "giffgaff"
#define GPRS_PASSWD "password"

// Global variables
LSMSClass sms;
gpsSentenceInfoStruct gps_info;
// Buffer for SMS messages
char buffSMSTx[141];
char buffSMSRx[141];
// Buffer to hold incoming SMS from number
#define MAX_SMS_NUMBER 30
char buffSMSFrom[MAX_SMS_NUMBER];
//Buffer for GPRS messages
char buffGPRS[256];
// Buffer for Power SMS
char powerSMS[140];
// Buffer for Power GPRS
char powerGPRS[256];
char* buff;

bool power_down_gps = true;
int battery_level = 0;


// Convenience function to log to the serial port if debugging is on
void log(const char * msg) {
    #ifdef DEBUG_SERIAL
    Serial.println(msg);
    #endif
}

bool connectGPRS(uint16_t timeout_ms) {
    log("Connecting GPRS")
    uint32_t timeout = millis() + timeout_ms;
    bool success = false;
    while (!success && timeout < millis()) {
        success = LGPRS.attachGPRS(GPRS_APN, GPRS_USER, GPRS_PASSWD);
        if (success) {
            log("Success");
        } else {
            log("Retrying...")
        }
        delay(GPRS_RETRY_DELAY);
    }
    if (success) {
        log("Connected");
    } else {
        log("Could not connect");
    }
    return success;
}

// Easy sending of SMS
int send_SMS(const char* to, const char * msg, uint8_t retry_count, uint16_t timeour_ms) {
    log("Beginning to send SMS");
    // TODO  This will fail as millis() approaches rollover
    uint32_t timeout = millis() + timeout_ms;
    bool success = false;
    int attempts = 0;
    while (!success && attempts < retry_count) {
        // Initiate SMS
        while(!sms.ready() && millis() < timeout){
            log("Waiting for SMS to be ready");
            delay(10);
        }
        if (sms.ready()){
            log("SMS Ready");
            // Start the SMS using the number to send to
            if (sms.beginSMS(to)) {
                log("Started SMS to:");
                log(to);
            } else {
                log("Start SMS Failed");
            }
            // Add the message body
            sms.print(msg);
            // Complete and send SMS
            if (sms.endSMS()) {
                log("SMS Sent");
                success = true;
            } else {
                log("SMS Send Failed");
                attempt++;
            }
        }
    }
    if(success) {
        return 0;
    } else {
        return -1
    }
}


// Receive SMS
bool check_SMS() {
    log("Checking for unread SMS");
    while(!sms.ready()){
        log("Waiting for SMS to be ready");
        delay(10);
    }
    log("SMS Ready");
    if(sms.available()) {
        log("We have a message!");
        // Get the number which sent the SMS message
        int i = sms.remoteNumber(buffSMSFrom, MAX_SMS_NUMBER);
        switch(i) {
            case 0:
                log("No new SMS - or no number?");
                // Clear out the number to make sure it doesn't retain last
                buffSMSFrom[0] = '\0';
                break;
            case MAX_SMS_NUMBER:
                log("From Number too long!");
                // Clear out the number to make sure it doesn't retain last
                buffSMSFrom[0] = '\0';
                break;
            default:
                log(buffSMSFrom);
                if(!strcmp(buffSMSFrom, NOTIFY_NUMBER)) {
                    log("Message from authorised number!");
                } else {
                    log("Message from unknown number");
                }
        }

        // Get the contents of the message into the buffer
        while(i < 140 && sms.peek() != -1) {
            buffSMS[i] = sms.read();
            i++;
        }
        //buffSMS[i] = '\0';
        log(buffSMS);
        // Delete the message
        sms.flush();
        return true;
    } else {
        log("No new messages");
        return false;
    }
}

// Process SMS for actions
void process_SMS() {
    log("Processing SMS Message");
    log(buffSMS);
}


// Easy sending of GPRS data
int send_GPRS() {
//int send_GPRS(const char* host, const char* location, const char * data) {
// TODO
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

    double latitude;
    double longitude;
    int lat_deg;
    int lon_deg;
    double lat_min;
    double lon_min;
    char lat_hemi;
    char lon_hemi;
    int tmp, hour, minute, second, num_satelites ;
    if(GPGGAstr[0] == '$')
    {
        log("Parsing GPS Data");
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

        // Copy data into global messages
        //sprintf(buffSMS, "%02d:%02d:%02d %09.4f %c, %010.4f %c from %d satellites", hour, minute, second, latitude, lat_hemi, longitude, lon_hemi, num_satelites);
        sprintf(buffSMS, "%02d:%02d:%02d %02d %07.4f %c, %02d %07.4f %c from %d satellites", hour, minute, second, lat_deg, lat_min, lat_hemi, lon_deg, lon_min, lon_hemi, num_satelites);
        sprintf(buffGPRS, "Location,%02d:%02d:%02d,%09.4f,%c,%010.4f,%c,%d", hour, minute, second, latitude, lat_hemi, longitude, lon_hemi, num_satelites);
        log("GPS Data:");
        log(buffSMS);
        log(buffGPRS);
    }
    else
    {
        log("Corrupt GPS Data");
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
    log("Getting GPS Data");
    LGPS.getData(&gps_info);
    // Parse the data into the location messages
    parse_GPGGA((const char *) gps_info.GPGGA);

    if (power_down_gps) {
        LGPS.powerOff();
        log("Powering off GPS to save power");
    }
    send_GPRS();
}


// Power related monitoring
void check_power() {
    log("Checking power status");
    int level = LBattery.level();
    bool charging = LBattery.isCharging();
    bool ignition = digitalRead(IGNITION_PIN);
    sprintf(powerSMS, "Battery level: %d, Charging: %d, Ignition: %d", level, charging, ignition);
    sprintf(powerGPRS, "Battery,%d,%d,%d", level, charging, ignition);
    if (level < battery_level) {
        send_SMS(NOTIFY_NUMBER, powerSMS);
    }
    battery_level = level;
    log(powerSMS);
}

void setup () {
    #ifdef DEBUG_SERIAL
    // Open the serial port for debug messages
    Serial.begin(9600);
    #endif

    // Setup IO Pins
    pinMode(IGNITION_PIN, INPUT);
    pinMode(IMMOBILISE_PIN, OUTPUT);
    digitalWrite(IMMOBILISE_PIN, LOW);

    //Turn on the GPS Subsystem
    LGPS.powerOn();
    delay(1000);
    update_location();
    check_power();
    send_SMS(NOTIFY_NUMBER, buffSMS, SMS_RETRIES);

}

void loop () {
    if (check_SMS()) {
        process_SMS();
    }
    delay(10000);
    //update_location();
    //check_power();
}
