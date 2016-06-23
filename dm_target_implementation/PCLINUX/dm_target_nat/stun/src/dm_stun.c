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
 * File        : DM_stun.c
 *
 * Created     : 29/12/2008
 * Author      : Initial version made in the OpenSTB project by Thierry GAYET (ALTEN)
		 Lot of modification made by Cesear di olivera
		 Final integration and adaptation with the ACS server and STUN server made by Thierry GAYET (ALTEN)
 *
 *---------------------------------------------------------------------------
 *
 *  TODO : fusionner DM_STUN_checkAllStunParameters() avec une fonction de récupération des données STUN
 *    	   qui en profiterait pour controler ces données une fois récupéré limitant ainsi les appels au
 *	   DM_Engine !!!
 *
 *---------------------------------------------------------------------------
 */

/**
 * @file DM_stun.c
 *
 * @brief stun threads implementation
 *
 **/
// --------------------------------------------------------------------
// System headers
// --------------------------------------------------------------------
#include <stdlib.h>				/* Standard system library  */
#include <stdio.h>				/* Standard I/O functions   */
#include <string.h>				/* Usefull string functions */
#include <pthread.h>				/* POSIX pthread managing   */
#include <time.h>				/* Time managing            */
#include <sys/time.h>				/* */
#include <arpa/inet.h>				/* For inet_aton            */
#include <assert.h>				/* */
#include <stdbool.h>				/* Need for the bool type   */

// --------------------------------------------------------------------
// lib STUN's headers
// --------------------------------------------------------------------
#include "dm_lib_stun.h"			/* */
#include "dm_lib_udp.h"				/* */
#include "dm_stun.h"				/* */

// --------------------------------------------------------------------
// DM_ENGINE's header
// --------------------------------------------------------------------
#include "DM_ENG_RPCInterface.h"  		/* DM_Engine module definition    */
#include "DM_GlobalDefs.h"			/* */
#include "DM_ENG_Parameter.h"			/* */
#include "DM_ENG_Common.h"			/* */
#include "dm_com_utils.h"			/* */
#include "DM_COM_ConnectionSecurity.h"		/* */
#include "CMN_Trace.h"

#define DM_ENG_THREAD_STACK_SIZE (64*1024)

typedef struct
{
	UInt16			binding;
	UInt16			timeout;
} 
#ifndef mips
__attribute((packed))
#endif
portType;

typedef struct
{
	UInt32            lanIp;
	UInt32            wanIp;
	UInt16            wanBindingPort;
	UInt16            timeoutPort;
	StunAddress4      stunServer;
	pthread_t         threadId;
	pthread_mutex_t   mutex_lock;
} 
#ifndef mips
__attribute((packed))
#endif
timeoutThreadStruct;

typedef struct
{
	/* memorize the binding port and timeout port used by the NAT*/
	portType       natReceivedPort;
	/*memorize the binding port used to send/receive messages by the stun client*/
	portType       stunClientPort;
	Socket         fdClient;
	StunAddress4   stunServer;
	UInt32         lanIp;      /* address LAN */
	UInt32         wanIp;      /* address WAN */
	UInt8          natDetected;
	UInt8          stunWanIpNotification;
	//UDP request values
	UInt64         messageId;
	UInt64         timeStamp;
	UInt8          countStopTimeoutDiscoveryThread;
	UInt8          lastStunMessageReceivedType;
	StunAtrString  username;
	StunAtrString  password;
	int            nTimeout;
	UInt8          TimeoutState;
	timeoutThreadStruct	timeoutThreadData;
	time_t         connectionFreqArray[MAXCONNECTIONREQUESTARRAYSIZE];
	bool           bExitWindow;
	bool           bCanNotificateWanIP;
	bool           bThrirdThreadLaunched;
} 
#ifndef mips
__attribute((packed))
#endif
stunThreadStruct;

// Main thread stun data structure
stunThreadStruct	mainStunThreadData;

// Discovery Timeout Thread Data Structure
stunThreadStruct	discoveryTimeoutStunThreadData;


typedef enum
{
	MESSAGE_UNKNOWN = 0,
	NONE,
	// Emission types
	BINDING_REQUEST,
	BINDING_CHANGE,
	MSG_INTEGRITY,
	TIMEOUT_DISCOVERY,
	// Reception types
	BINDING_RESPONSE,
	BINDING_ERROR_RESPONSE,
	MSG_INTEGRITY_RESPONSE,
	TIMEOUT_RESPONSE,
	UDP_REQUEST
} messageType;

typedef enum
{
	TIMEOUT_SEND,
	TIMEOUT_RECEIVED,
	TIMEOUT_EXPIRATION,
	TIMEOUT_NONE
} timeoutState;

typedef enum
{
	ENABLE        = true,
	DISABLE       = false,
	UNKNOWN_STATE = 2
} stunEnableState;

typedef enum
{
	ANY_RECEIVED,
	JUST_RECEIVED,
	ALREADY_RECEIVED
} stunMessageReceptionStateEnum;

typedef enum
{
	NOT_NOTIFIED,
	NOTIFIED,
	VALIDATED,
} stunWanIpNotificationEnum;

// --------------------------------------------------------------------
// GLOBAL VALUES
// --------------------------------------------------------------------
UInt8			g_timeoutState;
int 			g_timeout;
bool			g_bCanSendMessageFromClient;
StunAtrString		szConnectionRequestUsername;
StunAtrString		szConnectionRequestPassword;
#ifdef  FTX_TRACE_LEVEL
char*			g_DBG_nameTypeMessage[] = { "MESSAGE_UNKNOWN", "NONE", "BINDING_REQUEST", "BINDING_CHANGE", "MSG_INTEGRITY", "TIMEOUT_DISCOVERY", "BINDING_RESPONSE", "BINDING_ERROR_RESPONSE", "MSG_INTEGRITY_RESPONSE", "TIMEOUT_RESPONSE", "UDP_REQUEST" };
#endif

// --------------------------------------------------------------------
// New definitions
// --------------------------------------------------------------------
// A space caracter is the last character of this string so that its length is a multiple of four characters length=20
#define CONNECTION_REQUEST_BINDING_VALUE		"dslforum.org/TR-111 "
#define BINDING_CHANGE_NB_TRANSMIT			    (1)
#define TIMEOUT_PRECISION				            (1.5)
#define TIMEOUT_OFFSET                      (5) // 5 seconds
#define UDP_CONNECTION_REQUEST_ADDRESS_SIZE (256)
#define DEFAULT_ACS_PORT                    (80)
#define UNKNOWN						                  (0)
#define MAX_STOP_BEFORE_ERROR				        (10)
#define DM_STUN_EMPTY_UDP_CONNECTION_REQUEST_ADDRESS	""
#define DM_STUN_MAX_SIZE_PORT_STRING			  (2)
#define DM_STUN_NB_MAX_STEP				          (20)
#define DM_STUN_DEFAULT_MIN_TIMEOUT			    (121)
#define STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT (30)
#define STUN_UDP_CONNEXION_REQUEST_ADDRESS		        DM_PREFIX "ManagementServer.UDPConnectionRequestAddress"
#define STUN_UDP_CONNEXION_REQUEST_NOTIFICATION_LIMIT	DM_PREFIX "ManagementServer.UDPConnectionRequestAddressNotificationLimit"
#define STUN_ENABLE	                                  DM_PREFIX "ManagementServer.STUNEnable"
#define STUN_SERVER_ADDRESS	                          DM_PREFIX "ManagementServer.STUNServerAddress"
#define STUN_SERVER_PORT                              DM_PREFIX "ManagementServer.STUNServerPort"
#define STUN_USERNAME	                                DM_PREFIX "ManagementServer.STUNUsername"
#define STUN_PASSWORD	                                DM_PREFIX "ManagementServer.STUNPassword"
#define STUN_MAX_KEEP_ALIVE_PERIOD                    DM_PREFIX "ManagementServer.STUNMaximumKeepAlivePeriod"
#define STUN_MIN_KEEP_ALIVE_PERIOD                    DM_PREFIX "ManagementServer.STUNMinimumKeepAlivePeriod"
#define STUN_NAT_DETECTED	                            DM_PREFIX "ManagementServer.NATDetected"
#define LAN_IP                                        DM_PREFIX "LAN.IPAddress"
#define ACS_ADDRESS                                   DM_PREFIX "ManagementServer.URL"
#define CONNECTION_REQUEST_USERNAME                   DM_PREFIX "ManagementServer.ConnectionRequestUsername"
#define CONNECTION_REQUEST_PASSWORD	                  DM_PREFIX "ManagementServer.ConnectionRequestPassword"

// --------------------------------------------------------------------
// FUNCTIONS defined in the file
// --------------------------------------------------------------------
// __stun thread functions__
static void  DM_STUN_initValues(stunThreadStruct* stunThreadData);
static bool  DM_STUN_sendManagement(stunThreadStruct* stunThreadData);
static void  DM_STUN_readManagement(stunThreadStruct* stunThreadData);
static void  DM_STUN_readManagementTimeout(stunThreadStruct* stunThreadData);
static void  DM_STUN_stopThread(stunThreadStruct* stunThreadData);
static void  DM_STUN_startTimeoutDiscoveryThread(stunThreadStruct* stunThreadData);
static void* DM_STUN_timeoutDiscoveryManagementThread(stunThreadStruct* initial_stunThreadData);
static void* DM_STUN_timeoutDiscoveryThread(stunThreadStruct* stunThreadData);
// __general functions__
static bool  DM_STUN_sendMessage(Socket, StunAddress4*, UInt8, StunAtrString*, StunAtrString*, UInt32, UInt16);
static UInt8 DM_STUN_readMessage(stunThreadStruct*, int, UInt32*, UInt16*, UInt64*, UInt64*);
static bool  DM_STUN_extractUdpParameters(char*, UInt64*, UInt64*, char*, char*, u8*);
static char* DM_STUN_hextoAscii(u8* hexaMsg, u8 nbHexaDigit);
static void  DM_STUN_openSocket(Socket* fdSocket, UInt32 ip, UInt16 port);
static bool  DM_STUN_isHmacIdentical(StunAtrIntegrity*, char*, UInt16, StunAtrString*);
// __DB dialog functions__
static bool  DM_STUN_getParameterValue(char* parameterName, char** value);
static bool  DM_STUN_updateParameterValue(char* parameterName, DM_ENG_ParameterType type, char* value);
static bool  DM_STUN_updateValuesDatamodel(UInt32 pWanIp, UInt16 pPort, UInt8 pNatDetected);
static bool  DM_STUN_checkAllStunParameters();
static int   DM_STUN_ComputeGlobalTimeout(int step, int min, int max, int limit);

/**
 * @brief the DM_STUN thread
 *        launch the two threads to manage the STUN protocol and the timeout NAT session
 * 
 * @param none
 *
 * @return pthread_exit
 *
 */
void*
DM_STUN_stunThread()
{
	// --------------------------------------------------------------------
	// 				DECLARATIONS
	// --------------------------------------------------------------------
	UInt8			stunEnable;
	UInt8			previousStunEnable;
	UInt8			natDetected;
	char			udpConnectionRequestAddress[UDP_CONNECTION_REQUEST_ADDRESS_SIZE];
	char*			getParameterResult     = NULL;
	int       nMinValTimeout         = 0;
	int       nMaxValTimeout         = 0;
	bool			bStartDiscovery        = true;
	bool			bForceNoStartDiscovery = false;
	volatile UInt32 previousLanIp; // volatile to avoid a warning
	struct in_addr		tmp;

	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));
	
	INFO( "The STUN Client is starting " );
	
	// --------------------------------------------------------------------
	// Initialization
	// --------------------------------------------------------------------
	mainStunThreadData.wanIp                        = UNKNOWN;
	mainStunThreadData.natDetected                  = UNKNOWN;
	mainStunThreadData.lastStunMessageReceivedType  = MESSAGE_UNKNOWN;
	mainStunThreadData.messageId                    = 0;
	mainStunThreadData.timeStamp                    = 0;
	mainStunThreadData.bCanNotificateWanIP          = true;
	mainStunThreadData.bThrirdThreadLaunched        = false;
	mainStunThreadData.TimeoutState                 = TIMEOUT_NONE;
	stunEnable                                      = UNKNOWN_STATE;
	previousStunEnable                              = UNKNOWN_STATE;
	natDetected                                     = false;
	
	memset( &udpConnectionRequestAddress, 0x00, UDP_CONNECTION_REQUEST_ADDRESS_SIZE );
	memset( &(mainStunThreadData.timeoutThreadData), 0x00, sizeof(timeoutThreadStruct) );
	
	initConnectionFreqArray( mainStunThreadData.connectionFreqArray );
	pthread_mutex_init( &(mainStunThreadData.timeoutThreadData.mutex_lock), NULL );

	
	// --------------------------------------------------------------------
	// Check that all the parameters are well provided in the dataModel database
	// -------------------------------------------------------------------
	while ( false == DM_STUN_checkAllStunParameters() )
	{
		WARN( "STUN - Thread Client - All required parameters are not defined in the datamodel" );
		WARN( "STUN - Thread Client - Wait a while (some parameters could be set later)" );
		sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	} // IF
	
	// Init values
	DM_STUN_initValues( &mainStunThreadData );
	
	// --------------------------------------------------------------------
	// 	USERNAME AND PASSWORD FOR THE CONNECTION REQUEST (UDP/TCP)
	// --------------------------------------------------------------------
	// Read the current ConnectionRequestUsername & password
	// need for the UDPConnectionRequest in the dataModel
	// --------------------------------------------------------------------
	// Username :
	// --------------------------------------------------------------------
	memset( szConnectionRequestUsername.value, 0x00, STUN_MAX_STRING );
	DM_STUN_getParameterValue( CONNECTION_REQUEST_USERNAME, &getParameterResult );
	DBG( "STUN - Result_USR: '%s' ", getParameterResult );
	
	strcpy( szConnectionRequestUsername.value, getParameterResult );
	szConnectionRequestUsername.sizeValue = strlen( szConnectionRequestUsername.value );
	free( getParameterResult );
	
	// --------------------------------------------------------------------
	// Password :
	// --------------------------------------------------------------------
	memset( szConnectionRequestPassword.value, 0x00, STUN_MAX_STRING );
	DM_STUN_getParameterValue( CONNECTION_REQUEST_PASSWORD, &getParameterResult );
	DBG( "STUN - Result_PWD: '%s' ", getParameterResult );
	
	strcpy( szConnectionRequestPassword.value, getParameterResult );
	szConnectionRequestPassword.sizeValue = strlen( szConnectionRequestPassword.value );
	free(getParameterResult);
	
	DBG( "STUN - ConnectionRequestUsername = '%s' ", szConnectionRequestUsername.value );
	DBG( "STUN - ConnectionRequestPassword = '%s' ", szConnectionRequestPassword.value );
	
	// --------------------------------------------------------------------
	// 			INITIAL TIMEOUT VALUE
	// --------------------------------------------------------------------
	// The timeout is equal to the time determined by the timeoutDiscoveryThread to maintain the binding.
	// If the thread is not launched, this is equal to STUNMininmumKeepAlivePeriod This allow to use 
	// only one thread to manage the R/W
	// --------------------------------------------------------------------
	DM_STUN_getParameterValue( STUN_MIN_KEEP_ALIVE_PERIOD, &getParameterResult );
	nMinValTimeout = atol( getParameterResult ); //time in seconds
	free( getParameterResult );
	DM_STUN_getParameterValue( STUN_MAX_KEEP_ALIVE_PERIOD, &getParameterResult );
	nMaxValTimeout = atol( getParameterResult ); //time in seconds
	free( getParameterResult );
	// --------------------------------------------------------------------
	// If STUN_MIN_KEEP_ALIVE_PERIOD is equal to STUN_MAX_KEEP_ALIVE_PERIOD, so 
	// the timeout should be STUN_MIN_KEEP_ALIVE_PERIOD
	// --------------------------------------------------------------------
	if ( nMinValTimeout == nMaxValTimeout )
	{
		DBG( "nMinValTimeout (%d) = nMaxValTimeout (%d), so the Discovery pthread won't start", nMinValTimeout, nMaxValTimeout );
		bForceNoStartDiscovery = true;
	} // IF
	mainStunThreadData.nTimeout = nMinValTimeout;
	g_timeout		= mainStunThreadData.nTimeout;
	DBG( "STUN Initial Timeout value = %d seconds ", mainStunThreadData.nTimeout );
	
	// --------------------------------------------------------------------
	// Find a free port number for the main stun client
	// --------------------------------------------------------------------
	mainStunThreadData.stunClientPort.binding = stunRandomPort();
	DBG( "STUN Client UDP Port = %d ", mainStunThreadData.stunClientPort.binding );
	
	// --------------------------------------------------------------------
	// Read the currect IP address in the dataModel
	// --------------------------------------------------------------------
	DM_STUN_getParameterValue( LAN_IP, &getParameterResult );
	inet_aton( getParameterResult, &tmp );
	mainStunThreadData.lanIp = tmp.s_addr;
	free( getParameterResult );
	previousLanIp        = mainStunThreadData.lanIp;
	
	// --------------------------------------------------------------------------------------
	// Open the port found before
	// --------------------------------------------------------------------------------------
	DM_STUN_openSocket( &(mainStunThreadData.fdClient), mainStunThreadData.lanIp, mainStunThreadData.stunClientPort.binding );
	
	// --------------------------------------------------------------------------------------
	// Define an handle to the DM_STUN_stopThread function in case if the thread received a signal
	// --------------------------------------------------------------------------------------
	pthread_cleanup_push( (void(*)(void*))DM_STUN_stopThread, (void*)&mainStunThreadData );
	
	// --------------------------------------------------------------------------------------
	// Update the datamodel with the init values
	// --------------------------------------------------------------------------------------
	DM_STUN_updateValuesDatamodel( mainStunThreadData.wanIp, mainStunThreadData.stunClientPort.binding, mainStunThreadData.natDetected );
	INFO( "STUN Initialization completed successfully" );
	
	// --------------------------------------------------------------------------------------
	// Main loop of the stun client
	// --------------------------------------------------------------------------------------
	do
	{
		DBG( "Binding Thread -  - Begin Main Loop" );
		mainStunThreadData.bThrirdThreadLaunched = false;
		
		// --------------------------------------------------------------------------------------
		// Check if STUN is enabled in the dataModel
		// --------------------------------------------------------------------------------------
		DBG( "Check if the STUN is ENABLED in the dataModel..." );
		DM_STUN_getParameterValue( STUN_ENABLE, &getParameterResult );
		stunEnable = atol( getParameterResult );
		DBG( "--> STUN is [%s] ", stunEnable==ENABLE?"ENABLED":"DISABLED" );
		free( getParameterResult );
		
		// --------------------------------------------------------------------------------------
		// Check the latest IP address (can changed in DHCP mode) and update it according to the STUN context
		// If the LAN_IP_ADDRESS have an active notification, the information will arrived until the ACS server
		// If the value of the LAN_IP_ADDRESS have changed, close the previous socket then reopen one using the new values (IP_ADDR:PORT)
		// --------------------------------------------------------------------------------------
		DBG( "Check LAN IP modification" ); // DHCP renewal
		DM_STUN_getParameterValue( LAN_IP, &getParameterResult );
		DBG( "lanIp: %s", getParameterResult );
		inet_aton( getParameterResult, &tmp );
		mainStunThreadData.lanIp = tmp.s_addr;
		free( getParameterResult );
		if ( previousLanIp != mainStunThreadData.lanIp )
		{
			DBG( "The LAN IP address have changed ; The socket will be reopen using the current IP address. " );
			previousLanIp = mainStunThreadData.lanIp;
			DBG( "The old socket FD is : %d ", (int)mainStunThreadData.fdClient );
			close( mainStunThreadData.fdClient );
			DM_STUN_openSocket( &(mainStunThreadData.fdClient), mainStunThreadData.lanIp, mainStunThreadData.stunClientPort.binding );
			DBG( "The new socket FD is : %d ", (int)mainStunThreadData.fdClient );
		} else {
			DBG( "The LAN IP address have not changed ; nothing to do. " );
		} // IF
		
		// --------------------------------------------------------------------------------------
		// --> STUN was working and will end
		// --------------------------------------------------------------------------------------
		// Check if it is the first loop AND if STUN is disabled in the dataModel
		// or
		// Check if STUN was working before AND if STUN is disabled in the dataModel
		// --------------------------------------------------------------------------------------
		if ( ((UNKNOWN_STATE == previousStunEnable) && (DISABLE == stunEnable))
		  || ((ENABLE        == previousStunEnable) && (DISABLE == stunEnable)) )
		{
			// --------------------------------------------------------------------------------------
			// Display the reason of the different status
			// --------------------------------------------------------------------------------------
			if ( (UNKNOWN_STATE == previousStunEnable) && (DISABLE == stunEnable) )
			{
				DBG( "STUN wasn't working yet but is now disabled in the datoModel " );
			} // IF
			if ( (ENABLE == previousStunEnable) && (DISABLE == stunEnable) )
			{
				DBG( "STUN was enabled but is now disabled in the dataModel        " );
			} // IF
			previousStunEnable = DISABLE;	// Update the STUN state
			natDetected        = false;	// Update the NAT DETECTED value
			bStartDiscovery    = false;	// Disabled to start a new pthread discovery
		} // IF
		
		// --------------------------------------------------------------------------------------
		// --> STUN was not working and will start
		// --------------------------------------------------------------------------------------
		// Check if it is the first loop AND if STUN is enabled in the dataModel
		// or
		// Check if STUN was not working before AND if STUN is enabled in the dataModel
		// --------------------------------------------------------------------------------------
		else if ( ((UNKNOWN_STATE == previousStunEnable) && (ENABLE == stunEnable))
		       || ((DISABLE       == previousStunEnable) && (ENABLE == stunEnable)) )
		{
			// --------------------------------------------------------------------------------------
			// Display the reason of the different status
			// --------------------------------------------------------------------------------------
			if ( (UNKNOWN_STATE == previousStunEnable) && (ENABLE == stunEnable) )
			{
				DBG( "STUN wasn't working yet but is now enabled in the datoModel " );
			} // IF
			if ((DISABLE == previousStunEnable) && (ENABLE == stunEnable) )
			{
				DBG( "STUN was disabled but is now enabled in the datoModel       " );
			} // IF
			previousStunEnable = ENABLE;	// Update the STUN state
			bStartDiscovery    = true;	// Start the Discovery thread
			
			// --------------------------------------------------------------------------------------
			// Initialize the STUN context
			// --------------------------------------------------------------------------------------
			DM_STUN_initValues( &mainStunThreadData );
		} // IF
		
		// --------------------------------------------------------------------------------------
		// SEND message management
		// --------------------------------------------------------------------------------------
		if ( ENABLE == stunEnable )
		{
			DBG( "Binding Thread - STUN Client is ENABLED " );
			
			DBG( "Binding Thread - Send a binding request Message ");
			DM_STUN_sendManagement( &mainStunThreadData );
			
			// --------------------------------------------------------------------------------------
			// Set the current NAT timeout
			// --------------------------------------------------------------------------------------
			mainStunThreadData.nTimeout = g_timeout;
			DBG( "Binding Thread - Current Timeout %d sec ", g_timeout );
			
			// --------------------------------------------------------------------------------------
			// READ message management
			// --------------------------------------------------------------------------------------
			DBG( "Binding Thread - Read the Message - Begin");
			DM_STUN_readManagement( &mainStunThreadData );
			DBG( "Binding Thread - Read the Message - End");
			
			// --------------------------------------------------------------------------------------
			// Start the discoveru thread
			// --------------------------------------------------------------------------------------
			if ( (true == bStartDiscovery) && (false == bForceNoStartDiscovery) )
			{
				DBG( "Binding Thread - Start Timeout Discovery Thread");
				DM_STUN_startTimeoutDiscoveryThread( &mainStunThreadData );
				bStartDiscovery = false;
			} else if ( (true == bStartDiscovery) && (true == bForceNoStartDiscovery) ) {
				WARN( "Binding Thread - Do Not Start Timeout Discovery Thread");
				WARN( "Binding Thread - The timeout is set to %d seconds (min value). ", nMinValTimeout );
			} // IF
		} else {
			DBG( "Binding Thread - STUN disabled. No STUN messages will be send." );
			sleep( 1 );
		} // IF
		DBG( "Binding Thread - End Main Loop" );
	} while( true ); // --> what's about if we wanna stop the process ?
	
	// --------------------------------------------------------------------------------------
	// Dead code...
	// --------------------------------------------------------------------------------------
	pthread_cleanup_pop(1); // Close( mainStunThreadData.fdClient )
} // DM_STUN_stunThread

void
DM_STUN_startTimeoutDiscoveryThread(stunThreadStruct *stunThreadData)
{
	pthread_attr_t	timeoutThreadAttribute_ManagementThread;
	
	// -------------------------------------------
	// DM_STUN_timeoutDiscoveryManagement pthread
	// -------------------------------------------
	// 1. Stop if already running
	// 2. Initialization of the pthread context
	// 3. Start it
	// -------------------------------------------
	if ( 0 != stunThreadData->timeoutThreadData.threadId )
	{
		DBG( "Timeout discovery thread already running. Ask to cancel" );
		pthread_cancel( stunThreadData->timeoutThreadData.threadId );
		stunThreadData->timeoutThreadData.threadId = 0;
	} // IF
	pthread_attr_init( &timeoutThreadAttribute_ManagementThread );
	pthread_attr_setdetachstate( &timeoutThreadAttribute_ManagementThread, PTHREAD_CREATE_DETACHED  );
	pthread_attr_setstacksize( &timeoutThreadAttribute_ManagementThread,   DM_ENG_THREAD_STACK_SIZE );
	if ( 0 != pthread_create( &(stunThreadData->timeoutThreadData.threadId),
				  &timeoutThreadAttribute_ManagementThread,
				  (void*(*)(void*))DM_STUN_timeoutDiscoveryManagementThread,
				  (void*)&stunThreadData) )
	{
		EXEC_ERROR( "Starting Timeout discovery thread failed!!" );
	} else {
		pthread_detach( stunThreadData->timeoutThreadData.threadId );
	} // IF
} // DM_STUN_startTimeoutDiscoveryThread

/**
 * @brief First Timeout discovery thread used for discovering the windows timeout
 * @brief That will be the same as the function DM_STUN_stunthread()
 *
 * @param timeoutThreadData (R)
 *
 * @return pthread_exit(0);
 *
 */
void*
DM_STUN_timeoutDiscoveryManagementThread(stunThreadStruct* initial_stunThreadData)
{
	// --------------------------------------------------------------------
	// 				DECLARATIONS
 	// --------------------------------------------------------------------
//	stunThreadStruct	stunThreadData;
	UInt8             stunEnable;
	UInt8             previousStunEnable;
	UInt8             natDetected;
	char            * getParameterResult = NULL;
	UInt32            previousLanIp;
	struct in_addr    tmp;
	pthread_attr_t    timeoutThreadAttribute_DiscoveryThread;
	// discoveryTimeoutStunThreadData
	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));
	
	pthread_mutex_init( &(discoveryTimeoutStunThreadData.timeoutThreadData.mutex_lock), NULL );
	
	DBG( "TimeoutDiscovery Thread - --> DM_STUN_timeoutDiscoveryManagementThread() " );
	
	// --------------------------------------------------------------------
	// 				Initialisation
	// --------------------------------------------------------------------
	discoveryTimeoutStunThreadData.TimeoutState      = TIMEOUT_NONE;
	stunEnable                       = UNKNOWN_STATE;
	previousStunEnable               = UNKNOWN_STATE;
	natDetected                      = FALSE;
	previousLanIp                    = UNKNOWN;
	discoveryTimeoutStunThreadData.wanIp             = UNKNOWN;
	discoveryTimeoutStunThreadData.natDetected       = UNKNOWN;
	discoveryTimeoutStunThreadData.bCanNotificateWanIP   = false;
	discoveryTimeoutStunThreadData.bThrirdThreadLaunched = false;
	discoveryTimeoutStunThreadData.messageId         = 0;
	discoveryTimeoutStunThreadData.timeStamp         = 0;
	discoveryTimeoutStunThreadData.bExitWindow       = true;
	discoveryTimeoutStunThreadData.countStopTimeoutDiscoveryThread = 0;
	discoveryTimeoutStunThreadData.lastStunMessageReceivedType     = MESSAGE_UNKNOWN;
	memset( &(discoveryTimeoutStunThreadData.timeoutThreadData), 0x00, sizeof(timeoutThreadStruct) );
	discoveryTimeoutStunThreadData.timeoutThreadData.threadId      = 0;
	
	// --------------------------------------------------------------------
	// the timeout is equal to the time determined by the timeoutDiscoveryThread to maintain 
	// the binding. if the thread is not launched, this is equal to
	// STUNMininmumKeepAlivePeriod This allow to use only one thread to manage the R/W
	// --------------------------------------------------------------------
	DM_STUN_getParameterValue( STUN_MIN_KEEP_ALIVE_PERIOD, &getParameterResult );
	discoveryTimeoutStunThreadData.nTimeout = atol( getParameterResult );	// Time in seconds
	free( getParameterResult );
	DBG( "TimeoutDiscovery Thread - STUN Initial Timeout value = %d seconds.", discoveryTimeoutStunThreadData.nTimeout );
	
	// --------------------------------------------------------------------
	// Find a free port
	// --------------------------------------------------------------------
	do{
	  discoveryTimeoutStunThreadData.stunClientPort.binding = stunRandomPort();
	} 
	while( discoveryTimeoutStunThreadData.stunClientPort.binding == initial_stunThreadData->stunClientPort.binding );
	
	{ 
	  discoveryTimeoutStunThreadData.stunClientPort.timeout = stunRandomPort();
	}
	while( (discoveryTimeoutStunThreadData.stunClientPort.binding == discoveryTimeoutStunThreadData.stunClientPort.timeout) ||
	       (initial_stunThreadData->stunClientPort.binding        == discoveryTimeoutStunThreadData.stunClientPort.timeout) );
	       
	DBG( "TimeoutDiscovery Thread - STUN Client UDP Port        = %d ", discoveryTimeoutStunThreadData.stunClientPort.binding );
	DBG( "TimeoutDiscovery Thread - STUN Timeout Discovery Port = %d ", discoveryTimeoutStunThreadData.stunClientPort.timeout );
	
	// --------------------------------------------------------------------
	// Read the currect IP address
	// --------------------------------------------------------------------
	DM_STUN_getParameterValue( LAN_IP, &getParameterResult );
	inet_aton( getParameterResult, &tmp );
	discoveryTimeoutStunThreadData.lanIp = tmp.s_addr;
	free( getParameterResult );
	previousLanIp        = discoveryTimeoutStunThreadData.lanIp;
	
	// --------------------------------------------------------------------
	// Make some other settings
	// --------------------------------------------------------------------
	initConnectionFreqArray( discoveryTimeoutStunThreadData.connectionFreqArray );
	DM_STUN_initValues( &discoveryTimeoutStunThreadData );
	DM_STUN_openSocket( &(discoveryTimeoutStunThreadData.fdClient), discoveryTimeoutStunThreadData.lanIp, discoveryTimeoutStunThreadData.stunClientPort.binding );
	INFO( "TimeoutDiscovery Thread - STUN Initialization completed successfully." );
	
	// -------------------------------------------
	// Main loop of the Discovery pthread
	// -------------------------------------------
	WARN( "TimeoutDiscovery Thread - TimeoutDiscovery Thread - BEFORE THE MAIN LOOP of the DM_STUN_timeoutDiscoveryManagementThread " );
	do
	{
		DBG( "TimeoutDiscovery Thread - Main Loop Start" );
		DBG( "TimeoutDiscovery Thread - Current timeout duration = %d ", discoveryTimeoutStunThreadData.nTimeout );
		
		// --------------------------------------------------------------------
		// Test if STUN is enabled in the dataModel
		// --------------------------------------------------------------------
		DBG( "TimeoutDiscovery Thread - Check if the STUN is ENABLED" );
		DM_STUN_getParameterValue( STUN_ENABLE, &getParameterResult );
		stunEnable = atol( getParameterResult );
		DBG( "TimeoutDiscovery Thread - STUN is %s", stunEnable==ENABLE?"ENABLED":"DISABLED" );
		free( getParameterResult );
		
		// --------------------------------------------------------------------
		// Test if the LAN IP address have changed because of a DHCP new context
		// --------------------------------------------------------------------
		DBG( "TimeoutDiscovery Thread - Check if the LAN IP address have changed (DHCP)..." );
		DM_STUN_getParameterValue( LAN_IP, &getParameterResult );
		DBG( "TimeoutDiscovery Thread - lanIp: %s", getParameterResult );
		inet_aton( getParameterResult, &tmp );
		discoveryTimeoutStunThreadData.lanIp = tmp.s_addr;
		free( getParameterResult );
		if ( previousLanIp != discoveryTimeoutStunThreadData.lanIp )
		{
			DBG( "TimeoutDiscovery Thread - The LAN IP address have changed ; Reopen the socket." );
			previousLanIp = discoveryTimeoutStunThreadData.lanIp;
			close( discoveryTimeoutStunThreadData.fdClient );
			DM_STUN_openSocket( &(discoveryTimeoutStunThreadData.fdClient), discoveryTimeoutStunThreadData.lanIp, discoveryTimeoutStunThreadData.stunClientPort.binding );
		} else {
			DBG( "TimeoutDiscovery Thread - The LAN IP address have not changed ; nothing to do." );
		} // IF
		
		// --------------------------------------------------------------------
		// Check of the previous state and if stun is enabled
		// --------------------------------------------------------------------
		DBG( "TimeoutDiscovery Thread - STUN state         = %d (ENABLE=%d - DISABLE=%d)       ", stunEnable, ENABLE, DISABLE               );
		DBG( "TimeoutDiscovery Thread - PreviousStunEnable = %d (UNKNOWN_STATE=%d - ENABLE=%d) ", previousStunEnable, UNKNOWN_STATE, ENABLE );
		if ( ((UNKNOWN_STATE == previousStunEnable) && (DISABLE == stunEnable))
		  || ((ENABLE        == previousStunEnable) && (DISABLE == stunEnable)) )
		{
			DBG( "TimeoutDiscovery Thread - STUN is turned OFF" );
			previousStunEnable = DISABLE;
			natDetected        = FALSE;
			tmp.s_addr         = discoveryTimeoutStunThreadData.lanIp;
		} else if ( ((UNKNOWN_STATE == previousStunEnable) && (ENABLE == stunEnable))
		         || ((DISABLE       == previousStunEnable) && (ENABLE == stunEnable)) )
		{
			DBG( "TimeoutDiscovery Thread - STUN is turned ON" );
			previousStunEnable = ENABLE;
			DM_STUN_initValues( &discoveryTimeoutStunThreadData );
		} // IF
		
		// --------------------------------------------------------------------------------------
		// SEND message management
		// --------------------------------------------------------------------------------------
		DBG( "TimeoutDiscovery Thread - STUN state = %d (ENABLE=%d - DISABLE=%d) ", stunEnable, ENABLE, DISABLE );
		//DBG( "timeoutThreadData.threadId = %d ", (discoveryTimeoutStunThreadData.timeoutThreadData.threadId) );
		if ( (ENABLE == stunEnable) && (0 == discoveryTimeoutStunThreadData.timeoutThreadData.threadId) )
		{
			DBG( "TimeoutDiscovery Thread - STUN Client is ENABLED (Timeout discovery thread is not active) " );
			DBG( "TimeoutDiscovery Thread - Before the DM_STUN_sendManagement() function " );
			DM_STUN_sendManagement( &discoveryTimeoutStunThreadData );
			DBG( "TimeoutDiscovery Thread - After the DM_STUN_sendManagement() function  " );
		} else {
			DBG( "TimeoutDiscovery Thread - STUN Client is DISABLED (or Timeout discovery in progress). No message will be sent " );
		} // IF
		
		// --------------------------------------------------------------------------------------
		// READ a message
		// --------------------------------------------------------------------------------------
		DBG( "TimeoutDiscovery Thread - Before the DM_STUN_readManagement() function " );
		DM_STUN_readManagementTimeout( &discoveryTimeoutStunThreadData );
		DBG( "TimeoutDiscovery Thread - After the DM_STUN_readManagement() function  " );
		
		// --------------------------------------------------------------------------------------
		// Test if we already have the WanIP address
		// --------------------------------------------------------------------------------------
		DBG( "TimeoutDiscovery Thread - StunWanIpNotification flag = %d (NOT_NOTIFIED=%d, NOTIFIED=%d, VALIDATED=%d) ", discoveryTimeoutStunThreadData.stunWanIpNotification, NOT_NOTIFIED, NOTIFIED, VALIDATED );
		if ( VALIDATED == discoveryTimeoutStunThreadData.stunWanIpNotification )
		{
			DBG( "TimeoutDiscovery Thread - WAN IP address received so we can start the DM_STUN_timeoutDiscoveryThread() pthread..." );
			
			// --------------------------------------------------------------------------------------
			// ReInit the stunWanIpNotification flag with NOT_NOTIFIED
			// --------------------------------------------------------------------------------------
			DBG( "TimeoutDiscovery Thread - Set the stunWanIpNotification flag to NOT_NOTIFIED " );
			discoveryTimeoutStunThreadData.stunWanIpNotification = NOT_NOTIFIED;
			
			// -------------------------------------------
			// Start the DM_STUN_timeoutDiscoveryThread pthread (2)
			// -------------------------------------------
			// 1. Stop if already running
			// 2. Initialization of the pthread context
			// 3. Start it
			// -------------------------------------------
			DBG( "TimeoutDiscovery Thread - Before to start the DM_STUN_timeoutDiscoveryThread() pthread" );
			if ( 0 != discoveryTimeoutStunThreadData.timeoutThreadData.threadId )
			{
				DBG( "TimeoutDiscovery Thread - Timeout discovery thread already running. Ask to cancel the current one..." );
				pthread_cancel( discoveryTimeoutStunThreadData.timeoutThreadData.threadId );
				discoveryTimeoutStunThreadData.timeoutThreadData.threadId = 0;
			} // IF
			pthread_attr_init( &timeoutThreadAttribute_DiscoveryThread );
			pthread_attr_setdetachstate( &timeoutThreadAttribute_DiscoveryThread, PTHREAD_CREATE_DETACHED );
			pthread_attr_setstacksize( &timeoutThreadAttribute_DiscoveryThread, DM_ENG_THREAD_STACK_SIZE );
			discoveryTimeoutStunThreadData.timeoutThreadData.timeoutPort    = discoveryTimeoutStunThreadData.stunClientPort.timeout;
			discoveryTimeoutStunThreadData.timeoutThreadData.wanBindingPort = discoveryTimeoutStunThreadData.natReceivedPort.binding;
			discoveryTimeoutStunThreadData.timeoutThreadData.lanIp          = discoveryTimeoutStunThreadData.lanIp;
			discoveryTimeoutStunThreadData.timeoutThreadData.wanIp          = discoveryTimeoutStunThreadData.wanIp;
			discoveryTimeoutStunThreadData.timeoutThreadData.stunServer     = discoveryTimeoutStunThreadData.stunServer;
			discoveryTimeoutStunThreadData.bThrirdThreadLaunched		= true;
			if ( 0 != pthread_create( &(discoveryTimeoutStunThreadData.timeoutThreadData.threadId),
				  		  &timeoutThreadAttribute_DiscoveryThread,
				  		  (void*(*)(void*))DM_STUN_timeoutDiscoveryThread,
				  		  (void*)&(discoveryTimeoutStunThreadData)) )
			{
				EXEC_ERROR( "TimeoutDiscovery Thread - Starting Timeout discovery thread : NOK " );
			} else {
				DBG(   "TimeoutDiscovery Thread - Starting Timeout discovery thread : OK  " );
				pthread_detach( discoveryTimeoutStunThreadData.timeoutThreadData.threadId );
			} // IF
			DBG( "TimeoutDiscovery Thread - After to start the DM_STUN_timeoutDiscoveryThread() pthread" );
		} else {
			DBG( "TimeoutDiscovery Thread - The stunWanIpNotification flags is not set to VALIDATED. Don't start the third pthread !!" );
		} // IF
		DBG( "TimeoutDiscovery Thread - ----- END OF THE MAIN LOOP of the DM_STUN_timeoutDiscoveryManagementThread -----" );
	} while( discoveryTimeoutStunThreadData.bExitWindow );
	
	// -------------------------------------------
	// Close the temporaly FD used for the socket
	// -------------------------------------------
	DBG( "TimeoutDiscovery Thread - Closing the FD (%d) used for the socket ", discoveryTimeoutStunThreadData.fdClient );
	close( discoveryTimeoutStunThreadData.fdClient );
	
	DBG( "TimeoutDiscovery Thread - <-- DM_STUN_timeoutDiscoveryManagementThread() " );
	
	// -------------------------------------------
	// Exit the pthread (2)
	// -------------------------------------------
	pthread_exit( 0 );
} // DM_STUN_timeoutDiscoveryManagementThread

/**
 * @brief Second Timeout discovery thread used for discovering the windows timeout
 *
 * @param timeoutThreadData (R)
 *
 * @return pthread_exit(0);
 *
 */
void*
DM_STUN_timeoutDiscoveryThread(stunThreadStruct* stunThreadData)
{
	Socket		fdTimeout		= -1;
	int		minTimeout      = 0;
	int		maxTimeout      = 0;
	int		timeout         = 0;
	int		step            = 0;
	UInt32		previousLanIp       = stunThreadData->timeoutThreadData.lanIp;
	UInt8		timeoutReceptionState	= NONE;
	char*		getParameterResult    = NULL;
	struct in_addr	tmp;
	
	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));
	
	DBG( "--> DM_STUN_timeoutDiscoveryThread (Start Timeout Discovery Thread) " );
	
	DBG( "Timeout port     = %u ", stunThreadData->timeoutThreadData.timeoutPort    );
	DBG( "Nat binding port = %u ", stunThreadData->timeoutThreadData.wanBindingPort );
	DM_STUN_getParameterValue( STUN_MAX_KEEP_ALIVE_PERIOD, &getParameterResult );
	maxTimeout = atol( getParameterResult ); //time in seconds
	free( getParameterResult);
	DM_STUN_getParameterValue( STUN_MIN_KEEP_ALIVE_PERIOD, &getParameterResult );
	minTimeout = atol( getParameterResult ); //time in seconds
	free( getParameterResult );
	DBG( "Timeout discovery : initialization successfully performed" );
	DBG( "MinTimeout=%d - maxTimeout=%d", minTimeout, maxTimeout );
	if ( minTimeout == maxTimeout )
	{
		DBG( "STUN_MIN_KEEP_ALIVE_PERIOD and STUN_MAX_KEEP_ALIVE_PERIOD are identical " );
		DBG( "The timeout discovery is not necessary. Set the timeout to the minimum. " );
		timeout = minTimeout;
	} else {
		DM_STUN_openSocket( &fdTimeout, stunThreadData->timeoutThreadData.lanIp, stunThreadData->timeoutThreadData.timeoutPort );
		DBG( "Timeout discovery thread : start" );
		timeout = minTimeout; // Initial value !!
		step    = (maxTimeout-minTimeout)/DM_STUN_NB_MAX_STEP;
		if(0 == step) step=1;  // At least set step to 1 sec.
		DBG ( "Step = %d ", step );
		
		// ------------------------------------------------------------------------------------------
		// A message is send then the thread is asleep.
		// When the thread is wake up if the TIMEOUT is received, that means that the previous
		// time sleep is ok.
		// ------------------------------------------------------------------------------------------
		do
		{
			// ------------------------------------------------------------------------------------------
			// Set the TimeoutState flag to TIMEOUT_SEND
			// ------------------------------------------------------------------------------------------
			pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );

			DBG( "TimeoutDiscovery Thread - TimeoutReceptionState (before) = %d ", stunThreadData->TimeoutState );
			DBG( "TimeoutDiscovery Thread - Set TimeoutState to TIMEOUT_SEND");
			stunThreadData->TimeoutState = TIMEOUT_SEND;
			DBG( "TimeoutDiscovery Thread - TimeoutReceptionState (after) = %d  ", stunThreadData->TimeoutState );

			pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );
			
			// ------------------------------------------------------------------------------------------
			// Get the latest IP address and see if we need to reopen it
			// ------------------------------------------------------------------------------------------
			DM_STUN_getParameterValue( LAN_IP, &getParameterResult );
			inet_aton( getParameterResult, &tmp );
			stunThreadData->timeoutThreadData.lanIp = tmp.s_addr;
			free( getParameterResult );
			if ( previousLanIp != stunThreadData->timeoutThreadData.lanIp )
			{
				DBG( "TimeoutDiscovery Thread -The LAN Ip address have changed ; will close it then reopen using the last values. " );
				previousLanIp = stunThreadData->timeoutThreadData.lanIp;
				close( fdTimeout );
				DM_STUN_openSocket( &fdTimeout, stunThreadData->timeoutThreadData.lanIp, stunThreadData->timeoutThreadData.timeoutPort );
      			} else {
				DBG( "TimeoutDiscovery Thread -The LAN Ip address have not changed : Nothing to do." );
			} // IF
			
			// ------------------------------------------------------------------------------------------
			// Send a discovery message (Bind_Request with a RESPONSE_ADDRESS to the WANIP)
			// ------------------------------------------------------------------------------------------
			DBG( "TimeoutDiscovery Thread --> Before the DM_STUN_sendMessage() " );
			DBG( "TimeoutDiscovery Thread -STUN Timeout discovery message to the stun server. ");
			DBG( "TimeoutDiscovery Thread -(Before to send) TimeoutReceptionState=%d ", stunThreadData->TimeoutState );
			DM_STUN_sendMessage( fdTimeout,
					     &(stunThreadData->timeoutThreadData.stunServer),
					     TIMEOUT_DISCOVERY,
					     NULL,
					     NULL,
					     stunThreadData->timeoutThreadData.wanIp,
					     stunThreadData->timeoutThreadData.wanBindingPort );
			DBG( "TimeoutDiscovery Thread - <-After the DM_STUN_sendMessage() " );
			
			// ------------------------------------------------------------------------------------------
			// Wait for a while
			// ------------------------------------------------------------------------------------------
			DBG( "TimeoutDiscovery Thread - Sleep %d seconds", timeout );
			sleep( timeout );
			DBG( "TimeoutDiscovery Thread - Wake up" );
			
			// ------------------------------------------------------------------------------------------
			// Save the global TimeoutState flag
			// ------------------------------------------------------------------------------------------

			pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );

			DBG( "TimeoutDiscovery Thread - TimeoutReceptionState (before) = %d ", stunThreadData->TimeoutState );
			timeoutReceptionState = stunThreadData->TimeoutState;
			DBG( "TimeoutDiscovery Thread - TimeoutReceptionState (after)  = %d ", stunThreadData->TimeoutState );
			
			pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );

			
			// ------------------------------------------------------------------------------------------
			// Test if the latest global TimeoutState flag is a TIMEOUT_RECEIVED, update in the ReadManagement()
			// function ; if so, that's means that a BindingResponse have been received so we should continue
			// the timeout discovering...
			// ------------------------------------------------------------------------------------------
			DBG( "TimeoutDiscovery Thread - TimeoutReceptionState = %d (TIMEOUT_RECEIVED=%d) ", timeoutReceptionState, TIMEOUT_RECEIVED );
			if ( TIMEOUT_RECEIVED == timeoutReceptionState )
			{
				DBG( "TimeoutDiscovery Thread - ---- TIMEOUT_RECEIVED : SUCCEED with %d seconds ---- ", timeout );

				pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );

				timeout += step;
				
				DBG( "TimeoutDiscovery Thread - New timeout value = %d (step = %d)", timeout, step );
				
				//g_timeout = timeout;
				DBG( "TimeoutDiscovery Thread - Global timeout (before) = %d ", stunThreadData->nTimeout );
				stunThreadData->nTimeout = timeout;
				DBG( "TimeoutDiscovery Thread - Global timeout (after) = %d (new value) ", stunThreadData->nTimeout );
				pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );

			} // IF
			
			// ------------------------------------------------------------------------------------------
			// If the latest global TimeoutState flag is NOT a TIMEOUT_RECEIVED, any message have been
			// received so the current timeout is the timeout value expected.
			// ------------------------------------------------------------------------------------------
			else
			{
				DBG( "TimeoutDiscovery Thread - ---- TIMEOUT_RECEIVED : --- NOT --- SUCCEED with %d seconds ---- ", timeout );
			} // IF
		} while ( (timeout<=maxTimeout) && (timeoutReceptionState == TIMEOUT_RECEIVED) );
		
                // Compute the timeout value
		
                // --------------------------------------------------------------------------------------
                // Feed back the timeout found to the main stun client pthread (1)
                // --------------------------------------------------------------------------------------
                DBG("TIMEOUT DISCOVERED - Exact Value = %d seconds", timeout);
		g_timeout = DM_STUN_ComputeGlobalTimeout(step, minTimeout, maxTimeout, timeout);
                DBG("TIMEOUT DISCOVERED - Used Value = %d seconds", g_timeout);
			
                // --------------------------------------------------------------------------------------
                // Will stop the pthread (2)
                // --------------------------------------------------------------------------------------
                stunThreadData->bExitWindow = false;

	} // IF
	
	// --------------------------------------------------------------------------------------
	// Initialize the threadId to its initial value
	// --------------------------------------------------------------------------------------
	DBG( "Timeout discovery thread has finish its job. Cleanning..." );
	DBG( "Try to lock the mutex" );
	pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	stunThreadData->timeoutThreadData.threadId = 0;
	stunThreadData->bThrirdThreadLaunched      = false;
	pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	DBG( "mutex unlocked" );
	DBG( "Timeout discovery thread says good bye!" );
	
	// --------------------------------------------------------------------------------------
	// Close the temporaly FD used for the socket
	// --------------------------------------------------------------------------------------
	DBG( "Closing the FD (%d) used for the socket ", fdTimeout );
	close( fdTimeout );
	
	// --------------------------------------------------------------------------------------
	// Exit the pthread (3)
	// --------------------------------------------------------------------------------------
	DBG( "<-- DM_STUN_timeoutDiscoveryThread " );
	pthread_exit( 0 );
} // DM_STUN_timeoutDiscoveryThread

/**
 * @brief send STUN management
 *
 * @param stunThreadData
 *
 * @remarks variables used : 
 * @remarks fdClient (R)
 * @remarks lastStunMessageReceivedType (R)
 * @remarks stunServer (R)
 *
 * @return return TRUE if successfull, otherwise FALSE
 *
 */
bool
DM_STUN_sendManagement(stunThreadStruct* stunThreadData)
{
	bool	bRet = false;
 
	DBG( "Send STUN Message to the Server" );
	DBG( "lastStunMessageReceivedType = '%s' ", g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
	
	// --------------------------------------------------------------
	// If the previous message received was a BINDING ERROR RESPONSE
	// --------------------------------------------------------------
	if ( BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType )
	{
		// --------------------------------------------------------------
		// A Binding error response was received. A message integrity will be sent
		// second message send to the ACS to inform that the @ IP has changed
		// with this message, the modification is validated
		// --------------------------------------------------------------
		DBG( "Sending a BINDING_REQUEST message including an integrity attribute..." );
		bRet = DM_STUN_sendMessage( stunThreadData->fdClient,
					    &(stunThreadData->stunServer),
					    MSG_INTEGRITY,
					    &(stunThreadData->username),
					    &(stunThreadData->password), 0, 0 );
		stunThreadData->lastStunMessageReceivedType = NONE;
	} // IF
	
	// --------------------------------------------------------------
	// Or a normal BINDING RESPONSE
	// --------------------------------------------------------------
	else
	{
		// --------------------------------------------------------------
		// Normal case : binding request send
		// --------------------------------------------------------------
		DBG( "Sending a normal BINDING_REQUEST message..." );
		bRet = DM_STUN_sendMessage( stunThreadData->fdClient,
			  		    &(stunThreadData->stunServer),
			 		    BINDING_REQUEST,
					    &(stunThreadData->username),
					    NULL, 0, 0 );
		if ( NOTIFIED == stunThreadData->stunWanIpNotification )
		{
			// --------------------------------------------------------------
			// When the stunWanIpNotifation is NOTIFIED, and a binding is sended
			// this means that the ACS is aware of the wan IP modification with the STUN
			// message way or with the TR-069 way.
			//
			// So, the timeout discovery thread can be launched now. Setting the stunWanIpNotification
			// to VALIDATED, indicates that the timeoutDiscoveryThread can be launched...
			//
			// This is done only when the notification is done because the timeout discovery
			// thread will block the STUN emission...
			// --------------------------------------------------------------
			DBG( "Change the stunWanIpNotification flag to VALIDATED " );
			stunThreadData->stunWanIpNotification = VALIDATED;
		} // IF
	} // IF

	// --------------------------------------------------------------
	// Check the result of the previous UDP message sending
	// --------------------------------------------------------------
	if ( FALSE == bRet )
	{
		if ( BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType )
		{
			EXEC_ERROR( "Sending a BINDING ERROR RESPONSE message : NOK " );
		} else {
			EXEC_ERROR( "Sending a BINDING RESPONSE message       : NOK " );
		} // IF
	} else {
		if ( BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType )
		{
			DBG( "Sending a BINDING ERROR RESPONSE message : OK " );
		} else {
			DBG( "Sending a BINDING RESPONSE message       : OK " );
		} // IF
	} // IF
	
	return( bRet );
} // DM_STUN_sendManagement

/**
 * @brief read STUN and UDP messages
 *
 * @param stunThreadData
 * 
 * @remarks variables used : 
 * 
 * @remarks natReceivedPort (R/W)
 * @remarks stunClientPort (R)
 * @remarks fdClient (R)
 * @remarks lanIp (R)
 * @remarks wanIp (R/W)
 * @remarks timeStamp (R/W)
 * @remarks messageId (R/W)
 * @remarks lastStunMessageReceivedType (R/W)
 * @remarks natDetected (R/W)
 * @remarks countStopTimeoutDiscoveryThread (R/W)
 * @remarks stunServer
 *
 * @return nothing
 *
 */
void
DM_STUN_readManagement(stunThreadStruct* stunThreadData)
{
	int      elapsedTime    = 0;			// Time in seconds
	int      timeBeginLoop  = 0;			// Time in seconds
	int      timeout        = 0;			// Time in seconds
	struct   timezone tz;	
	struct   timeval  tv; 
	UInt8    natDetected;						// 
	UInt8	   messageType                = NONE;			// 
	UInt8	   stunMessageReceptionState  = ANY_RECEIVED;		// 
	UInt16   natPortReceived            = UNKNOWN;		// 
	UInt32   wanIpReceived              = UNKNOWN;		// 
	UInt64   timeStamp                  = 0;			// Unix timestamp (time since 1970 or epoch)
	UInt64   messageId                  = 0;			// Unique string used for define a session
	char     udpConnectionRequestAddress[UDP_CONNECTION_REQUEST_ADDRESS_SIZE];
	char     natDetectedStr[2];
	struct   in_addr		tmp;
	bool     timeoutDiscoveryMessageReceive	= false;
	bool     timeoutReached                 = false;
	
	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));

	pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );

	timeout = stunThreadData->nTimeout;
	DBG( "Current timeout = %d ", timeout );

	pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );

	
	DBG( "----- BEFORE THE MAIN LOOP of the DM_STUN_readManagement function ----" );
	do
	{
		DBG( "----- BEGIN OF THE MAIN LOOP of the DM_STUN_readManagement function ----" );
		gettimeofday( &tv, &tz );
		timeBeginLoop = tv.tv_sec;
		DBG( "-> Before to read the message... " );
		messageType   = DM_STUN_readMessage( stunThreadData, (timeout-elapsedTime), &wanIpReceived, &natPortReceived, &messageId, &timeStamp );
		DBG( "<- After to read the message. " );
		
		// --------------------------------------------------------------
		// all the UDP request will be considered. Its allows to deal with all this "important" requests...
		// all the TIMEOUT_RESPONSE will be considered.
		// On the other hand, only the first STUN message will be considered. The others will be ignored.
		// --------------------------------------------------------------
		DBG( "Type of the message received = %d ", (int)messageType );
		switch ( messageType )
		{
			// --------------------------------------------------------------
			//
			// --------------------------------------------------------------
			case NONE:
				INFO( "NONE message received : timeout expiration." );
				timeoutReached = true;
				break;
			
			// --------------------------------------------------------------
			// Receiving an unknown message
			// --------------------------------------------------------------
			case MESSAGE_UNKNOWN:
				INFO( "MESSAGE_UNKNWON message received." );
				break;
			
			// --------------------------------------------------------------
			// Receiving an UDP connection Request
			// --------------------------------------------------------------
			case UDP_REQUEST:
				DBG( "UDP_REQUEST message received            " );
				DBG( "Let's check if the message is correct..." );
				if ( timeStamp >= stunThreadData->timeStamp )
				{
					stunThreadData->timeStamp = timeStamp;
					if ( stunThreadData->messageId != messageId )
					{
						stunThreadData->messageId = messageId;
						DBG( "New timeStamp (%lld) and messageId (%lld) ", timeStamp, messageId );
						if ( FALSE == maxConnectionPerPeriodReached( stunThreadData->connectionFreqArray ) )
						{
							newConnectionRequestAllowed( stunThreadData->connectionFreqArray );
							
							// --------------------------------------------------------------
							// Ask to DM_Engine to open the session
							// --------------------------------------------------------------
							if ( 0 == DM_ENG_RequestConnection( DM_ENG_EntityType_SYSTEM ) )
							{
                						INFO( "The ACS is aware of the UDP REQUEST with message Id=%lld and time stamp=%lld", messageId, timeStamp );
								
								// --------------------------------------------------------------
								// Only one connection a time
								// --------------------------------------------------------------
								stunMessageReceptionState = ALREADY_RECEIVED;
							} else {
								EXEC_ERROR( "The ACS hasn't received the UDP REQUEST" );
							} // IF
						} else {
							INFO( "The UDP connection request was cancelled : too many connections" );
						} // IF
					} // IF
				}
				break;
			
			// --------------------------------------------------------------
			// Other cases
			// --------------------------------------------------------------
			default:
				// --------------------------------------------------------------
				// Check if we are in the (2) / (3) pthread
				// Condition 1 : bThrirdThreadLaunched  = 1             -> the discovery pthread have been launched
				// Condition 2 : stunThreadData->wanIp != wanIpReceived -> the wan IP have been discovered !!
				// --------------------------------------------------------------
				DBG( "bThrirdThreadLaunched=%d (true or false) ", stunThreadData->bThrirdThreadLaunched );
				if   (stunThreadData->wanIp != wanIpReceived) { DBG( "stunThreadData->wanIp != wanIpReceived" ); }
				else					      { DBG( "stunThreadData->wanIp  = wanIpReceived" ); }
				if ( (true == stunThreadData->bThrirdThreadLaunched) && (stunThreadData->wanIp != wanIpReceived) )
				{

					pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );
					stunThreadData->lastStunMessageReceivedType = BINDING_RESPONSE;
					timeoutDiscoveryMessageReceive              = true;

					pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );

					return;
				} // IF
				
				// --------------------------------------------------------------
				// If the pthread (3) is not started yet AND if the external WanIP address haven't been received,
				// don't force to exit this read funtion.
				// --------------------------------------------------------------
				else
				{
					DBG( "The timeout pthread is not launched AND the WanIP have not been found yet. Continuing..." );
				} // IF
				
				// --------------------------------------------------------------
				// All the STUN message are managed here
				// Except the TIMEOUT_RESPONSE or a BindingErrorResponse
				// --------------------------------------------------------------
				DBG( "It's probably a STUN MESSAGE. Let's check..." );
				if ( stunThreadData->wanIp != wanIpReceived )
				{
					DBG( "NEW WAN IP : first message received with this IP!!!" );
					
					// --------------------------------------------------------------
					// It's necessary a BINDING_REPONSE OR a MSG_INTEGRITY_RESPONSE message
					// --------------------------------------------------------------
					if ( (BINDING_RESPONSE != messageType) && (MSG_INTEGRITY_RESPONSE != messageType) )
					{
						EXEC_ERROR( "A binding response is assumed here. The algorithm MUST be modified" );
						EXEC_ERROR( "Maybe a BindingErrorResponse..." );
						ASSERT( BINDING_RESPONSE != messageType );
					} // IF
					stunThreadData->wanIp                   = wanIpReceived;
					stunThreadData->natReceivedPort.binding = natPortReceived;
					
					// --------------------------------------------------------------
					// Check if we can notificate in thi scope ?
					// Data to update : STUN_UDP_CONNEXION_REQUEST_ADDRESS (IP_ADDR:PORT)
					// Notificate the DM_Engine if required AND if the value have changed.
					// --------------------------------------------------------------
					DBG( "bCanNotificateWanIP=%d (true or false) ", stunThreadData->bCanNotificateWanIP );
					if ( true == stunThreadData->bCanNotificateWanIP )
					{
						DBG( "Notificate the STUN_UDP_CONNEXION_REQUEST_ADDRESS..." );
						tmp.s_addr = wanIpReceived;
						snprintf( udpConnectionRequestAddress, 
						  	UDP_CONNECTION_REQUEST_ADDRESS_SIZE,
						  	"%s:%u",
						  	inet_ntoa(tmp),
							stunThreadData->stunClientPort.binding );
						DM_STUN_updateParameterValue( STUN_UDP_CONNEXION_REQUEST_ADDRESS, DM_ENG_ParameterType_STRING, udpConnectionRequestAddress );
					} else {
						DBG( "No need to notificate the STUN_UDP_CONNEXION_REQUEST_ADDRESS..." );
					} // IF
					
					// --------------------------------------------------------------
					// First message send to the STUN server to inform that the @ IP has changed
					// this message MUST be send several times to prevent the UDP package loss
					// --------------------------------------------------------------
					DM_STUN_sendMessage( stunThreadData->fdClient,
							     &(stunThreadData->stunServer),
							     BINDING_CHANGE,
							     &(stunThreadData->username),
							     NULL, 0, 0 );
					
					// --------------------------------------------------------------
					// Indicates that the BINDING_CHANGE has been sended.
					// --------------------------------------------------------------
					if ( BINDING_RESPONSE == messageType ) {
						WARN( "Changed the stunWanIpNotification flag to NOTIFIED " );
						stunThreadData->stunWanIpNotification = NOTIFIED;
					} else if ( MSG_INTEGRITY_RESPONSE == messageType ) {
						WARN( "Changed the stunWanIpNotification flag to VALIDATED " );
						stunThreadData->stunWanIpNotification = VALIDATED;
					} // IF
					
					// --------------------------------------------------------------
					//                           NAT Detection
					// --------------------------------------------------------------
					// Notificate the DM_Engine if required AND if the value have changed.
					// Verification : if      (LanIP == WanIP) Then NAT = 0
					//		  else if (LanIP != WanIP) Then NAT = 1
					// --------------------------------------------------------------
					DBG( "WanIp= '%s' - 0x%X", inet_ntoa(tmp), stunThreadData->wanIp );
					tmp.s_addr = stunThreadData->lanIp;
					DBG( "LanIp= '%s' - 0x%X", inet_ntoa(tmp), (stunThreadData->lanIp) );
					if ( stunThreadData->wanIp != stunThreadData->lanIp )
					{
						INFO( "A NAT is detected" );
						natDetected = TRUE;
					} else {
						natDetected = FALSE;
						INFO( "A NAT is not detected" );
					} // IF
					DBG( "bCanNotificateWanIP=%d (true or false) ", stunThreadData->bCanNotificateWanIP );
					if ( (true == stunThreadData->bCanNotificateWanIP) && (stunThreadData->natDetected != natDetected) )
					{
						DBG( "Notificate the STUN_NAT_DETECTED..." );
						stunThreadData->natDetected = natDetected;
						snprintf( natDetectedStr, 2, "%u", stunThreadData->natDetected );
						DM_STUN_updateParameterValue( STUN_NAT_DETECTED, DM_ENG_ParameterType_BOOLEAN, natDetectedStr );
					} else {
						DBG( "No need to notificate the STUN_NAT_DETECTED..." );
					} // IF
				} // IF
				

				break;
		} // SWITCH
		
		if ( ALREADY_RECEIVED != stunMessageReceptionState )
		{
		  if ( (true == timeoutReached) && (BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType) ) {
		    // Nothing to do...
		  } else {
			  stunThreadData->lastStunMessageReceivedType = messageType;
			  DBG( "%s message is saved.", g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
			}
			if ( JUST_RECEIVED == stunMessageReceptionState )
			{
				stunMessageReceptionState = ALREADY_RECEIVED;
			} // IF
		} else {
			DBG( "Last message ignored: %s. A previous message already received: %s.",
				g_DBG_nameTypeMessage[messageType],
				g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
		} // IF
		
		// Execution time measurement
		gettimeofday( &tv, &tz );
		elapsedTime += tv.tv_sec-timeBeginLoop;
		DBG( "Elapsed time= %ds with timeout= %ds", elapsedTime, timeout );
		DBG( "----- END OF THE MAIN LOOP of the DM_STUN_readManagement function ----" );
	} while ( elapsedTime<timeout );
	

	
	if ( ANY_RECEIVED == stunMessageReceptionState )
	{
	  	if ( (true == timeoutReached) && (BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType) ) {
		    // Do nothing
		} else {
		  //any STUN message received : only timeout or none or udp message is received during this timeout session
		  stunThreadData->lastStunMessageReceivedType = messageType;
		}
	} // IF
	DBG(  "Read end. with elapsed time =%ds with timeout=%ds", elapsedTime, timeout );
	INFO( "%s message is considered as lastMessageReceived.", g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
} // DM_STUN_readManagement




/**
 * @brief read STUN and UDP messages
 *
 * @param stunThreadData
 * 
 * @remarks variables used : 
 * 
 * @remarks natReceivedPort (R/W)
 * @remarks stunClientPort (R)
 * @remarks fdClient (R)
 * @remarks lanIp (R)
 * @remarks wanIp (R/W)
 * @remarks timeStamp (R/W)
 * @remarks messageId (R/W)
 * @remarks lastStunMessageReceivedType (R/W)
 * @remarks natDetected (R/W)
 * @remarks countStopTimeoutDiscoveryThread (R/W)
 * @remarks stunServer
 *
 * @return nothing
 *
 */
void
DM_STUN_readManagementTimeout(stunThreadStruct* stunThreadData)
{
	int      elapsedTime    = 0;			// Time in seconds
	int      timeBeginLoop  = 0;			// Time in seconds
	int      timeout        = 0;			// Time in seconds
	struct   timezone tz;	
	struct   timeval  tv; 
	UInt8    natDetected;						// 
	UInt8	   messageType                = NONE;			// 
	UInt8	   stunMessageReceptionState  = ANY_RECEIVED;		// 
	UInt16   natPortReceived            = UNKNOWN;		// 
	UInt32   wanIpReceived              = UNKNOWN;		// 
	UInt64   timeStamp                  = 0;			// Unix timestamp (time since 1970 or epoch)
	UInt64   messageId                  = 0;			// Unique string used for define a session
	char     udpConnectionRequestAddress[UDP_CONNECTION_REQUEST_ADDRESS_SIZE];
	char     natDetectedStr[2];
	struct   in_addr		tmp;
	bool     timeoutDiscoveryMessageReceive	= false;
	bool     timeoutReached                 = false;

	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));
		
	pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );

	timeout = stunThreadData->nTimeout;
	DBG( "Current timeout = %d ", timeout );

	pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );

	
	DBG( "----- BEFORE THE MAIN LOOP of the DM_STUN_readManagement function ----" );
	do
	{
		DBG( "----- BEGIN OF THE MAIN LOOP of the DM_STUN_readManagement function ----" );
		gettimeofday( &tv, &tz );
		timeBeginLoop = tv.tv_sec;
		DBG( "-> Before to read the message... " );
		messageType   = DM_STUN_readMessage( stunThreadData, (timeout-elapsedTime), &wanIpReceived, &natPortReceived, &messageId, &timeStamp );
		DBG( "<- After to read the message. " );
		
		// --------------------------------------------------------------
		// all the UDP request will be considered. Its allows to deal with all this "important" requests...
		// all the TIMEOUT_RESPONSE will be considered.
		// On the other hand, only the first STUN message will be considered. The others will be ignored.
		// --------------------------------------------------------------
		DBG( "Type of the message received = %d ", (int)messageType );
		switch ( messageType )
		{
			// --------------------------------------------------------------
			//
			// --------------------------------------------------------------
			case NONE:
				INFO( "NONE message received : timeout expiration." );
				timeoutReached = true;
				break;
		
			// --------------------------------------------------------------
			// Other cases
			// --------------------------------------------------------------
			default:
				// --------------------------------------------------------------
				// Check if we are in the (2) / (3) pthread
				// If so we don't want to force to set the TimeoutState flag to TIMEOUT_RECEIVED
				// Condition 1 : bThrirdThreadLaunched  = 1             -> the discovery pthread have been launched
				// Condition 2 : stunThreadData->wanIp != wanIpReceived -> the wan IP have been discovered !!
				// --------------------------------------------------------------
				
				// A least one message received - Set TimeoutState to TIMEOUT_RECEIVED
                                pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );
                                DBG( "Set the TimeoutState to TIMEOUT_RECEIVED (Current Timeout = %d)", timeout );
				stunThreadData->TimeoutState = TIMEOUT_RECEIVED;
				timeoutDiscoveryMessageReceive = true;
				pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );			
				
				DBG( "bThrirdThreadLaunched=%d (true or false) ", stunThreadData->bThrirdThreadLaunched );
				if   (stunThreadData->wanIp != wanIpReceived) { DBG( "stunThreadData->wanIp != wanIpReceived" ); }
				else					      { DBG( "stunThreadData->wanIp  = wanIpReceived" ); }
				if ( (true == stunThreadData->bThrirdThreadLaunched) && (stunThreadData->wanIp != wanIpReceived) )
				{

					pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );

					stunThreadData->lastStunMessageReceivedType = BINDING_RESPONSE;
					timeoutDiscoveryMessageReceive              = true;

					pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );

					return;
				} // IF
				
				// --------------------------------------------------------------
				// If the pthread (3) is not started yet AND if the external WanIP address haven't been received,
				// don't force to exit this read funtion.
				// --------------------------------------------------------------
				else
				{
					DBG( "The timeout pthread is not launched AND the WanIP have not been found yet. Continuing..." );
				} // IF
				
				// --------------------------------------------------------------
				// All the STUN message are managed here
				// Except the TIMEOUT_RESPONSE or a BindingErrorResponse
				// --------------------------------------------------------------
				DBG( "It's probably a STUN MESSAGE. Let's check..." );
				DBG( "TimeoutState=%d (TIMEOUT_SEND=%d,TIMEOUT_RECEIVED=%d,TIMEOUT_EXPIRATION=%d,TIMEOUT_NONE=%d)", stunThreadData->TimeoutState, TIMEOUT_SEND, TIMEOUT_RECEIVED, TIMEOUT_EXPIRATION, TIMEOUT_NONE );
				if ( stunThreadData->wanIp != wanIpReceived )
				{
					DBG( "NEW WAN IP : first message received with this IP!!!" );
					
					// --------------------------------------------------------------
					// It's necessary a BINDING_REPONSE OR a MSG_INTEGRITY_RESPONSE message
					// --------------------------------------------------------------
					if ( (BINDING_RESPONSE != messageType) && (MSG_INTEGRITY_RESPONSE != messageType) )
					{
						EXEC_ERROR( "A binding response is assumed here. The algorithm MUST be modified" );
						EXEC_ERROR( "Maybe a BindingErrorResponse..." );
						ASSERT( BINDING_RESPONSE != messageType );
					} // IF
					stunThreadData->wanIp                   = wanIpReceived;
					stunThreadData->natReceivedPort.binding = natPortReceived;
					
					// --------------------------------------------------------------
					// Check if we can notificate in thi scope ?
					// Data to update : STUN_UDP_CONNEXION_REQUEST_ADDRESS (IP_ADDR:PORT)
					// Notificate the DM_Engine if required AND if the value have changed.
					// --------------------------------------------------------------
					DBG( "bCanNotificateWanIP=%d (true or false) ", stunThreadData->bCanNotificateWanIP );
					if ( true == stunThreadData->bCanNotificateWanIP )
					{
						DBG( "Notificate the STUN_UDP_CONNEXION_REQUEST_ADDRESS..." );
						tmp.s_addr = wanIpReceived;
						snprintf( udpConnectionRequestAddress, 
						  	UDP_CONNECTION_REQUEST_ADDRESS_SIZE,
						  	"%s:%u",
						  	inet_ntoa(tmp),
							stunThreadData->stunClientPort.binding );
						DM_STUN_updateParameterValue( STUN_UDP_CONNEXION_REQUEST_ADDRESS, DM_ENG_ParameterType_STRING, udpConnectionRequestAddress );
					} else {
						DBG( "No need to notificate the STUN_UDP_CONNEXION_REQUEST_ADDRESS..." );
					} // IF
					
					// --------------------------------------------------------------
					// First message send to the STUN server to inform that the @ IP has changed
					// this message MUST be send several times to prevent the UDP package loss
					// --------------------------------------------------------------
					DM_STUN_sendMessage( stunThreadData->fdClient,
							     &(stunThreadData->stunServer),
							     BINDING_CHANGE,
							     &(stunThreadData->username),
							     NULL, 0, 0 );
					
					// --------------------------------------------------------------
					// Indicates that the BINDING_CHANGE has been sended.
					// --------------------------------------------------------------
					if ( BINDING_RESPONSE == messageType ) {
						WARN( "Changed the stunWanIpNotification flag to NOTIFIED " );
						stunThreadData->stunWanIpNotification = NOTIFIED;
					} else if ( MSG_INTEGRITY_RESPONSE == messageType ) {
						WARN( "Changed the stunWanIpNotification flag to VALIDATED " );
						stunThreadData->stunWanIpNotification = VALIDATED;
					} // IF
					
					// --------------------------------------------------------------
					//                           NAT Detection
					// --------------------------------------------------------------
					// Notificate the DM_Engine if required AND if the value have changed.
					// Verification : if      (LanIP == WanIP) Then NAT = 0
					//		  else if (LanIP != WanIP) Then NAT = 1
					// --------------------------------------------------------------
					DBG( "WanIp= '%s' - 0x%X", inet_ntoa(tmp), stunThreadData->wanIp );
					tmp.s_addr = stunThreadData->lanIp;
					DBG( "LanIp= '%s' - 0x%X", inet_ntoa(tmp), (stunThreadData->lanIp) );
					if ( stunThreadData->wanIp != stunThreadData->lanIp )
					{
						INFO( "A NAT is detected" );
						natDetected = TRUE;
					} else {
						natDetected = FALSE;
						INFO( "A NAT is not detected" );
					} // IF
					DBG( "bCanNotificateWanIP=%d (true or false) ", stunThreadData->bCanNotificateWanIP );
					if ( (true == stunThreadData->bCanNotificateWanIP) && (stunThreadData->natDetected != natDetected) )
					{
						DBG( "Notificate the STUN_NAT_DETECTED..." );
						stunThreadData->natDetected = natDetected;
						snprintf( natDetectedStr, 2, "%u", stunThreadData->natDetected );
						DM_STUN_updateParameterValue( STUN_NAT_DETECTED, DM_ENG_ParameterType_BOOLEAN, natDetectedStr );
					} else {
						DBG( "No need to notificate the STUN_NAT_DETECTED..." );
					} // IF
				} // IF
				
				break;
		} // SWITCH
		
		if ( ALREADY_RECEIVED != stunMessageReceptionState )
		{
		  if ( (true == timeoutReached) && (BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType) ) {
		    // Nothing to do...
		  } else {
			  stunThreadData->lastStunMessageReceivedType = messageType;
			  DBG( "%s message is saved.", g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
			}
			if ( JUST_RECEIVED == stunMessageReceptionState )
			{
				stunMessageReceptionState = ALREADY_RECEIVED;
			} // IF
		} else {
			DBG( "Last message ignored: %s. A previous message already received: %s.",
				g_DBG_nameTypeMessage[messageType],
				g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
		} // IF
		
		// Execution time measurement
		gettimeofday( &tv, &tz );
		elapsedTime += tv.tv_sec-timeBeginLoop;
		DBG( "Elapsed time= %ds with timeout= %ds", elapsedTime, timeout );
		DBG( "----- END OF THE MAIN LOOP of the DM_STUN_readManagement function ----" );
	} while ( elapsedTime<timeout );
	
	// ----------------------------------
	// Check for the Timeout : no Bind response received
	// ----------------------------------
	DBG( "Try to lock the mutex " );
	pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	DBG( "TimeoutState=%d (TIMEOUT_SEND=%d,TIMEOUT_RECEIVED=%d,TIMEOUT_EXPIRATION=%d,TIMEOUT_NONE=%d) ", stunThreadData->TimeoutState, TIMEOUT_SEND, TIMEOUT_RECEIVED, TIMEOUT_EXPIRATION, TIMEOUT_NONE );
	DBG( "timeoutDiscoveryMessageReceive=%d (true or false) ", timeoutDiscoveryMessageReceive );
	DBG( "Test if we have TimeoutState=TIMEOUT_SEND AND timeoutDiscoveryMessageReceive=0 -> TimeoutState=TIMEOUT_EXPIRATION " );
	//if ( (TIMEOUT_SEND == stunThreadData->TimeoutState) && (false == timeoutDiscoveryMessageReceive) )
	if ( ((TIMEOUT_SEND == stunThreadData->TimeoutState) && (false == timeoutDiscoveryMessageReceive))
	  || ((TIMEOUT_NONE == stunThreadData->TimeoutState) && (false == timeoutDiscoveryMessageReceive)) )
	//if ( false == timeoutDiscoveryMessageReceive )
	{
		DBG( "No message received. NAT Timeout detection (TIMEOUT_EXPIRATION)             " );
		DBG( "Timeout Binding Response NOT received Set g_timeoutState=TIMEOUT_EXPIRATION " );
		DBG(" Set TimeoutState to TIMEOUT_EXPIRATION");
		stunThreadData->TimeoutState            = TIMEOUT_EXPIRATION;
		stunThreadData->natReceivedPort.timeout = UNKNOWN;
	} // IF
	pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	DBG( "Unlock the mutex " );
	
	if ( ANY_RECEIVED == stunMessageReceptionState )
	{
	  	if ( (true == timeoutReached) && (BINDING_ERROR_RESPONSE == stunThreadData->lastStunMessageReceivedType) ) {
		    // Do nothing
		} else {
		  //any STUN message received : only timeout or none or udp message is received during this timeout session
		  stunThreadData->lastStunMessageReceivedType = messageType;
		}
	} // IF
	DBG(  "Read end. with elapsed time =%ds with timeout=%ds", elapsedTime, timeout );
	INFO( "%s message is considered as lastMessageReceived.", g_DBG_nameTypeMessage[stunThreadData->lastStunMessageReceivedType] );
} // DM_STUN_readManagement



/**
 * @brief send the message asked
 *
 * @param Socket (R)
 * @param stunServer (R)
 * @param messageType (R)
 * @param username (R)
 * @param password (R)         : only for the MSG_INTEGRITY message
 * @param wanIp (R)            : only for the TIMEOUT_DISCOVERY message
 * @param receptionPortWan (R) : only for the TIMEOUT_DISCOVERY message
 *
 * @return TRUE if sucessfull otherwise FALSE
 *
 */
bool
DM_STUN_sendMessage(Socket fdSocket, StunAddress4* stunServer, UInt8 messageType, StunAtrString* username, StunAtrString* password, UInt32 wanIp, UInt16 receptionPortWan)
{
	char		msg[STUN_MAX_MESSAGE_SIZE];
	int		msgLen			= STUN_MAX_MESSAGE_SIZE;
	bool		err			= FALSE;
	int		i			= 0;
	int		nbTransmitMessage	= 1;
	StunMessage	request;
	StunAtrString	hmacKey;
	
	// Configure the common message content
	memset( &request, 0x00, sizeof(StunMessage) );
	hmacKey.sizeValue      = 0;
	request.msgHdr.msgType = BindRequestMsg;
	for ( i=0 ; i<16 ; i=i+4 )
	{
		int r = stunRand ();
		request.msgHdr.id.octet[i+0] = r >> 0;
		request.msgHdr.id.octet[i+1] = r >> 8;
		request.msgHdr.id.octet[i+2] = r >> 16;
		request.msgHdr.id.octet[i+3] = r >> 24;
	} // FOR
	
	// ---------------------------------------------------------
	// Find the right message
	// ---------------------------------------------------------
	switch(messageType)
	{
		// ---------------------------------------------------------
		// BINDING_REQUEST message
		// ---------------------------------------------------------
		case BINDING_REQUEST:
			INFO( "Send a BINDING_REQUEST message" );
			if ( NULL == username )
			{
				EXEC_ERROR( "The username is null" );
				ASSERT( FALSE );
			} // IF
			request.hasConnectionRequestBinding        = true;
			strncpy( request.connectionRequestBinding.value, CONNECTION_REQUEST_BINDING_VALUE, STUN_MAX_STRING );
			request.connectionRequestBinding.sizeValue = strlen( CONNECTION_REQUEST_BINDING_VALUE );
			request.hasUsername                        = true;
			request.username                           = *username;
			break;
		
		// ---------------------------------------------------------
		// BINDING_CHANGE message
		// ---------------------------------------------------------
		case BINDING_CHANGE:
			INFO( "Send a BINDING_CHANGE message" );
			if ( NULL == username )
			{
				EXEC_ERROR( "The username is null" );
				ASSERT(FALSE);
			} // IF
			// This message MUST be send several times to prevent the UDP package loss
			nbTransmitMessage                          = BINDING_CHANGE_NB_TRANSMIT;
			request.hasConnectionRequestBinding        = true;
			strncpy( request.connectionRequestBinding.value, CONNECTION_REQUEST_BINDING_VALUE, STUN_MAX_STRING );
			request.connectionRequestBinding.sizeValue = strlen( CONNECTION_REQUEST_BINDING_VALUE );
			request.hasBindingChange                   = TRUE;
			request.hasUsername                        = true;
			request.username                           = *username;
			break;
			
		// ---------------------------------------------------------
		// MSG_INTEGRITY message
		// ---------------------------------------------------------
		case MSG_INTEGRITY:
			INFO( "Send a MSG_INTEGRITY message" );
			if ( NULL == username )
			{
				EXEC_ERROR( "The username is null" );
				ASSERT( FALSE );
			} // IF
			if ( NULL == password )
			{
				EXEC_ERROR( " The password is null" );
				ASSERT(FALSE);
			} // IF
			request.hasConnectionRequestBinding=true;
			strncpy( request.connectionRequestBinding.value, CONNECTION_REQUEST_BINDING_VALUE, STUN_MAX_STRING );
			request.connectionRequestBinding.sizeValue = strlen( CONNECTION_REQUEST_BINDING_VALUE );
			request.hasBindingChange                   = TRUE;
			request.hasUsername                        = true;
			request.username                           = *username;
			request.hasMessageIntegrity                = TRUE;
			hmacKey                                    = *password;
			break;
			
		// ---------------------------------------------------------
		// TIMEOUT_DISCOVERY message
		// ---------------------------------------------------------
		case TIMEOUT_DISCOVERY:
			INFO( "Send a TIMEOUT_DISCOVERY message" );
			request.hasResponseAddress        = TRUE;
			request.responseAddress.ipv4.addr = htonl( wanIp );
			request.responseAddress.ipv4.port = receptionPortWan;
			break;
	}
	
	msgLen = stunEncodeMessage(&request, msg, msgLen, &hmacKey);
	for( i=0 ; i<nbTransmitMessage ; i++ )
	{
		if ( true == sendMessage( fdSocket, msg, msgLen, stunServer->addr, stunServer->port ) )
		{
			DBG( "Message sended to the STUN server : OK (len= %d) ", msgLen );
			err = TRUE;
		} else {
			EXEC_ERROR("FAIL to send the STUN message");
		} // IF
	} // IF
	
	return( err );
} // DM_STUN_sendMessage

/**
 * @brief read a message from the Socket
 *
 * @param stunThreadData
 * @param timeoutAvailable (R)
 * @param wanIp (W)
 * @param port (W)
 * @param messageId (W)
 * @param timeStamp (W)
 * 
 * @remarks variables used in the stunThreadData :
 * @remarks fdSocket (R)
 * @remarks username (R)
 * @remarks password (R)
 *
 * @return the messageType:
 * @remarks NONE
 * @remarks BINDING_RESPONSE
 * @remarks TIMEOUT_RESPONSE
 * @remarks MSG_INTEGRITY_RESPONSE
 * @remarks BINDING_ERROR_RESPONSE
 * @remarks UDP_REQUEST
 * @remarks MESSAGE_UNKNOWN
 *
 */
UInt8
DM_STUN_readMessage(stunThreadStruct* stunThreadData,int timeoutAvailable, UInt32* wanIp, UInt16* port, UInt64* messageId, UInt64* timeStamp)
{
	char			msg[STUN_MAX_MESSAGE_SIZE];
	int			msgLen;
	struct timeval		timeout;
	struct in_addr		tmp;
	fd_set			set;
	StunAddress4		answerFrom;
	StunMessage		answerMessage;
	UInt8			messageType= MESSAGE_UNKNOWN;
	char			username[STUN_MAX_STRING];
	char			cnonce[STUN_MAX_STRING];
	StunAtrIntegrity	signatureReceived;
	
	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));
	
	// ----------------------------------------------------------------
	// Initialisation
	// ----------------------------------------------------------------
	memset( msg, 0x00, STUN_MAX_MESSAGE_SIZE );
	msgLen          = STUN_MAX_MESSAGE_SIZE;
	timeout.tv_sec  = timeoutAvailable;
	timeout.tv_usec = 0;
	FD_ZERO( &set );
	FD_SET( stunThreadData->fdClient, &set );
	
	// ----------------------------------------------------------------
	// Wait for an UDP message from the STUN server
	// ----------------------------------------------------------------
	if ( !((select (stunThreadData->fdClient + 1, &set, NULL, NULL, &timeout) > 0) && (FD_ISSET (stunThreadData->fdClient, &set))) )
	{
		messageType = NONE;
		DBG( "SET the messageType flag with TIMEOUT_RESPONSE (%d) ", NONE );
		DBG( "Timeout expiration. No message received             "       );
	} else {
		if ( false == getMessage(stunThreadData->fdClient, msg, &msgLen, &answerFrom.addr, &answerFrom.port) )
		{
			messageType = MESSAGE_UNKNOWN;
			DBG( "SET the messageType flag with TIMEOUT_RESPONSE (%d) ", MESSAGE_UNKNOWN );
			DBG( "Unknown message received                            "                  );
		} else {
			DBG ( "Message received : OK (len= %d) ", msgLen );
			memset( &answerMessage, 0x00, sizeof(StunMessage) );
			DBG( "Before the stunParseMessage() function..." );
			if ( 0 != stunParseMessage(msg, msgLen, &answerMessage) )
			{
				DBG( "After the stunParseMessage() function..." );
				DBG( "Parsing STUN message : OK " );
				DBG( "answerMessage.hasMappedAddress=%d ", answerMessage.hasMappedAddress  );
				DBG( "answerMessage.hasReflectedFrom=%d ", answerMessage.hasReflectedFrom  );
				DBG( "answerMessage.hasSourceAddress=%d ", answerMessage.hasSourceAddress  );
				DBG( "answerMessage.hasChangedAddress%d ", answerMessage.hasChangedAddress );
				if ( (answerMessage.hasMappedAddress) && (answerMessage.hasSourceAddress) && (answerMessage.hasChangedAddress) )
				{
					if ( answerMessage.hasReflectedFrom )
					{
						DBG( "answerMessage.hasReflectedFrom is SET" );
						DBG( "SET the messageType flag with TIMEOUT_RESPONSE (%d) ", TIMEOUT_RESPONSE );
						messageType = TIMEOUT_RESPONSE;
						INFO( "TIMEOUT_RESPONSE received" );
					} else if ( answerMessage.hasMessageIntegrity ) {
						DBG( "answerMessage.hasReflectedFrom is not SET" );
						INFO( "MSG_INTEGRITY_RESPONSE received" );
						
						// ----------------------------------------------------------------
						// Let's check if the hmac is ok
						// ----------------------------------------------------------------
						if ( DM_STUN_isHmacIdentical( &(answerMessage.messageIntegrity),
									      msg,
									      (msgLen-sizeof(StunAtrIntegrity)-2*sizeof(UInt16)), &(stunThreadData->password) ) )
						{
							DBG( "SET the messageType flag with MSG_INTEGRITY_RESPONSE (%d) ", MSG_INTEGRITY_RESPONSE );
							messageType = MSG_INTEGRITY_RESPONSE;
						} else {
							DBG( "SET the messageType flag with MSG_INTEGRITY_RESPONSE (%d) ", MSG_INTEGRITY_RESPONSE );
							//messageType = MESSAGE_UNKNOWN;
							// TODO : force to have a good validation for the bindResponse message received!!
							messageType = MSG_INTEGRITY_RESPONSE;
						} // IF
					} else {
						messageType = BINDING_RESPONSE;
						DBG( "SET the messageType flag with BINDING_RESPONSE (%d) ", BINDING_RESPONSE );
						INFO( "BINDING_RESPONSE received" );
					} // IF
				} else if ( answerMessage.hasErrorCode )
				{
					DBG( "SET the messageType flag with BINDING_ERROR_RESPONSE (%d) ", BINDING_ERROR_RESPONSE );
					messageType = BINDING_ERROR_RESPONSE;
					INFO( "BINDING_ERROR_RESPONSE received" );
				} else {
					EXEC_ERROR( "Unknown STUN message type. probably an ALGORITHM issue" );
					ASSERT( FALSE );
				} // IF
				
				// ----------------------------------------------------------------
				// Extract the NAT ip and port number in host byte order needed for the dm_lib_udp
				// ----------------------------------------------------------------
				char* tmpAddrStr = NULL;
				tmp.s_addr = answerFrom.addr;
				tmpAddrStr = inet_ntoa( tmp );
				DBG( "AnswerFrom.addr=%s:%u ", inet_ntoa(tmp), answerFrom.port );
				tmp.s_addr=answerMessage.mappedAddress.ipv4.addr;
				tmpAddrStr = inet_ntoa( tmp );
				DBG( "AnswerMessage.mappedAddress.ipv4.addr=%s:%u", inet_ntoa(tmp), answerMessage.mappedAddress.ipv4.port );
				if ( answerMessage.hasMappedAddress )
				{
					*wanIp = ntohl( answerMessage.mappedAddress.ipv4.addr );
					*port  = answerMessage.mappedAddress.ipv4.port;
				} // IF
			} else {
				DBG( "UDP_REQUEST message received" );
				// ----------------------------------------------------------------
        			// Request example:
				// ------------------------------------------------------------------
				// GET http://10.1.1.1:8080?ts=1120673700&id=1234&un=CPE57689&cn=XTGRWIPC6D3IPXS3&sig=3545F7B5820D76A3DF45A3A509DA8D8C38F13512 HTTP/1.1
				// GET ?ts=1237219762255&id=1237219762255&un=openstb&cn=1237219762255&sig=4407D5B2A2B8286E0A635B8D2BCEA6BA8FE9E3AF HTTP/1.1
				// Host: 192.168.1.3:23490
				// Content-Length: 0
				// ----------------------------------------------------------------
				DBG( "Test if it is an UDP Connection request message\n%s", msg );
				if ( (strstr(msg,"GET") == msg) && (strstr(msg,"HTTP/1.1") != NULL ) )
				{
					messageType = UDP_REQUEST;
					DBG( "UDP_REQUEST received. the message is HTTP/1.1 compliant and the Method GET is used" );
					
					// ----------------------------------------------------------------
					// Extract the parameter from the querystring
					// ----------------------------------------------------------------
					if ( FALSE == DM_STUN_extractUdpParameters( msg, timeStamp, messageId, username, cnonce, signatureReceived.hash) )
					{
						DBG( "Extracting parameters from the querystring : NOK " );
						messageType = MESSAGE_UNKNOWN;
					} else {
						DBG( "Extracting parameters from the querystring : OK " );
						DBG( "Timestamp                     = '%lld ' ", *timeStamp );
						DBG( "MessageID                     = '%lld ' ", *messageId );
						DBG( "Username                      = '%s'    ", (char*)username  );
						DBG( "Conce                         = '%s'    ", (char*)cnonce    );
						DBG( "Signature (password) received = '%s'    ", signatureReceived.hash );
						if ( strcmp(szConnectionRequestUsername.value, username) != 0 )
						{
							EXEC_ERROR( "Validation of the two usernames : NOK " );
							messageType = MESSAGE_UNKNOWN;
						} else {
							DBG( "Validation of the two usernames : OK " );
							
							// ----------------------------------------------------------------
							// let's prepare the signature
							// ----------------------------------------------------------------
							char* msg = (char*)calloc( 1, 2*sizeof(UInt64)+2*STUN_MAX_STRING );
							snprintf( msg, (2*sizeof(UInt64)+2*STUN_MAX_STRING), "%lld%lld%s%s", *timeStamp, *messageId, username, cnonce );
							DBG( "Message (ts msgid username conce) = '%s' (size=%d) ", (char*)msg, strlen(msg) );
							
							// ----------------------------------------------------------------
							// Let's compare the two HMACS WITH the SHA1 crypto algorithm
							// ----------------------------------------------------------------
							if( ! DM_STUN_isHmacIdentical(&signatureReceived, msg, strlen(msg), &szConnectionRequestPassword) )
							{
								EXEC_ERROR( "Validation of the two passwords : NOK " );
								messageType = MESSAGE_UNKNOWN;
							} else {
								DBG( "Validation of the two passwords : OK " );
							} // IF
							
							if ( NULL != msg )
							{
								free( msg );
								msg = NULL;
							}
						} // IF
					} // IF
				} else {
					INFO( "The UDP Connection Request is not correct" );
				} // IF
			} // IF
		} // IF
	} // IF
	
	return( messageType );
} // DM_STUN_readMessage

/**
 * @brief stopThread
 *
 * @param stunThreadData (R)
 * 
 * @remarks cancel the timeoutThread and close the sockets
 *          this function is called when the stunThread is cancelled
 *          thanks to pthread_cleanup_pop function in the stunTread function
 * 
 * @return pthread_exit(0);
 *
 */
void DM_STUN_stopThread(stunThreadStruct* stunThreadData)
{
	// Protection to avoid multiple calls...
	pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	if ( 0 != stunThreadData->timeoutThreadData.threadId )
	{
	DBG( "Timeout discovery thread already running. Ask to cancel" );
	pthread_cancel( stunThreadData->timeoutThreadData.threadId );
	} // IF
	DBG( "STUN client thread has been asked to stop. Cleanning..." );
	close( stunThreadData->fdClient );
	DBG(  "the binding socket is closed" );
	INFO( "STUN client thread says Good Bye!!!" );
	
	pthread_exit( 0 );
} // DM_STUN_stopThread

/**
 * @brief Update the dataModel values
 *
 * @param pWanIp	IP address used
 * @param pPort		Port used on the external side of the gateway
 * @param pNatDetected	boolean that indicate if the network use a NAT gateway of not
 *
 * @return nothing
 *
 * @note   example of call : DM_STUN_updateValuesDatamodel( stunThreadData->lanIp, stunThreadData.natDetected );
 */
bool
DM_STUN_updateValuesDatamodel(UInt32 pWanIp, UInt16 pPort, UInt8 pNatDetected)
{
	char		udpConnectionRequestAddress[UDP_CONNECTION_REQUEST_ADDRESS_SIZE];
	char		natDetectedStr[2];
	bool		bRet			= true;
	bool		bRetUdateParamValue	= false;
	struct in_addr	tmp;

	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));
			
	// ------------------------------------------------
	// Update the UDP_CONNECTION_REQUEST_ADDRESS (string)
	// ------------------------------------------------
	if ( pWanIp == 0 )
	{
		DBG( "Update of the UDP_CONNECTION_REQUEST_ADDRESS with the current value " );
		tmp.s_addr = pWanIp;
		snprintf( udpConnectionRequestAddress, UDP_CONNECTION_REQUEST_ADDRESS_SIZE, "%s:%u", inet_ntoa(tmp), pPort );
		bRetUdateParamValue = DM_STUN_updateParameterValue( STUN_UDP_CONNEXION_REQUEST_ADDRESS, DM_ENG_ParameterType_STRING, udpConnectionRequestAddress );
	} else {
		DBG( "Initial update of the UDP_CONNECTION_REQUEST_ADDRESS with a default value (empty string). " );
		bRetUdateParamValue = DM_STUN_updateParameterValue( STUN_UDP_CONNEXION_REQUEST_ADDRESS, DM_ENG_ParameterType_STRING, DM_STUN_EMPTY_UDP_CONNECTION_REQUEST_ADDRESS );
	} // IF
	if ( true == bRetUdateParamValue )
	{
		DBG(   "Updating the UDP CONNECTION REQUEST ADDRESS value : OK  " );
	} else {
		EXEC_ERROR( "Updating the UDP CONNECTION REQUEST ADDRESS value : NOK " );
		bRet = false;
	} // IF
	
	// ------------------------------------------------
	// Update the STUN_NAT_DETECTED (boolean)
	// ------------------------------------------------
	snprintf( natDetectedStr, DM_STUN_MAX_SIZE_PORT_STRING, "%u", pNatDetected );
	bRetUdateParamValue = DM_STUN_updateParameterValue( STUN_NAT_DETECTED, DM_ENG_ParameterType_BOOLEAN, natDetectedStr );
	if ( true == bRetUdateParamValue )
	{
		DBG(   "Updating the UDP CONNECTION REQUEST ADDRESS value : OK  " );
	} else {
		EXEC_ERROR( "Updating the UDP CONNECTION REQUEST ADDRESS value : NOK " );
		bRet = false;
	} // IF
	
	// ------------------------------------------------
	// Check the result of the two last update
	// ------------------------------------------------
	if ( bRet == true ) {
		DBG(  "Update of the STUN dataModel values : OK  " );
	} else {
		EXEC_ERROR( "Update of the STUN dataModel values : NOK " );
	} // IF
	
	return( bRet );
} // DM_STUN_updateValuesDatamodel

/**
 * @brief Initialize all the STUN values
 *
 * @param stunThreadData
 * 
 * @remarks variables used : 
 * @remarks natReceivedPort (R/W)
 * @remarks stunClientBindingPort (R)
 * @remarks countStopTimeoutDiscoveryThread (R/W)
 * @remarks lastStunMessageReceivedType (R/W)
 * @remarks natDetected (R/W)
 * @remarks lanIp (R)
 *
 * @return nothing
 *
 */
void
DM_STUN_initValues(stunThreadStruct *stunThreadData)
{
	char*		getParameterResult = NULL;
	struct in_addr	tmp;
	
	DBG("STUN - DM_STUN_initValues");

	// Memset to 0 tmp
	memset((void *) &tmp, 0x00, sizeof(tmp));

	// __update variables__
	stunThreadData->natReceivedPort.binding			= UNKNOWN;
	stunThreadData->natReceivedPort.timeout			= UNKNOWN;
	stunThreadData->lastStunMessageReceivedType		= MESSAGE_UNKNOWN;
	
	// By default, the natDetected flag is set to false
	stunThreadData->natDetected				= FALSE;
	stunThreadData->countStopTimeoutDiscoveryThread		= 0;
	stunThreadData->stunWanIpNotification			= NOT_NOTIFIED;
	
	// __retrieve variables from DB__
	
	// ******** RETRIEVE THE STUN USERNAME ********
	while(false == DM_STUN_getParameterValue( STUN_USERNAME,&getParameterResult ) ){
	  WARN("STUN - No STUN_USERNAME. Wait a while (parameter value could be set later)");
	  sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	}
	if(NULL == getParameterResult) {
	  DBG("STUN - Username = None (no value)");
    snprintf( stunThreadData->username.value, STUN_MAX_STRING, "%s", "" );
	} else {
    DBG("STUN - Username = %s", getParameterResult);
    snprintf( stunThreadData->username.value, STUN_MAX_STRING, "%s", getParameterResult );
  }
	stunThreadData->username.sizeValue = strlen(stunThreadData->username.value);
	free( getParameterResult );

  // ******** RETRIEVE THE STUN PASSWORD ********	
	while(false == DM_STUN_getParameterValue( STUN_PASSWORD,&getParameterResult ) ) {
	  WARN("STUN - No STUN_PASSWORD. Wait a while (parameter value could be set later)");
	  sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	}
	if(NULL == getParameterResult) {
	  DBG("STUN - Password = None (no value)");
	snprintf( stunThreadData->password.value, STUN_MAX_STRING, "%s", "" );	  
  } else {
	  DBG("STUN - Password = %s", getParameterResult);
	  snprintf( stunThreadData->password.value, STUN_MAX_STRING, "%s", getParameterResult );	
  }	
	
	stunThreadData->password.sizeValue=strlen( stunThreadData->password.value );
	free( getParameterResult );
	
	// ******** RETRIEVE THE STUN SERVER ADDRESS ********
	while(false == DM_STUN_getParameterValue( STUN_SERVER_ADDRESS, &getParameterResult ) ||
	      (NULL == getParameterResult)) {
	  WARN("STUN - No STUN_SERVER_ADDRESS. Wait a while (parameter value could be set later)");
	  sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	}
  DBG("STUN - StunServerAddress = %s", getParameterResult);

	inet_aton( getParameterResult, &tmp );
	//in host byte order needed for the dm_lib_udp
	stunThreadData->stunServer.addr = ntohl( tmp.s_addr );
	free( getParameterResult );

	// ******** RETRIEVE THE STUN SERVER PORT ********
	while(false == DM_STUN_getParameterValue( STUN_SERVER_PORT, &getParameterResult ) ||
	      (NULL == getParameterResult)) {
	  WARN("STUN - No STUN_SERVER_PORT. Wait a while (parameter value could be set later)");
	  sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	}
  DBG("STUN - ServerPort = %s", getParameterResult);

	
	stunThreadData->stunServer.port = (UInt16)atol( getParameterResult );
	free( getParameterResult );
	
	if ( ( stunThreadData->stunServer.addr == 0) || (stunThreadData->stunServer.port == 0) )
	{
		DBG( "there are no STUN server address or port. the ACS address will be used" );
		while(false == DM_STUN_getParameterValue( ACS_ADDRESS, &getParameterResult ) ||
	        (NULL == getParameterResult)) {
	    WARN("STUN - No ACS_ADDRESS. Wait a while (parameter value could be set later)");
	    sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	  }
	  DBG("STUN - ACS Address = %s", getParameterResult);
		inet_aton( getParameterResult, &tmp );
		// in host byte order needed for the dm_lib_udp
		stunThreadData->stunServer.addr = ntohl( tmp.s_addr );
		free( getParameterResult );
		stunThreadData->stunServer.port = DEFAULT_ACS_PORT;
	} // IF
	
	// set the timeout to the minimum
	while(false == DM_STUN_getParameterValue( STUN_MIN_KEEP_ALIVE_PERIOD, &getParameterResult ) ||
	      (NULL == getParameterResult)) {
	  WARN("STUN - No STUN_MIN_KEEP_ALIVE_PERIOD. Wait a while (parameter value could be set later)");
	  sleep(STUN_SLEEP_TIME_ON_PARAM_VALUE_WAIT);
	}
	 DBG("STUN - Min Period = %s", getParameterResult);
	//  DBG("Try to lock the mutex");
	pthread_mutex_lock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	stunThreadData->nTimeout = atol( getParameterResult ); //time in seconds
	pthread_mutex_unlock( &(stunThreadData->timeoutThreadData.mutex_lock) );
	//  DBG("mutex unlocked");
	free( getParameterResult );

	INFO( "STUN context successfully initialized." );
} // DM_STUN_initValues

/**
 * @brief Convert a hexa string into an ASCII one
 *
 * @param pHexStr	String in hexadecimal to convert in ASCII
 * @param nSize		Initial ASCII string size
 *
 * @return Return the hex string converted in ASCII
 *
 */
char*
DM_STUN_hextoAscii(u8* hexaMsg, u8 nbHexaDigit)
{
	char*	tmpStr  = NULL;
	int	i	= 0;
	char	charStr[3];
 
	tmpStr = (char*)malloc( nbHexaDigit*20 );
	memset( tmpStr, '\0', nbHexaDigit );
	for( i=0; i<nbHexaDigit; i++ )
	{
		snprintf( charStr, 3, "%.2X", hexaMsg[i] );
		strcat( tmpStr, charStr );
	}

	return( tmpStr );
} // DM_STUN_hextoAscii

/**
 * @brief DM_STUN_isHmacIdentical : compare the received HMAC-SHA1 with the generated 
 *
 * @param signatureReceived
 * @param msg
 * @param msgLength
 * @param password
 *
 * @return TRUE if th HMAC are equals
 *
 */
bool
DM_STUN_isHmacIdentical(StunAtrIntegrity* signatureReceived, char* msg, UInt16 msgLength, StunAtrString* password)
{
	StunAtrIntegrity	HexaSignatureGenerated;
	char*			AsciiSignatureGenerated = NULL;
	bool			err			= FALSE;
	//int			i;
	
	// --------------------------------------------------------------------------
	// Initialization of the HexaSignatureGenerated string
	// --------------------------------------------------------------------------
	memset( &HexaSignatureGenerated, 0x00, HMAC_SIZE );
	
	// --------------------------------------------------------------------------
	// Generate the MAC_SHA1 according to the informations sent by the ACS server
	// --------------------------------------------------------------------------
	DBG( "Generate the SHA1 signature according to the following data : "                               );
	DBG( "Password (ConnectionRequestPassword) = '%s' (size=%d) ", password->value, password->sizeValue );
	DBG( "Signature received                   = '%s'           ", signatureReceived->hash              );
	DBG( "Message                              = '%s' (size=%d) ", msg, strlen(msg)                     );
	hmac_sha1( (u8*)password->value, password->sizeValue, (u8*)msg, msgLength, HexaSignatureGenerated.hash  );
	
	// --------------------------------------------------------------------------
	// Convert the hex string previously generated into aan ascii one
	// --------------------------------------------------------------------------
	AsciiSignatureGenerated = DM_STUN_hextoAscii( HexaSignatureGenerated.hash, HMAC_SIZE );
	DBG( "HASH SHA1 generated = '%s' ", (char*)AsciiSignatureGenerated );
	
	// --------------------------------------------------------------------------
	// Compare the two ASCII strings
	// --------------------------------------------------------------------------
	DBG( "Compare '%s' with '%s' : ", AsciiSignatureGenerated, (char*)signatureReceived->hash );
	if ( 0 == strncasecmp( (char*)AsciiSignatureGenerated, (char*)signatureReceived->hash, HMAC_SIZE ) )
	{
		DBG(  "HMAC identical" );
		err = TRUE;
	} else {
		EXEC_ERROR( "HMAC are not identical" );
	} // IF
	
	// --------------------------------------------------------------------------
	// Free the memory allocated for the Hex2Ascii conversion
	// --------------------------------------------------------------------------
	if ( NULL != AsciiSignatureGenerated )
	{
		free( AsciiSignatureGenerated );
		AsciiSignatureGenerated = NULL;
	} // IF
	
	return( err );
} // DM_STUN_isHmacIdentical

/**
 * @brief DM_STUN_openSocket : open an UDP socket
 *
 * @param fdSocket (R/W)
 * @param ip (R)
 * @param port (R)
 * 
 * @remarks cancel the timeoutThread and close the sockets
 *          this function is called when the stunThread is cancelled
 *          thanks to pthread_cleanup_pop function in the stunTread function
 * 
 * @return pthread_exit(0);
 *
 */
void
DM_STUN_openSocket(Socket* fdSocket, UInt32 ip, UInt16 port)
{
	if ( -1 != (*fdSocket= openPort(port,ntohl(ip) )) )
	{
		INFO( "Opening port #%u : OK ", port );
	} else {
		EXEC_ERROR( "Opening port #%u: NOK ", port );
		ASSERT( false );
	} // IF
} // DM_STUN_openSocket

/**
 * @brief extract all the parameters in the UDP_REQUEST
 *
 * @param none
 *
 * @return TRUE if all the values are well extracted, otherwise FALSE
 *
 */
bool
DM_STUN_extractUdpParameters(char* message, UInt64* timeStamp, UInt64* messageId, char* username, char* cnonce, u8* signatureReceived)
{
	UInt8	size;
	UInt8	parametersFound = 0;
	UInt16	i		= 0;
	bool	err		= TRUE;	// a mettre en true ?
	char	pBuffer[50];
	
	// -------------------------------------------------------------------------------
	// Parsing the querystring in order to extract the different items
	// -------------------------------------------------------------------------------
	for ( i=0 ; (i < STUN_MAX_MESSAGE_SIZE) && (parametersFound != 5) ; i++ )
	{
		// -------------------------------------------------------------------------------
		// ?ts=
		// -------------------------------------------------------------------------------
		if ( (message[i]=='?') && (message[i+1]=='t') && (message[i+2]=='s') && (message[i+3]=='=') )
		{
			DBG( "--- Param.1 : TimeStamp ---" );
			i+=4;
			memset( pBuffer, '\0', 50 );
			parametersFound++;
			for ( size=0 ; message[i+size]!='&' ; size++ );
			memcpy( pBuffer, message+i, sizeof(char)*size );
			DBG( "Buffer='%s' (size=%d) ", pBuffer, size );
			*timeStamp = /*ntohl(*/ (unsigned long long)strtoull( pBuffer, (char**)NULL, 10 ) /*)*/;
			DBG( "TimeStamp= %lld (size=%d) ", *timeStamp, size );
		} // IF
		
		// -------------------------------------------------------------------------------
		// &id=
		// -------------------------------------------------------------------------------
		else if ( (message[i]=='&') && (message[i+1]=='i') && (message[i+2]=='d') && (message[i+3]=='=') )
		{
			DBG( "--- Param.2 : MessageID ---" );
			i+=4;
			memset( pBuffer, 0x00, 50 );
			parametersFound++;
			for ( size=0 ; message[i+size]!='&' ; size++ );
			memcpy( pBuffer, message+i, sizeof(char)*size );
			DBG( "Buffer='%s' (size=%d) ", pBuffer, size );
			*messageId = /*ntohl(*/ (unsigned long long)strtoull( pBuffer, (char**)NULL, 10 ) /*)*/;
			DBG( "MessageId= %lld (size=%d) ", *messageId, size );
		} // IF
		
		// -------------------------------------------------------------------------------
		// &un=
		// -------------------------------------------------------------------------------
		else if ( (message[i]=='&') && (message[i+1]=='u') && (message[i+2]=='n') && (message[i+3]=='=') )
		{
			DBG( "--- Param.3 : UserName ---" );
			i+=4;
			parametersFound++;
			for ( size=0 ; message[i+size]!='&' ; size++ );
			DBG( "Size=%d", size );
			memcpy(username,message+i,sizeof(char)*size);
			username[size]='\0';
			DBG( "Username= %s (nb=%d)", username, strlen(username) );
		} // IF
		
		// -------------------------------------------------------------------------------
		// &cn=
		// -------------------------------------------------------------------------------
		else if ( (message[i]=='&') && (message[i+1]=='c') && (message[i+2]=='n') && (message[i+3]=='=') )
		{
			DBG( "--- Param.4 : Conce ---" );
			i+=4;
			parametersFound++;
			for ( size=0 ; message[i+size]!='&' ; size++ );
			DBG( "Size=%d", size );
			memcpy( cnonce, message+i, sizeof(char)*size );
			cnonce[size]='\0';
			DBG( "Cnonce= '%s' (size=%d - nb=%d) ", cnonce, size, strlen(cnonce) );
		} // IF
		
		// -------------------------------------------------------------------------------
		// &sig=
		// -------------------------------------------------------------------------------
		else if ( (message[i]=='&') && (message[i+1]=='s') && (message[i+2]=='i') && (message[i+3]=='g') && (message[i+4]=='=') )
		{
			DBG( "--- Param.5 : Signature computed by the ACS server ---" );
			char pSigTab[HMAC_SIZE+1];
			memset( pSigTab, '\0', HMAC_SIZE+1 );
			memset( signatureReceived, '\0', HMAC_SIZE );
			i+=5;
			parametersFound++;
			memcpy( signatureReceived, message+i, HMAC_SIZE );
			memcpy( pSigTab, signatureReceived, HMAC_SIZE );
			DBG( "Signature received = '%s' ", pSigTab );
		} // IF
	} // FOR
	
	// -------------------------------------------------------------------------------
	// Check that we have extract the right number of parameters from the querystring
	// -------------------------------------------------------------------------------
	if ( parametersFound == 5 ) {
		err = TRUE;
	} // IF
	
	return( err );
} // DM_STUN_extractUdpParameters

/**
 * @brief Get a value from the DB
 *
 * @param parameterName (R)
 * @param value (R/w)
 *
 * @return Nothing
 *
 */
bool
DM_STUN_getParameterValue(char* parameterName, char** value)
{
	DM_ENG_ParameterValueStruct**	pResult = NULL;
	char*				pParameterName[2];
	int				err;
	bool				bRet    = true;
	
	DBG("STUN - DM_STUN_getParameterValue - parameterName = %s", parameterName);
  *value = NULL;
  
	// -------------------------------------------------------------------------------
	// Initialize the array for the DM_ENGINE call with a NULL at the end
	// -------------------------------------------------------------------------------
	pParameterName[0]=parameterName;
	pParameterName[1]=NULL;
	
	// -------------------------------------------------------------------------------
	// Request the value of the parameter name given fro mthe DM_Engine
	// -------------------------------------------------------------------------------
	err = DM_ENG_GetParameterValues( DM_ENG_EntityType_ANY, (char**)pParameterName, &pResult );
	
	if ( (err == 0) && (pResult != NULL) && (strcmp(parameterName, (char*)pResult[0]->parameterName) == 0 ) )
	{
		DBG( "Parameter name : %s = '%s' (type = %d)", (char*)pResult[0]->parameterName, (char*)pResult[0]->value, pResult[0]->type );
		if(NULL != (char*)pResult[0]->value ) {
		  *value= strdup( (char*)pResult[0]->value );
		} 
		DM_ENG_deleteTabParameterValueStruct(pResult);
	} else {
		EXEC_ERROR( "error code = %d ", err);

		bRet = false;
	} // IF
	
	return( bRet );
} // DM_STUN_getParameterValue

/**
 * @brief check the stun parameters in the DB
 *
 * @param none
 *
 * @return TRUE if DB is OK, otherwise FALSE
 *
 */
static bool
DM_STUN_checkAllStunParameters()
{
	char*                         pParameterName[15];
	bool                          dbStatus   = FALSE;
	DM_ENG_ParameterValueStruct** pResult    = NULL;
	int                           i          = -1;
	int                           minTimeout = -1;
	int                           maxTimeout = -1;
	char                          tmpStr[2];
	int                           databaseOK = 1; // -1 when not OK (default is OK)
	DBG( "STUN - Make sure all required STUN parameter are in the datamodel" );
	
	pParameterName[0]  = STUN_UDP_CONNEXION_REQUEST_ADDRESS;
	pParameterName[1]  = STUN_UDP_CONNEXION_REQUEST_NOTIFICATION_LIMIT;
	pParameterName[2]  = STUN_ENABLE;
	pParameterName[3]  = STUN_SERVER_ADDRESS;
	pParameterName[4]  = STUN_SERVER_PORT;
	pParameterName[5]  = STUN_USERNAME;
	pParameterName[6]  = STUN_PASSWORD;
	pParameterName[7]  = STUN_MAX_KEEP_ALIVE_PERIOD;
	pParameterName[8]  = STUN_MIN_KEEP_ALIVE_PERIOD;
	pParameterName[9]  = STUN_NAT_DETECTED;
	pParameterName[10] = LAN_IP;
	pParameterName[11] = CONNECTION_REQUEST_USERNAME;
	pParameterName[12] = CONNECTION_REQUEST_PASSWORD;
	pParameterName[13] = ACS_ADDRESS;
	pParameterName[14] = NULL;
	
	if ( 0 == DM_ENG_GetParameterValues(DM_ENG_EntityType_ANY, (char**)pParameterName, &pResult) ) {

		if ( NULL != pResult ){

			for( i=0 ; i < 14 ; i++ ) {

				if ( NULL != pResult[i]->value ) {
					DBG( "STUN - (i=%d) Parameter name : %s = '%s' (type = %d) ", i, (char*)pResult[i]->parameterName, (char*)pResult[i]->value, pResult[i]->type );
					
					if ( 0 == strcmp(STUN_MAX_KEEP_ALIVE_PERIOD, (char*)pResult[i]->parameterName) )
					{
						maxTimeout = atol(pResult[i]->value);
					}
					else if ( 0 == strcmp(STUN_MIN_KEEP_ALIVE_PERIOD, (char*)pResult[i]->parameterName) )
					{
						minTimeout = atol(pResult[i]->value);
					} // IF
					
				} else { // No default Value
				
				  // No default Value --> Check parameters that request a default value
				  if( (0 == strcmp(STUN_ENABLE,                (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(STUN_SERVER_ADDRESS,        (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(STUN_SERVER_PORT,           (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(STUN_MAX_KEEP_ALIVE_PERIOD, (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(STUN_MIN_KEEP_ALIVE_PERIOD, (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(LAN_IP,                     (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(ACS_ADDRESS,                (char*)pResult[i]->parameterName)) ||
				      (0 == strcmp(STUN_NAT_DETECTED,          (char*)pResult[i]->parameterName)) || 
				      (0 == strcmp(STUN_UDP_CONNEXION_REQUEST_NOTIFICATION_LIMIT, (char*)pResult[i]->parameterName)) ) {
				     		EXEC_ERROR( "STUN - The '%s' parameter[%d] has a NULL value",    (char*)pResult[i]->parameterName, i );
						
						   // Datbase does not allow to support STUN
						   databaseOK = -1; 
						  }
				} // IF
			} // FOR
		} else {
		  // Datbase does not allow to support STUN
      databaseOK = -1; 
			EXEC_ERROR( "STUN - The DM_ENG_GetParameterValues result is NULL " );
		} // IF
		
		DM_ENG_deleteTabParameterValueStruct( pResult );
	} else {
		EXEC_ERROR( "STUN - Calling the DM_ENG_GetParameterValues : NOK" );
    // Datbase does not allow to support STUN
    databaseOK = -1; 
	} // IF
	
	if ( 1 == databaseOK ) {
	  // Set dbStatus to TRUE
		dbStatus = TRUE;
		DBG("STUN - The database contains all the STUN values");
		
		if ( minTimeout < 0 )
		{
			EXEC_ERROR( "STUN - minTimeout<0" );
			minTimeout = 10;
			snprintf( tmpStr, 2, "%u", minTimeout );
			DM_STUN_updateParameterValue( STUN_MIN_KEEP_ALIVE_PERIOD, DM_ENG_ParameterType_BOOLEAN, tmpStr );
		} // IF
		
		if ( (minTimeout>maxTimeout) || (maxTimeout<0) )
		{
			EXEC_ERROR("STUN - minTimeout>maxTimeout or maxTimeout<0");
			//let's set min=max...
			maxTimeout = minTimeout;
			snprintf( tmpStr, 2, "%u", maxTimeout );
			DM_STUN_updateParameterValue( STUN_MAX_KEEP_ALIVE_PERIOD, DM_ENG_ParameterType_BOOLEAN, tmpStr );
		}
	} else {
		EXEC_ERROR( "STUN - The database doesn't contain all the STUN values (i=%d)", i );
	} // IF
	
	
	return dbStatus;
} // DM_STUN_checkAllStunParameters

/**
 * @brief Update a value from the DB
 *
 * @param parameterName (R)
 * @param DM_ENG_ParameterType (R)
 * @param value (R)
 *
 * @return Return true if okay else false
 *
 */
bool
DM_STUN_updateParameterValue(char* parameterName, DM_ENG_ParameterType type, char* value)
{
	bool bRet = false;
	DM_ENG_ParameterValueStruct*		pParameterList[2];
	DM_ENG_ParameterStatus			status;
	DM_ENG_SetParameterValuesFault**	faults = NULL;
	
	pParameterList[0] = DM_ENG_newParameterValueStruct(parameterName,type, value);
	pParameterList[1] = NULL;
	
	DBG( "updating %s parameter with %s value", parameterName, value );
	if ( DM_ENG_SetParameterValues( DM_ENG_EntityType_SYSTEM, pParameterList, "", &status, &faults) != 0 )
	{
		EXEC_ERROR( "update not performed" );
		UInt8 i = 0;
		while ( NULL != faults[i] )
		{
			EXEC_ERROR( "Error %u while updating the '%s' parameter.", faults[i]->faultCode, faults[i]->parameterName );
			i++;
		} // WHILE
		ASSERT( FALSE );
	} else {
		bRet = true;
		DBG( "Updating the '%s'='%s' parameter : OK ", parameterName, value );
	} // IF
	
	// Free the memory previously allocated
	DM_ENG_deleteParameterValueStruct( pParameterList[0] );
	if ( NULL != faults )
	{
		DM_ENG_deleteTabSetParameterValuesFault( faults );
	}
	
	return( bRet );
} // DM_STUN_updateParameterValue


int
DM_STUN_ComputeGlobalTimeout(int _step, int _min, int _max, int _limit)
{
  int timeoutComputed = _min;
  DBG("Compute the Global Timeout (step = %d, min = %d, max = %d, limit = %d)", _step, _min, _max, _limit);
  
  timeoutComputed = _limit - (int)(_step * (TIMEOUT_PRECISION + 1)) - TIMEOUT_OFFSET; // Compute the used value (limit - (1.5 + 1) step - 5 sec)
  
  if(timeoutComputed < _min) timeoutComputed = _min;
  
  if(timeoutComputed > _max) timeoutComputed = _max;
  
  DBG("Computed Global Timeout = %d", timeoutComputed); 
  
  return timeoutComputed;
}

