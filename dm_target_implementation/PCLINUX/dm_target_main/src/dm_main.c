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
 * File        : dm_main.c
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
 * @file dm_main.c
 *
 * @brief 
 *
 **/

/*
 * At each build, the dm_main.c must be commited in order to update these keywords
 * N.B. The svn:keywords must be set to Revision Date
 */
#define REV_KEYWORD  "$Revision: 3841 $" 
#define DATE_KEYWORD "$Date: 2015-12-17 11:28:03 +0100 (jeu. 17 d√©c. 2015) $"

const char* _VERSION = "3.1";

#ifndef _InternetGatewayDevice_
#define DEVICE_TYPE "Device"
#else
#define DEVICE_TYPE "IGD"
#endif

#ifndef STUN_ENABLED_ON_TR069_AGENT
#define STUN_INFO "without STUN"
#else
#define STUN_INFO "with STUN"
#endif

#include <stdio.h>			/* Standard I/O functions			  	*/
#include <unistd.h>			/* Standard unix functions, like getpid()	  	*/
#include <signal.h>			/* Signal name macros, and the signal() prototype 	*/
#include <stdlib.h>			/* Standard system library			  	*/
#include <string.h>			/* Usefull string functions				*/
#include <errno.h>			/* Usefull for system errors				*/
#include <getopt.h>			/* Needs for managing parameters			*/
#include <fcntl.h>			/*	*/
#include <sys/stat.h>	  /*	*/
#include <sys/types.h>	/*	*/
#include <ctype.h>      /* for isprint */

#include "DM_ENG_Global.h"

// For Trace
#define FTX_TRACES_MAIN
#include "CMN_Trace.h"

// Enable the tracing support
#ifdef DEBUG
#define SHOW_TRACES 1
#define TRACE_PREFIX	"[DM-MAIN] %s:%s(),%d: ", __FILE__, __FUNCTION__, __LINE__
#endif

#define MAX_FILE_NAME_SIZE (256) // The maximum path and filename size

// DM_COM's header
#include "dm_com.h"                /* DM_COM definition */

#ifdef _DBusIpcEnabled_
#include "DM_IPC_DBusInterface.h"  /* DBus Interface */
#endif

#include "dm_main.h"			        /* Definition for the main entry point */
#include "DM_ENG_Device.h"
// DM_ENGINE's header
#include "DM_ENG_ParameterBackInterface.h" 	/* Public definition of the DM_ENGINE lower layer DM_Engine */

#ifdef STUN_ENABLED_ON_TR069_AGENT
#include "dm_stun.h"
#endif

#ifndef DATABASENAME
#define DATABASENAME "parameter.csv"
#endif

// Recue exit mechanism if the normal exit not working
static DM_CMN_ThreadId_t _rescueExitThreadId = 0;

static DM_CMN_Mutex_t _exitMutex = NULL;
static DM_CMN_Cond_t _exitSynchro = NULL;

static bool _exitInProgress = false;

extern  dm_com_struct  g_DmComData;	 	     /* Global data structure for the DM_COM module	      */

// The Parameter Obj Path (CSV file location 
char* DATA_PATH = NULL; /* Global path which define the path to the datamodel  */

/* Global Proxy Address to use */
char * PROXY_ADDRESS_STR = NULL;

/* Global network iterface to use */
char * ETH_INTERFACE = "eth0";

/* Global 16 random char str computed for Connection Request URL */
char * g_randomCpeUrl = NULL;

#ifdef STUN_ENABLED_ON_TR069_AGENT
static DM_CMN_ThreadId_t _stunThreadId = 0;
#endif

/* Private routine declaration */
static int  DM_MAIN_CheckLockFile(IN const char *pPidfile);
static int  DM_MAIN_RemoveLockFile(IN const char *pPidfile);
static int  DM_MAIN_CreateLockFile(IN const char *pPidfile, IN       int   nPIDProcess);
// static void DM_Agent_Stop(int nSig);
static void DM_MAIN_ShowUsage(IN const char *pBinaryName);

/**
 * @brief Function called when a ctrl-C is catched. This function stop the Device Management Agent correctly
 * @brief The main entry is like a "DM_A start" function
 *
 * @param  argc		number of arguments
 * @param  argv		list of parameters
 * @param  env		list of environement's variables
 *
 * @return none
 */
int
main(int  argc,
     char *argv[],
     char* env[] UNUSED)
{
	DMRET	nRet    = DM_ERR; // Return code of the main test program
	int  nArgcode = -1;
	int  nPidProcess;
	char szFullPathPid[MAX_FILE_NAME_SIZE];
   DM_ENG_InitLevel level = DM_ENG_InitLevel_REBOOT; // par dÈfaut, on considËre que l'agent dÈmarre suite ‡ un REBOOT

  // Initialize trace subsystem
#ifndef WIN32
	FTX_TRACES_INIT("CWMP");
#else
        WIN_TRACE_INIT;
#endif
	DBG("main - Begin");
	
   DM_CMN_Thread_initMutex(&_exitMutex);
   DM_CMN_Thread_initCond(&_exitSynchro);

	memset(szFullPathPid, '\0', MAX_FILE_NAME_SIZE);
	
  // -------------------------------------------------------------
	// Initialisation of the default's parameters
	// -------------------------------------------------------------
	memset((void *)  &g_DmComData,  0x00, sizeof(dm_com_struct) );	

	
  // Create the randomly chosen CPE URL (This URL is provided in the inform message
	// and is used by the ACS to connect to the CPE HTTP Server). The Randomly Chosen
	// URL must be used by the DM_ENGINE to build the inform message.
	g_randomCpeUrl = (char *) malloc(CPE_URL_SIZE + 1);
	_generateRandomString(g_randomCpeUrl, CPE_URL_SIZE);
	DBG("CPE URL: %s\n", g_randomCpeUrl);
  
	// ---------------------------------------------------------------------------------------
	// Register some terminaison signal to catched
	// ---------------------------------------------------------------------------------------
	signal( SIGINT,  DM_Agent_Stop );	/* Term - Interrupt from keyboard	*/
	signal( SIGABRT, DM_Agent_Stop );	/* Core - Abort signal from abort	*/
	signal( SIGTERM, DM_Agent_Stop );	/* Term - Termination signal		*/
#ifndef WIN32
	signal( SIGTSTP, DM_Agent_Stop );	/* Stop - Stop typed at tty		*/
	signal( SIGTTIN, DM_Agent_Stop );	/* tty  - input for background process	*/
	signal( SIGTTOU, DM_Agent_Stop );	/* tty  - output for background process	*/
	signal( SIGUSR1, DM_Agent_Stop );	/* usr  - output for background process	*/
#endif
	// ---------------------------------------------------------------------------------------
	// Check parameters given to the binary with the getopt funtions
	// ---------------------------------------------------------------------------------------
	if ( argc == 1 )
	{
		// If argc = 1 : no parameter given display a usage message
		DM_MAIN_ShowUsage( (char*)argv[0] );
		exit( -1 );
	} else {

		// -------------------------------------------------------------
		while( (nArgcode = getopt( argc, argv, "rc:k:a:p:i:x:v" )) != -1 )
		{
			// And analyse them
			switch( nArgcode )
			{
            // -------------------------------------------------------------
            // Definition : used to display revision
            // priority   : optional
            // -------------------------------------------------------------
            case 'v':
            {
               char* rev = strdup(REV_KEYWORD+1);   *(rev+strlen(REV_KEYWORD)-2) = '\0';
               char* date = strdup(DATE_KEYWORD+7); *(date+16) = '\0';
               printf("cwmpd %s %s - Version: %s - %s- %s\n", DEVICE_TYPE, STUN_INFO, _VERSION, rev, date);
               free(rev);
               free(date);
            }
            break;

            // -------------------------------------------------------------
            // Definition : path to the database model (local csv file)
            // priority   : mandatory (so no default values)
            // -------------------------------------------------------------
            case 'p':
            {
               DATA_PATH = optarg;
            }
            break;
            
				// -------------------------------------------------------------
				// Definition : Proxy (ipaddress:port to use)
				// priority   : Optional
				// -------------------------------------------------------------
				case 'x':
				{
					PROXY_ADDRESS_STR = optarg;
				}
				break;
		
				// -------------------------------------------------------------
				// Definition : Eth interface (interface to use, default: eth0)
				// priority   : Optional
				// -------------------------------------------------------------
				case 'i':
				{
					ETH_INTERFACE = optarg;
				}
				break;					
				// -------------------------------------------------------------
				// Definition : path and certificate to use in case of the ssl mode is using
				// priority   : mandatory when using HTTPS
				// -------------------------------------------------------------
				case 'c':
				{
				  g_DmComData.szCertificatStr = strdup( optarg );
				}
				break;

				// -------------------------------------------------------------
				// Definition : path and certificate on the Device RSA key file
				// priority   : mandatory when using HTTPS
				// -------------------------------------------------------------
				case 'k':
				{
					g_DmComData.szRsaKeyStr = strdup( optarg );
				}
				break;

            // -------------------------------------------------------------
            // Definition : path and certificate on the ACS Cetification Authority
            // priority   : Optional (if not set, the received Karma Public Certificat is not checked.
            // -------------------------------------------------------------
            case 'a':
            {
               g_DmComData.szCaAcsStr = strdup( optarg );
            }
            break;            

            // -------------------------------------------------------------
            // Definition : used to restart (resume) agent without sending BOOT event to ACS
            // priority   : Optional
            // -------------------------------------------------------------
            case 'r':
            {
               level = DM_ENG_InitLevel_RESUME;
            }
            break;            

				// -------------------------------------------------------------
				// Unknow parameter
				// -------------------------------------------------------------
				case '?':
				{
					if ( isprint(optopt)) 
					{
					  EXEC_ERROR( "Unknow parameter '-%c'!!", optopt );
					} else {
					  EXEC_ERROR( "Unknown option character '\\x%x'!!", optopt );
					}
				}
				break;
			} /* SWITCH */
		} /* WHILE */
	} /* IF ELSE*/
	

#ifndef _InternetGatewayDevice_ /* This flag is set in the main Makefile when DEVICE_TYPE is specified (IGD or D) */
/* Prefix type is Device*/
  WARN("DEVICE TYPE is DEVICE");
#else
/* Prefix type is InternetGatewayDevice*/
  WARN("DEVICE TYPE is INTERNET GATEWAY DEVICE");
#endif	

	DBG( "----------------------------------------------------------------------------" );
	DBG( "-> SUMMARY of the parameters used with the DM agent : " );
	DBG( "----------------------------------------------------------------------------" );
	DBG( "1. PATH to the datamodel       = '%s' ", DATA_PATH      );
	DBG( "----------------------------------------------------------------------------" );

	// ---------------------------------------------------------------------------------------
	// Be sure that another instance is not running yet
	// ---------------------------------------------------------------------------------------
	nPidProcess = (int)getpid();
	snprintf( szFullPathPid, MAX_FILE_NAME_SIZE, "%s/%s", VARRUNPATH, PIDFILE );
	if ( DM_MAIN_CheckLockFile( szFullPathPid ) == -1 )	{
		//DBG( "Creating lock file (%s) ...", szFullPathPid );
		DM_MAIN_CreateLockFile( szFullPathPid, nPidProcess );
	} else {
		printf( "The current daemon (%s) is already running. \n", argv[0] );
		printf( "If it's not the case, maybe the daemon didn't end normally." );
		printf( "In order to correct the problem, remove the %s file and relaunch it.", szFullPathPid );
		printf( "(Eg: sudo rm -f %s) ", szFullPathPid);
		printf( "Aborting.\n" );
		exit( -1 );
	} /* IF ELSE */

	// ---------------------------------------------------------------------------------------
	// Check mandatory parameters
	// When SSL is used szCertificatStr and szRsaKeyStr must be provided
	// ---------------------------------------------------------------------------------------
	// If Device Certificat is provided, the RSA Key must be provided
	if( ((NULL != g_DmComData.szCertificatStr) && (NULL == g_DmComData.szRsaKeyStr)) || 
	    ((NULL == g_DmComData.szCertificatStr) && (NULL != g_DmComData.szRsaKeyStr)) ) {
    EXEC_ERROR("ERROR - When using SSL, the Device Public Certificat and RSA Kay file must be provided.");
		DM_MAIN_ShowUsage( (char*)argv[0] );
		exit(-1);
	}

	if ( DATA_PATH != NULL ){
		// Starting of the Device Management agent
		DBG( "---> [ Device Management Agent started (PID = %d) ] ", nPidProcess );
		
		// ---------------------------------------------------------------------------------------
		// In the DM_A, the DM_ENGINE must be launched before the DM_COM
		// ---------------------------------------------------------------------------------------
		DBG( "Starting DM_ENGINE module..." );
		
		// ---------------------------------------------------------------------------------------
		// Load the database (false is the current one and true is the backup one : reset factory )
		// ---------------------------------------------------------------------------------------
		DBG( "Loading the local database ('%s/%s')", DATA_PATH, DATABASENAME );
		nRet = DM_ENG_ParameterManager_init( DATA_PATH, level );
      if (nRet != 0) exit( nRet );

      // ---------------------------------------------------------------------------------------
      // Run the DM_COM init
      // ---------------------------------------------------------------------------------------
      DBG("Start DM_COM - Begin");
      nRet = DM_COM_START();
      if (nRet != 0) exit( nRet );
      DBG("Start DM_COM - End");

#ifdef _DBusIpcEnabled_
      // ---------------------------------------------------------------------------------------
      // Init the DBus Interface
      // ---------------------------------------------------------------------------------------
      DBG("Init DBus Interface - Begin");
      nRet = DM_IPC_InitDBusConnection();
      if (nRet != 0) exit( nRet );
      DBG("Init DBus Interface - End");
#endif

		// ---------------------------------------------------------------------------------------
		// Run the DM_DEVICE init
		// ---------------------------------------------------------------------------------------
		DBG("Start DM_ENG_Device_init - Begin");
		DM_ENG_Device_init();
      DBG("Start DM_ENG_Device_init - End");

		// ---------------------------------------------------------------------------------------
		// The agent will be able to start itself
		// ---------------------------------------------------------------------------------------
   DBG( "Enabling the DM_ENGINE module... " );
   DM_ENG_ParameterManager_dataNewValue( DM_ENG_DEVICE_STATUS_PARAMETER_NAME, DM_ENG_DEVICE_STATUS_UP );

   DM_CMN_Thread_sleep(1);

#ifdef STUN_ENABLED_ON_TR069_AGENT
    //----------------------------------------------------------------------------------
    // Start the STUN Client Thread
    //----------------------------------------------------------------------------------
   if (DM_CMN_Thread_create(DM_STUN_stunThread, NULL, false, &_stunThreadId) != 0)
   {
     EXEC_ERROR("Starting stun thread failed!!");
     ASSERT(FALSE);
   }
#endif
   
   // Wait Parameter Manager thread complete (should never occurs --> Infinite loop)
	 DM_ENG_ParameterManager_join();

#ifdef STUN_ENABLED_ON_TR069_AGENT
    if (_stunThreadId != 0)
    {
      DBG("Ask to close the stun client thread");
      DM_CMN_Thread_cancel( _stunThreadId );
      _stunThreadId=0;
    }
#endif

#ifdef _DBusIpcEnabled_
     // ---------------------------------------------------------------------------------------
     // Close the DBus Interface
     // ---------------------------------------------------------------------------------------
     DBG("Close DBUS Connection");
     DM_IPC_CloseDBusConnection();
#endif

     // ---------------------------------------------------------------------------------------
     // Stop and release the dm_com module
     // ---------------------------------------------------------------------------------------
     DBG("Stop DM_COM Module");
     DM_COM_STOP();

     // ---------------------------------------------------------------------------------------
     // Stop and release the dm_device module
     // ---------------------------------------------------------------------------------------
     DBG("Release DM_DEVICE");
     DM_ENG_Device_release();

     // ---------------------------------------------------------------------------------------
     // Free data stored in memory
     // ---------------------------------------------------------------------------------------
     DBG("Release ParameterManager");
     DM_ENG_ParameterManager_release();

		nRet = DM_OK;
	} else {
		DBG( "One of the mandatory parameters is/are missing!!" );
		DM_MAIN_ShowUsage( (char*)argv[0] );
	} /* IF */

   // ---------------------------------------------------------------------------
   // Remove the lock file : /var/run/adm/pid
   // ---------------------------------------------------------------------------
   memset(szFullPathPid, '\0', MAX_FILE_NAME_SIZE);
   snprintf( szFullPathPid, MAX_FILE_NAME_SIZE, "%s/%s", VARRUNPATH, PIDFILE );
   if ( DM_MAIN_CheckLockFile( szFullPathPid ) != -1 ) {
      DM_MAIN_RemoveLockFile( szFullPathPid );
   }

   // ---------------------------------------------------------------------------------------
   // Exit the binary
   // ---------------------------------------------------------------------------------------
   WARN( "[0] <--- [ Device Management Agent ended ] " );

   DM_CMN_Thread_lockMutex(_exitMutex);
   _exitInProgress = true;
   DM_CMN_Thread_signalCond(_exitSynchro);
   DM_CMN_Thread_unlockMutex(_exitMutex);

// It's better not to destroy these mutex and cond which are still used by runRescueExit
//   DM_CMN_Thread_destroyMutex(_exitMutex);
//   DM_CMN_Thread_destroyCond(_exitSynchro);

	// Close Trace Subsystem
#ifndef WIN32
   FTX_TRACES_CLOSE();
#else
   WIN_TRACE_CLOSE;
#endif

	return( nRet );
} /* MAIN */

/*
 * Run of thread that triggers a rescue exit after 10 s, if the agent not normally stop
 */
static void *runRescueExit(void* arg UNUSED)
{
   DM_CMN_Thread_lockMutex(_exitMutex);
   if (!_exitInProgress)
   {
      DM_CMN_Timespec_t timeout = { 10, 0 }; // 10 s d'attente max
      timeout.sec += time(NULL);
      DM_CMN_Thread_timedWaitCond(_exitSynchro, _exitMutex, &timeout);
   }
   DM_CMN_Thread_unlockMutex(_exitMutex);

   if (!_exitInProgress)
   {
      EXEC_ERROR("*** Forced exit after timeout ***");
      exit(1);
   }
   return 0;
}

/**
 * @brief Function called when a ctrl-C is catched. This function stop the Device Management Agent correctly
 *
 * @param  nSig number of the signal received
 *
 * @return none
 *
 * @remarks This function is defined as a static one 'cos it can be used to count signals
 * @remarks For now, when a signal trapped is received, we will quit the main application in the right way.
 * @remarks but it will be possible to do something else.
 *
 */
void
DM_Agent_Stop(int nSig UNUSED)
{
	WARN( "*** '%d' SIGNAL TERMINAISON CATCHED!! ***", nSig );

	// ---------------------------------------------------------------------------------------
	// Stop the Scheduler thread
	// ---------------------------------------------------------------------------------------
	DBG("Set ParameterManager to DOWN");
	DM_ENG_ParameterManager_dataNewValue( DM_ENG_DEVICE_STATUS_PARAMETER_NAME, DM_ENG_DEVICE_STATUS_DOWN );

   DM_CMN_Thread_create(runRescueExit, NULL, false, &_rescueExitThreadId);

} /* DM_Agent_Stop */

/**
 * @brief Function which display the usage of this binary
 *
 * @param  pBinary Name of the binary called
 *
 * @return none
 *
 */
static void
DM_MAIN_ShowUsage(IN const char *pBinaryName)
{
	printf( "The %s binary was expecting a parameter!!\n", pBinaryName);
	printf( "Usage : %s -p <CSV PATH> -c <PUB CERT> -k <PRIV KEY> -a <CERT AUTH> -x <PROXY>\n", pBinaryName );
	printf( "  -p PATH   : define the path to the datamodel used (CSV file)                 \n" );
	printf( "  -c SSL Device Pub Cert : path and filename which define the device public certificate (Mandatory when using https)\n" );
   printf( "  -k SSL Device RSA Key  : path and filename which define the device RSA key file (Mandatory when using https)\n" );
   printf( "  -a SSL CA ACS          : path and filename which define the ACS Certification Authority (Optional)\n" ); 
	printf( "  -x        : if set define the proxy address and port to use (ipaddress:port) \n\n" );	
	printf( "  -i ETH    : define the ethernet interface to use (eth0, br-lan, ...). Default is eth0 \n\n" );	

	printf( "Example : %s ./data (will search the ./data/%s) \n", pBinaryName, DATABASENAME );
} /* DM_MAIN_ShowUsage */

/**
 * @brief Function which display the usage of this binary
 *
 * @param  none
 *
 * @return Return -1 if the file doesn't exist else return 0
 *
 */
static int
DM_MAIN_CheckLockFile(IN const char * pPidfile)
{
	int	   nRet = -1;
  FILE * file = NULL;
  
  if(NULL != pPidfile) {
    file = fopen(pPidfile, "r");
    if (file != NULL)   {
        fclose(file);
        nRet = 0;
    }
  }

	return( nRet );
} /* DM_MAIN_CheckLockFile */

/**
 * @brief Function which display the usage of this binary
 *
 * @param  none
 *
 * @return Return -1 if the file was already existing of it a problem occured during the creation process ; else return 0
 *
 */
static int
DM_MAIN_CreateLockFile(IN const char *pPidfile,
                       IN       int   nPIDProcess)
{
	int	nRet 		= -1;
	size_t	nLength		= 0;
	int     nFd		= -1;
	char	szBuffer[10];
	mode_t	mode;

#ifndef WIN32
	mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP |  S_IROTH;
#else
	mode = S_IRUSR | S_IWUSR;
#endif
	nFd  = open( pPidfile, O_WRONLY | O_EXCL | O_CREAT, mode );
	if ( nFd != -1 )
	{
		//DBG( "Creating lock file (%s) : OK ", pPidfile );
		// bzero( szBuffer, 10 );
		memset((void *) szBuffer, 0x00, 10);
		
		// Writing the pid number inside it
		sprintf( szBuffer, "%d", nPIDProcess );
		nLength = (int)strlen( szBuffer );
		write( nFd, szBuffer, nLength );

		// Close the lock file
		close( nFd );
		nRet = 0;
	} else {
		//ERROR( "Creating lock file (%s) : NOK ", pPidfile );
	}
	
	return( nRet );
} /* DM_MAIN_CreateLockFile */

/**
 * @brief Function which display the usage of this binary
 *
 * @param  none
 *
 * @return Return -1 if the file haven't been found or if a problem occured during the removing step ; else return 0
 *
 */
static int
DM_MAIN_RemoveLockFile(IN const char *pPidfile)
{
	int nRet = -1;
	
	nRet = unlink( pPidfile );
	if ( nRet != -1 ) {
		nRet = 0;
	}
	
	return( nRet );
} /* DM_MAIN_RemoveLockFile */

/* END OF FILE */
