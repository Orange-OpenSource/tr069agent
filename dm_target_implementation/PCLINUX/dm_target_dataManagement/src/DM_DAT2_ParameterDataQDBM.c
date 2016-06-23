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
 * File        : DM_DAT2_ParameterDataQDBM.c
 *
 * Created     : 2008/06/05
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

#include "DM_ENG_ParameterData.h"
#include "DM_ENG_ParameterValueStruct.h"
#include "DM_ENG_EntityType.h"
#include "DM_GlobalDefs.h"
#include <sys/stat.h>
#include <depot.h>

const char* DM_ENG_PARAMETER_PREFIX = DM_PREFIX;

// Parameter names definitions
char* DM_TR106_ACS_URL                    = DM_PREFIX "ManagementServer.URL";
char* DM_TR106_ACS_USERNAME               = DM_PREFIX "ManagementServer.Username";
char* DM_TR106_ACS_PASSWORD               = DM_PREFIX "ManagementServer.Password";
char* DM_TR106_CONNECTIONREQUESTURL       = DM_PREFIX "ManagementServer.ConnectionRequestURL";
char* DM_TR106_CONNECTIONREQUESTUSERNAME  = DM_PREFIX "ManagementServer.ConnectionRequestUsername";
char* DM_TR106_CONNECTIONREQUESTPASSWORD  = DM_PREFIX "ManagementServer.ConnectionRequestPassword";

const char* DM_ENG_PARAMETER_KEY_NAME           = DM_PREFIX "ManagementServer.ParameterKey";
const char* DM_ENG_DEVICE_STATUS_PARAMETER_NAME = DM_PREFIX "DeviceInfo.DeviceStatus";

const char* DM_ENG_PERIODIC_INFORM_ENABLE_NAME  = DM_PREFIX "ManagementServer.PeriodicInformEnable";
const char* DM_ENG_PERIODIC_INFORM_INTERVAL_NAME = DM_PREFIX "ManagementServer.PeriodicInformInterval";
const char* DM_ENG_PERIODIC_INFORM_TIME_NAME    = DM_PREFIX "ManagementServer.PeriodicInformTime";

const char* DM_ENG_DEVICE_MANUFACTURER_NAME     = DM_PREFIX "DeviceInfo.Manufacturer";
const char* DM_ENG_DEVICE_MANUFACTURER_OUI_NAME = DM_PREFIX "DeviceInfo.ManufacturerOUI";
const char* DM_ENG_DEVICE_PRODUCT_CLASS_NAME    = DM_PREFIX "DeviceInfo.ProductClass";
const char* DM_ENG_DEVICE_SERIAL_NUMBER_NAME    = DM_PREFIX "DeviceInfo.SerialNumber";

static const char* PARAMETERS_INI_FILE = "parameters.csv";
static const char* PARAMETERS_DB_FILE = "parameters.db";
static const char* PARAMETERS_CTRL_FILE = "dm_control.data";
static const char* PARAMETERS_LOCK_FILE = "parameters.lock";

static const char* NULL_CODE = "_";
static const int MAX_NB_ATTR = 13;

#define MAX_LINE_LEN 800
static char _line[MAX_LINE_LEN];

static const char* _WRITABLE = "W";
//static const char* _READONLY = "R";

static const char* _YES = "Y";
//static const char* _NO = "N";

static char* _parametersIniFile = NULL;
static char* _parametersDbFile = NULL;
static char* _parametersCtrlFile = NULL;
static char* _parametersLockFile = NULL;

static time_t _lastModifCtrl = 0;
static DM_ENG_Parameter _current = { NULL, DM_ENG_ParameterType_UNDEFINED, 0, 0, 0, 0, 0, NULL, 0, 0, 0, 0, 0, NULL, NULL, NULL };

static DEPOT *_depot = NULL;

// Données persistantes en plus des paramètres
static DM_ENG_EventStruct* events = NULL;
static time_t periodicInformTime = 0;
static char* sScheduledInformTime = NULL;
static char* sScheduledInformCommandKey = NULL;
static char* sRebootCommandKey = NULL;
static DM_ENG_TransferRequest* transferRequests = NULL;
static DM_ENG_DownloadRequest* downloadRequests = NULL;
static int retryCount = 0;

static int _nbParameterWrite = 0;
static int _nbControlWrite = 0;

static int _readDmControl();
static void _writeDmControl();

static char* _resetCrc();
static char* _readln(FILE* fp);
static bool _checkCrc(FILE* fp);
static void _writeln(FILE* fp, char* line);
static void _writeCrc(FILE* fp);

static bool _controlDataChanged = false;

/**
 * Creates the parameters database with data in _parametersIniFile
 * @return Returns 0 if OK or 1 else
 */
static int _createDatabase()
{
   /* open the database */
   if(!(_depot = dpopen(_parametersDbFile, DP_OWRITER | DP_OCREAT, -1)))
   {
     fprintf(stderr, "dpopen: %s\n", dperrmsg(dpecode));
     return 1;
   }

   FILE *fp;
   char name[MAX_LINE_LEN];
   fp = fopen(_parametersIniFile, "r");
   if (fp == NULL) // fichier inexistant
   {
      fprintf(stderr, "*** Fichier %s introuvable ***\n", _parametersIniFile);
      return 1;
   }

   char* val;
   while( fgets(_line, MAX_LINE_LEN, fp) != NULL )
   {
      size_t len = strlen(_line);
      // sous Linux supprimer éventuellement les 2 derniers caracteres CR et LF
      while ((len>=1) && (_line[len-1]<' ')) { _line[--len] = '\0'; }
      if (len <= 1) break;

      val = strchr(_line, ';');
      if (val == NULL) break;

      *val = '\0';
      val++;

      strcpy(name, DM_ENG_PARAMETER_PREFIX);
      strcat(name, _line);

      /* store the record */
//      if(!dpput(_depot, _line, -1, val, -1, DP_DOVER))
      if(!dpput(_depot, name, -1, val, -1, DP_DOVER))
      {
        fprintf(stderr, "dpput: %s\n", dperrmsg(dpecode));
      }
   }
   fclose(fp);

   /* close the database */
   if(!dpclose(_depot))
   {
     fprintf(stderr, "dpclose: %s\n", dperrmsg(dpecode));
     return 1;
   }
   return 0;
}

/**
 * Initializes the parameters database access
 * @return Returns 0 if OK or 1 else
 */
static int _initDatabase(bool firstTime)
{
   /* open the database */
   if(!(_depot = dpopen(_parametersDbFile, DP_OWRITER, -1)))
   {
      fprintf(stderr, "dpopen: %s\n", dperrmsg(dpecode));
      if (firstTime && (dpecode == DP_EBROKEN) && dprepair(_parametersDbFile))
      {
         return _initDatabase(false);
      }
      return 1;
   }
  return 0;
}

void DM_ENG_ParameterData_init(const char* dataPath, bool factoryReset)
{
   if (dataPath != NULL)
   {
      if (_parametersIniFile!=NULL) { free(_parametersIniFile); }
      if (_parametersDbFile!=NULL) { free(_parametersDbFile); }
      if (_parametersLockFile!=NULL) { free(_parametersLockFile); }

      _parametersIniFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_INI_FILE) + 2);
      strcpy(_parametersIniFile, dataPath);
      strcat(_parametersIniFile, "/");
      strcat(_parametersIniFile, PARAMETERS_INI_FILE);
      _parametersDbFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_DB_FILE) + 2);
      strcpy(_parametersDbFile, dataPath);
      strcat(_parametersDbFile, "/");
      strcat(_parametersDbFile, PARAMETERS_DB_FILE);
      _parametersCtrlFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_CTRL_FILE) + 2);
      strcpy(_parametersCtrlFile, dataPath);
      strcat(_parametersCtrlFile, "/");
      strcat(_parametersCtrlFile, PARAMETERS_CTRL_FILE);
      _parametersLockFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_LOCK_FILE) + 2);
      strcpy(_parametersLockFile, dataPath);
      strcat(_parametersLockFile, "/");
      strcat(_parametersLockFile, PARAMETERS_LOCK_FILE);
   }
   else
   {
      if (_parametersIniFile==NULL) { _parametersIniFile = strdup(PARAMETERS_INI_FILE); }
      if (_parametersDbFile==NULL) { _parametersDbFile = strdup(PARAMETERS_DB_FILE); }
      if (_parametersCtrlFile==NULL) { _parametersCtrlFile = strdup(PARAMETERS_CTRL_FILE); }
      if (_parametersLockFile==NULL) { _parametersLockFile = strdup(PARAMETERS_LOCK_FILE); }
   }

   remove(_parametersLockFile); // suppression du verrou s'il était resté (suite à un plantage)
   int res = 1;
   if (factoryReset)
   {
      DBG("factoryReset");
      remove(_parametersDbFile);
      remove(_parametersCtrlFile);
      _lastModifCtrl = 0;
   }
   else
   {
      DBG("NOT factoryReset");
      res = _initDatabase(true);
      if (res != 0)
      {
         EXEC_ERROR("*** Initialization Error on database %s ***", _parametersDbFile);
         EXEC_ERROR("*** Factory Reset... ***");
         remove(_parametersDbFile); // ou renommer pour éviter de tout perdre ? !!
      }
      else
      {
         // Suppression des valeurs gardée en "cache" pour une question de performance et qui peuvent être modifiée au reboot
         for (DM_ENG_ParameterData_getFirst(); _current.name != NULL; DM_ENG_ParameterData_getNext())
         {
            if ((_current.storageMode != DM_ENG_StorageMode_DM_ONLY) && (_current.value != NULL))
            {
               if (_current.value != DM_ENG_EMPTY) { free(_current.value); }
               _current.value = NULL;
               DM_ENG_Parameter_setDataChanged(true);
               DM_ENG_ParameterData_sync();
            }
         }
         _readDmControl();
#ifdef NO_PERSISTENT_SCHEDULED_INFORM
      // effacer sScheduledInformTime et sScheduledInformCommandKey, si persistance désactivée
      DM_ENG_FREE(sScheduledInformTime);
      DM_ENG_FREE(sScheduledInformCommandKey);
#endif
      }
   }
   if (res != 0)
   {
      if ((_createDatabase() != 0) || (_initDatabase(false) != 0))
      {
         EXEC_ERROR("*** Impossible to create the database %s ***", _parametersDbFile);
         exit(1);
      }
      DM_ENG_addEventStruct(&events, DM_ENG_newEventStruct(DM_ENG_EVENT_BOOTSTRAP, NULL));
   }
}

static void _releaseData()
{
   /* close the database */
   if(!dpclose(_depot))
   {
     fprintf(stderr, "dpclose: %s\n", dperrmsg(dpecode));
   }
}

static void _releaseCtrl()
{
   // les données existantes éventuelles sont à supprimer
   if (events != NULL) { DM_ENG_deleteAllEventStruct(&events); }

   periodicInformTime = 0;
   if (sScheduledInformTime != NULL) { free(sScheduledInformTime); sScheduledInformTime = NULL; }
   if (sScheduledInformCommandKey != NULL) { free(sScheduledInformCommandKey); sScheduledInformCommandKey = NULL; }
   if (sRebootCommandKey != NULL) { free(sRebootCommandKey); sRebootCommandKey = NULL; }
   if (transferRequests != NULL) { DM_ENG_deleteAllTransferRequest(&transferRequests); transferRequests = NULL; }
}

void DM_ENG_ParameterData_release()
{
//   printf("NB écriture / Paramètres : %d, Contrôles : %d\n", _nbParameterWrite, _nbControlWrite);
   _releaseData();
   _releaseCtrl();
}

int DM_ENG_ParameterData_getNumberOfParameters()
{
   /* returns the number of the records, -1 if unsuccessful */
   return dprnum(_depot);
}

//////////////////////////////////////
//////// Accès aux paramètres ////////

static void _releaseCurrent()
{
   if (_current.name != NULL) // N.B. _current.name == NULL => tous les autres NULL
   {
      DM_ENG_ParameterData_sync();

      free(_current.name);
      _current.name = NULL;
      if (_current.accessList != NULL)
      {
         if (_current.accessList != DM_ENG_DEFAULT_ACCESS_LIST) { DM_ENG_deleteTabString(_current.accessList); }
         _current.accessList = NULL;
      }
      if (_current.value != NULL)
      {
         if (_current.value != DM_ENG_EMPTY) { free(_current.value); }
         _current.value = NULL;
      }
      if (_current.backValue != NULL)
      {
         if (_current.backValue != DM_ENG_EMPTY) { free(_current.backValue); }
         _current.backValue = NULL;
      }
      _current.type = DM_ENG_ParameterType_UNDEFINED;
   }
}

/**
 * Initializes an iterator and returns the name of the first parameter in the database
 * @return The first parameter in the database
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getFirst()
{
   DM_ENG_ParameterData_getFirstName();
   return DM_ENG_ParameterData_getCurrent();
}

/**
 * When an iterator is initialized and returns the name of the next parameter in the database
 * @return The next parameter or NULL if there is no more parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getNext()
{
   DM_ENG_ParameterData_getNextName();
   return DM_ENG_ParameterData_getCurrent();
}

/**
 * Initializes an iterator and returns the name of the first parameter in the database
 * @return The first parameter in the database
 */
char* DM_ENG_ParameterData_getFirstName()
{
   /* initialize the iterator */
   if(!dpiterinit(_depot))
   {
      fprintf(stderr, "dpiterinit: %s\n", dperrmsg(dpecode));
      return NULL;
   }
   return DM_ENG_ParameterData_getNextName();
}

/**
 * When an iterator is initialized and returns the name of the next parameter in the database
 * @return The next parameter or NULL if there is no more parameter
 */
char* DM_ENG_ParameterData_getNextName()
{
   _releaseCurrent();
   _current.name = dpiternext(_depot, NULL);
   return _current.name;
}

void DM_ENG_ParameterData_resumeParameterLoopFrom(char* resumeName)
{
   if ((_current.name == NULL) || (strcmp(_current.name, resumeName) != 0))
   {
      DM_ENG_ParameterData_getFirstName();
      while ((_current.name != NULL) && (strcmp(_current.name, resumeName) != 0)) { DM_ENG_ParameterData_getNextName(); }
   }
}

static char* _next;
static char _nullCode; // caractère utilisé pour représenter la valeur de chaîne NULL (distincte de la chaîne vide), positionné à 1 si pas de chaîne NULL possible 

static char* _nextToken()
{
   char* res = _next;
   while (*_next != '\0')
   {
      if ((*_next == ';') || (*_next < 32))
      {
         *_next = '\0';
         _next++;
         break;
      }
      _next++;
   }
   return (*res==_nullCode ? NULL : res);
}

static char* _firstToken(char* val, char nc)
{
   _next = val;
   _nullCode = nc;
   return _nextToken();
}

/**
 * @return The current parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getCurrent()
{
   if ((_depot == NULL) || (_current.name == NULL)) return NULL;

   if (_current.type == DM_ENG_ParameterType_UNDEFINED)
   {
      // Chargement du paramètre

      char* val;
      if(!(val = dpget(_depot, _current.name, -1, 0, -1, NULL)))
      {
        //fprintf(stderr, "dpget: %s\n", dperrmsg(dpecode));
        return NULL;
      }

      char* attr = _firstToken(val, 1);
      _current.minValue = 0; // par défaut
      _current.maxValue = 0; // par défaut
      _current.type = DM_ENG_stringToType(attr, &_current.minValue, &_current.maxValue);

      if (_current.type != DM_ENG_ParameterType_UNDEFINED)
      {
         attr = _nextToken();
         // 0 : DM_ENG_StorageMode_DM_ONLY, 1 : DM_ENG_StorageMode_SYSTEM_ONLY, 2 : DM_ENG_StorageMode_MIXED
         _current.storageMode = DM_ENG_charToInt(*attr);
         if ((int)_current.storageMode == -1) { _current.storageMode = DM_ENG_StorageMode_DM_ONLY; }

         attr = _nextToken();
         // 2 codages possibles : codage chiffré ou littéral (R/W)
         int iAttr = DM_ENG_charToInt(*attr);
         _current.writable = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr, _WRITABLE)==0) );

         attr = _nextToken();
         // 2 codages possibles : codage chiffré ou littéral (Y/N)
         iAttr = DM_ENG_charToInt(*attr);
         _current.immediateChanges = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr, _YES)==0) );

         attr = _nextToken();
         // 0 : pas de notification, 1 : notif passive, 2 : notif active
         _current.notification = DM_ENG_charToInt(*attr);
         if ((int)_current.notification == -1) { _current.notification = DM_ENG_NotificationMode_OFF; }

         attr = _nextToken();
         // 2 codages possibles : codage chiffré ou littéral (Y/N)
         iAttr = DM_ENG_charToInt(*attr);
         _current.mandatoryNotification = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr, _YES)==0) );

         attr = _nextToken();
         // 2 codages possibles : codage chiffré ou littéral (Y/N)
         iAttr = DM_ENG_charToInt(*attr);
         _current.activeNotificationDenied = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr, _YES)==0) );

         attr = _nextToken();
         // NULL si chaîne vide
         _current.accessList = (*attr==0 ? NULL
                             : (strcmp(attr, DM_ENG_DEFAULT_ACCESS_LIST[0])==0 ? (char**)DM_ENG_DEFAULT_ACCESS_LIST : DM_ENG_splitCommaSeparatedList(attr)));

         attr = _nextToken();
         // NULL si chaîne vide
         _current.value = (*attr==0 ? NULL : strdup(attr));

         attr = _nextToken();
         // codé sinon 0
         iAttr = DM_ENG_charToInt(*attr);
         _current.loadingMode = ( iAttr != -1 ? iAttr : 0 );

         attr = _nextToken();
         // codé sinon 0
         iAttr = DM_ENG_charToInt(*attr);
         _current.state = ( iAttr != -1 ? iAttr : 0 );

         attr = _nextToken();
         // NULL si chaîne vide
         _current.backValue = (*attr==0 ? NULL : strdup(attr));
      }

      free(val);
   }
   return &_current;
}

/**
 *
 * @param name Name of a requested Parameter.
 *
 * @return The parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getParameter(const char* name)
{
   if (name == NULL) return NULL;

   if ((_current.name != NULL) && (strcmp(_current.name, name) != 0))
   {
      _releaseCurrent();
   }

   if (_current.name == NULL) { _current.name = strdup(name); }

   return DM_ENG_ParameterData_getCurrent();
}

static void _setCurrent(DM_ENG_Parameter* param)
{
   if ((_current.name != NULL) && (strcmp(_current.name, param->name) != 0))
   {
      _releaseCurrent(); // sauvegarde en même temps le current précédent si nom différent
   }

   if (_current.name == NULL) { _current.name = strdup(param->name); }

   _current.type = param->type;
   _current.storageMode = param->storageMode;
   _current.writable = param->writable;
   _current.immediateChanges = param->immediateChanges;

   if ((_current.accessList != NULL) && (_current.accessList != DM_ENG_DEFAULT_ACCESS_LIST)) { DM_ENG_deleteTabString(_current.accessList); }
   _current.accessList = (param->accessList==NULL ? NULL : DM_ENG_copyTabString(param->accessList));

   _current.notification = param->notification;
   _current.mandatoryNotification = param->mandatoryNotification;
   _current.activeNotificationDenied = param->activeNotificationDenied;
   _current.loadingMode = param->loadingMode;
   _current.state = param->state;

   if ((_current.backValue != NULL) && (_current.backValue != DM_ENG_EMPTY)) { free(_current.backValue); }
   _current.backValue = (param->backValue==NULL ? NULL : strdup(param->backValue));

   if ((_current.value != NULL) && (_current.value != DM_ENG_EMPTY)) { free(_current.value); }
   _current.value = (param->value==NULL ? NULL : strdup(param->value));

   DM_ENG_Parameter_setDataChanged(true);
}

void DM_ENG_ParameterData_createInstance(DM_ENG_Parameter* param, unsigned int instanceNumber)
{
   char* objectName = strdup(param->name);
   sprintf(_line, "%s%u.", objectName, instanceNumber);
   DM_ENG_Parameter* first = DM_ENG_newInstanceParameter(_line, param);
   DM_ENG_Parameter* prev = first;
   size_t objLen = strlen(objectName);
   char* name = NULL;
   for (name = DM_ENG_ParameterData_getFirstName(); name!=NULL; name = DM_ENG_ParameterData_getNextName())
   {
      if ((strlen(name) > objLen) && (strncmp(name, objectName, objLen)==0) && (name[objLen]=='.'))
      {
         sprintf(_line+objLen, "%u%s", instanceNumber, name+objLen);
         prev->next = DM_ENG_newInstanceParameter(_line, DM_ENG_ParameterData_getCurrent());
         prev = prev->next;
      }
   }
   free(objectName);

   // Stockage de tous les paramètres créés
   DM_ENG_Parameter* prm = NULL;
   for (prm = first; prm!=NULL; prm = prm->next)
   {
      _setCurrent(prm);
   }
   DM_ENG_ParameterData_sync();
   DM_ENG_deleteAllParameter(&first);
}

void DM_ENG_ParameterData_deleteObject(char* objectName)
{
   // Recherche des paramètres à supprimer
   DM_ENG_ParameterName* aSupprimer = NULL;
   size_t objLen = strlen(objectName);
   char* name = NULL;
   for (name = DM_ENG_ParameterData_getFirstName(); name!=NULL; name = DM_ENG_ParameterData_getNextName())
   {
      if (strncmp(name, objectName, objLen)==0) // param à supprimer
      {
         DM_ENG_addParameterName(&aSupprimer, DM_ENG_newParameterName(name));
      }
   }

   // Suppression effective
   DM_ENG_ParameterName* prmName = aSupprimer;
   while (prmName != NULL)
   {
      /* delete the record */
      if(!dpout(_depot, prmName->name, -1))
      {
         fprintf(stderr, "dpout: %s\n", dperrmsg(dpecode));
      }
      prmName = prmName->next;
   }
   DM_ENG_deleteAllParameterName(&aSupprimer);
}

//////////////////////////////////
//////// Opérations d'E/S ////////

static void setEvents(char* line)
{
   if (line == NULL) return;
   char* code = _firstToken(line, *NULL_CODE);
   while ((code!=NULL) && (*code!='\0'))
   {
      char* key = _nextToken();
      DM_ENG_addEventStruct(&events, DM_ENG_newEventStruct(code, key));
      code = _nextToken();
   }
}

static void setOthersData(char* line)
{
   if (line == NULL) return;
   periodicInformTime = atol(_firstToken(line, *NULL_CODE));
   char* next = _nextToken();
   sScheduledInformTime = (next==NULL ? NULL : strdup(next));
   next = _nextToken();
   sScheduledInformCommandKey = (next==NULL ? NULL : strdup(next));
   next = _nextToken();
   sRebootCommandKey = (next==NULL ? NULL : strdup(next));
   next = _nextToken();
   retryCount = (next==NULL ? 0 : atoi(next));
}

static void addNewTransferRequest(char* line)
{
   if (line == NULL) return;
   bool d = atoi(_firstToken(line, *NULL_CODE));
   int s = atoi(_nextToken());
   time_t t = atol(_nextToken());
   char* ft = _nextToken();
   char* url = _nextToken();
   char* un = _nextToken();
   char* pw = _nextToken();
   unsigned int fs = atol(_nextToken());
   char* tfn = _nextToken();
   char* su = _nextToken();
   char* fu = _nextToken();
   char* ck = _nextToken();
   char* au = _nextToken();
   unsigned int code = atol(_nextToken());
   time_t start = atol(_nextToken());
   time_t comp = atol(_nextToken());
   unsigned int id = atol(_nextToken());
   DM_ENG_TransferRequest* dr = DM_ENG_newTransferRequest(d, s, t, ft, url, un, pw, fs, tfn, su, fu, ck, au);
   dr->faultCode = code;
   dr->startTime = start;
   dr->completeTime = comp;
   dr->transferId = id;
   DM_ENG_addTransferRequest(&transferRequests, dr);
}

static void addNewDownloadRequest(char* line)
{
   if (line == NULL) return;
   char* ft = _firstToken(line, *NULL_CODE);
   int nbArgs = atoi(_nextToken());
   DM_ENG_ArgStruct* args[nbArgs+1];
   int i;
   for (i=0; i<nbArgs; i++)
   {
      char* name = _nextToken();
      char* val = _nextToken();
      args[i] = DM_ENG_newArgStruct(name, val);
   }
   args[nbArgs] = NULL;
   DM_ENG_addDownloadRequest(&downloadRequests, DM_ENG_newDownloadRequest(ft, args));
   for (i=0; i<nbArgs; i++)
   {
      DM_ENG_deleteArgStruct(args[i]);
   }
}

/**
 * Lit les données de contrôle
 * N.B. La lecture n'est effectivement réalisée que si les données ont été modifiées (par une cause extérieure)
 */
static int _readDmControl()
{
   FILE *fp;
   struct stat prop;
   char* line;
   fp = fopen(_parametersCtrlFile, "r");
   if (fp == NULL) return 1; // fichier inexistant

   fstat(fileno(fp), &prop);
   if (prop.st_mtime > _lastModifCtrl) // les données ont été modifiées ou ne sont pas encore lues
   {
      _releaseCtrl();

      _lastModifCtrl = prop.st_mtime;
      if (_checkCrc(fp))
      {
         setEvents( _readln(fp) );
         setOthersData( _readln(fp) );
         while( ((line = _readln(fp)) != NULL) && (*line != '#') )
         {
            addNewTransferRequest(line);
         }
         if (line != NULL)
         {
            while( (line = _readln(fp)) != NULL )
            {
               addNewDownloadRequest(line);
            }
         }
      }
      else
      {
         WARN( "CRC Error => Control data lost" );
      }
   }
   fclose(fp);
   return 0;
}

static const int _CONST_SIZE_DBVAL = 75;

static void _storeCurrent()
{
   if (_current.name != NULL)
   {
      int size = _CONST_SIZE_DBVAL;
      char* al = DM_ENG_buildCommaSeparatedList(_current.accessList);
      if (al != NULL) { size+= strlen(al); }
      if (_current.value != NULL) { size+= strlen(_current.value); }
      if (_current.backValue != NULL) { size+= strlen(_current.backValue); }
      char* dbVal = (char*) malloc(size);

      char* val = _current.value;
      char* bVal = _current.backValue;
      char* sType = DM_ENG_typeToString(_current.type, _current.minValue, _current.maxValue);
      sprintf(dbVal, "%s;%d;%d;%d;%d;%d;%d;%s;%s;%d;%d;%s", sType,
              _current.storageMode, _current.writable, _current.immediateChanges,
              _current.notification, _current.mandatoryNotification, _current.activeNotificationDenied, (al==0 ? "" : al),
              (val==0 ? "" : val), _current.loadingMode, _current.state, (bVal==0 ? "" : bVal));

      /* store the record */
      if(!dpput(_depot, _current.name, -1, dbVal, -1, DP_DOVER))
      {
         fprintf(stderr, "dpput: %s\n", dperrmsg(dpecode));
      }
      free(sType);
      free(dbVal);
      DM_ENG_FREE(al);
   }
}

static void _writeDmControl()
{
   FILE *fp;
   struct stat prop;
   fp = fopen(_parametersCtrlFile, "w");
   if (fp)
   {
      _resetCrc();
      *_line = '\0';
      DM_ENG_EventStruct* evt = events;
      while (evt!=NULL)
      {
         sprintf(_line+strlen(_line), (evt->next==NULL ? "%s;%s" : "%s;%s;"), evt->eventCode,
                 (evt->commandKey==NULL ? NULL_CODE : evt->commandKey));
         evt = evt->next;
      }
      _writeln(fp, _line);
      sprintf(_line, "%ld;%s;%s;%s;%d", periodicInformTime,
        (sScheduledInformTime==NULL ? NULL_CODE : sScheduledInformTime),
        (sScheduledInformCommandKey==NULL ? NULL_CODE : sScheduledInformCommandKey),
        (sRebootCommandKey==NULL ? NULL_CODE : sRebootCommandKey),
         retryCount );
      _writeln(fp, _line);
      DM_ENG_TransferRequest* dr = transferRequests;
      while(dr!=NULL)
      {
         sprintf(_line, "%d;%d;%ld;%s;%s;%s;%s;%u;%s;%s;%s;%s;%s;%u;%ld;%ld;%u",
            dr->isDownload, dr->state, dr->transferTime,
            (dr->fileType==NULL ? NULL_CODE : dr->fileType),
            (dr->url==NULL ? NULL_CODE : dr->url),
            (dr->username==NULL ? NULL_CODE : dr->username),
            (dr->password==NULL ? NULL_CODE : dr->password),
            dr->fileSize,
            (dr->targetFileName==NULL ? NULL_CODE : dr->targetFileName),
            (dr->successURL==NULL ? NULL_CODE : dr->successURL),
            (dr->failureURL==NULL ? NULL_CODE : dr->failureURL),
            (dr->commandKey==NULL ? NULL_CODE : dr->commandKey),
            (dr->announceURL==NULL ? NULL_CODE : dr->announceURL),
            dr->faultCode, dr->startTime, dr->completeTime, dr->transferId );
         _writeln(fp, _line);
         dr = dr->next;
      }
      if (downloadRequests != NULL)
      {
         _line[0] = '#'; _line[1] = '\0';
         _writeln(fp, _line);
         DM_ENG_DownloadRequest* dr = downloadRequests;
         while (dr!=NULL)
         {
            DM_ENG_ArgStruct** args = dr->args;
            sprintf(_line, "%s;%d", dr->fileType, DM_ENG_tablen((void**)args));
            if (args != NULL)
            {
               int i;
               for (i=0; args[i] != NULL; i++)
               {
                  sprintf(_line+strlen(_line), ";%s;%s", args[i]->name, (args[i]->value==NULL ? NULL_CODE : args[i]->value));
               }
            }
            _writeln(fp, _line);
            dr = dr->next;
         }
      }
      _writeCrc(fp);
      fclose(fp);
      fp = fopen(_parametersCtrlFile, "r");
      if (fp)
      {
         fstat(fileno(fp), &prop);
         _lastModifCtrl = prop.st_mtime;
         fclose(fp);
      }
   }
}

/////////////////////////////////////////////////
//////////////// Periodic Inform ////////////////

time_t DM_ENG_ParameterData_getPeriodicInformTime()
{
   return periodicInformTime;
}

void DM_ENG_ParameterData_setPeriodicInformTime(time_t t)
{
   periodicInformTime = t;
   _controlDataChanged = true;
}

/////////////////////////////////////////////////
/////////////// Scheduled Inform ////////////////

/**
 *
 */
time_t DM_ENG_ParameterData_getScheduledInformTime()
{
   if (sScheduledInformTime == NULL) return 0;

   char* virg = strchr(sScheduledInformTime, ',');
   if (virg != NULL) { *virg = '\0'; }
   time_t scheduledInformTime = atol(sScheduledInformTime);
   if (virg != NULL) { *virg = ','; }
   return scheduledInformTime;
}

/**
 *
 */
char* DM_ENG_ParameterData_getScheduledInformCommandKey()
{
   if (sScheduledInformCommandKey == NULL) return NULL;

   char* virg = strchr(sScheduledInformCommandKey, ',');
   if (virg != NULL) { *virg = '\0'; }
   char* scheduledInformCommandKey = strdup(sScheduledInformCommandKey);
   if (virg != NULL) { *virg = ','; }
   return scheduledInformCommandKey;
}

/**
 *
 */
void DM_ENG_ParameterData_nextScheduledInformCommand()
{
   if ((sScheduledInformTime == NULL) || (sScheduledInformCommandKey == NULL)) return;

   char* oldScheduledInformTime = sScheduledInformTime;
   char* oldScheduledInformCommandKey = sScheduledInformCommandKey;
   char* virg = strchr(sScheduledInformTime, ',');
   sScheduledInformTime = ((virg == NULL) ? NULL : strdup(virg+1));
   virg = strchr(sScheduledInformCommandKey, ',');
   sScheduledInformCommandKey = ((virg == NULL) ? NULL : strdup(virg+1));
   free(oldScheduledInformTime);
   free(oldScheduledInformCommandKey);
   _controlDataChanged = true;
}

/**
 * Add a scheduled Inform
 */
void DM_ENG_ParameterData_addScheduledInformCommand(time_t time, char* commandKey)
{
   if ((time > 0) && (commandKey != NULL))
   {
      char* sTime = DM_ENG_longToString(time);
      if (sScheduledInformTime == NULL) // && (sScheduledInformCommandKey == NULL)
      {
         sScheduledInformTime = sTime;
         if (sScheduledInformCommandKey != NULL) { free(sScheduledInformCommandKey); } // toujours faux !!
         sScheduledInformCommandKey = strdup(commandKey);
      } else {
          size_t sizeNewScheduledInformTime = strlen(sScheduledInformTime)+strlen(sTime)+2;
          char* newScheduledInformTime = (char*)malloc(sizeNewScheduledInformTime);
    
          size_t sizenewScheduledInformCommandKey = strlen(sScheduledInformCommandKey)+strlen(commandKey)+2;
          char* newScheduledInformCommandKey = (char*)malloc(sizenewScheduledInformCommandKey);
          
         // *newScheduledInformTime = '\0';
         // *newScheduledInformCommandKey = '\0';
          memset((void *) newScheduledInformTime,       '\0', sizeNewScheduledInformTime);
          memset((void *) newScheduledInformCommandKey, '\0', sizenewScheduledInformCommandKey);    
          
         time_t prevTime = DM_ENG_ParameterData_getScheduledInformTime();
         while ((prevTime!=0) && (prevTime <= time))
         {
            char* sPrevTime = DM_ENG_longToString(prevTime);
            char* prevCmdKey = DM_ENG_ParameterData_getScheduledInformCommandKey();
            DM_ENG_ParameterData_nextScheduledInformCommand();
            strcat(newScheduledInformTime, sPrevTime);
            strcat(newScheduledInformTime, ",");
            strcat(newScheduledInformCommandKey, prevCmdKey);
            strcat(newScheduledInformCommandKey, ",");
            free(sPrevTime);
            free(prevCmdKey);
            prevTime = DM_ENG_ParameterData_getScheduledInformTime();
         }
         strcat(newScheduledInformTime, sTime);
         free(sTime);
         strcat(newScheduledInformCommandKey, commandKey);
         if (prevTime != 0)
         {
            strcat(newScheduledInformTime, ",");
            strcat(newScheduledInformTime, sScheduledInformTime);
            strcat(newScheduledInformCommandKey, ",");
            strcat(newScheduledInformCommandKey, sScheduledInformCommandKey);
         }
         if (sScheduledInformTime != NULL) { free(sScheduledInformTime); }
         if (sScheduledInformCommandKey != NULL) { free(sScheduledInformCommandKey); }
         sScheduledInformTime = newScheduledInformTime;
         sScheduledInformCommandKey = newScheduledInformCommandKey;
      }
      _controlDataChanged = true;
   }
}

/////////////////////////////////////////////////
////////////// Gestion des events ///////////////

bool DM_ENG_ParameterData_isInEvents(const char* evCode)
{
   DM_ENG_EventStruct* evt = events;
   while ((evt != NULL) && (strcmp(evt->eventCode, evCode) != 0))
   {
      evt = evt->next;
   }
   return (evt!=NULL);
}

/*
 * On ajoute un événement s'il n'est pas déjà présent
 */
void DM_ENG_ParameterData_addEvent(const char* ec, char* ck)
{
   DM_ENG_EventStruct* evt = events;
   while (evt != NULL)
   {
      if ((evt->eventCode == ec) && (evt->commandKey==NULL ? (ck==NULL) : ((ck!=NULL) && (strcmp(evt->commandKey, ck)==0))))
      {
         return; // déjà présent
      }
      evt = evt->next;
   }
   DM_ENG_addEventStruct(&events, DM_ENG_newEventStruct(ec, ck));
   _controlDataChanged = true;
}

void DM_ENG_ParameterData_deleteEvent(const char* evCode)
{
   DM_ENG_EventStruct** pEvent = &events;
   DM_ENG_EventStruct* ev = events;
   while (ev != NULL)
   {
      if (ev->eventCode == evCode)
      {
         *pEvent = ev->next;
         DM_ENG_deleteEventStruct(ev);
         ev = *pEvent;
         _controlDataChanged = true;
      }
      else
      {
         pEvent = &ev->next;
         ev = ev->next;
      }
   }
}

void DM_ENG_ParameterData_deleteAllEvents()
{
   DM_ENG_deleteAllEventStruct(&events);
   _controlDataChanged = true;
}

DM_ENG_EventStruct* DM_ENG_ParameterData_getEvents()
{
   return events;
}

/////////////////////////////////////////////////
//////// Requêtes de transfer en attente ////////

DM_ENG_TransferRequest* DM_ENG_ParameterData_getTransferRequests()
{
   return transferRequests;
}

void DM_ENG_ParameterData_insertTransferRequest(DM_ENG_TransferRequest* tr)
{
   _controlDataChanged = true;

   if (transferRequests == NULL)
   {
      tr->transferId = 1;
      transferRequests = tr;
      return;
   }

   DM_ENG_TransferRequest* prev = NULL;
   DM_ENG_TransferRequest* curr = transferRequests;
   while ((curr != NULL) && (tr->transferTime >= curr->transferTime))
   {
      prev = curr;
      curr = prev->next;
   }

   // calcul du transferId
   if ((prev == NULL) || ((curr != NULL) && (curr->transferId > prev->transferId)))
   {
      tr->transferId = 2*curr->transferId;
   }
   else
   {
      tr->transferId = 2*prev->transferId+1;
   }

   tr->next = curr;
   if (prev == NULL)
   {
      transferRequests = tr;
   }
   else
   {
      prev->next = tr;
   }
}

void DM_ENG_ParameterData_removeTransferRequest(unsigned int transferId)
{
   DM_ENG_TransferRequest* prev = NULL;
   DM_ENG_TransferRequest* curr = transferRequests;
   while ((curr != NULL) && (curr->transferId != transferId))
   {
      prev = curr;
      curr = prev->next;
   }
   if (curr != NULL)
   {
      if (prev == NULL)
      {
         transferRequests = curr->next;
      }
      else
      {
         prev->next = curr->next;
      }
      DM_ENG_deleteTransferRequest(curr);
      _controlDataChanged = true;
   }
}

DM_ENG_TransferRequest* DM_ENG_ParameterData_updateTransferRequest(unsigned int transferId, int state, unsigned int code, time_t start, time_t comp)
{
   DM_ENG_TransferRequest* curr = transferRequests;
   while ((curr != NULL) && (curr->transferId != transferId))
   {
      curr = curr->next;
   }
   if (curr != NULL)
   {
      curr->state = state;
      curr->faultCode = code;
      curr->startTime = start;
      curr->completeTime = comp;
      _controlDataChanged = true;
   }
   return curr;
}

//////////////////////////////////////////////
//////// Download Request from device ////////

/**
 * Returns the first element of the download request list.
 * 
 * @return Pointer to the first download request
 */
DM_ENG_DownloadRequest* DM_ENG_ParameterData_getDownloadRequests()
{
   return downloadRequests;
}

/**
 * Adds a request download in the list
 * 
 * @param dr A download request from device
 */
void DM_ENG_ParameterData_addDownloadRequest(DM_ENG_DownloadRequest* dr)
{
   DM_ENG_addDownloadRequest(&downloadRequests, dr);
   _controlDataChanged = true;
}

/**
 * Removes the firt download request in the list
 */
void DM_ENG_ParameterData_removeFirstDownloadRequest()
{
   DM_ENG_DownloadRequest* first = downloadRequests;
   if (first != NULL)
   {
      downloadRequests = first->next;
      DM_ENG_deleteDownloadRequest(first);
      _controlDataChanged = true;
   }
}

////////////////////////////////////
//////// Reboot Command Key ////////

/**
 * Delegation call for ParametersData.setRebootCommandKey()
 */
void DM_ENG_ParameterData_setRebootCommandKey(char* commandKey)
{
   if (sRebootCommandKey != commandKey)
   {
      if (sRebootCommandKey != NULL) { free(sRebootCommandKey); }
      sRebootCommandKey = (commandKey==NULL ? NULL : strdup(commandKey));
      _controlDataChanged = true;
   }
}

/**
 * Add a Reboot commandKey
 */
void DM_ENG_ParameterData_addRebootCommandKey(char* commandKey)
{
   if (commandKey != NULL)
   {
      if (sRebootCommandKey == NULL)
      {
         sRebootCommandKey = strdup(commandKey);
      }
      else if (*commandKey=='*')
      {
         *sRebootCommandKey = '*';
      }
      else
      {
         char* oldCommandKey = sRebootCommandKey;
         sRebootCommandKey = (char*)malloc(strlen(oldCommandKey) + strlen(commandKey) + 2);
         strcpy(sRebootCommandKey, oldCommandKey);
         strcat(sRebootCommandKey, ",");
         strcat(sRebootCommandKey, commandKey);
         free(oldCommandKey);
      }
      _controlDataChanged = true;
   }
}

/**
 * Delegation call for ParametersData.getRebootCommandKey()
 */
char* DM_ENG_ParameterData_getRebootCommandKey()
{
   return sRebootCommandKey;
}

/////////////////////////////////
////////// Retry count //////////

/**
 *
 */
void DM_ENG_ParameterData_resetRetryCount()
{
   retryCount = 0;
   _controlDataChanged = true;
}

/**
 * Delegation call for ParametersData.getRetryCount()
 */
int DM_ENG_ParameterData_getRetryCount()
{
   return retryCount;
}

/**
 * Delegation call for ParametersData.incRetryCount()
 */
int DM_ENG_ParameterData_incRetryCount()
{
   retryCount++;
   _controlDataChanged = true;
   return retryCount;
}

/**
 * Cette méthode doit être appelée lorsque les data sont verrouillées.
 * Elle permet la sauvegarde des données avant déverrouillage au cas où on aurait un plantage
 * ou un redémarrage avant un unlock().
 */
void DM_ENG_ParameterData_sync()
{
   /*if (_controlDataChanged || DM_ENG_Parameter_isDataChanged())
   {
      fprintf(stderr,"+++ %d %d\n", _controlDataChanged, DM_ENG_Parameter_isDataChanged());
   }*/
   if (_controlDataChanged)
   {
      _nbControlWrite++;
      _writeDmControl();
      _controlDataChanged = false;
   }
   if (DM_ENG_Parameter_isDataChanged())
   {
      _nbParameterWrite++;
      _storeCurrent();
      DM_ENG_Parameter_setDataChanged(false);
   }
}

////////////////////////////////
//////// Parameter lock ////////

/**
 * Verrouille la ressource "paramètres" si disponible
 * @return true si la ressource a pu être verrouillée
 */
bool DM_ENG_ParameterData_tryLock()
{
   bool resOk = false;
   FILE *fp = fopen(_parametersLockFile, "r");

   if (fp == NULL) // fichier inexistant, ressource "paramètres" disponible
   {
      resOk = true;
      fp = fopen(_parametersLockFile, "w");
      if (fp == NULL) return false;
   }
   fclose(fp);

   if (resOk)
   {
      _readDmControl(); // N.B. Le fichier est lu seulement si modifié depuis la dernière lecture
   }
   return resOk;
}

/**
 * Supprime le verrou sur la ressource "paramètres"
 */
void DM_ENG_ParameterData_unLock()
{
   remove(_parametersLockFile);
}

/////////////////////////////////
//////// Calcul du CRC32 ////////

static unsigned long _crc32 = 0L;
static unsigned long _crc32Table[256];
static bool _crc32TableComputed = false;

static void _createCrc32Table()
{
  unsigned long c;
  int n, k;
  for (n = 0; n < 256; n++)
  {
    c = (unsigned long) n;
    for (k = 0; k < 8; k++)
    {
      c = (c & 1 ? 0xedb88320L ^ (c >> 1) : c >> 1);
    }
    _crc32Table[n] = c;
  }
  _crc32TableComputed = true;
}

static void _updateCrc32(unsigned char *buf)
{
  unsigned long c = _crc32 ^ 0xffffffffL;
  int n;

  if (!_crc32TableComputed) { _createCrc32Table(); }
  for (n = 0; buf[n] != '\0'; n++)
  {
    c = _crc32Table[(c ^ buf[n]) & 0xff] ^ (c >> 8);
  }
  _crc32 = c ^ 0xffffffffL;
}

////////////////////////////////////////////////////////////////////////////
//////// Lecture et écriture des données avec vérification du CRC32 ////////

static bool _crcOK = false;

static char* _resetCrc()
{
   _crc32 = 0L;
   _crcOK = false;
  return NULL;
}


static char* _readln(FILE* fp)
{
   if (fgets(_line, MAX_LINE_LEN, fp) == NULL)
   {
      return NULL;
   }

   size_t len = strlen(_line);
   // sous Linux supprimer éventuellement les 2 derniers caracteres CR et LF
   while ((len>=1) && (_line[len-1]<' ')) { _line[--len] = '\0'; }

   if ((len>1) && (*_line == '*')) // ligne "*..." -> vérification du CRC
   {
      char sCrc32[10];
      sprintf(sCrc32, "%lX", _crc32);
      _crcOK = (strcmp(sCrc32, _line+1) == 0) || (_line[1] == '*'); // si ligne == "**..." on omet la vérification
      if (!_crcOK)
      {
         EXEC_ERROR( "CRC Error" );
      }
      return NULL;
   }
   _updateCrc32((unsigned char *) _line);
   return _line;
}

static bool _checkCrc(FILE* fp)
{
   _resetCrc();
   while (_readln(fp) != NULL);
   rewind(fp);
   _crc32 = 0L;
   return _crcOK;
}

static void _writeln(FILE* fp, char* line)
{
   fprintf(fp, "%s\n", line);
   _updateCrc32((unsigned char *) line);
}

static void _writeCrc(FILE* fp)
{
   fprintf(fp, "*%lX\n", _crc32);
}
