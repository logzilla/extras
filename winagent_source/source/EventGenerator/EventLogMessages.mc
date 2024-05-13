; // EventLogMsgs.mc  
; // ********************************************************  

; // Use the following commands to build this file:  

; //   mc -s EventLogMsgs.mc  
; //   rc EventLogMsgs.rc  
; //   link /DLL /SUBSYSTEM:WINDOWS /NOENTRY /MACHINE:x86 EventLogMsgs.Res   
; // ********************************************************  

LanguageNames =
    (
        English = 0x0409:Messages_ENU
        Russian = 0x0019:Messages_RUS
    )



; // - Event categories -  
; // Categories must be numbered consecutively starting at 1.
; // ********************************************************  

MessageId=0x1  
Severity=Success  
SymbolicName=INSTALL_CATEGORY  
Language=English  
Installation  
.
Language=Russian
установить
.

MessageId=0x2  
Severity=Success  
SymbolicName=QUERY_CATEGORY  
Language=English  
Database Query  
.
Language=Russian
запрос
.

MessageId=0x3  
Severity=Success  
SymbolicName=REFRESH_CATEGORY  
Language=English  
Data Refresh  
.
Language=Russian
обновить
.

; // - Event messages -  
; // *********************************  

MessageId = 1000  
Severity = Success  
Facility = Application  
SymbolicName = AUDIT_SUCCESS_MESSAGE_ID_1000  
Language=English  
My application message text, in English, for message id 1000, called from %1.
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.

MessageId = 1001  
Severity = Warning  
Facility = Application  
SymbolicName = AUDIT_FAILED_MESSAGE_ID_1001  
Language=English  
My application message text, in English, for message id 1001, called from %1.
.
Language=Russian
Сейчас мы вовлечены в великую гражданскую войну, проверяя, сможет ли этот 
народ или любой другой народ, задуманный и преданный своему делу, долго 
существовать. Нас встречают на великом поле битвы той войны. Мы пришли, 
чтобы освятить часть этого поля, как место последнего упокоения тех, 
кто отдал здесь свои жизни, чтобы этот народ мог жить. Вполне уместно 
и правильно, что мы должны это сделать.
.

MessageId = 1002  
Severity = Success  
Facility = Application  
SymbolicName = GENERIC_INFO_MESSAGE_ID_1002  
Language=English  
My generic information message in English, for message id 1002.
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.

MessageId = 1003  
Severity = Warning  
Facility = Application  
SymbolicName = GENERIC_WARNING_MESSAGE_ID_1003  
Language=English  
My generic warning message in English, for message id 1003, called from %1.
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.

MessageId = 1004  
Severity = Success  
Facility = Application  
SymbolicName = UPDATE_CYCLE_COMPLETE_MESSAGE_ID_1004  
Language=English  
The update cycle is complete for %%5002.
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.

MessageId = 1005  
Severity = Warning  
Facility = Application  
SymbolicName = SERVER_CONNECTION_DOWN_MESSAGE_ID_1005  
Language=English  
The refresh operation did not complete because the connection to server %1 could not be established.
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.

; // - Event log display name -  
; // ********************************************************  

MessageId = 5001  
Severity = Success  
Facility = Application  
SymbolicName = EVENT_LOG_DISPLAY_NAME_MSGID  
Language=English  
Sample Event Log  
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.

; // - Event message parameters -  
; //   Language independent insertion strings  
; // ********************************************************  

MessageId = 5002  
Severity = Success  
Facility = Application  
SymbolicName = EVENT_LOG_SERVICE_NAME_MSGID  
Language=English  
SVC_UPDATE.EXE  
.
Language=Russian
Четыре десятка семь лет назад наши отцы произвели на этом континенте 
новую нацию, зачатую в Свободе и преданную идее, что все люди созданы равными.
.
