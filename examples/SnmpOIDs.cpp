#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <SimpleSNMP.h>

/**
 * snmmpOIDs.cpp
 *
 * Registers a few of the standard Mib II oids and provides the support functions required to return the corresponsing data
 *
 * 23 April 2022 - Version 1.0, initial build
 *
 * Read OID support functions should assemble the response data and then call snmp.sendresponse() to send it.
 * Write OID support should make the appropriate change then call snmp.sendrespomse() with the actual data that was changed
 * All functions can access SimpleSNMP.workingpdu struct as required which contains pointers to the main elements of the received snmp frame
 *
 * ToDo: Add suport oid 1.3.6.1.2.1.2.2.1.1 uptime 64 bit
 *       Add support for stack depth oid 1.3.6.1.4.1.55577.0.12.0
 **/

void getSystemDescription(void); // 1.3.6.1.2.1.1.1.0
void getSystemOid(void);         // 1.3.6.1.2.1.1.2.0
void getSystemUptime(void);      // 1.3.6.1.2.1.1.3.0
void getSystemContact(void);     // 1.3.6.1.2.1.1.4.0
void getSystemName(void);        // 1.3.6.1.2.1.1.5.0
void getSystemLocation(void);    // 1.3.6.1.2.1.1.6.0
void setSystemLocation(void);    // 1.3.6.1.2.1.1.6.0
void getSystemServices(void);    // 1.3.6.1.2.1.1.7.0
void getSnmpInPkts(void);        // 1.3.6.1.2.1.11.1
void getSnmpOutPkts(void);       // 1.3.6.1.2.1.11.2

SimpleSNMP snmp; // Define an instance of the main snmp suppport class

const char *ssid = "SSID";         // The SSID (name) of the Wi-Fi network you want to connect to
const char *password = "PASSWORD"; // The password of the Wi-Fi network

void setup(void)
{
    Serial.begin(74880); // Start the Serial communication to send messages to the computer
    delay(10);
    Serial.println('\n');

    WiFi.begin(ssid, password); // Connect to the network
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.println(" ...");

    int i = 0;
    while (WiFi.status() != WL_CONNECTED)
    { // Wait for the Wi-Fi to connect
        delay(1000);
        Serial.print(++i);
        Serial.print(' ');
    }

    Serial.println('\n');
    Serial.println("Connection established!");
    Serial.print("IP address:\t");
    Serial.println(WiFi.localIP()); // Send the IP address of the ESP8266 to the computer

    snmp.insertNode(PSTR("1.3.6.1.2.1.1.1.0"), getSystemDescription); // System Description
    snmp.insertNode(PSTR("1.3.6.1.2.1.1.2.0"), getSystemOid);         // System OID
    snmp.insertNode(PSTR("1.3.6.1.2.1.1.3.0"), getSystemUptime);      // System up time in 1/100th seconds
    snmp.insertNode(PSTR("1.3.6.1.2.1.1.4.0"), getSystemContact);     // System Contact
    snmp.insertNode(PSTR("1.3.6.1.2.1.1.5.0"), getSystemName);        // System Name
    snmp.insertNode(PSTR("1.3.6.1.2.1.1.6.0"), getSystemLocation);    // System Location
    snmp.insertNode(PSTR("1.3.6.1.2.1.1.7.0"), getSystemServices);    // System Services

    snmp.addRWaction(PSTR("1.3.6.1.2.1.1.6.0"), setSystemLocation); // Set system location

    snmp.insertNode(PSTR("1.3.6.1.2.1.11.1.0"), getSnmpInPkts);  // snmpInPkts
    snmp.insertNode(PSTR("1.3.6.1.2.1.11.2.0"), getSnmpOutPkts); // snmpOutPkts

    Serial.println("SNMP OIDs registered");
}

void loop(void)
{
    snmp.action(); // Process snmp.  Need to call this regularly
                   /* Do your own thing here */
}

/**************************************************
 * Supporting functions for standard Mib II values
 * ************************************************/

void getSystemDescription(void) // 1.3.6.1.2.1.1.1.0
{
#ifdef ESP8266
    snmp.sendResponse((char *)PSTR("ESP8266"));
#else
#ifdef ESP32
    snmp.sendResponse((char *)PSTR("ESP32"));
#endif
#endif
}

void getSystemOid(void) // 1.3.6.1.2.1.1.2.0
{
    // Use your own enterprise OID here, this one belongs to IBM!
    // https://www.iana.org/assignments/enterprise-numbers/enterprise-numbers
    ASNTYPE sysOid[] = {0x06, 0x07, 0x2B, 0x06, 0x01, 0x04, 0x01, 0x02, 0x01}; // 1.3.6.1.4.1.2.1 IBM encoded.
    snmp.sendResponse(sysOid);
}

void getSystemUptime(void) // 1.3.6.1.2.1.1.3.0
{
    time_t uptime = millis() / 10l;                     // Get uptime in milli seconds, convert to timerticks 1/100th seconds - Will wrap shortly after a year
    snmp.sendResponse(uptime, SNMP_DATATYPE_TIMETICKS); // Send it
                                                        // See OID .1.3.6.1.6.3.10.2.1.3 for 64 bit uptime
}

void getSystemContact(void) // 1.3.6.1.2.1.1.4.0
{
    snmp.sendResponse((char *)PSTR("SimpleSNMP@m1cje.uk"));
}

void getSystemName(void) // 1.3.6.1.2.1.1.5.0
{
#ifndef __PROJECT
#define __PROJECT "SimpleSNMP"
#endif
    snmp.sendResponse((char *)PSTR(__PROJECT));
}

static char ourLocation[] = "Somewhere Nearby";
void getSystemLocation(void) // 1.3.6.1.2.1.1.6.0
{
    snmp.sendResponse(ourLocation); // Send the current location data
}
void setSystemLocation(void) // 1.3.6.1.2.1.1.6.0
{
    // getUserData() copies the user data field from the receive buffer to our buffer
    // returns the length of the source user data field which may be more or less than the length of our buffer
    int len = snmp.getUserData(snmp.workingpdu.setvalueasn1, (byte *)ourLocation, sizeof(ourLocation) - 1);
    ourLocation[len > (int)sizeof(ourLocation) ? (int)sizeof(ourLocation) : len] = '\0'; // Add the null terminator
    Serial.print("System location set to ");
    Serial.println(ourLocation);
    snmp.sendResponse(ourLocation);
}

void getSystemServices(void) // 1.3.6.1.2.1.1.7.0
{
    snmp.sendResponse(0x40); // one byte bit field
                             //  snmp.sendResponse(0x40, SNMP_DATATYPE_INTEGER, 1); // one byte bit field
                             /*
                             sysServices OBJECT-TYPE
                             SYNTAX INTEGER (0..127)
                             MAX-ACCESS read-only
                             STATUS current
                             DESCRIPTION
                             "A value which indicates the set of services that this entity may potentially offer. The value is a sum. This sum initially takes the value zero. Then, for
                             each layer, L, in the range 1 through 7, that this node performs transactions for, 2 raised to (L - 1) is added to the sum. For example, a node which performs only
                             routing functions would have a value of 4 (2^(3-1)). In contrast, a node which is a host offering application services would have a value of 72 (2^(4-1) + 2^(7-1)).
                             Note that in the context of the Internet suite of protocols, values should be calculated accordingly:
                             layer functionality
                             0x01 1 physical (e.g., repeaters)
                             0x02 2 datalink/subnetwork (e.g., bridges)
                             0x04 3 internet (e.g., supports the IP)
                             0x08 4 end-to-end (e.g., supports the TCP)
                             0x40 7 applications (e.g., supports the SMTP)
                             For systems including OSI protocols, layers 5 and 6 may also be counted."
                             */
}

void getSnmpInPkts(void) // 1.3.6.1.2.1.11.1
{
    snmp.sendResponse(snmp.snmpPacketsRecv, SNMP_DATATYPE_COUNTER32); // Send it
}
void getSnmpOutPkts(void) // 1.3.6.1.2.1.11.2
{
    snmp.sendResponse(snmp.snmpPacketsSent, SNMP_DATATYPE_COUNTER32); // Send it
}
