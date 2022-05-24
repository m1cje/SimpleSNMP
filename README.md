# SimpleSNMP

## Library for SNMP management support on esp iot

SNMP is a network management protocol used primarily to retreive statistical data from network connected devices.<br>
The SNMP standards provide a number of predefined data objects called Mib II variables.  You can optionally choose to implement any of these you require, or you can add your own new data objects to report on.<br>
The long number strings you will see on any SNMP based system represent these data objects.  Eg 1.3.6.1.2.1.1.6.0 is the Mib II identifier assigned to the system location string.<br>
SimpleSNMP does not implement any object ids (OIDs) by default, these are implemented by you in your code.<br>
Traditionally SNMP has been a dificult system to implement as it is not blessed with any clear documentation. This library is designed to provide a subset of functionality in a simple to use manner, adequete to implement basic tracking of any data points that are of interest.

* Small footprint
* Simple to implement
* Supports SNMP v1 & v2c
* Supports getreq (read), getnextreq getbulk and setreq (write)
* Does not support SNMP v3
* Does not support SNMP traps

SimpleSNMP is only part of the story, the library is a server implementation that enables data retrieval by a third party client application.<br>
At it's most basic the client could be the snmp get/set command line utilities available on Linux or Windows operating systems.<br>
Typical usage with snmpget would be.-
```
    snmpget -v2c -cpublic -d -r0 192.168.9.150 1.3.6.1.2.1.1.6.0
```
which would normally return the system location string.
At the other end of the scale are full blown NMS (Network Management Systems) applications that will poll your data on a schedule and produce graphical reports for your perusal.<br>
Examples such as MRTG, Cacti, Nagios, Solarwinds to name a few.<br>
SimpleSNMP supports both get and put operations so can be used as a configuration tool for your application data as well as a rporting tool.<br>
Typical usage with snmpset would be.-
```
    snmpset -v2c -cpublic -d -r0 192.168.9.150 1.3.6.1.2.1.1.6.0 s "Hello World..."
```
which would normally set the system location string to say Hello World...

### Using the Library

#### include <SimpleSNMP.h>
The include command needs to be at the start of your source code along with any other library includes you are using
#### SimpleSNMP snmp;
This command is required to create an instance of the SimpleSNMP server.  It should be included near the top of your source code before the setup() and loop() functions
#### snmp.action();
This command needs to be included in your main loop() function code.  It needs to be called regularly to check for received data.<br>
And that's it! It wont do much at this point but it should compile without any errors.  SNMP uses the UDP protocol over IP so you will of course need to provide a working network connection for SimpleSNMP to listen on.  It should coexist nicely with any other networking libraries you are using such as OTA or HTTP. It only needs ownership of udp port 161.
### Configuring read access
Now you will need to write some support functions to provide the data that will be returned against each request.  Start with something simple such as the standard Mib II request for the system name which is OID 1.3.6.1.2.1.1.5.0
```
void getSystemName(void) // 1.3.6.1.2.1.1.5.0
{
  snmp.sendResponse("SimpleSNMP");
}
```
This function will send the "SimpleSNMP" text string back when it's requested.  SimpleSNMP is generally PROGMEM aware so you can put your responses into flash memory if appropriate.  eg.
```
snmp.sendResponse((char *)PSTR("SimpleSNMP"));
```
Once you have written your support function you will need to inform SimpleSNMP so it knows what OID this function should respond to.  To do this you will call the *insertNode()* function.  Probably this will be done within your setup() function call.
```
  snmp.insertNode(PSTR("1.3.6.1.2.1.1.5.0"), getSystemName); // System Name
```
### Adding write support
Once you have configured read support for an OID you can add a write function.  This is done in the same way as the read function.  Your support function will need to save the received value somewhere.
```
  snmp.addRWaction(PSTR("1.3.6.1.2.1.1.5.0"), setSystemName); // Set system name
```
Both read and write functions should be declared as void types with no parameters. eg.
```
void setSystemLocation(void) // 1.3.6.1.2.1.1.6.0
{
  int len = snmp.getUserData(snmp.workingpdu.setvalueasn1, (byte *)ourLocation, sizeof(ourLocation)-1);
  ourLocation[len > (int)sizeof(ourLocation) ? (int)sizeof(ourLocation) : len] = '\0'; // Add the null terminator
  snmp.sendResponse(ourLocation);
}
```
The value that was passed over SNMP can be retreived using the _snmp.getUserData()_ library function.
```
  int len = snmp.getUserData(snmp.workingpdu.setvalueasn1, (byte *)buffer, sizeof(buffer));
```
_getUserData()_ returns the size of the user data field which may be more or less than the size you requested.  The first parameter passed is a pointer to an asn.1 formatted data item.  Normally this will be the pointer to the value field that was sent by the snmp client.  The value data pointer is available in the workingpdu structure which stores a pointer to each of the elements of the snmp packet.  Note that the structure data is only valid until the next packet is received so it needs to be copied somewhere else for processing by your application.<br>
The function will copy data from the receive buffer to your buffer specified in the second parameter up to the length indicated in the third parameter.  Note, If you are receiving charcter string data then SNMP will not typically include a null terminator at the end of the string so you will need to add this yourself if you need it.  The length returned by getUserData() indicates the received data length and should be checked for validity.
### ASN.1 format
Most data sent and received over SNMP will be formatted according to the ASN.1 standard.  There are many lengthy douments available on the internet if you are interested but in summary, all data is sent as a type byte followed by a length firld followed by the actual data field.  ef to send the number 10 you would assemble a 3 byte field as follows, 0x02 0x01 0x0A which is type 2 = integer, length 1 byte of data then the actual data.
#### snmp.workingpdu
This struct holds pointers to each field within the received data frame.  The actual data stays in the rxbuffer.  See below for the detailed breakdown<br>
Normally the only element that should be required is the _setvalueasn1_ pointer which points to the user data received from a set request.  All the pointer fields will be formatted as asn.1 data.
### Function Reference
#### getUserData()
```
  int length = getUserData(byte *asn1,byte *buffer,int bufferlength);
```
##### Description
Copies the data from an asn.1 formatted source buffer to a destination buffer
##### Parameters
  _byte *asn1_ a pointer to an asn.1 formatted buffer.  The function will copy the data element from the source to the destination buffer<br>
  _byte *buffer_ a pointer to the destination buffer.  This should be large enougth to hold the received user data.<br>
  _int bufferlength_ the maximum size that will be copied into the destination.  If the source data is less than bufferlength then only the source data will be copied.<br>
##### Returns
  _byte length_ The length field from the ASN.1 formatted source data.<br>
##### Typical usage
```
  int len = snmp.getUserData(snmp.workingpdu.setvalueasn1, (byte *)ourLocation, sizeof(ourLocation) - 1);
```
where _snmp.workingpdu.setvalueasn1_ is the pointer to the received user data field passed in the workingpdu structure, _yourlocation_ is the where you want to store the value passed followed by the size of the buffer, -1 to allow room to store a trailing null character.
#### action()
```
void action(void);
```
##### Description
This is the main service function that decodes the received data frame and calls the appropriate service function.  It needs to be called regularly to avoid buffer overrun on the recieved data.
##### Parameters
None
##### Returns
Nothing
##### Typical usage
```
void loop()
{
  snmp.action();
}
```
#### insertNode()

```
    void insertNode(const char *oidtext, void (*action)());
```
##### Description
Adds your callback function against a given object identifier.<br>
This function should be called once for each oid you want to register, normlly within your setup().<br>
The function is PROGMEM aware so you can store your oid value in flash.<br>
##### Parameters
_const char *oidtext_ This is the object identifier to be registered.  It should start with 1.3. as this sequence is specially encoded with SNMP.<br>
_void action(void)_ This is a pointer to your callback function that will be called whenever you receive a request that matches the oidtext value.<br>
##### Returns
Nothing
##### Typical usage
```
  snmp.insertNode(PSTR("1.3.6.1.2.1.1.1.0"), getSystemDescription); // System Description
```
#### addRWaction()
```
    bool addRWaction(const char *oidtext, void (*action)());
```
##### Description
Adds your callback function against a given object identifier.<br>
This function should be called once for each oid you want to add a write service function to, normlly called within your setup().<br>
Note the oid must have been previously added for read with the insertnode() function.<br>
The function is PROGMEM aware so you can store your oid value in flash.<br>
##### Parameters
_const char *oidtext_ This is the object identifier to be updated.<br>
_void action(void)_ This is a pointer to your callback function that will be called whenever you receive a write request that matches the oidtext value.<br>
##### Returns
true if added or false if the oid was not found.
##### Typical usage
```
  snmp.addRWaction(PSTR("1.3.6.1.2.1.1.1.0"), getSystemDescription); // System Description
```
#### setROcommunity() & setRWcommunity()
```
    void setROcommunity(const char *name);
```
##### Description
These two functions can be called to change the default community names.<br>
The RO community defaults to _public_ and the RW community name to _private_<br>
The functions are PROGMEM aware so you can store your community name value in flash.<br>
Note the community name is limited to 20 characters max<br>
##### Parameters
_const char *name_ This is the new community name to be used.
##### Returns
Nothing
##### Typical usage
```
  snmp.setROcommunity(PSTR("newname"));
```
#### sendResponse()
```
    void sendResponse(long long value, SNMP_DATA_TYPE type);              // Sends a non int type int, length is the length implicit in the type
    void sendResponse(long long value, SNMP_DATA_TYPE type, byte length); // Sends a non int type int, length is the length of the value
    void sendResponse(char value);                                        // Sends a byte
    void sendResponse(byte value);                                        // Sends a byte
    void sendResponse(int16_t value);                                     // Sends an int
    void sendResponse(uint16_t value);                                    // Sends an unsigned int
    void sendResponse(int32_t value);                                     // Sends an int
    void sendResponse(uint32_t value);                                    // Sends an unsigned int
    void sendResponse(int64_t value);                                     // Sends an int
    void sendResponse(uint64_t value);                                    // Sends an unsigned int
    void sendResponse(char *value);                                       // Sends a string, PROGMEM friendly
    void sendResponse(float value);                                       // Sends a float
    void sendResponse(double value);                                      // Sends a double
    void sendResponse(ASNTYPE *value);                                    // Sends an encoded object buffer, used to send an oid or a null field.
    void sendResponse(IPAddress value);                                   // Sends an IP address type object
```
##### Description
sendResponse() is the main function used to return the requested value from your service function.  It is heavily overloaded to support all the recognised SNMP data types.<br>
This function should be called as the last line in your service function.  The function will assemble the response frame and send it back to the SNMP client.<br>
Once this function has been called the previously received request data will be wiped ready for the next request.<br>
Note that write service functions also need to call sendResponse() as a final action to confirm back to the client the actual data that was written.
##### Parameters
_value_ This is the value appropriate for the oid requested
##### Returns
Nothing
##### Typical usage
```
snmp.sendResponse((char *)PSTR("ESP8266"));
```
### Global data Reference
#### snmpPacketsSent & snmpPacketsRecv
```
    unsigned long snmpPacketsSent = 0;
    unsigned long snmpPacketsRecv = 0;
```
##### Description
These variables are available holding a count of the snmp packets sent and received
#### workingpdu
```
  struct pdudata
  {
    byte *rxdata;       // pointer to the start of the received data buffer, this is the actual data received, hopefully as an asn.1 formatted frame
    byte *versionasn1;  // points to the version number field
    byte *comstrasn1;   // points to the community string field
    byte *reqidasn1;    // points to the request id field
    byte *errorasn1;    // points to the error field
    byte *erroridxasn1; // points to the error index field
    byte *oidasn1;      // points to the oid field
    byte *nextoidasn1;  // optional field, points to the next oid on a getnetxreq otherwise null
    byte *setvalueasn1; // optional field, points to the user data field on a setreq, used by the set function otherwise null
    byte *getvalueasn1; // optional field, points to the user data field to be returned on a getreq or getnextreq, set by the get function otherwise null
    byte requesttype;   // Set to the pdu type, getreq, getnextreq, getbulkreq of setreq supported
    byte version;       // Set to the SNMP version number
  };
```
##### Description
The workingpdu struct is available once a packet has been recieved and sucessfully parsed until the appropriate response has been sent.  Typically the only field likely to be required is the setvalueasn1 pointer which will be used to find the set value requested on a set request.
