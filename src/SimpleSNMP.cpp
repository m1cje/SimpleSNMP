#include <Arduino.h>
#include <WiFiUdp.h>
#include <SimpleSNMP.h>

#ifndef myLog_P
#define myLog_P Serial.printf_P
#endif

/********************************************
 * 27 Mar 2022 - Initial build
 *
 * To Do.-
 * snmp v3
 * snmp traps
 * package and publish
 *
 *******************************************/

/***********************************
 * Global variables
 * *********************************/
WiFiUDP snmpudp;           // udp port object
byte gpbuff[MAX_OID_SIZE]; // General purpose buffer space, always assume it's invalid before use

/**************************************************************************************************************************************************************
 * public snmpnode support functions
 **************************************************************************************************************************************************************/

snmpNode::snmpNode(const char *oidtext, void (*action)()) // Default constructor
{
    this->oid = oidtext;            // Store pointer to command text
    this->RWcommandAction = NULL;   // Init pointer to RW function
    this->ROcommandAction = action; // Store pointer to function to read the value
                                    //    this->SNMPreqtype = SNMP_TYPECODE_NOTSET; // Store the request type, RO or RW (0xA0 or 0xA3)
    this->next = NULL;              // Init pointer to next item in the list
}

/**************************************************************************************************************************************************************
 * public SimpleSNMP support functions
 **************************************************************************************************************************************************************/

///////////////////////////////////////////////////////////////////////////
// Constructor, initialise efault values
///////////////////////////////////////////////////////////////////////////
SimpleSNMP::SimpleSNMP(void)
{
    head = NULL;                            // Initialise liked list
    strcpy_P(ROcommunity, PSTR("public"));  // Default community names
    strcpy_P(RWcommunity, PSTR("private")); // Default community names
    snmpudp.begin(161);                     // SNMP is always on port 161
}

///////////////////////////////////////////////////////////////////////////
// Destructor, stop the udp session
///////////////////////////////////////////////////////////////////////////
SimpleSNMP::~SimpleSNMP(void)
{
    snmpudp.flush();
    snmpudp.stop();
}

///////////////////////////////////////////////////////////////////////////
// Set the read only community name
///////////////////////////////////////////////////////////////////////////
void SimpleSNMP::setROcommunity(const char *name) // Set the RO community name
{
    strncpy_P(ROcommunity, name, MAX_COMSTR_SIZE);
}

///////////////////////////////////////////////////////////////////////////
// Set the read write community name
///////////////////////////////////////////////////////////////////////////
void SimpleSNMP::setRWcommunity(const char *name) // Set the RW community name
{
    strncpy_P(RWcommunity, name, MAX_COMSTR_SIZE);
}

///////////////////////////////////////////////////////////////////////////
// Copies the asn.1 formatted user data to buff, returns the length of the asn.1 data field
///////////////////////////////////////////////////////////////////////////
byte SimpleSNMP::getUserData(byte *asn, byte *buff, byte len)
{
    memcpy_P(buff, asn + 2, len);
    return getASNlen(asn);
}

///////////////////////////////////////////////////////////////////////////
// Main loop() function, needs to be called regularly from main()
// Checks  if a frame has been received and processes it if found
///////////////////////////////////////////////////////////////////////////
void SimpleSNMP::action(void)
{
    int packetSize = snmpudp.parsePacket();

    if (packetSize)
    {
        snmpPacketsRecv++;                                  // increment packet count
        byte *packetBuffer = new byte[packetSize + 1];      // Add one to size just in case
        int rxlen = snmpudp.read(packetBuffer, packetSize); // Read incoming data

        SNMP_PARSE_STAT_CODES parse_error = parsepdu(packetBuffer, rxlen); // Parse the pdu and check data is a valid snmp record, store relevant fields into public variables
        if (parse_error == SNMP_PACKET_SUCCESS)
        {
            oid2char(); // Store oid decode into gpbuff
            switch (workingpdu.requesttype)
            {
            case SNMP_TYPECODE_GETREQ:
            case SNMP_TYPECODE_GETBULKREQ:
                processGetRequest((const char *)gpbuff); // search list for oid matching received values and call processing function attached
                break;
            case SNMP_TYPECODE_GETNEXTREQ:
                processGetNextRequest((const char *)gpbuff); // search list for oid matching received values and call processing function attached
                break;
            case SNMP_TYPECODE_GSETREQ:
                processSetRequest((const char *)gpbuff); // search list for oid matching received values and call processing function attached
                break;
            default:
                break;
            }
        }
        else
        {
            myLog_P(PSTR("Receive packet rejected (error %d) %s\r\n"), parse_error, FPSTR(getErrorText(parse_error)));
            dumpRaw(packetBuffer);
            dumpData(packetBuffer, rxlen, 1); // list fields
        }
        memset(&workingpdu, 0, sizeof(workingpdu)); // Remove all links to the rx data buffer
        delete[] packetBuffer;                      // Remove the rx data buffer
    }
}

///////////////////////////////////////////////////
// Send response functions
///////////////////////////////////////////////////

void SimpleSNMP::sendResponse(char value) // Sends a byte
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 1);
}
void SimpleSNMP::sendResponse(byte value) // Sends a byte
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 1);
}
void SimpleSNMP::sendResponse(int16_t value) // Sends a byte
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 2);
}
void SimpleSNMP::sendResponse(uint16_t value) // Sends an unsigned int
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 2);
}
void SimpleSNMP::sendResponse(int32_t value) // Sends an int
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 4);
}
void SimpleSNMP::sendResponse(uint32_t value) // Sends an unsigned int
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 4);
}
void SimpleSNMP::sendResponse(int64_t value) // Sends an int
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 8);
}
void SimpleSNMP::sendResponse(uint64_t value) // Sends an unsigned int
{
    sendResponse((long long)value, SNMP_DATATYPE_INTEGER, 8);
}

// Send string response, PROGMEM friendly for source string
void SimpleSNMP::sendResponse(char *value)
{
    byte responseValueBuffer[MAX_OID_SIZE - 30];                          // Buffer used to build the response packet, allow space for version, comstr etc
    responseValueBuffer[0] = SNMP_DATATYPE_OCTETSTRING;                   // type char string
    responseValueBuffer[1] = strlen_P(value);                             // length for a byte value
    strncpy_P((char *)responseValueBuffer + 2, value, MAX_OID_SIZE - 32); // Copy data into buffer, value can be in PROGMEM
    buildResponseBuffer(gpbuff, responseValueBuffer);                     // Build the complete response frame
    sendResponseBuffer(gpbuff);                                           // and send it
}

// Send an asn.1 type response, value needs to be an asn.1 formatted field, eg when sending an oid  response
void SimpleSNMP::sendResponse(ASNTYPE *value)
{
    buildResponseBuffer(gpbuff, value); // Build the complete response frame
    sendResponseBuffer(gpbuff);         // and send it
}

// Send an asn.1 type response, value needs to be an integer type
void SimpleSNMP::sendResponse(long long value, SNMP_DATA_TYPE type)
{
    sendResponse(value, type, 0);
}
void SimpleSNMP::sendResponse(long long value, SNMP_DATA_TYPE type, byte length)
{
    // Serial.printf("%s: Type %d Value %lld\r\n", __func__, type, value);
    byte responseValueBuffer[20]; // Buffer used to build the response packet, allow space for version, comstr etc
    switch (type)
    {
    case SNMP_DATATYPE_COUNTER32:
        responseValueBuffer[0] = SNMP_DATATYPE_COUNTER32; // type coubnter32
        responseValueBuffer[1] = 4;                       // data length
        responseValueBuffer[2] = (value >> 24) & 0xFF;
        responseValueBuffer[3] = (value >> 16) & 0xFF;
        responseValueBuffer[4] = (value >> 8) & 0xFF;
        responseValueBuffer[5] = value & 0xFF;
        break;
    case SNMP_DATATYPE_INTEGER:                // 8 -- 32 bit integer
    case SNMP_DATATYPE_UNSIGNED:               // 8 -- 32 bit unsigned integer
    case SNMP_DATATYPE_INT64:                  // 64 bit integer
    case SNMP_DATATYPE_SIGNED64:               // 64 bit integer
    case SNMP_DATATYPE_UNSIGNED64:             // 64 bit unsigned integer
        responseValueBuffer[0] = type;         // type integer
        responseValueBuffer[1] = 0x01;         // length for a byte value
        responseValueBuffer[2] = value & 0xFF; // length store the lower eight bits

        for (byte i = 1; i < length; i++) // Shift in extra bytes if longer than 1
        {
            value >>= 8; // shift the next byte into the lsb
            for (int j = 0; j < i; j++)
                responseValueBuffer[i - j + 2] = responseValueBuffer[i - j + 1]; // shuffle bytes along one place
            responseValueBuffer[2] = value & 0xFF;                               // msb
            responseValueBuffer[1]++;                                            // increase length by a byte
        }
        break;
    case SNMP_DATATYPE_TIMETICKS: // type timeticks, non signed integer
    case SNMP_DATATYPE_UTCTIME:
        responseValueBuffer[0] = type;
        responseValueBuffer[1] = 4; // data length
        responseValueBuffer[2] = value >> 24;
        responseValueBuffer[3] = value >> 16;
        responseValueBuffer[4] = value >> 8;
        responseValueBuffer[5] = value;
        break;
    case SNMP_DATATYPE_NULL:
    case SNMP_DATATYPE_NOTSET:
        responseValueBuffer[0] = type;
        responseValueBuffer[1] = 0; // data length
        break;
    // case SNMP_DATATYPE_BOOLEAN:     // Not integer
    // case SNMP_DATATYPE_BITSTRING:   // Not integer
    // case SNMP_DATATYPE_OCTETSTRING: // Not integer
    // case SNMP_DATATYPE_OID:         // Not integer
    // case SNMP_DATATYPE_IPADDRESS:   // Not integer
    // case SNMP_DATATYPE_FLOAT:       // Not integer
    // case SNMP_DATATYPE_DOUBLE:      // Not integer
    default:
        return;
    }
    buildResponseBuffer(gpbuff, responseValueBuffer); // Build the complete response frame
    sendResponseBuffer(gpbuff);                       // and send it
}

// Send an asn.1 type response, value needs to be an IP address object
void SimpleSNMP::sendResponse(IPAddress value)
{
    byte responseValueBuffer[6]; // Buffer used to build the response packet, allow space for version, comstr etc

    responseValueBuffer[0] = SNMP_DATATYPE_IPADDRESS;
    responseValueBuffer[1] = 4; // data length
    responseValueBuffer[2] = value[0];
    responseValueBuffer[3] = value[1];
    responseValueBuffer[4] = value[2];
    responseValueBuffer[5] = value[3];
    buildResponseBuffer(gpbuff, responseValueBuffer); // Build the complete response frame
    sendResponseBuffer(gpbuff);                       // and send it
}

// Send an asn.1 type response, value needs to be a float
void SimpleSNMP::sendResponse(float value)
{
    byte responseValueBuffer[6]; // Buffer used to build the response packet, allow space for version, comstr etc
    union
    {
        float rval;
        byte ival[4];
    } v;

    responseValueBuffer[0] = SNMP_DATATYPE_FLOAT;
    responseValueBuffer[1] = 4; // data length
    v.rval = value;
    for (byte i = 0; i < 4; i++)
        responseValueBuffer[5 - i] = v.ival[i];

    buildResponseBuffer(gpbuff, responseValueBuffer); // Build the complete response frame
    sendResponseBuffer(gpbuff);                       // and send it
}

// Send an asn.1 type response, value needs to be a float
void SimpleSNMP::sendResponse(double value)
{
    byte responseValueBuffer[10]; // Buffer used to build the response packet, allow space for version, comstr etc
    union
    {
        double rval;
        byte ival[8];
    } v;

    responseValueBuffer[0] = SNMP_DATATYPE_DOUBLE;
    responseValueBuffer[1] = 8; // data length
    v.rval = value;
    for (byte i = 0; i < 8; i++)
        responseValueBuffer[9 - i] = v.ival[i];

    buildResponseBuffer(gpbuff, responseValueBuffer); // Build the complete response frame
    sendResponseBuffer(gpbuff);                       // and send it
}

/**************************************************************************************************************************************************************
 * private SimpleSNMP class support functions
 **************************************************************************************************************************************************************/

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Locates the snmp data in the rxbuffer and stores pointers to each element into the workingpdu struct
// pdu points to a complete received frame buffer formatted as asn.1 data.
// Returns SUCCESS if all data present is validated otherwise returns an error code indicating the error
//////////////////////////////////////////////////////////////////////////////////////////////////////////
SNMP_PARSE_STAT_CODES SimpleSNMP::parsepdu(byte *pdu, uint16_t rxlen)
{
    memset(&workingpdu, 0, sizeof(workingpdu)); // Initialie the index list
    workingpdu.rxdata = pdu;                    // Store a pointer to the start of the data, this can be used to validate we have a full buffer

    if (pdu[0] != 0x30) // Check we got a snmp message type packet header, all snmp frames start with an 0x30
        return SNMP_PACKET_INVALID;

    if (rxlen != pdu[1] + 2) // a valid asn.1 pdu will have the data type in byte[0] and the data length in byte[1] so byte[1] should equal the data read length
        return SNMP_LENGTH_PACKET_INVALID;

    if (!getversion(pdu)) // extract SNMP version and store in global oid buffer
        return SNMP_VERSION_NOT_SUPPORTED;

    if (!getcomstr(pdu)) // extract community and store in global buffer
        return SNMP_COMSTR_NOT_FOUND;

    getType(pdu); // extract the request type, the request id, the error & the error index fields
    switch (workingpdu.requesttype)
    {
    case SNMP_TYPECODE_GETREQ:
    case SNMP_TYPECODE_GETNEXTREQ:
        if (!checkROcomstr())
            return SNMP_COMMUNITYSTRING_NOT_MATCHED;
        break;
    case SNMP_TYPECODE_GSETREQ:
        if (!checkRWcomstr())
            return SNMP_COMMUNITYSTRING_NOT_MATCHED;
        break;
    default:
        return SNMP_REQTYPE_NOT_SUPPORTED;
        break;
    }

    workingpdu.oidasn1 = workingpdu.rxdata; // Store start of data pointer, getoid() will update this or clear it.
    if (!getoid(workingpdu.oidasn1))        // extract pointer to oid data field, also extracts user data if its a setreq
        return SNMP_OID_NOT_FOUND;

    return SNMP_PACKET_SUCCESS;
}

///////////////////////////////////////////////////////////////////////////
// Returns a PROGMEM pointer to a text string for each error type
///////////////////////////////////////////////////////////////////////////
const char *snmpErrorText[] PROGMEM = {"SUCCESS", "INVALID", "LENGTH", "OID", "COMSTR", "VERSION", "REQUEST", "COMSTR"};
const char *SimpleSNMP::getErrorText(byte errno) // Returns a pointer to the error text
{
    return snmpErrorText[errno];
}

///////////////////////////////////////////////////////////////////////////
// Checks the last received pdy against the RO community string
// Returns true if they match
///////////////////////////////////////////////////////////////////////////
bool SimpleSNMP::checkROcomstr(void) // Checks the last received record for a community string match
{
    return !strcmp(ROcommunity, decodeComStr());
}
///////////////////////////////////////////////////////////////////////////
// Checks the last received pdy against the RW community string
// Returns true if they match
///////////////////////////////////////////////////////////////////////////
bool SimpleSNMP::checkRWcomstr(void) // Checks the last received record for a community string match
{
    return !strcmp(RWcommunity, decodeComStr());
}

///////////////////////////////////////////////////////////////////////////
// extracts the snmp request type from a udp frame and stores it into the reqtype buffer
// pdu points to a frame buffer formatted as asn.1 data.
// Returns true for compatibility, should not be able to fail
///////////////////////////////////////////////////////////////////////////
bool SimpleSNMP::getType(byte *pdu) // Gets the SNMP record type
{
    byte *phs = workingpdu.rxdata + workingpdu.rxdata[6] + 7; // PDU Header Start pointer
    workingpdu.requesttype = phs[0];                          // request type byte
    workingpdu.reqidasn1 = phs + 2;                           // request id starts 2 bytes beyond
    workingpdu.errorasn1 = workingpdu.reqidasn1 + workingpdu.reqidasn1[1] + 2;
    workingpdu.erroridxasn1 = workingpdu.errorasn1 + workingpdu.errorasn1[1] + 2;
    return true;
}

///////////////////////////////////////////////////////////////////////////
// extracts the snmp version number from a udp frame and siores it into the version buffer
// pdu points to a frame buffer formatted as asn.1 data.
// Returns true if it finds an acceptable version, ie version 1 or 2
///////////////////////////////////////////////////////////////////////////
bool SimpleSNMP::getversion(byte *pdu)
{
    bool versionOK = false;
    workingpdu.versionasn1 = pdu + 2;
    workingpdu.version = decodeInt(workingpdu.versionasn1) + 1;            // 0 == version 1, 1 == version 2, 4 == version 3
    workingpdu.version = workingpdu.version == 4 ? 3 : workingpdu.version; // Fixup snmp version 3
    if (workingpdu.version == 1 || workingpdu.version == 2)
        versionOK = true;
    return versionOK;
}

///////////////////////////////////////////////////////////////////////////
// extracts the community string from a udp frame and siores it into the comstr buffer
// pdu points to a frame buffer formatted as asn.1 data.
// Returns true if it finds a valid community string
///////////////////////////////////////////////////////////////////////////
bool SimpleSNMP::getcomstr(byte *pdu)
{
    workingpdu.comstrasn1 = workingpdu.rxdata + 5; // start of rxdata + 2 bytes for type field + 3 bytes for version field, next field should be the community string
    return true;
}

///////////////////////////////////////////////////////////////////////////
// Decodes the asn.1 community string
// Defaults to converting the working comstr field
// Stores the result into gpbuff
// Retuns a pointer to gpbuff if successfull else NULL
// asn points to a buffer formatted as asn.1 octet string data.
///////////////////////////////////////////////////////////////////////////
char *SimpleSNMP::decodeComStr(void) // Returns a pointer to the community string formatted as a null terminated string, uses gpbuff
{
    return decodeComStr(workingpdu.comstrasn1);
}
char *SimpleSNMP::decodeComStr(byte *asn) // Returns a pointer to the community string formatted as a null terminated string, uses gpbuff
{
    if (!asn)
        return NULL;
    if (workingpdu.comstrasn1[1])
        strncpy((char *)gpbuff, (const char *)workingpdu.comstrasn1 + 2, workingpdu.comstrasn1[1]); // Copy community string
    gpbuff[workingpdu.comstrasn1[1]] = 0x00;                                                        // NULL terminate the string
    return (char *)gpbuff;
}

/////////////////////////////////////////////////////////////////////////////////////
// extracts the oid from a udp frame and stores it into the oid buffer
// pdu points to a frame buffer formatted as asn.1 data.
// If it's a compound data type then it is recursed into
// If it's a primitive then it's ignored unless it's an oid in which case it's location is stored
// Returns true if it finds a valid oid
// Only processes the first primitive data field in a compound data type then returns
// if it finds an oid and it's a setreq then it also stores the pointer to the value field
//////////////////////////////////////////////////////////////////////////////////////
bool SimpleSNMP::getoid(byte *oidasn1)
{
    bool found = false;

    switch (oidasn1[0])
    {
    // compound data types, recurse into these
    case SNMP_DATATYPE_VARBIND:
    case SNMP_TYPECODE_GETNEXTREQ:
    case SNMP_TYPECODE_GSETREQ:
    case SNMP_TYPECODE_GETREQ:
        found = getoid(oidasn1 + getASNhdrlen(oidasn1)); // Skip over other known data types
        break;
    // primitive data types, skip over these
    case SNMP_DATATYPE_INTEGER:
    case SNMP_DATATYPE_OCTETSTRING:
    case SNMP_DATATYPE_NULL:
        found = getoid(oidasn1 + getASNhdrlen(oidasn1) + getASNlen(oidasn1)); // Skip over other known data types
        break;
    // OID data type, process this
    case SNMP_DATATYPE_OID:
        workingpdu.oidasn1 = oidasn1;                                                       // Store pointer to oid field
        if (workingpdu.requesttype == SNMP_TYPECODE_GSETREQ)                                // If it's a set request, user data field should immidietly follow the oid field
            workingpdu.setvalueasn1 = oidasn1 + getASNhdrlen(oidasn1) + getASNlen(oidasn1); // set pointer to user data field
        found = true;
        break;
    default:
        workingpdu.oidasn1 = NULL; // clear oid pointer as we didn't find one
        break;
    }
    return found;
}

// Converts an oid ASN.1 data field to a char string
// Returns true if sucessfully converted
// Stores output into gpbuff
bool SimpleSNMP::oid2char(void)
{
    return oid2char(workingpdu.oidasn1);
}
bool SimpleSNMP::oid2char(byte *asn)
{
    bool result = true;         // Assume it works unless we get an error
    char *oid = (char *)gpbuff; // Set up char pointer to make copying easier

    memset(gpbuff, 0, sizeof(gpbuff)); // Clear output buffer

    if (asn == NULL || asn[0] != SNMP_DATATYPE_OID || asn[2] != 0x2B) // This is not a valid oid data buffer so abort
        return false;

    int ofl = 0;                                // output counter
    snprintf_P(oid, MAX_OID_SIZE, PSTR("1.3")); // translate first byte (always 0x2B = 1.3.)
    for (auto i = 1; i < asn[1]; i++)           // Start 1 octet in to skip the initial 0x2B == 1.3 oid
    {
        ofl += snprintf_P(oid + strlen(oid), MAX_OID_SIZE - ofl, PSTR(".%ld"), decodeOidInt(asn + i + 2)); // decode value

        while (asn[i + 2] & 0x80) // Skip over multibyte value
            i++;

        if (ofl > MAX_OID_SIZE || ofl < 0) // Check we didn't overflow the buffer or get an encoding error fro sprintf
            result = false;
    }
    return result;
}

long SimpleSNMP::decodeOidInt(byte *pdu) // Decodes oid values, pdu points to the next octet, no type or length passed
{
    long rval = pdu[0] & 0x7F; // Store the initial octet with the flag bit masked
    int i = 0;
    while ((pdu[i] & 0x80) && i < (int)sizeof(long))
    {
        rval <<= 7;              // shift the existing bits left
        i++;                     // point to the next octet
        rval |= (pdu[i] & 0x7F); // or the next octet onto the existing
    }
    return rval;
}

byte SimpleSNMP::getASNlen(byte *asn) // Returns the data length value from an asn.1 buffer
{
    byte len = 1;               // allow for type byte
    if (asn[1] & 0x80)          // offset if bit set
        len += (asn[1] & 0x7f); // Add the number of offset bytes
    return asn[len];
}

byte SimpleSNMP::getASNhdrlen(byte *asn) // Returns the length of the header from an asn.1 buffer, ie type byte + length of length field
{
    byte len = 2;               // allow for type byte and length field
    if (asn[1] & 0x80)          // if offset bit set
        len += (asn[1] & 0x7f); // Add the number of offset bytes
    return len;
}

long SimpleSNMP::decodeInt(byte *asn) // Decodes multi byte integers, note all ints should be signed
{
    if (!asn) // Check we have something to decode
        return 0L;
    long rval = decodeUnsignedInt(asn); // Decode integer

    // Sort out sign extension for negative numbers
    int i = asn[1];
    if ((i == 1) && (rval > 0x7f)) // fixup negative
        rval |= 0xFFFFFF80;
    if ((i == 2) && (rval > 0x7fff)) // fixup negative
        rval |= 0xFFFF8000;
    if ((i == 3) && (rval > 0x7fffff)) // fixup negative
        rval |= 0xFF800000;
    return rval;
}

unsigned long SimpleSNMP::decodeUnsignedInt(byte *asn) // Decodes multi byte integers, note all ints should be signed
{
    if (!asn) // Check we have something to decode
        return 0L;
    unsigned long rval = decodeInt64(asn);
    return rval;
}

// This function underpins the other signed int decodes
long long SimpleSNMP::decodeInt64(byte *asn) // Decodes multi byte integers, note all ints should be signed
{
    if (!asn) // Check we have something to decode
        return 0LL;
    long long rval = decodeUnsignedInt64(asn);

    // Sort out sign extension for negative numbers
    int i = asn[1];
    if ((i == 1) && (rval > 0x7f)) // fixup negative
        rval |= 0xFFFFFFFFFFFFFF80;
    if ((i == 2) && (rval > 0x7fff)) // fixup negative
        rval |= 0xFFFFFFFFFFFF8000;
    if ((i == 3) && (rval > 0x7fffff)) // fixup negative
        rval |= 0xFFFFFFFFFF800000;
    if ((i == 4) && (rval > 0x7fffffff)) // fixup negative
        rval |= 0xFFFFFFFF80000000;
    if ((i == 5) && (rval > 0x7fffffffff)) // fixup negative
        rval |= 0xFFFFFF8000000000;
    if ((i == 6) && (rval > 0x7fffffffffff)) // fixup negative
        rval |= 0xFFFF800000000000;
    if ((i == 7) && (rval > 0x7fffffffffffff)) // fixup negative
        rval |= 0xFF80000000000000;

    return rval;
}

// This function underpins the other unsigned int decodes
unsigned long long SimpleSNMP::decodeUnsignedInt64(byte *asn) // Decodes multi byte integers, note all ints should be signed
{
    if (!asn) // Check we have something to decode
        return 0LL;
    unsigned long long rval = 0;
    switch (asn[0])
    {
    case SNMP_DATATYPE_INTEGER:
    case SNMP_DATATYPE_UNSIGNED:
    case SNMP_DATATYPE_TIMETICKS:
    case SNMP_DATATYPE_INT64:
    case SNMP_DATATYPE_SIGNED64:
    case SNMP_DATATYPE_UNSIGNED64:
        for (int i = 0; i < asn[1]; i++)
            rval = (rval << 8) | asn[2 + i]; // Accumulate the value
        break;
    }
    return rval;
}

float SimpleSNMP::decodeFloat(byte *asn) // Decodes multi byte integers, note all ints should be signed
{
    if (!asn || asn[1] != 4) // Check we have something to decode
        return 0.0;
    union
    {
        float rval;
        byte ival[4];
    } v;

    for (byte i = 0; i < 4; i++)
        v.ival[i] = asn[5 - i];
    return v.rval;
}

double SimpleSNMP::decodeDouble(byte *asn) // Decodes multi byte integers, note all ints should be signed
{
    if (!asn || asn[1] != 8) // Check we have something to decode
        return 0.0;
    union
    {
        double rval;
        byte ival[8];
    } v;

    for (byte i = 0; i < 8; i++)
        v.ival[i] = asn[9 - i];
    return v.rval;
}

///////////////////////////////////////////////////
// Send response functions
///////////////////////////////////////////////////

void SimpleSNMP::sendErrorResponse(SNMP_ERROR_CODE errorno) // Sends an error response
{
    workingpdu.errorasn1[2] = errorno;           // Set error type to 4 RO error
    byte responseValueBuffer[2];                 // buffer to store the value field, 8 bytes for the long long then 2 for the type and length
    responseValueBuffer[0] = SNMP_DATATYPE_NULL; // type integer
    responseValueBuffer[1] = 0x00;               // length for a NULL value
    sendResponse(responseValueBuffer);           // send no oid response error
}

// Builds the response frame
// responsebuffer points to a buffer large enougth to hold the response
// valueBuffer points to an ASN.1 formatted object holding the value to be returned.
void SimpleSNMP::buildResponseBuffer(byte *responseBuffer, byte *valueBuffer) // Builds the snmp response frame at responsebuffer
{
    responseBuffer[0] = 0x30;                                                                     // SNMP message type = 0x30
    responseBuffer[1] = 0;                                                                        // Length of type data
    appendASN1(responseBuffer, workingpdu.versionasn1);                                           // add version
    appendASN1(responseBuffer, workingpdu.comstrasn1);                                            // add community string
    responseBuffer[1] += addSnmpResponsePDU(responseBuffer + responseBuffer[1] + 2, valueBuffer); // add response pdu, P1 = point to append to, ie start of request type object
}

// adds the response pdu to the end of the response buffer
// valuebuffer is an ASN.1 formatted object containing the response data
// responsebuffer points to the start of the new data
// Does not update the parent length byte, updates it;s own length byte
// Returns the length of the added data
byte SimpleSNMP::addSnmpResponsePDU(byte *responseBuffer, byte *valueBuffer)
{
    responseBuffer[0] = 0xa2;                            // Response id type
    responseBuffer[1] = 0x00;                            // data length
    appendASN1(responseBuffer, workingpdu.reqidasn1);    // Append the request id field and updates the length
    appendASN1(responseBuffer, workingpdu.errorasn1);    // Append the error field and updates the length
    appendASN1(responseBuffer, workingpdu.erroridxasn1); // Append the error index field and updates the length
    responseBuffer[1] += addSnmpVBlist(responseBuffer + responseBuffer[1] + 2, valueBuffer);
    return responseBuffer[1] + 2; // Length of the pdu type obkect
}

// Adds the VBlist pdu to the reponse buffer
// Returns the length of the added data
// Does not update the buffer length feild
byte SimpleSNMP::addSnmpVBlist(byte *responseBuffer, byte *valueBuffer)
{
    responseBuffer[0] = 0x30;                                           // Response id type
    responseBuffer[1] = addSnmpVBtype(responseBuffer + 2, valueBuffer); // Set length to the total length of the next varibind
    return responseBuffer[1] + 2;                                       // Length of the pdu type obkect
}

// Adds the VBtype pdu to the response buffer
// Adds the source OID to the response buffer
// Adds the Value field data to the response buffer
// Updates the buffer length field
// Returns the length of the added data
byte SimpleSNMP::addSnmpVBtype(byte *responseBuffer, byte *valueBuffer)
{
    responseBuffer[0] = 0x30; // Response id type
    responseBuffer[1] = 0x00; // data length
    if (workingpdu.nextoidasn1)
        appendASN1(responseBuffer, workingpdu.nextoidasn1); // add nextoid for getnext if there is one
    else
        appendASN1(responseBuffer, workingpdu.oidasn1); // else append the request oid
    appendASN1(responseBuffer, valueBuffer);            // Append the user data response value and updates the length
    return responseBuffer[1] + 2;                       // Length of the pdu type obkect
}

// Appends a source asn.1 object to dest, make sure there is room in the dest for the copy.
// dest should be a compound asn.1 object, src will be appended to the end of the compound object
// updates the destination data length
// Returns the length of the added data
byte SimpleSNMP::appendASN1(byte *dest, byte *src)
{
    memcpy(dest + dest[1] + 2, src, src[1] + 2); // copy field data plus header to the end of the dest buffer
    dest[1] += src[1] + 2;                       // add length of the appended data to the dest field length
    return src[1] + 2;                           // Return the length of the appended data
}

// Sends the response buffer back to the requestor
// Returns the udp.endpacket() response code, 1 if ok, 0 if error
bool SimpleSNMP::sendResponseBuffer(byte *responseBuffer) // Sends the snmp response frame at responsebuffer, returns true if ok
{
    snmpudp.beginPacket(snmpudp.remoteIP(), snmpudp.remotePort());
    snmpudp.write((char *)responseBuffer, responseBuffer[1] + 2);
    snmpPacketsSent++; // Increment Tx count
    return snmpudp.endPacket();
}

// Finds an oid field in the source pdu
// Recurses through the pdu data structure until it finds a pdu
// Probably crashes if there isn't an oid object but this should never happen as we are only called if we have received a valid request
// Returns a pointer to the start of the pdu field
byte *SimpleSNMP::findOid(byte *pdu) // Searches a pdu for the oid data type record
{
    byte *found = NULL;
    switch (pdu[0]) // walk the pdu to find the oid start
    {
    case SNMP_DATATYPE_VARBIND:
    case SNMP_TYPECODE_GETNEXTREQ:
    case SNMP_TYPECODE_GSETREQ:
    case SNMP_TYPECODE_GETREQ:
        found = findOid(pdu + 2); // compound type, recurse into it
        break;
    case SNMP_DATATYPE_OID:
        found = pdu;
        break;
    default:
        found = findOid(pdu + 2 + pdu[1]); // Skip over other known data types
        break;
    }
    return found;
}

///////////////////////////////////////////////////
// Debug functions
///////////////////////////////////////////////////

/*******************************************************************
 * Prints the contents of the workingpdu struct
 *******************************************************************/
void SimpleSNMP::dumpStruct(void)
{
    dumpData(workingpdu.rxdata);
    myLog_P(PSTR("workingpdu::\r\n\trxdata:"));
    dumpField(workingpdu.rxdata);
    myLog_P(PSTR("\tsnmp version:%d\r\n"), workingpdu.version);
    myLog_P(PSTR("\tcomstr:%s\r\n"), decodeComStr());
    myLog_P(PSTR("\treqid:%ld\r\n"), decodeInt(workingpdu.reqidasn1));
    myLog_P(PSTR("\terror:%ld\r\n"), decodeInt(workingpdu.errorasn1));
    myLog_P(PSTR("\terror idx:%ld\r\n"), decodeInt(workingpdu.erroridxasn1));
    oid2char(); // Store oid into gpbuff
    myLog_P(PSTR("\tOID:%s\r\n"), (char *)gpbuff);
    oid2char(workingpdu.nextoidasn1); // Store oid into gpbuff
    myLog_P(PSTR("\tNext OID:%s\r\n"), (char *)gpbuff);
    myLog_P(PSTR("\tget value:"));
    dumpField(workingpdu.getvalueasn1);
    myLog_P(PSTR("\r\n\tset value:"));
    dumpField(workingpdu.setvalueasn1);
    myLog_P(PSTR("\r\n\n"));
}

void SimpleSNMP::dumpData(byte *pdu) // Lists the encoded data fields
{
    if (pdu)
        dumpData(pdu, pdu[1], 1);
}

/*******************************************************************
 * Recursively walks an asn.1 data buffer
 * prints the contents of each field found
 *******************************************************************/
void SimpleSNMP::dumpData(byte *pdu, byte oidlen, int level) // Lists the encoded data fields
{
    byte plen = 0;         // Stores initial data length so we can decrement it
    int cnt = 0;           // Level counter, incremented every time we recurse
    const char *otype = 0; // Pointer to the object type description

    while (plen < oidlen) // Loop until we run out of data
    {
        byte ftype = pdu[0];        // Store object type
        byte flen = getASNlen(pdu); // Store object length
        bool constructed = 0;       // Set to the number of bytes to skip when we have a constructed data type that needs recursing into
        byte extradata = 0;         // Set to force skip over extra 0x9F in int64 type
        cnt++;                      // Increment level count
        switch (ftype)
        {
        case SNMP_DATATYPE_INTEGER:
            otype = PSTR("Integer");
            break;
        case SNMP_DATATYPE_OCTETSTRING:
            otype = PSTR("Octet String");
            break;
        case SNMP_DATATYPE_NULL:
            otype = PSTR("Null");
            break;
        case SNMP_DATATYPE_OID:
            otype = PSTR("OID");
            break;
        case SNMP_DATATYPE_UTCTIME:
            otype = PSTR("UTCtime");
            break;
        case SNMP_DATATYPE_IPADDRESS:
            otype = PSTR("IP Address");
            break;
        case SNMP_DATATYPE_UNSIGNED:
            otype = PSTR("Unsigned");
            break;
        case SNMP_DATATYPE_TIMETICKS:
            otype = PSTR("TimeTick");
            break;
        case SNMP_DATATYPE_INT64:
            otype = PSTR("Int64");
            constructed = true;
            extradata = 1; // Int64 type has an extra 0x9b byte in it's header
            break;
        case SNMP_DATATYPE_VARBIND:
            otype = PSTR("Varbind");
            constructed = true;
            break;
        case SNMP_TYPECODE_GETREQ:
            otype = PSTR("Get Request");
            constructed = true;
            break;
        case SNMP_TYPECODE_GETNEXTREQ:
            otype = PSTR("Get Next Request");
            constructed = true;
            break;
        case SNMP_TYPECODE_GETRESPONSE:
            otype = PSTR("Get Respomse");
            constructed = true;
            break;
        case SNMP_TYPECODE_GSETREQ:
            otype = PSTR("Set Request");
            constructed = true;
            break;
        case SNMP_TYPECODE_GETBULKREQ:
            otype = PSTR("Get Bulk Respomse");
            constructed = true;
            break;
        case SNMP_DATATYPE_FLOAT:
            otype = PSTR("Float");
            break;
        case SNMP_DATATYPE_DOUBLE:
            otype = PSTR("Double");
            break;
        case SNMP_DATATYPE_SIGNED64:
            otype = PSTR("Signed 64bit");
            break;
        case SNMP_DATATYPE_UNSIGNED64:
            otype = PSTR("UnSigned 64bit");
            break;
        default:
            otype = PSTR("Unknown");
            break;
        }
        myLog_P(PSTR("  Field %d.%d, type %u (%s), length %u"), level, cnt, ftype, FPSTR(otype), flen);
        dumpField(pdu); // Display field contents, prints the final crlf
        if (constructed)
            dumpData(pdu + getASNhdrlen(pdu) + extradata, (byte)(flen - extradata), level + 1); // Recurse into constructed data type passing contents of constructed data type
        flen += extradata;                                                                      // skip the extra byte in INT64
        flen += 2;                                                                              // Add the two header bytes
        pdu += flen;                                                                            // move pointer to next field
        plen += flen;                                                                           // Subtract remaining data length
    }
}

/*******************************************************************
 * Displays the contents of a asn.1 data field as hex data
 * Prints the header in brackets followed by the data
 * After the hex dump it displays the decoded field contents
 *******************************************************************/
void SimpleSNMP::dumpField(byte *pdu) // Displays the contents of a asn.1 field as hex bytes
{
    if (!pdu) // NULL field, return
        return;
    myLog_P(PSTR(" ["));

    for (auto i = 0; i < getASNhdrlen(pdu); i++) // Print header field raw hex data
    {
        myLog_P(PSTR(" (%02.2X)"), pdu[i]);
    }

    for (auto i = 0; i < getASNlen(pdu); i++) // Print data field raw hex data
    {
        myLog_P(PSTR(" %02.2X"), pdu[i + 2]);
    }

    switch (pdu[0])
    {
    case SNMP_DATATYPE_INTEGER:
        myLog_P(PSTR(" {%ld}"), decodeInt(pdu));
        break;
    case SNMP_DATATYPE_UNSIGNED:
        myLog_P(PSTR(" {%lu}"), decodeUnsignedInt(pdu));
        break;
    case SNMP_DATATYPE_INT64:
        myLog_P(PSTR(" {Compound Int64}"), decodeInt64(pdu));
        break;
    case SNMP_DATATYPE_FLOAT:
        myLog_P(PSTR(" {%f}"), decodeFloat(pdu));
        break;
    case SNMP_DATATYPE_DOUBLE:
        myLog_P(PSTR(" {%lf}"), decodeDouble(pdu));
        break;
    case SNMP_DATATYPE_SIGNED64:
        myLog_P(PSTR(" {%lld}"), decodeInt64(pdu));
        break;
    case SNMP_DATATYPE_UNSIGNED64:
        myLog_P(PSTR(" {%llu}"), decodeInt64(pdu));
        break;
    case SNMP_DATATYPE_OCTETSTRING:
        myLog_P(PSTR(" {"));
        for (auto i = 0; i < pdu[1]; i++)
        {
            myLog_P(PSTR("%c"), pdu[i + 2] < ' ' ? '.' : pdu[i + 2]); // display char or dot if non printing char
        }
        myLog_P(PSTR("}"));
        break;
    case SNMP_DATATYPE_OID:
        myLog_P(PSTR(" {"));
        if (pdu[2] == 0x2B)
        {
            myLog_P(PSTR("1.3"));
            for (auto i = 1; i < pdu[1]; i++) // Start 1 octet in to skip the initial 0x2B == 1.3 oid
            {
                if (pdu[i + 2] & 0x80)
                {
                    myLog_P(PSTR(".%ld"), decodeOidInt(pdu + i + 2)); // decode multibyte value
                    while (pdu[i + 2] & 0x80)
                        i++; // Skip over multibyte value
                }
                else
                {
                    myLog_P(PSTR(".%d"), pdu[i + 2]);
                }
            }
        }
        else
        {
            myLog_P(PSTR("Invalid OID"));
        }
        myLog_P(PSTR("}"));
        break;
    case SNMP_DATATYPE_IPADDRESS:
        myLog_P(PSTR(" {%d.%d.%d.%d}"), pdu[2], pdu[3], pdu[4], pdu[5]); // IP address data type
        break;
    case SNMP_DATATYPE_TIMETICKS:
    {
        time_t tt = (time_t)decodeInt(pdu);
        myLog_P(PSTR(" {%lld seconds}"), tt);
        break;
    }
    case SNMP_DATATYPE_VARBIND:
    case SNMP_TYPECODE_GETREQ:
    case SNMP_TYPECODE_GSETREQ:
    case SNMP_TYPECODE_GETNEXTREQ:
    case SNMP_TYPECODE_GETBULKREQ:
        myLog_P(PSTR(" {Varibind}")); // varibind copound data type
        break;
    case SNMP_DATATYPE_NULL:
        myLog_P(PSTR(" {Null}")); // varibind copound data type
        break;
    default:
        myLog_P(PSTR(" {Unknown type}")); // unknown data type
        break;
    }
    myLog_P(PSTR("]\r\n"));
}

/*******************************************************************
 * Print out an asn.1 record as hex bytes for pasting into decoder
 *******************************************************************/
void SimpleSNMP::dumpRaw(byte *pdu)
{
    myLog_P(PSTR("Raw data"));
    for (auto i = 0; i < pdu[1] + 2; i++)
    {
        myLog_P(PSTR(" %02.2X"), pdu[i]);
    }
    myLog_P(PSTR("\r\n")); // crnl
}

/*************************************************************************************************************************************************************************/

void SimpleSNMP::insertNode(const char *oidtext, void (*action)()) // Function to insert a new node
{
    snmpNode *newNode = new snmpNode(oidtext, action); // Create the new Node.
    if (head == NULL)                                  // First item so assign to head
    {
        //        head = newNode;
        head = new snmpNode(oidtext, action); // Create the roor node
        return;
    }
    // Find the last item in the list
    snmpNode *temp = head;
    while (temp->next != NULL)
        temp = temp->next; // Update temp
    temp->next = newNode;  // Insert at the end.
}

// Walks the list looking for a oid match and if found updates the RW function pointer
bool SimpleSNMP::addRWaction(const char *oidfind, void (*action)()) // Function to add a RW action to a node
{
    snmpNode *flist = head;
    while (flist != NULL) // Traverse the list.
    {
        bool cresult = true;              // compare result flag
        byte i = 0;                       // pointer into source compare string
        char cp = pgm_read_byte(oidfind); // get first byte to compare
        while (cp)                        // while it's not a 0
        {
            if (cp != pgm_read_byte(flist->oid + i)) // if strings are not the same
            {
                cresult = false; // set false result
                break;           // breakout with result false
            }
            i++;                             // point to next char
            cp = pgm_read_byte(oidfind + i); // get next char
        }

        if (cresult) // Check for command match
        {
            flist->RWcommandAction = action; // Update function pointer
                                             //            flist->SNMPreqtype = SNMP_TYPECODE_GSETREQ;
            return true;                     // indicate that we matched the command
        }
        flist = flist->next; // Iterate to next member
    }
    return false; // failed to match the command
}

// Walks the list looking for a command match and if found calls the attached function to process the command
bool SimpleSNMP::processGetRequest(const char *oidfind)
{
    snmpNode *flist = head; // initialise flist to point to the first entry in the list
    while (flist != NULL)   // Traverse the list.
    {
        // myLog_P(PSTR("%s: Looking for %s, found %s\r\n"), __func__, oidfind, flist->oid);
        if (compareStr_P(oidfind, flist->oid)) // Check for command match
        {
            if (flist && flist->ROcommandAction) // check is wasn't the last one and we have a function to call
                flist->ROcommandAction();        // Run command, command function should build and send the appropriate response record
            return true;                         // indicate that we matched the command and break out
        }
        flist = flist->next; // Iterate to next member
    }
    sendErrorResponse(SNMP_NOSUCHNAME);
    return false; // failed to match the command
}

// Walks the list looking for a command match and if found calls the next nodes attached function to process the command
bool SimpleSNMP::processGetNextRequest(const char *oidfind) // Function to print the nodes of the linked list.  Returns true if command was matched
{
    snmpNode *flist = head;
    while (flist != NULL) // Traverse the list.
    {
        if (compareStr_P(oidfind, flist->oid)) // Check for exact command match and return the next oid
        {
            flist = flist->next; // Jump to next node
            workingpdu.nextoidasn1 = char2oid(flist->oid);
            if (flist && flist->ROcommandAction) // check is wasn't the last one and we have a function to call
                flist->ROcommandAction();        // Run command, command function should build and send the appropriate response record
            return true;                         // indicate that we matched the command and break out
        }
        else if (comparepStr_P(oidfind, flist->oid)) // Check for partial command match and return matching oid
        {
            workingpdu.nextoidasn1 = char2oid(flist->oid);
            if (flist && flist->ROcommandAction) // check is wasn't the last one and we have a function to call
                flist->ROcommandAction();        // Run command, command function should build and send the appropriate response record
            return true;                         // indicate that we matched the command and break out
        }
        flist = flist->next; // Iterate to next member if no match
    }
    sendErrorResponse(SNMP_NOSUCHNAME);
    return false; // failed to match the command
}

// Walks the list looking for a command match and if found calls the attached function to process the command
bool SimpleSNMP::processSetRequest(const char *oidfind) // Function to print the nodes of the linked list.  Returns true if command was matched
{
    snmpNode *flist = head;
    while (flist != NULL) // Traverse the list.
    {
        if (compareStr_P(oidfind, flist->oid)) // Check for command match
        {
            if (flist->RWcommandAction) // Check this oid has a RW function attached
            {
                flist->RWcommandAction(); // Run command,  command function should action the change then build and send the appropriate response record
                return true;              // indicate that we matched the command
            }
            else // oid matched but not RW
            {
                sendErrorResponse(SNMP_READONLY);
                return false; // indicate that we matched the command but it was RO
            }
        }
        flist = flist->next; // Iterate to next member
    }
    sendErrorResponse(SNMP_NOSUCHNAME);
    return false; // failed to match the command
}

// Walks the list displaying each element found
// level is the recursion level to display
bool SimpleSNMP::dumpList(void)
{
    return dumpList(head, 1); // Start listing at level 1
}
bool SimpleSNMP::dumpList(snmpNode *flist, byte level)
{
    if (flist)
    {
        myLog_P(PSTR("%s: Level %3d"), __func__, level);
        myLog_P(PSTR(" oid:[%30s]"), FPSTR(flist->oid));
        myLog_P(PSTR("ROfunc: [%s]"), flist->ROcommandAction ? "Set" : "Null");
        myLog_P(PSTR("\tRWfunc: [%s]"), flist->RWcommandAction ? "Set" : "Null");
        return dumpList(flist->next, ++level); // recurse into next item
    }
    return true;
}

// Compares two strings, PROGMEM safe
// Returns if both match and are the same length
bool SimpleSNMP::compareStr_P(const char *o1, const char *o2)
{
    char c1 = pgm_read_byte(o1++); // Read first character
    char c2 = pgm_read_byte(o2++); // Read first character

    while (c1 && c2 && c1 == c2)
    {
        c1 = pgm_read_byte(o1++); // Read first character
        c2 = pgm_read_byte(o2++); // Read first character
    }
    if (c1 == '\0' && c2 == '\0')
        return true;
    return false;
}

// Compares two strings, PROGMEM safe
// Returns true if o1 matches the start of o2
// o1 is the string we are looking for in o2
bool SimpleSNMP::comparepStr_P(const char *o1, const char *o2)
{
    char c1 = pgm_read_byte(o1++); // Read first character
    char c2 = pgm_read_byte(o2++); // Read first character

    while (c1 && c2 && c1 == c2)
    {
        c1 = pgm_read_byte(o1++); // Read first character
        c2 = pgm_read_byte(o2++); // Read first character
    }
    if (c1 == '\0') // We got to the end of o1 with a match
        return true;
    return false;
}

// Converts a char string oid text back into an asn.1 encoded oid, PROGMEM safe
// Returns a pointer to the converted asn.1 buffer or NULL if not found
byte *SimpleSNMP::char2oid(const char *oidtext) // Converts and oid text string back to an encoded oid asn.1 field and stores it in nextoid
{
    if (strncmp_P("1.3.", oidtext, 4))
        return NULL;

    nextoid[0] = SNMP_DATATYPE_OID;
    nextoid[1] = 1;    // data length
    nextoid[2] = 0x2B; // "1.3. static encoding"

    oidtext += 4; // points to the first char after the initial 1.3.

    int tidx = 3;      // pointer into the output buffer
    byte numstart = 0; // stores offset to the start of the converted output number
    long tokval = 0;
    while (oidtext) // read until no more tokens
    {
        tokval = atol_P(oidtext);
        numstart = tidx; // store offset to the start of the converted output number

        nextoid[tidx] = tokval & 0x7F; // Store lsb
        nextoid[1]++;                  // Length

        while (tokval > 127) // Fix up values gt 127, tidx points to the start of the number
        {
            memmove(nextoid + numstart + 1, nextoid + numstart, 4); // move bytes along, max oid number is 32 bits long
            tokval >>= 7;                                           // Shuffle input along 7 bits
            nextoid[numstart] = tokval & 0x7F;                      // Store lsb
            nextoid[numstart] |= 0x80;                              // Turn on overflow bit
            nextoid[1]++;                                           // Length
            tidx++;                                                 // Move ptr to next free byte
        }
        tidx++;                            // Move ptr to next free byte
        oidtext = findTok_P(oidtext, '.'); // Find the next token, set to null if no token found
    }
    return nextoid;
}

// Converts a string to a long, PROGMEM safe
// Returns a long, returns 0 if not numeric string
// Converts up to the first non numeric char
long SimpleSNMP::atol_P(const char *s)
{
    long op = 0;
    char c = pgm_read_byte(s++); // Read first character
    while (c >= '0' && c <= '9')
    {
        op *= 10;
        op += (c - '0');
        c = pgm_read_byte(s++); // Read next character
    }
    return op;
}

// s = null terminated string to search
// delimiter = just that
// return a pointer to the next char after the delimiter of null if no delim found
// PROGMEM safe on input string
const char *SimpleSNMP::findTok_P(const char *s, char delimiter)
{
    char c = pgm_read_byte(s++); // Read first character
    while (c)                    // while it's not null term
    {
        if (delimiter == c)     // found delim
            return s++;         // return pointer to next char after delim
        c = pgm_read_byte(s++); // get next char
    }
    return NULL; // did not find the delim
}
