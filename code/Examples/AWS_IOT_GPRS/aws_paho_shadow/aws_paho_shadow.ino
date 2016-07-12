// SVB
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <vmsock.h>

#include <signal.h>
#include <limits.h>

#include "linkit_aws_header.h"
#include "aws_mtk_iot_config.h"

#ifdef connect
#undef connect
#endif

#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LGPRS.h>
#include <LGPS.h>   // for GPS Positioning

// Convenience function to log to the serial port if debugging is on
char logMsg[256];
void log(const char * msg) {
    #ifdef DEBUG_SERIAL
    Serial.println(msg);
    #endif
}

// How long should our incoming buffer be?
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 4096

/*********************************************************
* Get config details from the aws_iot_config.h file
*********************************************************/
char HostAddress[] = AWS_IOT_MQTT_HOST;
VMINT port = AWS_IOT_MQTT_PORT;
char cafileName[] = AWS_IOT_ROOT_CA_FILENAME;
char clientCRTName[] = AWS_IOT_CERTIFICATE_FILENAME;
char clientKeyName[] = AWS_IOT_PRIVATE_KEY_FILENAME;


/*********************************************************
* Global variables
*********************************************************/

// MQTT AWS IOT Related
QoSLevel qos = QOS_0;
int32_t i;
IoT_Error_t rc;
char shadowTxBuffer[256];
char deltaBuffer[256];
MQTTClient_t mqttClient;
char *pJsonStringToUpdate;
char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
size_t sizeOfJsonDocumentBuffer;

// Data Related
gpsSentenceInfoStruct gps_info;
bool immobilised = false;
float lat = 0.0;
float lon = 0.0;
bool ignition = false;

jsonStruct_t json_immobilised;
jsonStruct_t json_lat;
jsonStruct_t json_lon;
jsonStruct_t json_ignition;

ShadowParameters_t sp;

// Structures to hold the data
typedef struct {
    double lat;
    double lon;
    bool ignition;
    bool immobilised;
} ShadowReported;

ShadowReported reported;

typedef struct {
    bool immobilised;
} ShadowDesired;

ShadowDesired desired;

/*********************************************************
* AWS IOT Functions and callbacks
*********************************************************/
boolean nativeLoop(void* user_data);

// Called when we have sent update data
// Callback requested in publish_shadow function calling aws_iot_shadow_update
void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
    const char *pReceivedJsonDocument, void *pContextData) {

    if (pReceivedJsonDocument != NULL) {
        sprintf(logMsg, "Received JSON: %s\n", pReceivedJsonDocument);
        log(logMsg);
    }
    if (status == SHADOW_ACK_TIMEOUT) {
        Serial.println("Update Timeout --");
    } else if (status == SHADOW_ACK_REJECTED) {
        Serial.println("Update Rejected XX");
    } else if (status == SHADOW_ACK_ACCEPTED) {
        Serial.println("Update Accepted !!");
    }
}

// Called when the Immobilization data changes
// Callback requested in mqtt_start function
void immobiliseActuator_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext) {
    if (pContext != NULL) {
        bool immob = *(bool *)(pContext->pData);
        immobilised = immob;
        log("Delta - Immobilise state changed to  ");
        log(immob ? "True" : "False");
    }
}


// invoked in main thread context
// Callback requested in bearere_open function
void bearer_callback(VMINT handle, VMINT event, VMUINT data_account_id, void *user_data)
{
    if (VM_BEARER_WOULDBLOCK == g_bearer_hdl)
    {
        g_bearer_hdl = handle;
    }

    switch (event)
    {
        case VM_BEARER_DEACTIVATED:
        break;
        case VM_BEARER_ACTIVATING:
        break;
        case VM_BEARER_ACTIVATED:
        LTask.post_signal();
        break;
        case VM_BEARER_DEACTIVATING:
        break;
        default:
        break;
    }
}

// Setup mqtt
boolean  mqtt_start(void* ctx)
{
    rc = NONE_ERROR;
    i = 0;
    aws_iot_mqtt_init(&mqttClient);

    sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

    json_immobilised.cb = immobiliseActuator_Callback;
    json_immobilised.pData = &immobilised;
    json_immobilised.pKey = "immobilised";
    json_immobilised.type = SHADOW_JSON_BOOL;

    json_lat.cb = NULL;
    json_lat.pKey = "lat";
    json_lat.pData = &lat;
    json_lat.type = SHADOW_JSON_FLOAT;

    json_lon.cb = NULL;
    json_lon.pKey = "lon";
    json_lon.pData = &lon;
    json_lon.type = SHADOW_JSON_FLOAT;

    json_ignition.cb = NULL;
    json_ignition.pData = &ignition;
    json_ignition.pKey = "ignition";
    json_ignition.type = SHADOW_JSON_BOOL;

    sp = ShadowParametersDefault;
    sp.pMyThingName = AWS_IOT_MY_THING_NAME;
    sp.pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;
    sp.pHost = HostAddress;
    sp.port = port;
    sp.pClientCRT = AWS_IOT_CERTIFICATE_FILENAME;
    sp.pClientKey = AWS_IOT_PRIVATE_KEY_FILENAME;
    sp.pRootCA = AWS_IOT_ROOT_CA_FILENAME;

    Serial.print("  . Shadow Init... ");
    rc = aws_iot_shadow_init(&mqttClient);
    if (NONE_ERROR != rc) {
        Serial.println("Error in connecting...");
    }
    Serial.println("ok");

    rc = aws_iot_shadow_connect(&mqttClient, &sp);

    if (NONE_ERROR != rc) {
        Serial.println("Shadow Connection Error");
    }

    rc = aws_iot_shadow_register_delta(&mqttClient, &json_immobilised);

    if (NONE_ERROR != rc) {
        Serial.println("Shadow Register Delta Error");
    }

    //temperature = STARTING_ROOMTEMPERATURE;

    Serial.println("  . mqtt_start finished...ok");

    return true;
}


// Open the bearer
boolean bearer_open(void* ctx){
    if (WIFI_USED) {
        g_bearer_hdl = vm_bearer_open(VM_BEARER_DATA_ACCOUNT_TYPE_WLAN ,  NULL, bearer_callback);
    } else {
        g_bearer_hdl = vm_bearer_open(VM_APN_USER_DEFINE ,  NULL, bearer_callback);
    }
    if(g_bearer_hdl >= 0)
        return true;
    return false;
}


/* Main setup function */
void setup() {
    LTask.begin();

    Serial.begin(9600);
    while(!Serial)
        delay(100);

    // keep retrying until connected to AP
    if (WIFI_USED){
        LWiFi.begin();
        Serial.print("  . Connecting to AP...");
        Serial.flush();
        while (!LWiFi.connectWPA(WIFI_AP, WIFI_PASSWORD)) {
            delay(1000);
            Serial.println(".");
        }
        printWifiStatus();
    } else {
        Serial.print("  . Connecting to GPRS...");
        Serial.flush();
        while (!LGPRS.attachGPRS(GPRS_APN, GPRS_USERNAME, GPRS_PASSWORD))
        {
            delay(500);
        }
    }

    Serial.println("ok");

    delay(10000);

    // This seems like a fudge to me, but is required to get mbedTLS to work
    CONNECT_IP_ADDRESS = AWS_IOT_MQTT_IP;
    CONNECT_PORT = AWS_IOT_MQTT_PORT;

    LTask.remoteCall(&bearer_open, NULL);
    LTask.remoteCall(&mqtt_start, NULL);
}

/* for analogRead or other API, some may not be able to called in remoteCall.
You may need to call it in loop function and then pass the parameter to nativeLoop through a pointer. */
void loop()
{
    int aa[1];
    aa[0] =digitalRead(10);
    Serial.flush();
    updateData();
    // May need to pass a struct of the data???
    LTask.remoteCall(nativeLoop, (void*)aa);
    delay(20000);
}

/* message could be passed value to publish_Shadow */
int publish_Shadow(char * topic, char * message) {
    int rc = NONE_ERROR;

    rc = aws_iot_shadow_yield(&mqttClient, 1000);   //please don't try to put it lower than 1000, otherwise it may going to timeout easily and no response
    delay(1000);

    // Create the JSON to be sent
    rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
    if (rc == NONE_ERROR) {
        rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 4, &json_immobilised, &json_ignition, &json_lat, &json_lon);
        if (rc == NONE_ERROR) {
            rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
            if (rc == NONE_ERROR){
                Serial.print("Update Shadow: ");
                Serial.println(JsonDocumentBuffer);
                rc = aws_iot_shadow_update(&mqttClient, AWS_IOT_MY_THING_NAME, JsonDocumentBuffer, ShadowUpdateStatusCallback, NULL, 10, true);
                Serial.print(" rc for aws_iot_shadow_update is ");
                Serial.println(rc);
            }
        }
    }
    Serial.println("Update sent...");
    return rc;
}

char mqtt_message[2048];

boolean nativeLoop(void* user_data) {

    int *bb = (int*)user_data;
    sprintf(mqtt_message, "%s : your passing message", *bb);

    publish_Shadow("mtkTopic5", mqtt_message);
    Serial.flush();
}

void updateData() {
    lat = 51.1234;
    lon = -1.1234;
    ignition = true;
}

void printWifiStatus()
{
    // print the SSID of the network you're attached to:
    Serial.print("SSID: ");
    Serial.println(LWiFi.SSID());

    // print your LWiFi shield's IP address:
    IPAddress ip = LWiFi.localIP();
    Serial.print("IP Address: ");
    Serial.println(ip);

    // print the received signal strength:
    long rssi = LWiFi.RSSI();
    Serial.print("signal strength (RSSI):");
    Serial.print(rssi);
    Serial.println(" dBm");
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
                    //sprintf(buffSMS, "%02d:%02d:%02d %02d %07.4f %c, %02d %07.4f %c from %d satellites", hour, minute, second, lat_deg, lat_min, lat_hemi, lon_deg, lon_min, lon_hemi, num_satelites);
                    //sprintf(buffGPRS, "Location,%02d:%02d:%02d,%09.4f,%c,%010.4f,%c,%d", hour, minute, second, latitude, lat_hemi, longitude, lon_hemi, num_satelites);
                    //log("GPS Data:");
                    //log(buffSMS);
                    //log(buffGPRS);
                    lat = latitude;
                    lon = longitude;
                }
                else
                {
                    log("Corrupt GPS Data");
                }
            }

            void update_location() {

                //Get the data from the GPS
                log("Getting GPS Data");
                LGPS.getData(&gps_info);
                // Parse the data into the location messages
                parse_GPGGA((const char *) gps_info.GPGGA);

            }
