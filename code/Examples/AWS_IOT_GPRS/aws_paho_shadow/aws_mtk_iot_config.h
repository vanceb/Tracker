/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

/**
 * @file aws_mtk_iot_config.h
 * @brief AWS IoT specific configuration file
 */

#ifndef SRC_SHADOW_IOT_SHADOW_CONFIG_H_
#define SRC_SHADOW_IOT_SHADOW_CONFIG_H_


// Get from AWS IOT console
// =================================================
#define AWS_IOT_MQTT_HOST              "A3RR65K7WU55ZQ.iot.us-east-1.amazonaws.com"
//#define AWS_IOT_MQTT_HOST              "54.209.1.110"
#define AWS_IOT_MQTT_IP              "54.209.1.110"
#define AWS_IOT_MQTT_PORT              8883
#define AWS_IOT_MQTT_CLIENT_ID         "Landy"
#define AWS_IOT_MY_THING_NAME          "Landy"
#define AWS_IOT_ROOT_CA_FILENAME      "root-CA.crt"
#define AWS_IOT_CERTIFICATE_FILENAME   "61f170849b-certificate.pem.crt"
#define AWS_IOT_PRIVATE_KEY_FILENAME   "61f170849b-private.pem.key"
// =================================================

//set to use Wifi or GPRS
#define WIFI_USED false  //true (Wifi) or false (GPRS)

/* change Wifi settings here */
#define WIFI_AP "orbit"
#define WIFI_PASSWORD "3 garfield road"
#define WIFI_AUTH LWIFI_WPA  // choose from LWIFI_OPEN, LWIFI_WPA, or LWIFI_WEP

/* change GPRS settings here */
#define GPRS_APN "giffgaff.com"   //for giffgaff
#define GPRS_USERNAME "giffgaff"
#define GPRS_PASSWORD "password"

#endif /* SRC_SHADOW_IOT_SHADOW_CONFIG_H_ */
