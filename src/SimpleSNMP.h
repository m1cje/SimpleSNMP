#pragma once
#include <Arduino.h>

#define MAX_OID_SIZE 128   // Largest oid allowed
#define MAX_COMSTR_SIZE 20 // Largest community string allowed

enum SNMP_PARSE_STAT_CODES // packet parser status return codes
{
    SNMP_PACKET_SUCCESS = 0,
    SNMP_PACKET_INVALID = 1,
    SNMP_LENGTH_PACKET_INVALID = 2,
    SNMP_OID_NOT_FOUND = 3,
    SNMP_COMSTR_NOT_FOUND = 4,
    SNMP_VERSION_NOT_SUPPORTED = 5,
    SNMP_REQTYPE_NOT_SUPPORTED = 6,
    SNMP_COMMUNITYSTRING_NOT_MATCHED = 7,
};

enum SNMP_TYPE_CODE
{
    SNMP_TYPECODE_NOTSET = 0x00,
    SNMP_TYPECODE_GETREQ = 0xA0,
    SNMP_TYPECODE_GETNEXTREQ = 0xA1,
    SNMP_TYPECODE_GETRESPONSE = 0xA2,
    SNMP_TYPECODE_GSETREQ = 0xA3,
    SNMP_TYPECODE_GETBULKREQ = 0xA5,
    SNMP_TYPECODE_INFORMREQ = 0xA6,
    SNMP_TYPECODE_TRAPV2 = 0xA7,
};

enum SNMP_DATA_TYPE
{
    SNMP_DATATYPE_NOTSET = 0x00,
    SNMP_DATATYPE_BOOLEAN = 0x01,
    SNMP_DATATYPE_INTEGER = 0x02,
    SNMP_DATATYPE_BITSTRING = 0x03,
    SNMP_DATATYPE_OCTETSTRING = 0x04,
    SNMP_DATATYPE_NULL = 0x05,
    SNMP_DATATYPE_OID = 0x06,
    SNMP_DATATYPE_UTCTIME = 0x23,
    SNMP_DATATYPE_VARBIND = 0x30,
    SNMP_DATATYPE_IPADDRESS = 0x40,
    SNMP_DATATYPE_COUNTER32 = 0x41,
    SNMP_DATATYPE_UNSIGNED = 0x42,
    SNMP_DATATYPE_TIMETICKS = 0x43, // 1/100th of a second
    SNMP_DATATYPE_INT64 = 0x44,
    SNMP_DATATYPE_FLOAT = 0x78,
    SNMP_DATATYPE_DOUBLE = 0x79,
    SNMP_DATATYPE_SIGNED64 = 0x7A,
    SNMP_DATATYPE_UNSIGNED64 = 0x7B,
};

enum SNMP_ERROR_CODE // https://www.ibm.com/docs/en/zos/2.2.0?topic=snmp-major-minor-error-codes-value-types
{
    SNMP_NOERROR = 0,
    SNMP_NOSUCHNAME = 2,
    SNMP_READONLY = 4,
};

typedef byte SNMP_NULL; // Used by send to flag a null data type feild (0x05,0x00)
typedef byte ASNTYPE;

// struct holding pointers to each field within the received data frame.  The actual data stays in the rxbuffer
struct pdudata
{
    byte *rxdata;       // pointer to the received data buffer
    byte *versionasn1;  // points to the version number field
    byte *comstrasn1;   // points to the community string field
    byte *reqidasn1;    // points to the request id field
    byte *errorasn1;    // poijnts to the rrror field
    byte *erroridxasn1; // points to the error index field
    byte *oidasn1;      // points to the oid field
    byte *nextoidasn1;  // optional field, points to the next oid on a getnetxreq
    byte *setvalueasn1; // optional field, points to the user data field on a setreq, used by the set function
    byte *getvalueasn1; // optional field, points to the user data field to be returned on a getreq or getnextreq, set by the get function
    byte requesttype;   // Set to the pdu type, getreq, getnextreq of setreq
    byte version;       // Set to the version number
};

// class snmpNode is used to store the instance data and the pointer to the next node, a new instance is created for every oid to be supported
class snmpNode
{
public:
    const char *oid;           // Pointer to command text to match, data is decoded into char *
    void (*ROcommandAction)(); // pointer to function to read the value
    void (*RWcommandAction)(); // pointer to function to set the value
    snmpNode *next;            // Pointer to next instance

    snmpNode(const char *oidtext, void (*action)()); // Default constructor
};

// class mysnmp is the main worker class
class SimpleSNMP
{
public:
    SimpleSNMP(void);  // Default constructor
    ~SimpleSNMP(void); // Default destructor

    // Request functions
    void action(void);                                       // Called regularly from main() to process the snmp subsystem
    void setROcommunity(const char *name);                   // Set the RO community name
    void setRWcommunity(const char *name);                   // Set the RW community name
    void insertNode(const char *oidtext, void (*action)());  // Function to insert a node at the end of the linked list
    bool addRWaction(const char *oidfind, void (*action)()); // Function to add a RW action to a node

    // Reply functions
    void sendResponse(long long value, SNMP_DATA_TYPE type);              // Sends an int, length is the length of the value
    void sendResponse(long long value, SNMP_DATA_TYPE type, byte length); // Sends an int, length is the length of the value
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
    void sendResponse(ASNTYPE *value);                                    // Sends an encoded object buffer, used to send an oid or a null field, used gpbuff
    void sendResponse(IPAddress value);                                   // Sends an encoded object buffer, used to send an oid or a null field, used gpbuff
    byte getUserData(byte *asn, byte *buff, byte len);                    // Copies the user data to buff, returns the length of the source data field

    unsigned long snmpPacketsSent = 0; // Count of packets sent to snmp
    unsigned long snmpPacketsRecv = 0; // Count of packets received from snmp
    struct pdudata workingpdu;         // Exposes the current request data for use by oid support functions

private:
    // Request functions
    SNMP_PARSE_STAT_CODES parsepdu(byte *oid, uint16_t rxlen); // Processes the received frame
    long decodeInt(byte *msg);                                 // Reads an encoded integer and returns the value
    unsigned long decodeUnsignedInt(byte *msg);                // Reads an encoded integer and returns the value
    long long decodeInt64(byte *msg);                          // Reads an encoded 64 bit integer and returns the value
    unsigned long long decodeUnsignedInt64(byte *msg);         // Reads an encoded 64 bit integer and returns the value
    float decodeFloat(byte *msg);                              // Reads an encoded float (4 bytes) and returns the value
    double decodeDouble(byte *msg);                            // Reads an encoded double (8 bytes) and returns the value
    long decodeOidInt(byte *msg);                              // Reads an OID encoded integer and returns the contents
    bool oid2char(void);                                       // Converts an oid data buffer to a char string, puts result into gpbuff, converts the working comstr field
    bool oid2char(byte *pdu);                                  // Converts an oid data buffer to a char string, puts result into gpbuff
    bool getoid(byte *pdu);                                    // extracts the oid from a udp frame and siores it into the oid buffer
    bool getcomstr(byte *pdu);                                 // Stores the pointer to the ASN.1 community string
    char *decodeComStr(byte *asn);                             // Returns a pointer to the community string formatted as a null terminated string, uses gpbuff
    char *decodeComStr(void);                                  // Returns a pointer to the community string formatted as a null terminated string, uses gpbuff, converts the working comstr field
    bool getversion(byte *pdu);                                // extracts the snmp version from a udp frame and stores it into the version buffer
    bool getType(byte *pdu);                                   // Gets the SNMP record type
    bool checkROcomstr(void);                                  // Checks the last received record for a community string match
    bool checkRWcomstr(void);                                  // Checks the last received record for a community string match
    byte *findOid(byte *pdu);                                  // Searches a pdu for the oid data type record
    byte appendASN1(byte *dest, byte *src);                    // Appends an asn.1 object to the end of an existing asn.1 object
    const char *getErrorText(byte errno);                      // Returns a pointer to the error text
    byte getASNlen(byte *asn);                                 // Returns the length of the data element from an asn.1 buffer
    byte getASNhdrlen(byte *asn);                              // Returns the length of the asn header

    // Response functions
    void buildResponseBuffer(byte *responseBuffer, byte *valueBuffer); // Builds the snmp response frame at responsebuffer
    bool sendResponseBuffer(byte *responseBuffer);                     // Sends the snmp response frame at responsebuffer, returns true if ok
    byte addSnmpResponsePDU(byte *responseBuffer, byte *valueBuffer);  // addd response pdu
    byte addSnmpVBlist(byte *responseBuffer, byte *valueBuffer);       // addd response pdu
    byte addSnmpVBtype(byte *responseBuffer, byte *valueBuffer);       // addd response pdu
    // void sendResponse(long long value, int length);                    // Sends an int, length is the length of the value
    void sendErrorResponse(SNMP_ERROR_CODE errorno); // Sends an error response

    // List processing functions
    byte nextoid[MAX_OID_SIZE];                           // buffer holding the asn.1 formatted oid for the next oid, used by getnextreq
    bool processGetRequest(const char *oidfind);          // Function to process the linked list. command parameter is the oid to be found
    bool processGetNextRequest(const char *oidfind);      // Function to process the linked list. command parameter is the oid to be found, supports partial match
    bool processSetRequest(const char *oidfind);          // Function to process the linked list. command parameter is the oid to be found
    bool compareStr_P(const char *o1, const char *o2);    // Compares two strings PROGMEM safe
    bool comparepStr_P(const char *o1, const char *o2);   // Compares two strings PROGMEM safe, returns true if o1 matches the start of o2
    byte *char2oid(const char *oidtext);                  // Converts a char oid string to an encoded asn.1 field, puts result into respoid buffer
    long atol_P(const char *s);                           // Converts a string to long, PROOGMEM safe
    const char *findTok_P(const char *s, char delimiter); // finds delim, returns a pointer to the next char after delim, PROGMEM safe

    // Debug functions
    void dumpStruct(void);                            // Dumps the workingpdu structure contents
    void dumpRaw(byte *oid);                          // Dumps an asn.1 object as a hex string for cutting and pasting into a decoder
    void dumpData(byte *oid);                         // Lists the encoded data fields
    void dumpData(byte *oid, byte oidlen, int level); // Lists the encoded data fields
    void dumpField(byte *msg);                        // Breaks down an asn.1 object into it's omponent parts
    bool dumpList(void);                              // Dumps the contents of the linked list
    bool dumpList(snmpNode *element, byte level);     // Dumps the contents of the linked list

    // variables
    uint16_t port;                     // Stores the SNMP port, usually 161
    snmpNode *head;                    // pointer to first element of the linked list
    char ROcommunity[MAX_COMSTR_SIZE]; // RO community
    char RWcommunity[MAX_COMSTR_SIZE]; // RW community
};
