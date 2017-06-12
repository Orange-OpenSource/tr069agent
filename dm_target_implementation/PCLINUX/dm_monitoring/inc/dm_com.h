/*---------------------------------------------------------------------------
 * Project     : TR069 Generic Agent
 *
 * Copyright (C) 2014 Orange
 *
 * This software is distributed under the terms and conditions of the 'Apache-2.0'
 * license which can be found in the file 'LICENSE.txt' in this package distribution
 * or at 'http://www.apache.org/licenses/LICENSE-2.0'.
 *
 *---------------------------------------------------------------------------
 * File        : dm_com.h
 *
 * Created     : 22/05/2008
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

/**
 * @file dm_com.h
 *
 * @brief main header of the communication module
 *
 **/

#ifndef _DM_COM_H_
#define _DM_COM_H_

#include <stdio.h>     	  /* standard I/O functions                         */
#include <unistd.h>    	  /* standard unix functions, like getpid()         */
#include <signal.h>   	  /* signal name macros, and the signal() prototype */
#include <stdlib.h>    	  /* standard system library	    */
#include <time.h>      	  /* usual and useful time functions    */
#include <sys/types.h> 	  /* various type definitions, like pid_t           */
#include <sys/stat.h>	    /* */

#ifndef WIN32
#include <sys/socket.h>	  /* */
#else
#include <winsock2.h>
#endif

#include <errno.h>	      /* Usefull for system errors	    */
#include "DM_ENG_Common.h"
#include "DM_CMN_Thread.h"

#include "dm_com_utils.h"

#include "DM_COM_GenericDomXmlParserInterface.h"
#include "DM_COM_GenericHttpServerInterface.h"
#include "DM_COM_GenericHttpClientInterface.h"

#include "CMN_Type_Def.h" 

// DM_ENGINE's header
#include "DM_ENG_RPCInterface.h"  /* high RPC interface definition    */

// The boolean below is used to retrieve the cwmp-1-X (CWMP Namespace Identifier) default flag used in 
// the Soap message (X = 0 or 1). If the cwmp-1-1 flag is the default value, the CPE uses this flag and check if the
// ACS supports it (controling ACS response). If the ACS does not support the version, the CPE will decide to use the cwmp-1-0.
extern const bool DM_COM_IS_FORCED_CWMP_1_0_USAGE;

// Size of the CPE URL
extern const int CPE_URL_SIZE;
// CPE PORT VALUE - Port assigned by IANA for CWMP : 7547 (current used 50805)
extern const int CPE_PORT;
 

// -------------------------------------------------------------
// Main definition
// -------------------------------------------------------------
#define  VARRUNPATH  "/var/run/"
#define  PIDFILE	  "cwmpd.pid"
#define  ETCDOTCONFPATH  "/etc/adm"
#define  DOTCONF	  "adm.conf"

// -------------------------------------------------------------
// New constantes, Enum and structure definitions
// -------------------------------------------------------------
#ifndef WIN32
#define  INFINITE            (1)
#endif

#define  TYPE_CPE            (0)
#define  TYPE_ACS            (1)
#define  NB_REDIRECT         (5)
#define  EMPTY_HTTP_MESSAGE	 ""
#define  UNKNOWMMETHOD       (-1)
#define  GETPARAMETERVALUE 	 (1)
#define  SETPARAMETERVALUE 	 (2)
#define  MINIMUM_TIMEOUT     (30)
#define  MAXENVELOPPE        (1)
#define  EVENTCODES_MAXVALUE (10)
#define	 HTTP_SIZEVARIABLE	 (257)
#define  RPC_CPE_RETURN_CODE_OK  (0)


// -------------------------------------------------------------
// Information about security
// The certificate must be in the same path than the agent
// NOTE : There is two kind of certificates: PEM, DER or ENG
// -------------------------------------------------------------
#define  ACS_SSLCERTTYPE    "PEM"
#define  SIZEURL	          (255)
#define  SIZECERT           (255)
#define  MAX_NAME_PWD_SIZE  (255)
typedef enum { NONE=0, BASIC, DIGEST, SSL } AUTH;
#define CRYPTO_NONE          "none"
#define  CRYPTO_SSL          "ssl"
#define  CRYPTO_HTTP_BASIC	 "basic"
#define  CRYPTO_HTTP_DIGEST	 "digest"

// -------------------------------------------------------------
// Error messages (only in english language - no international support)
// -------------------------------------------------------------
#define  ERROR_INVALID_PARAMETERS	"Invalid parameters!!"

// -------------------------------------------------------------
// SOAP tags used for the parsing and the creation of the SOAP messages
// -------------------------------------------------------------
// #define  SOAPSKELETON  "<soapenv:Envelope xmlns:soapenc=\"http://schemas.xmlsoap.org/soap/encoding/\" xmlns:soapenv=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:cwmp=\"urn:dslforum-org:cwmp-1-0\"><soapenv:Header></soapenv:Header><soapenv:Body></soapenv:Body></soapenv:Envelope>"


extern char* DM_COM_SoapEnv_NS;
extern char* DM_COM_ArrayTypeAttrName;

#define DM_COM_DEFAULT_SOAPENV_NS "soapenv"
#define DM_COM_DEFAULT_SOAPENC_NS "soapenc"

// Tag used to build the Skeleton
#define xmlns_soapenv_attribut_value           "http://schemas.xmlsoap.org/soap/envelope/"
#define xmlns_soapenc_attribut                 "xmlns:" DM_COM_DEFAULT_SOAPENC_NS
#define xmlns_soapenc_attribut_value           "http://schemas.xmlsoap.org/soap/encoding/"
#define xmlns_cwmp_attribut                    "xmlns:cwmp"
#define xmlns_xsi_attribut                     "xmlns:xsi"
#define xmlns_xsi_attribut_value               "http://www.w3.org/2001/XMLSchema-instance"
#define xmlns_xsd_attribut                     "xmlns:xsd"
#define xmlns_xsd_attribut_value               "http://www.w3.org/2001/XMLSchema"
#define xsi_type_attribut                      "xsi:type"
#define xsi_type_attribut_value                "xsd:string"
#define xmlns_cwmp_attribut                    "xmlns:cwmp"
#define xmlns_cwmp_1_0_attribut_value          "urn:dslforum-org:cwmp-1-0"
#define xmlns_cwmp_1_1_attribut_value          "urn:dslforum-org:cwmp-1-1"


#define UNDEFINED_UTC_DATETIME   "0001-01-01T00:00:00Z"
#define LIMIT_SIZE_SOAP_MESSAGE  (32768)
#define NO_CONTENT               ""
#define DM_COM_ENV_TAG           "Envelope"
#define DM_COM_HEADER_TAG        "Header"
#define DM_COM_BODY_TAG          "Body"
#define HEADER_ID                "cwmp:ID"
#define HEADER_HOLDREQUEST       "cwmp:HoldRequests"
#define DM_COM_MUST_UNDERSTAND_ATTR "mustUnderstand"
#define HEADER_ATTR_VAL          "1"
#define NAMESPACE_ATTR           "xmlns:cwmp"

// -------------------------------------------------------------
// Soap FAULT message
// -------------------------------------------------------------
#define  FAULT_MESSAGE        "Fault"
#define  DM_COM_FAULT_TAG     "Fault"
#define  FAULT_CODE           "faultcode"
#define  FAULT_CODE_CONTENT   "Client"
#define  FAULT_STRING         "faultstring"
#define  FAULT_STRING_CONTENT "CWMP fault"
#define  FAULT                "cwmp:Fault"
#define  FAULT_ATTR           "xmlns:cwmp"
#define  FAULTCODE2           "FaultCode"
#define  FAULTSTRING2         "FaultString"

// -------------------------------------------------------------
// Soap tags from the ACS server
// -------------------------------------------------------------
#define  FAULT_ACS         "cwmp:Fault"
#define  FAULT_DETAIL      "detail"
#define  FAULT_CODE_ACS    "FaultCode"
#define  FAULT_STRING_ACS  "FaultString"
#define  JSP_EXCEPTION_ACS "<b>exception</b>"

// -------------------------------------------------------------
// RPC commands (cwmp:<RPCMETHOD>)
// -------------------------------------------------------------
#define  RPC_ADDOBJECT               "cwmp:AddObject"
#define  RPC_DELETEOBJECT            "cwmp:DeleteObject"
#define  RPC_DOWNLOAD                "cwmp:Download"
#define  RPC_FACTORYRESET            "cwmp:FactoryReset"
#define  RPC_GETPARAMETERATTRIBUTES  "cwmp:GetParameterAttributes"
#define  RPC_GETPARAMETERNAMES       "cwmp:GetParameterNames"
#define  RPC_GETPARAMETERVALUES      "cwmp:GetParameterValues"
#define  RPC_GETRPCMETHODS           "cwmp:GetRPCMethods"
#define  RPC_REBOOT                  "cwmp:Reboot"
#define  RPC_SCHEDULEINFORM          "cwmp:ScheduleInform"
#define  RPC_SETPARAMETERATTRIBUTES  "cwmp:SetParameterAttributes"
#define  RPC_SETPARAMETERVALUES      "cwmp:SetParameterValues"
#define  RPC_UPLOAD                  "cwmp:Upload"
#define  RPC_GETQUEUEDTRANSFERS      "cwmp:GetQueuedTransfers"
#define  RPC_GETALLQUEUEDTRANSFERS   "cwmp:GetAllQueuedTransfers"
#define  RPC_SETVOUCHERS             "cwmp:SetVouchers"
#define  RPC_GETOPTIONS              "cwmp:GetOptions"

// -------------------------------------------------------------
// Data types
// -------------------------------------------------------------
#define  ATTR_TYPE         "xsi:type"
#define  XSD_INT           "xsd:int"
#define  XSD_UNSIGNEDINT   "xsd:unsignedInt"
#define  XSD_LONG          "xsd:long"
#define  XSD_BOOLEAN       "xsd:boolean"
#define  XSD_DATETIME      "xsd:dateTime"
#define  XSD_STRING        "xsd:string"
#define  XSD_ANY           "xsd:any"

// -------------------------------------------------------------
// Parameters for the RPC commands
// -------------------------------------------------------------
#define  PARAM_COMMANDKEY          "CommandKey"
#define  CWMP_PARAM_COMMANDKEY     "cwmp:CommandKey"
#define  PARAM_DELAY               "Delay"
#define  PARAM_DELAYSECONDS        "DelaySeconds"
#define  CWMP_PARAM_DELAYSECONDS   "cwmp:DelaySeconds"
#define  PARAM_FAILUREURL          "FailureURL"
#define  CWMP_PARAM_FAILUREURL     "cwmp:FailureURL"
#define  PARAM_FILESIZE            "FileSize"
#define  CWMP_PARAM_FILESIZE       "cwmp:FileSize"
#define  PARAM_FILETYPE            "FileType"
#define  CWMP_PARAM_FILETYPE       "cwmp:FileType"
#define  PARAM_NEXTLEVEL           "NextLevel"
#define  PARAM_OBJECTNAME          "ObjectName"
#define  PARAM_NAME                "Name"
#define  PARAM_NOTIFICATION        "Notification"
#define  PARAM_NOTIFICATION_CHANGE "NotificationChange"
#define  PARAM_ACCESSLIST          "AccessList"
#define  PARAM_TRANSFER_LIST       "TransferList"
#define  PARAM_ACCESSLIST_CHANGE   "AccessListChange"
#define  PARAM_PARAMETERKEY        "ParameterKey"
#define  PARAM_PARAMETERLIST       "ParameterList"
#define  CWMP_PARAM_PARAMETERLIST  "cwmp:ParameterList"
#define  PARAM_PARAMETERNAMES      "ParameterNames"
#define  CWMP_PARAM_PARAMETERNAMES "cwmp:ParameterNames"
#define  PARAM_PARAMETERNAME       "ParameterName"
#define  PARAM_PARAMETERPATH       "ParameterPath"
#define  PARAM_PASSWORD            "Password"
#define  CWMP_PARAM_PASSWORD       "cwmp:Password"
#define  PARAM_SUCCESSURL          "SuccessURL"
#define  CWMP_PARAM_SUCCESSURL     "cwmp:SuccessURL"
#define  PARAM_TARGETFILENAME      "TargetFileName"
#define  CWMP_PARAM_TARGETFILENAME "cwmp:TargetFileName"
#define  PARAM_URL                 "URL"
#define  CWMP_PARAM_URL            "cwmp:URL"
#define  PARAM_USERNAME            "Username"
#define  CWMP_PARAM_USERNAME       "cwmp:Username"
#define  PARAM_VALUE               "Value"
#define  PARAM_STRING              "string"

#define  STRING_LIST_ATTR_VAL1     "string[0%d]"
#define  STRING_LIST_ATTR_VAL2     "string[%d]"

// -------------------------------------------------------------
// Inform messages
// -------------------------------------------------------------
#define  INFORM                 "cwmp:Inform"
#define  INFORM_DEVICEID        "DeviceId"
#define  INFORM_MANUFACTURER    "Manufacturer"
#define  INFORM_OUI             "OUI"
#define  INFORM_PRODUCTCLASS    "ProductClass"
#define  INFORM_SERIALNUMBER    "SerialNumber"
#define  INFORM_EVENT           "Event"
#define  DM_COM_ARRAY_TYPE_ATTR "arrayType"
#define  INFORM_EVENT_ATTR_VAL1 "cwmp:EventStruct[0%d]"
#define  INFORM_EVENT_ATTR_VAL2 "cwmp:EventStruct[%d]"
#define  INFORM_EVENTSTRUCT     "EventStruct"
#define  INFORM_EVENT_CODE      "EventCode"
#define  INFORM_COMMANDKEY      "CommandKey"
#define  INFORM_MAXENVELOPES   "MaxEnvelopes"
#define  INFORM_CURRENTTIME     "CurrentTime"
#define  INFORM_RETRYCOUNT      "RetryCount"
#define  INFORM_PARAMETERLIST   "ParameterList"
#define  INFORM_PARAMETERLIST_ATTR_VAL1  "cwmp:ParameterValueStruct[0%d]"
#define  INFORM_PARAMETERLIST_ATTR_VAL2  "cwmp:ParameterValueStruct[%d]"
#define  INFORM_PARAMETERLIST_ATTRIBUTE_VAL1  "cwmp:ParameterAttributeStruct[0%d]"
#define  INFORM_PARAMETERLIST_ATTRIBUTE_VAL2  "cwmp:ParameterAttributeStruct[%d]"
#define  INFORM_PARAMETERVALUESTRUCT     "ParameterValueStruct"
#define  INFORM_NAME        "Name"
#define  INFORM_VALUE       "Value"
#define  INFORM_VALUE_ATTR  "xsi:type"

// -------------------------------------------------------------
// Inform Response
// -------------------------------------------------------------
#define  INFORMRESPONSE_MAXENVELOPES	"MaxEnvelopes"

// -------------------------------------------------------------
// Kicked Response
// -------------------------------------------------------------
#define  KICKEDRESPONSE_NEXTURL  "NextURL"

// -------------------------------------------------------------
// GetRPCmethods
// -------------------------------------------------------------
#define  GERRPCMETHODSCOMMAND      "Command"
#define  GETRPCMETHOSSREFERER      "Referer"
#define  GETRPCMETHODSARG          "Arg"
#define  GETRPCMETHODSNEXT         "Next"
#define  GETRPCMETHODSMETHODLIST   "MethodList"

// -------------------------------------------------------------
// DownloadResponse
// -------------------------------------------------------------
#define  DOWNLOADRESPONSE_STATUS       "Status"
#define  DOWNLOADRESPONSE_STARTTIME    "StartTime"
#define  DOWNLOADRESPONSE_COMPLETETIME	"CompleteTime"

// -------------------------------------------------------------
// DownloadResponse
// -------------------------------------------------------------
#define  UPLOADRESPONSE_STATUS         DOWNLOADRESPONSE_STATUS
#define  UPLOADRESPONSE_STARTTIME      DOWNLOADRESPONSE_STARTTIME
#define  UPLOADRESPONSE_COMPLETETIME   DOWNLOADRESPONSE_COMPLETETIME

// -------------------------------------------------------------
// DeleteObjectResponse
// -------------------------------------------------------------
#define  DELETEOBJECT_STATUS  "Status"

// -------------------------------------------------------------
// AddObjectResponse
// -------------------------------------------------------------
#define  ADDOBJECT_STATUS          "Status"
#define  ADDOBJECT_INSTANCENUMBER  "InstanceNumber"

#define   SETPARAMETERVALUERESPONSESTATUS "Status"

// -------------------------------------------------------------
// GetParameterName
// -------------------------------------------------------------

// -------------------------------------------------------------
// GetParameterValues
// -------------------------------------------------------------
#define  GETPARAMETERVALUESNBPARAM	(50)

// -------------------------------------------------------------
// ParameterValueStruct
// -------------------------------------------------------------
#define  PARAMETERVALUESTRUCT  "ParameterValueStruct"

// -------------------------------------------------------------
// GETALLQUEUEDTRANSFERSTRUCT
// -------------------------------------------------------------
#define GETALLQUEUEDTRANSFERSTRUCT "AllQueuedTransferStruct"
#define GETALLQUEUEDTRANSFER_COMMANDKEY     "CommandKey"
#define GETALLQUEUEDTRANSFER_STATE          "State"
#define GETALLQUEUEDTRANSFER_ISDOWNLOAD     "IsDownload"
#define GETALLQUEUEDTRANSFER_FILETYPE       "FileType"
#define GETALLQUEUEDTRANSFER_FILESIZE       "FileSize"
#define GETALLQUEUEDTRANSFER_TARGETFILENAME "TargetFileName"


// -------------------------------------------------------------
// SetParameterValueFault
// -------------------------------------------------------------
#define  SETPARAMETERVALUEFAULT "SetParameterValuesFault"
#define  PARAMETERNAME          "Name"
#define  FAULTCODE              "FaultCode"
#define  FAULTSTRING            "FaultString"

// -------------------------------------------------------------
// SetParameterAttributesStruct
// -------------------------------------------------------------
#define SETPARAMETERATTRIBUTESSTRUCT     "SetParameterAttributesStruct"
#define PARAMETERATTIBUTESSTRUCT         "ParameterAttibutesStruct"

// -------------------------------------------------------------
// Transfert Complete data structure
// -------------------------------------------------------------
#define  TRANSFERTCOMPLETE             "cwmp:TransferComplete"
#define  TRANSFERTCOMPLETECOMMANDKEY	 "CommandKey"
#define  TRANSFERTCOMPLETEFAULTSTRUCT  "FaultStruct"
#define  TRANSFERTCOMPLETEFAULTCODE    "FaultCode"
#define  TRANSFERTCOMPLETEFAULTSTRING  "FaultString"
#define  TRANSFERTCOMPLETESTARTTIME    "StartTime"
#define  TRANSFERTCOMPLETECOMPLETETIME "CompleteTime"

// -------------------------------------------------------------
// RequestDownload data structure
// -------------------------------------------------------------
#define REQUESTDOWNLOAD               "cwmp:RequestDownload"
#define REQUESTDOWNLOAD_FILETYPE      "FileType"
#define REQUESTDOWNLOAD_FILETYPEARG   "FileTypeArg"
#define REQUESTDOWNLOAD_CWMP_ARG_STRUCT "cwmp:Argstruct[%d]"
#define REQUESTDOWNLOAD_ARGSTRUCT     "ArgStruct"

// -------------------------------------------------------------
// ParameterInfoStruct data structure
// -------------------------------------------------------------
#define  PARAMETERINFOSTRUCT_LIST       "ParameterList"
#define  PARAMETERINFOSTRUCT_ATTR_VAL1	"cwmp:ParameterInfoStruct[0%d]"
#define  PARAMETERINFOSTRUCT_ATTR_VAL2	"cwmp:ParameterInfoStruct[%d]"
#define  PARAMETERINFOSTRUCT            "ParameterInfoStruct"
#define  PARAMETERINFOSTRUCT_NAME       "Name"
#define  PARAMETERINFOSTRUCT_WRITABLE   "Writable"

// -------------------------------------------------------------
// ParameterAttributeStruct data structure
// -------------------------------------------------------------
#define  PARAMETERATTRIBUTESTRUCTLIST          "ParameterList"
#define  PARAMETERATTRIBUTESTRUCT_ATTR_VAL1    "cwmp:ParameterInfoStruct[0%d]"
#define  PARAMETERATTRIBUTESTRUCT_ATTR_VAL2    "cwmp:ParameterInfoStruct[%d]"
#define  PARAMETERATTRIBUTESTRUCT              "ParameterAttributeStruct"
#define  PARAMETERATTRIBUTESTRUCT_PARAMNAME    "Name"
#define  PARAMETERATTRIBUTESTRUCT_NOTIFICATION "NotificationChange"
#define  PARAMETERATTRIBUTESTRUCT_ACCESSLIST   "AccessListChange"

// -------------------------------------------------------------
// CPE's response tags
// -------------------------------------------------------------
#define  GETRPCMETHODSRESPONSE          "cwmp:GetRPCMethodsResponse"
#define  SETPARAMETERVALUESRESPONSE     "cwmp:SetParameterValuesResponse"
#define  GETPARAMETERVALUESRESPONSE     "cwmp:GetParameterValuesResponse"
#define  SETPARAMETERATTRIBUTESRESPONSE "cwmp:SetParameterAttributesResponse"
#define  GETPARAMETERATTRIBUTESRESPONSE "cwmp:GetParameterAttributesResponse"
#define  GETPARAMETERNAMERESPONSE       "cwmp:GetParameterNamesResponse"
#define  REBOOT_RESPONSE                "cwmp:RebootResponse"
#define  FACTORYRESET_RESPONSE          "cwmp:FactoryResetResponse"
#define  SCHEDULEINFORM_RESPONSE        "cwmp:ScheduleInformResponse"
#define  DOWNLOADRESPONSE               "cwmp:DownloadResponse"
#define  UPLOADRESPONSE                 "cwmp:UploadResponse"
#define  DELETEOBJECTRESPONSE           "cwmp:DeleteObjectResponse"
#define  ADDOBJECTRESPONSE              "cwmp:AddObjectResponse"
#define  GETALLQUEUEDTRANSFERTSRESPONSE "cwmp:GetAllQueuedTransfersResponse"

// -------------------------------------------------------------
// ACS's reponse tags
// -------------------------------------------------------------
#define  INFORMRESPONSE	            "cwmp:InformResponse"
#define  TRANSFERTCOMPLETERESPONSE  "cwmp:TransferCompleteResponse"
#define  REQUESTDOWNLOADRESPONSE    "cwmp:RequestDownloadResponse"
#define  KICKEDRESPONSE	            "cwmp:KickedResponse"

// -------------------------------------------------------------
// CMD : DM_ACS_GetRPCMethods
// -------------------------------------------------------------

// -------------------------------------------------------------
// CMD : DM_ACS_Inform
// -------------------------------------------------------------
#define  SIZE_MANUFACTURER    (65)
#define  SIZE_OUI	            (7)
#define  SIZE_PRODUCTCLASS    (65)
#define  SIZE_SERIALNUMBER    (65)
#define  SIZE_EVENTCODE	      (65)
#define  SIZE_PARAM_VAL_NAME  (257)
#define  MAX_ENVELOPPE_TR069  (1)
#define   MAX_NUMBER_OF_RPC_COMMAND_PER_BODY (1)

// -------------------------------------------------------------
// CMD : DM_ACS_TransferComplete
// -------------------------------------------------------------
#define 	SIZE_COMMANDKEY	        (32)
#define  SIZE_FAULTSTRUCT_STRING  (256)

// -------------------------------------------------------------
// CMD : DM_ACS_RequestDownload
// -------------------------------------------------------------
#define  SIZE_FILETYPE	   (64)
#define  SIZE_FILETYPEARG  (16)
#define  SIZE_ARG_NAME	   (64)
#define  SIZE_ARG_VALUE	   (256)

// -------------------------------------------------------------
// CMD : DM_ACS_Kicked
// -------------------------------------------------------------
#define  SIZE_COMMAND	  (32)
#define  SIZE_REFERER	  (64)
#define  SIZE_ARG	      (256)
#define  SIZE_NEXT	    (1024)
#define  SIZE_NEXT_URL	(1024)


// -------------------------------------------------------------
// CWMP ID Definition
// -------------------------------------------------------------
#define HEADER_ID_SIZE (50)

// -------------------------------------------------------------
// ACS's return code
// -------------------------------------------------------------
#define  ACS_ERROR_METHOD_NOT_SUPPORTED  (8000)
#define  ACS_ERROR_REQUEST_DENIED        (8001)
#define  ACS_ERROR_INTERNAL_ERROR        (8002)
#define  ACS_ERROR_INVALID_ARGUMENTS     (8003)
#define  ACS_ERROR_RESSOURCES_EXCEEDED   (8004)
#define  ACS_ERROR_RETRY_REQUEST	       (8005)

#ifndef DMRET
typedef         int	  DMRET;
#define  DM_OK	  0
#define  DM_ERR	  -1
#endif

// Max buffer size
#define TMPBUFFER_SIZE (150)


#ifndef unsignedInt
typedef unsigned int unsignedInt;
#endif

#ifndef FaultStruct
typedef struct _FaultStruct_
{
	unsignedInt  FaultCode;
	char	  FaultString[SIZE_FAULTSTRUCT_STRING];
} __attribute((packed)) FaultStruct;
#endif

/**
 * @brief  Definition of the network setting
 */
#ifndef DM_LANStruct
typedef struct _LAN_t
{
	char 	  *AddressingType;
	char 	  *IPAddress;
	char 	  *SubnetMask;
	char 	  *DefaultGateway;
	char 	  *DNSServers;
	char 	  *MACAddress;
	bool 	  *MACAddressOverride;
} __attribute((packed)) DM_LANStruct;
#endif

/**
 * @brief The server identifier
 */
#ifndef DM_ServerHttp
typedef struct
{
	int	  nExitHttpSvr;	  // Need to exit the main http server loop
	int         nPortNumber;	  // the server port
	DM_CMN_ThreadId_t  thread_server;	  // server thread identifier
	int 	    sockfd_serveur;	  // file descriptor of the server socket
} __attribute((packed)) DM_ServerHttp;
#endif

/**
 * @brief  HTTP variable data structure definition
 */
#ifndef DM_HttpVarStruct
typedef struct _httpvar
{
	/* TODO: use dynamic allocation */
	char	  data[HTTP_SIZEVARIABLE];
	char	  value[HTTP_SIZEVARIABLE];
} __attribute((packed)) DM_HttpVarStruct;
#endif

/**
 * @brief  HTTP frame definition
 */
#ifndef DM_HttpStruct
typedef struct _httpstruct
{
	char	  *method;
	char	  *version;
	char	  *path;
} __attribute((packed)) DM_HttpStruct;
#endif

#ifndef DM_SoapHeader
typedef struct _SoapHeader
{
	char	  *ID;
	bool	  noMoreRequests;
	bool	  holdRequests;
} __attribute((packed)) DM_SoapHeader;
#endif

#ifndef DM_ObjectList
typedef struct _ObjectList
{
	int	  ID;
	char	  *name;
	void	  *ptrObject;
	struct _ObjectList	*next;
} __attribute((packed)) DM_ObjectList;
#endif

#ifndef DownloadStruct
typedef struct _DownloadStruct
{
	char	  CommandKey[33];
	char	  FileType[65];
	char	  URL[257];
	char	  Username[257];
	char	  Password[257];
	unsigned int  FileSize;
	char	  TargetFileName[257];
	unsigned int  DelaySeconds;
	char	  SuccessURL[257];
	char	  FailureURL[257];
} __attribute((packed)) DownloadStruct;
#endif

#ifndef InformStruct
typedef struct _InformStruct
{
	DM_ENG_DeviceIdStruct  deviceIdStruct;
	DM_ENG_EventStruct  eventStruct[16];
	unsigned int  MaxEnvelopes;
	unsigned int  RetryCount;
	dateTime   CurrentTime;
	DM_ENG_ParameterValueStruct	ParameterList;
} __attribute((packed)) InformStruct;
#endif

#ifndef DM_ACS
typedef struct _acs_t
{
	bool 	  EmptyMessage;
	DM_SoapHeader   soapHeader;
	int 	  nMaxEnvelopes;
	char 	  **MethodList;
	InformStruct   informStruct;
} __attribute((packed)) DM_ACS;
#endif

/**
 * @brief  Definition of the callback model
 *
 */
typedef  size_t (*Callback)(void   *ptr,
                            size_t  size,
                            size_t  nmemb,
                            void   *stream);

/**
 * @brief  Data structure for Soap XML exchanges
 *
 */
#ifndef SoapXml
typedef struct _SoapXml
{
	GenericXmlDocumentPtr pParser;  // XML Document
	GenericXmlNodePtr     pRoot;    // Root Node of the XML document
	GenericXmlNodePtr     pHeader;  // Header Node of the XML document
	GenericXmlNodePtr     pBody;    // Body Node of the XML document

	char                  pSoapID[HEADER_ID_SIZE];
	unsigned int	        nHoldRequest;
} __attribute((packed)) DM_SoapXml;
#endif

/**
 * @brief  Main data structure of the DM_COM module
 *
 */
#ifndef dm_com_struct
typedef struct _dm_com_struct
{
	
	// Information about the http server
	DM_ServerHttp	ServerHttp;	
	
	// Network informations
	// DM_LANStruct	Network;
	
	// Information about the security
	// In order to generate the HTTP BASIC AUTH. login/pwd : echo -n 'dummy:secret' | openssl enc -a -e
	AUTH  nKindCryptoACS;	      // Kind of crypto CPE -> ACS
	char  * szCertificatStr;    // File containing the certificate (public device certificat)
	char  * szRsaKeyStr;        // File which contain the RSA Device Key
	char  * szCaAcsStr;         // File containing the ACS Certification Authority 

	// ACS information
	DM_ACS  Acs;

	// Indicator of the session
	bool   bSession;
	time_t bSessionOpeningTime;  // Time (in seconds) when the ACS Session has been created.
	bool   bIRTCInProgess;

	// STUN activation
	bool  bStunActivation; /* Stun Thread  */

} __attribute((packed)) dm_com_struct;
#endif

/************************************************************************************
 * Prototypes definitions
 */

/**
 * @brief Call be the HTTP Server to notify when the server is started
 *
 * @param none
 *
 * @return return None
 *
 */
void DM_HttpServerStarted();

/**
 * @brief Function called by the HTTP Client engine in order to get the http content
 *
 * @param httpDataMsgString  
 * @param msgSize  
 *
 * @return Size of the data received
 *
 */
size_t
DM_HttpCallbackClientData(char   * httpDataMsgString, size_t   msgSize);


/**
 * @brief Function called by the HTTP Client engine in order to get the http content
 *
 * @param http message  
 * @param http message size  
 * @param lastHttpResponseCode  (HTTP_OK 200 , HTTP_CREATED 201, ...)
 * @param stream	Pointer to the buffer which contain the HTTP header	
 *
 * @return Size of the data received
 *
 */
size_t
DM_HttpCallbackClientHeader(char   * httpHeaderMsgString,
                            size_t   msgSize,
			                      int      lastHttpResponseCode);






/**
 * @brief Start the DM_COM module
 *
 * @param none
 * 
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_COM_START();

/**
 * @brief Stop the DM_COM module
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_COM_STOP();

/**
 * @brief Initialize the DM_COM module
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_COM_INIT();

/**
 * @brief Configure the HTTP CLient 
 * Configure the URL ACS and SSL Options
 *
 * @param none
 *
 * @return return void
 *
 */
void
DM_COM_ConfigureHttpClient(char * acsUrl,
                           char * acsUsername,
                              char * acsPassword);

/**
 * @brief Indicate if the HTTP Server is started.
 *
 * @param none
 *
 * @return return The http server status (True if started, false otherwise)
 *
 */
bool _isHttpServerStarted();


/**
 * @brief Function which initialize the SOAP structure
 *
 * @param pSoapMsg soap structure to initialize
 *
 * @return return DM_OK is okay else DM_ERR
 *
 * @Remarks g_DmComData.SoapMsgReceived
 */
DMRET
DM_InitSoapMsgReceived(IN DM_SoapXml *pSoapMsg);

/**
 * @brief Function which analyse/prepare a SOAP before to be parse
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_AnalyseSoapMessage(IN       DM_SoapXml	* pSoapMsg,
                      IN const char       * pMessage,
                      IN       int          nTypeSoap,
                      IN bool               allEnveloppeAttributs);

/**
 * @brief Function which parse a SOAP enveloppe and look for each body tag
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_ParseSoapEnveloppe(GenericXmlNodePtr bodyNodePtr, char* soapIdStr, unsigned int holdRequests);

/**
 * @brief Function which parse a SOAP message and extract the RPC commands
 *
 * @param pBody current référence to the body tag
 *
 * @return return DM_OK is okay else DM_ERR
 *
 * @remarks it could have several body tag in a soap message
 *
 */
DMRET
DM_ParseSoapBodyMessage(IN GenericXmlNodePtr pBody, IN char* soapIdStr, IN unsigned int holdRequests);

/**
 * @brief Call the DM_ENG_GetRPCMethods
 *
 * @param pSoapId ID of the SOAP request	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetRPCMethods(IN const char *pSoapId);

/**
 * @brief Call the DM_ENG_AddObject function
 *
 * @param pSoapId	 ID of the SOAP request
 * @param pObjectName	 Name og the object to create
 * @param pParameterKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_AddObject(IN const char *pSoapId,
   IN const char *pObjectName,
   IN const char *pParameterKey);

/**
 * @brief Call the DM_ENG_DeleteObject function
 *
 * @param pSoapId	 ID of the SOAP request
 * @param pObjectName	 Name of the object to delete
 * @param pParameterKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_DeleteObject(IN const char *pSoapId,
      IN const char *pObjectName,
      IN const char *pParameterKey);

/**
 * @brief Call the DM_ENG_FactoryReset function
 *
 * @param pSoapId ID of the SOAP request
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_FactoryReset(IN const char *pSoapId);

/**
 * @brief Call the DM_ENG_Reboot function
 *
 * @param pSoapId	ID of the SOAP request
 * @param pCommandKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_Reboot(IN const char *pSoapId,
	      IN const char *pCommandKey);

/**
 * @brief Call the DM_ENG_ScheduleInform function
 *
 * @param pSoapId	ID of the SOAP request
 * @param nDelay	
 * @param pCommandKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_ScheduleInform(IN const char         *pSoapId,
        IN const char         *pDelay,
        IN const char         *pCommandKey);

/**
 * @brief Call the DM_ENG_Download function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pFileType
 * @param pUrl
 * @param pUsername
 * @param pPassword
 * @param pFileSize
 * @param pTargetFileName
 * @param pDelay
 * @param pSuccessURL
 * @param pFailureURL
 * @param pCommandKey
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_Download(IN const char         *pSoapId,
  IN const char         *pFileType,
  IN const char         *pUrl,
  IN const char         *pUsername,
  IN const char         *pPassword,
  IN       unsigned int  nFileSize,
  IN const char         *pTargetFileName,
  IN       unsigned int  nDelay,
  IN const char         *pSuccessURL,
  IN const char         *pFailureURL,
  IN const char         *pCommandKey);
  
/**
 * @brief Call the DM_ENG_Upload function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pFileType
 * @param pUrl
 * @param pUsername
 * @param pPassword
 * @param pDelay
 * @param pCommandKey
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_Upload(
  IN const char         *pSoapId,
  IN const char         *pFileType,
  IN const char         *pUrl,
  IN const char         *pUsername,
  IN const char         *pPassword,
  IN unsigned int        nDelay,
  IN const char         *pCommandKey);
  
/**
 * @brief Call the DM_ENG_GetAllQueuedTransfers function
 *
 * @param soapId
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET 
DM_SUB_GetAllQueuedTransferts(IN const char *pSoapId);

/**
 * @brief Call the DM_ENG_GetParameterNames function
 *
 * @param pSoapId ID of the SOAP request
 * @param pPath
 * @param nNextLevel	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetParameterNames(IN const char *pSoapId,
	   IN const char *pPath,
	   IN       bool  nNextLevel);

/**
 * @brief Call the SetParameterValues function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pParameterList
 * @param pParameterKey	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_SetParameterValues(IN const char                  *pSoapId,
	    IN       DM_ENG_ParameterValueStruct **pParameterList,
	    IN const char                  *pParameterKey);

/**
 * @brief Call the DM_ENG_GetParameterValues function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pParameterName	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetParameterValues(IN const char *pSoapId,
	    IN const char **pParameterName);

/**
 * @brief function which remake the xml buffer from a SCEW/EXPAT data structure
 *
 * @param pElement  Current element of the XML tree (
 *
 * @Return DM_OK is okay else DM_ERR
 *
 * @Remarks for a complete tree, pElement = pRoot
 */
DMRET
DM_RemakeXMLBuffer(/* IXML_Document * */ GenericXmlDocumentPtr pElement);

/**
 * @brief Find the type of a parameter
 *
 * @param pParameterType	Type of the parameter to find
 * @param nTypeFound  Number which define the type found
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_FindTypeParameter(IN  const char *pParameterType,
       OUT       int  *nTypeFound);

/**
 * @brief Create and send a SOAP response
 *
 * @param pSoapId	SOAP Identifier
 * @param nFaultCode	Return code of a RPC method	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SoapFaultResponse(IN const char *pSoapId,
	             IN       int   nFaultCode);

/**
 * @brief Create and send an empty response Message
 *
 * @param pSoapId	SOAP Identifier
 * @param nFaultCode	tag of the response message	
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SoapEmptyResponse(IN const char *pSoapId,
       IN const char *pResponseTag);

/**
 * @brief Function which initialize the temp. XML buffer
 *
 * @param none
 *
 * @Return DM_OK is okay else DM_ERR
 *
 */
DMRET
DM_FreeTMPXMLBufferCpe();

/**
 * @brief Function which update the retry buffer whith 
 *        the last emitted request.
 *
 * @param A Pointer on the message to store (string) and its size
 *
 * @Return DM_OK
 *
 */
DMRET
DM_UpdateRetryBuffer(const char * soapMsgToStoreStr, int size) ;

/**
 * @brief Call the DM_ENG_SetParameterAttributes
 *
 * @param pSoapId	ID of the SOAP request
 * @param pResult	List 
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_SetParameterAttributes(IN const char           *pSoapId,
                              IN       DM_ENG_ParameterAttributesStruct *pParameterList[]);

/**
 * @brief Call the DM_ENG_GetParameterAttributes function
 *
 * @param pSoapId  ID of the SOAP request
 * @param pParameterName	List of parameters
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_SUB_GetParameterAttributes(IN const char  *pSoapId,
	        IN const char **pParameterName);

/**
 * @brief Function which remove the SOAP Header_ID from the global array
 *
 * @param pIDToRemove	Header ID to remove from the global array
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_RemoveHeaderIDFromTab(IN const char *pIDToRemove);

/**
 * @brief function which generate a Header_ID and save it into the global array
 *
 * @param pID_generated	Pointer to the unique header ID
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_GenerateUniqueHeaderID(OUT char *pID_generated);

/**
 * @brief Function which add a Header_ID to a SOAP message
 *
 * @param pUniqueHeaderID Header ID to add to the soap XML message
 * @param pSoapMsg	  Pointer to the XML/SOAP object to update
 *
 * @Return DM_OK is okay else DM_ERR
 */
DMRET
DM_AddSoapHeaderID(IN const char       *pUniqueHeaderID,
     IN       DM_SoapXml *pSoapMsg);

/**
 * @brief Private Entry point for ACS Session Supervision Thread
 *
 * @param void
 *
 * @Return None
 *
 * Note: This routine is used to supervise the ACS Session 
 *       whitin a dedicated thread.
 */
void*
DM_COM_ACS_SESSION_SUPERVISOR(void* data);

/**
 * @brief This private routine is used to update the ACS SESSION TIMER.
 *        Each time a message is received from the ACS, this timer is updated to
 *        the current time.
 *
 * @param void
 *
 * @Return None
 *
 */
void 
_updateAcsSessionTimer();

/**
 * @brief This private routine is used to retry the previous ACS HTTP Message
 *        The retry is performed on ACS Message reception with fault code set to 8005 (RetryRequest)
 *
 * @param void
 *
 * @Return None
 *
 */
void
_retryRequest();

/**
 * @brief Private routine used to send an authentication
 *        request tp the ACS.
 *
 * @param The socket aon wich the message is sent
 *
 * @Return OK DM_OK or ERROR
 *
 */
DMRET
_sendAuthenticationRequest(int nSocksExp);


/**
 * @brief Private routine. This method, is used to check if the parameter
 *                         is present more than one time in a single SET RPC Cmd.
 *
 * @param A pointer on the parameterList (last entry is NULL)
 *
 * @Return TRUE if the SET RPC Cmd is valid. False otherwise.
 *
 */
bool
_isValidRpcCommandList(DM_ENG_ParameterValueStruct * pParameterList[]);


/* 
* Private routine used to build the Soap Message Skeleton
* Parameters: A pointer on the enveloppe XML Document
* return true on success (false otherwise)
*/
bool 
_buildSoapMessageSkeleton(GenericXmlDocumentPtr   pParser,
                          bool                    allEnveloppeAttributs);

#endif /* _DM_COM_H_ */
