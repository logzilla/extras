 // EventLogMsgs.mc  
 // ********************************************************  
 // Use the following commands to build this file:  
 //   mc -s EventLogMsgs.mc  
 //   rc EventLogMsgs.rc  
 //   link /DLL /SUBSYSTEM:WINDOWS /NOENTRY /MACHINE:x86 EventLogMsgs.Res   
 // ********************************************************  
 // - Event categories -  
 // Categories must be numbered consecutively starting at 1.
 // ********************************************************  
//
//  Values are 32 bit values laid out as follows:
//
//   3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//  +---+-+-+-----------------------+-------------------------------+
//  |Sev|C|R|     Facility          |               Code            |
//  +---+-+-+-----------------------+-------------------------------+
//
//  where
//
//      Sev - is the severity code
//
//          00 - Success
//          01 - Informational
//          10 - Warning
//          11 - Error
//
//      C - is the Customer code flag
//
//      R - is a reserved bit
//
//      Facility - is the facility code
//
//      Code - is the facility's status code
//
//
// Define the facility codes
//


//
// Define the severity codes
//


//
// MessageId: INSTALL_CATEGORY
//
// MessageText:
//
// Installation  
//
#define INSTALL_CATEGORY                 0x00000001L

//
// MessageId: QUERY_CATEGORY
//
// MessageText:
//
// Database Query  
//
#define QUERY_CATEGORY                   0x00000002L

//
// MessageId: REFRESH_CATEGORY
//
// MessageText:
//
// Data Refresh  
//
#define REFRESH_CATEGORY                 0x00000003L

 // - Event messages -  
 // *********************************  
//
// MessageId: AUDIT_SUCCESS_MESSAGE_ID_1000
//
// MessageText:
//
// My application message text, in English, for message id 1000, called from %1.
//
#define AUDIT_SUCCESS_MESSAGE_ID_1000    0x000003E8L

//
// MessageId: AUDIT_FAILED_MESSAGE_ID_1001
//
// MessageText:
//
// My application message text, in English, for message id 1001, called from %1.
//
#define AUDIT_FAILED_MESSAGE_ID_1001     0x800003E9L

//
// MessageId: GENERIC_INFO_MESSAGE_ID_1002
//
// MessageText:
//
// My generic information message in English, for message id 1002.
//
#define GENERIC_INFO_MESSAGE_ID_1002     0x000003EAL

//
// MessageId: GENERIC_WARNING_MESSAGE_ID_1003
//
// MessageText:
//
// My generic warning message in English, for message id 1003, called from %1.
//
#define GENERIC_WARNING_MESSAGE_ID_1003  0x800003EBL

//
// MessageId: UPDATE_CYCLE_COMPLETE_MESSAGE_ID_1004
//
// MessageText:
//
// The update cycle is complete for %%5002.
//
#define UPDATE_CYCLE_COMPLETE_MESSAGE_ID_1004 0x000003ECL

//
// MessageId: SERVER_CONNECTION_DOWN_MESSAGE_ID_1005
//
// MessageText:
//
// The refresh operation did not complete because the connection to server %1 could not be established.
//
#define SERVER_CONNECTION_DOWN_MESSAGE_ID_1005 0x800003EDL

 // - Event log display name -  
 // ********************************************************  
//
// MessageId: EVENT_LOG_DISPLAY_NAME_MSGID
//
// MessageText:
//
// Sample Event Log  
//
#define EVENT_LOG_DISPLAY_NAME_MSGID     0x00001389L

 // - Event message parameters -  
 //   Language independent insertion strings  
 // ********************************************************  
//
// MessageId: EVENT_LOG_SERVICE_NAME_MSGID
//
// MessageText:
//
// SVC_UPDATE.EXE  
//
#define EVENT_LOG_SERVICE_NAME_MSGID     0x0000138AL

