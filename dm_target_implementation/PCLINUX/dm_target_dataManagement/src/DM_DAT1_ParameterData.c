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
 * File        : DM_DAT1_ParameterData.c
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
#include "DM_ENG_Global.h"
#include "DM_GlobalDefs.h"
#include "CMN_Trace.h"
#include <sys/stat.h>
#include <stdlib.h>

static const char* PARAMETERS_INI_FILE = "parameters.csv";
static const char* PARAMETERS_OBJ_FILE = "parameters.data";
static const char* PARAMETERS_BACKUP_OBJ_FILE = "parameters.data~";
static const char* PARAMETERS_CTRL_FILE = "dm_control.data";
static const char* PARAMETERS_LOCK_FILE = "parameters.lock";

static const char* NULL_CODE = "_";
static const int MAX_NB_ATTR = 15;

#define MAX_LINE_LEN 800
static char _line[MAX_LINE_LEN];

static char* _parametersIniFile = NULL;
static char* _parametersObjFile = NULL;
static char* _parametersBackupObjFile = NULL;
static char* _parametersCtrlFile = NULL;
static char* _parametersLockFile = NULL;

static DM_ENG_Parameter* parameters = NULL;
static int nbParameters = 0;
static time_t _lastModifData = 0;
static time_t _lastModifCtrl = 0;

// Données persistantes en plus des paramètres
static DM_ENG_EventStruct* events = NULL;
static time_t periodicInformTime = 0;
static char* sScheduledInformTime = NULL;
static char* sScheduledInformCommandKey = NULL;
static char* sRebootCommandKey = NULL;
static DM_ENG_TransferRequest* transferRequests = NULL;
static unsigned int _lastTransferId = 0;
static DM_ENG_DownloadRequest* downloadRequests = NULL;
static int retryCount = 0;

static int _nbParameterWrite = 0;
static int _nbControlWrite = 0;

static int _readFileParameters(const char* fichierLu);
static void _writeParameters();
static int _readDmControl();
static void _writeDmControl();

static char* _resetCrc();
static bool _isCrcOK();
static char* _readln(FILE* fp);
static bool _checkCrc(FILE* fp);
static void _writeln(FILE* fp, char* line);
static void _writeCrc(FILE* fp);

static bool _controlDataChanged = false;

int DM_ENG_ParameterData_init(const char* dataPath, bool factoryReset)
{
   DBG("DM_ENG_ParameterData_init - Begin");
   if (dataPath != NULL)
   {
      DBG("dataPath != NULL");

      if (_parametersIniFile!=NULL) { free(_parametersIniFile); }
      if (_parametersObjFile!=NULL) { free(_parametersObjFile); }
      if (_parametersBackupObjFile!=NULL) { free(_parametersBackupObjFile); }
      if (_parametersCtrlFile!=NULL) { free(_parametersCtrlFile); }
      if (_parametersLockFile!=NULL) { free(_parametersLockFile); }

      _parametersIniFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_INI_FILE) + 2);
      strcpy(_parametersIniFile, dataPath);
      strcat(_parametersIniFile, "/");
      strcat(_parametersIniFile, PARAMETERS_INI_FILE);
      DBG("_parametersIniFile = %s", _parametersIniFile);
      _parametersObjFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_OBJ_FILE) + 2);
      strcpy(_parametersObjFile, dataPath);
      strcat(_parametersObjFile, "/");
      strcat(_parametersObjFile, PARAMETERS_OBJ_FILE);
      DBG("_parametersObjFile = %s", _parametersObjFile);
      _parametersBackupObjFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_BACKUP_OBJ_FILE) + 2);
      strcpy(_parametersBackupObjFile, dataPath);
      strcat(_parametersBackupObjFile, "/");
      strcat(_parametersBackupObjFile, PARAMETERS_BACKUP_OBJ_FILE);
      DBG("_parametersBackupObjFile = %s", _parametersBackupObjFile);
      _parametersCtrlFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_CTRL_FILE) + 2);
      strcpy(_parametersCtrlFile, dataPath);
      strcat(_parametersCtrlFile, "/");
      strcat(_parametersCtrlFile, PARAMETERS_CTRL_FILE);
      DBG("_parametersCtrlFile = %s", _parametersCtrlFile);
      _parametersLockFile = (char*)malloc(strlen(dataPath) + strlen(PARAMETERS_LOCK_FILE) + 2);
      strcpy(_parametersLockFile, dataPath);
      strcat(_parametersLockFile, "/");
      strcat(_parametersLockFile, PARAMETERS_LOCK_FILE);
      DBG("_parametersLockFile = %s", _parametersLockFile);
   }
   else
   {
      if (_parametersIniFile==NULL) { _parametersIniFile = strdup(PARAMETERS_INI_FILE); }
      if (_parametersObjFile==NULL) { _parametersObjFile = strdup(PARAMETERS_OBJ_FILE); }
      if (_parametersBackupObjFile==NULL) { _parametersBackupObjFile = strdup(PARAMETERS_BACKUP_OBJ_FILE); }
      if (_parametersCtrlFile==NULL) { _parametersCtrlFile = strdup(PARAMETERS_CTRL_FILE); }
      if (_parametersLockFile==NULL) { _parametersLockFile = strdup(PARAMETERS_LOCK_FILE); }
   }

   remove(_parametersLockFile); // suppression du verrou s'il était resté (suite à un arrêt brutal)
   int res = 1;
   if (!factoryReset)
   {
      DBG("NOT Factory Reset");
      res = _readFileParameters(_parametersObjFile);
      if (res != 0) // Pb, on tente la lecture du fichier de backup
      {
         WARN("Error while reading the data file => Reading the backup file");
         remove(_parametersObjFile);
         _lastModifData = 0;
         res = _readFileParameters(_parametersBackupObjFile);
         if (res == 0)
         {
            _writeParameters();
         }
         else
         {
            WARN("Error while reading the backup file => Factory reset");
         }
      }
      if (res == 0)
      {
         _readDmControl();
#ifdef NO_PERSISTENT_SCHEDULED_INFORM
         // effacer sScheduledInformTime et sScheduledInformCommandKey, si persistance désactivée
         DM_ENG_FREE(sScheduledInformTime);
         DM_ENG_FREE(sScheduledInformCommandKey);
#endif
      }
   }
   else
   {
      DBG("Factory Reset");
      remove(_parametersObjFile);
      remove(_parametersCtrlFile);
      _lastModifData = 0;
      _lastModifCtrl = 0;
      res = _readFileParameters(_parametersIniFile);
      if (res != 0)
      {
         EXEC_ERROR("*** File not found : %s ***", _parametersIniFile);
         return 1;
      }
      _writeParameters();
   }
   return res;
}

static void _releaseData()
{
   // les données existantes éventuelles sont à supprimer
   if (nbParameters > 0)
   {
      DM_ENG_deleteAllParameter(&parameters);
      nbParameters = 0;
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
   return nbParameters;
}

//////////////////////////////////////
//////// Accès aux paramètres ////////

static DM_ENG_Parameter* _current = NULL;

DM_ENG_Parameter* DM_ENG_ParameterData_getFirst()
{
   _current = parameters;
   return _current;
}

DM_ENG_Parameter* DM_ENG_ParameterData_getNext()
{
   if (_current != NULL) { _current = _current->next; }
   return _current;
}

char* DM_ENG_ParameterData_getFirstName()
{
   _current = parameters;
   return (_current == NULL ? NULL : _current->name);
}

char* DM_ENG_ParameterData_getNextName()
{
   if (_current != NULL) { _current = _current->next; }
   return (_current == NULL ? NULL : _current->name);
}

void DM_ENG_ParameterData_resumeParameterLoopFrom(char* resumeName)
{
   if ((_current == NULL) || (strcmp(_current->name, resumeName) != 0))
   {
      _current = parameters;
      while ((_current != NULL) && (strcmp(_current->name, resumeName) != 0)) { _current = _current->next; }
   }
}

DM_ENG_Parameter* DM_ENG_ParameterData_getCurrent()
{
   return _current;
}

/**
 *
 * @param name Name of a requested Parameter.
 *
 * @return The parameter
 */
DM_ENG_Parameter* DM_ENG_ParameterData_getParameter(const char* name)
{
//   DBG("DM_ENG_ParameterData_getParameter - name = %s", name);
   if (name != NULL)
   {
      DM_ENG_Parameter* param;
      for (param = parameters; param!=NULL; param = param->next)
      {
          if (strcmp(name, param->name)==0) return param;
      }
   }
   return NULL;
}

static void _putParameter(DM_ENG_Parameter* param)
{
   DM_ENG_Parameter* prev = NULL;
   DM_ENG_Parameter* curr = NULL;
   for (curr = parameters; curr!=NULL; curr = curr->next)
   {
      if (strcmp(param->name, curr->name)==0) break;
      prev = curr;
   }
   if (curr!=NULL) // remplacement d'un param existant
   {
      if (prev==NULL) // 1er param (cas impossible : c'est la racine !)
      {
         parameters = param;
      }
      else
      {
         prev->next = param;
      }
      param->next = curr->next;
      DM_ENG_deleteParameter(curr);
   }
   else // nouveau paramètre
   {
      DM_ENG_addParameter(&parameters, param);
      nbParameters++;
   }
}

void DM_ENG_ParameterData_addParameter(DM_ENG_Parameter* param)
{
   DM_ENG_addParameter(&parameters, param);
   nbParameters++;
   DM_ENG_Parameter_setDataChanged(true);
}

void DM_ENG_ParameterData_createInstance(DM_ENG_Parameter* param, unsigned int instanceNumber)
{
   char* objectName = param->name;
   sprintf(_line, "%s%u.", objectName, instanceNumber);
   _putParameter(DM_ENG_newInstanceParameter(_line, param));
   size_t objLen = strlen(objectName);
   DM_ENG_Parameter* prm = NULL;
   for (prm = parameters; prm!=NULL; prm = prm->next)
   {
      if ((strlen(prm->name) > objLen) && (strncmp(prm->name, objectName, objLen)==0)
           && ((prm->name)[objLen]=='.'))
      {
         sprintf(_line+objLen, "%u%s", instanceNumber, (prm->name)+objLen);
         _putParameter(DM_ENG_newInstanceParameter(_line, prm));
      }
   }
   DM_ENG_Parameter_setDataChanged(true);
}

/*
 * Supprime 1 ou plusieurs paramètres du data model
 * N.B. Seule la suppression de parmètres résultant d'une extension (possédant un configKey) est autorisée 
 *
 * @param name Soit le nom d'un paramètre à supprimer, soit le nom d'un noeud qui chapeaute un ensemble de paramètres à supprimer
 * @param configKey Si non nul, la suppression est restreinte aux paramètres possédant cette valeur de configKey
 *
 * @return true ssi au moins un paramètre a été trouvé
 */
bool DM_ENG_ParameterData_deleteParameters(const char* name, char* configKey)
{
   bool res = false;
   size_t len = (name == NULL ? 0 : strlen(name));
   if (len != 0)
   {
      char* deConfigKey = DM_ENG_decodeValue(configKey); // Attention, NULL si rien à décoder !
      if (deConfigKey == NULL) { deConfigKey = configKey; }
      DM_ENG_Parameter* prev = NULL;
      DM_ENG_Parameter* prm = parameters;
      bool isNode = (name[len-1] == '.');
      while (prm != NULL)
      {
         if ( (prm->configKey != NULL) // Ne sont supprimés que des paramètres résultant d'une extension du data model initial
          && (isNode ? (strncmp(prm->name, name, len) == 0) : (strcmp(prm->name, name) == 0)) // nom correspondant
          && ((configKey == NULL) || DM_ENG_Parameter_removeConfigKey(prm, deConfigKey)) ) // configKey correspondant
         {
            if ((configKey == NULL) || (prm->configKey == NULL))
            {
               DM_ENG_Parameter* supp = prm;
               prm = prm->next;
               if (prev == NULL) // 1er param (cas impossible : c'est la racine !)
               {
                  parameters = prm;
               }
               else
               {
                  prev->next = prm;
               }
               DM_ENG_deleteParameter(supp);
            }
            DM_ENG_Parameter_setDataChanged(true);
            res = true;
            if (!isNode) break; // un seul param à supprimer
         }
         else
         {
            prev = prm;
            prm = prm->next;
         }
      }
      if (deConfigKey != configKey) { free(deConfigKey); } 
   }
   return res;
}

void DM_ENG_ParameterData_deleteObject(char* objectName)
{
   int nbSupp=0; // nb de param supprimés
   DM_ENG_Parameter* prev = NULL;
   DM_ENG_Parameter* prm = parameters;
   size_t objLen = strlen(objectName);
   while (prm != NULL)
   {
      if (strncmp(prm->name, objectName, objLen)==0) // param à supprimer
      {
         if (prev == NULL) // 1er param (cas impossible : c'est la racine !)
         {
            parameters = prm->next;
         }
         else
         {
            prev->next = prm->next;
         }
         DM_ENG_Parameter* supp = prm;
         prm = prm->next;
         DM_ENG_deleteParameter(supp);
         nbSupp++;
      }
      else
      {
         prev = prm;
         prm = prm->next;
      }
   }
   if (nbSupp != 0)
   {
      nbParameters -= nbSupp;
      DM_ENG_Parameter_setDataChanged(true);
   }
}

//////////////////////////////////
//////// Opérations d'E/S ////////

static char* it;
static char* nextToken(char* ch)
{
   if (ch!=NULL) { it = ch; }
   char* res = it;
   while ((*it!=';') && (*it>=' ')) { it++; }
   if (*it!='\0')
   {
      *it='\0';
      it++;
   }
   return (*res==*NULL_CODE ? NULL : res);
}

static void setEvents(char* line)
{
   if (line == NULL) return;
   char* code = nextToken(line);
   while ((code!=NULL) && (*code!='\0'))
   {
      char* key = nextToken(NULL);
      DM_ENG_addEventStruct(&events, DM_ENG_newEventStruct(code, key));
      code = nextToken(NULL);
   }
}

static void setOthersData(char* line)
{
   if (line == NULL) return;
   periodicInformTime = atol(nextToken(line));
   char* next = nextToken(NULL);
   sScheduledInformTime = (next==NULL ? NULL : strdup(next));
   next = nextToken(NULL);
   sScheduledInformCommandKey = (next==NULL ? NULL : strdup(next));
   next = nextToken(NULL);
   sRebootCommandKey = (next==NULL ? NULL : strdup(next));
   next = nextToken(NULL);
   retryCount = (next==NULL ? 0 : atoi(next));
}

static void addNewTransferRequest(char* line)
{
   if (line == NULL) return;
   bool d = atoi(nextToken(line));
   int s = atoi(nextToken(NULL));
   time_t t = atol(nextToken(NULL));
   char* ft = nextToken(NULL);
   char* url = nextToken(NULL);
   char* un = nextToken(NULL);
   char* pw = nextToken(NULL);
   unsigned int fs = atol(nextToken(NULL));
   char* tfn = nextToken(NULL);
   char* su = nextToken(NULL);
   char* fu = nextToken(NULL);
   char* ck = nextToken(NULL);
   char* au = nextToken(NULL);
   unsigned int code = atol(nextToken(NULL));
   time_t start = atol(nextToken(NULL));
   time_t comp = atol(nextToken(NULL));
   unsigned int id = atol(nextToken(NULL));
   DM_ENG_TransferRequest* dr = DM_ENG_newTransferRequest(d, s, t, ft, url, un, pw, fs, tfn, su, fu, ck, au);
   dr->faultCode = code;
   dr->startTime = start;
   dr->completeTime = comp;
   dr->transferId = id;
   if (id > _lastTransferId) { _lastTransferId = id; }
   DM_ENG_addTransferRequest(&transferRequests, dr);
}

static void addNewDownloadRequest(char* line)
{
   if (line == NULL) return;
   char* ft = nextToken(line);
   int nbArgs = atoi(nextToken(NULL));
   DM_ENG_ArgStruct* args[nbArgs+1];
   int i;
   for (i=0; i<nbArgs; i++)
   {
      char* name = nextToken(NULL);
      char* val = nextToken(NULL);
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
 * Lit les données dans le fichier désigné en paramètre
 * N.B. La lecture n'est effectivement réalisée que si les données ont été modifiées (par une cause extérieure)
 */
static int _readFileParameters(const char* fichierLu)
{
   int res = 0;
   bool isIniFile = (fichierLu == _parametersIniFile);
   FILE *fp;
   struct stat prop;
   char* line;
   char* attr[MAX_NB_ATTR];
   fp = fopen(fichierLu, "r");
   if (fp == NULL) // fichier inexistant
   {
      WARN( "*** File not found ***" );
      return 1;
   }

   fstat(fileno(fp), &prop);
   if (prop.st_mtime > _lastModifData) // les données ont été modifiées ou ne sont pas encore lues
   {
      _releaseData();

      _lastModifData = prop.st_mtime;
      _resetCrc();
      DM_ENG_Parameter* lastParam = NULL;
      while( (line = _readln(fp)) != NULL )
      {
         int len = strlen(line);
         if ((len <= 1) || (*line == '#')) continue; // ligne vide ou ligne de commentaires

         int c = 0;
         int i = 0; // indice parametre
         while ((c<=len) && (i<MAX_NB_ATTR))
         {
            attr[i++] = line + c;
            while ((c<len) && (line[c] != ';')) { c++; }
            line[c++] = '\0';
         }
         while (i < MAX_NB_ATTR) { attr[i++] = ""; }
         char* decVal = DM_ENG_decodeValue(attr[9]);
         if (decVal != NULL) { attr[9] = decVal; }
         char* decBVal = DM_ENG_decodeValue(attr[12]);
         if (decBVal != NULL) { attr[12] = decBVal; }
         char* decKVal = DM_ENG_decodeValue(attr[13]);
         if (decKVal != NULL) { attr[13] = decKVal; }
         char* decSysVal = DM_ENG_decodeValue(attr[14]);
         if (decSysVal != NULL) { attr[14] = decSysVal; }
         DM_ENG_Parameter* newParam = DM_ENG_newParameter(attr, isIniFile);
         DM_ENG_FREE(decVal);
         DM_ENG_FREE(decBVal);
         DM_ENG_FREE(decKVal);
         DM_ENG_FREE(decSysVal);
         if (lastParam == NULL)
         {
            parameters = newParam;
         }
         else
         {
            lastParam->next = newParam;
         }
         lastParam = newParam;
         nbParameters++;
      }
      if (!isIniFile && !_isCrcOK())
      {
         WARN( "*** CRC Error ***" );
         res = 2;
      }
   }
   fclose(fp);
   return res;
}

/*
 * Lit les paramètres enregistrés
 */
static int _readParameters()
{
   if (_readFileParameters(_parametersObjFile) != 0) // N.B. Le fichier est lu seulement si modifié depuis la dernière lecture
   {
      WARN("Error while reading the data file => Reading the backup file");
      remove(_parametersObjFile);
      _lastModifData = 0;
      int res = _readFileParameters(_parametersBackupObjFile); // On tente la lecture du fichier de backup
      if (res != 0)
      {
         EXEC_ERROR( "Error while reading the backup file => Need factory reset" );
         return res;
      }
      _writeParameters();
   }
   return 0;
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

static void _writeParameters()
{
   FILE *fp;
   struct stat prop;
   size_t prefixLen = strlen(DM_ENG_PARAMETER_PREFIX);
   rename(_parametersObjFile, _parametersBackupObjFile);
   fp = fopen(_parametersObjFile, "w");
   if (fp)
   {
      _resetCrc();
      DM_ENG_Parameter* param;
      for (param = parameters; param!=NULL; param = param->next)
      {
         if (strncmp(param->name, DM_ENG_PARAMETER_PREFIX, prefixLen)==0) // robustesse : test normalement tjrs vrai
         {
            char* al = DM_ENG_buildCommaSeparatedList(param->accessList);
            char* val = param->value;
            char* encVal = DM_ENG_encodeValue(val);
            if (encVal != NULL) { val = encVal; }
            char* bVal = param->backValue;
            char* encBVal = DM_ENG_encodeValue(bVal);
            if (encBVal != NULL) { bVal = encBVal; }
            char* kVal = param->configKey;
            char* encKVal = DM_ENG_encodeValue(kVal);
            if (encKVal != NULL) { kVal = encKVal; }
            char* sysVal = param->definition;
            char* encSysVal = DM_ENG_encodeValue(sysVal);
            if (encSysVal != NULL) { sysVal = encSysVal; }
            char* sType = DM_ENG_typeToString(param->type, param->minValue, param->maxValue);
            sprintf(_line, "%s;%s;%d;%d;%d;%d;%d;%d;%s;%s;%d;%d;%s;%s;%s",
                    (param->name)+prefixLen, sType,
                    param->storageMode, param->writable, param->immediateChanges,
                    param->notification, param->mandatoryNotification, param->activeNotificationDenied, (al==0 ? "" : al),
                    (val==0 ? "" : val), param->loadingMode, param->state, (bVal==0 ? "" : bVal), (kVal==0 ? "" : kVal), (sysVal==0 ? "" : sysVal));
            _writeln(fp, _line);
            DM_ENG_FREE(al);
            DM_ENG_FREE(encVal);
            DM_ENG_FREE(encBVal);
            DM_ENG_FREE(encKVal);
            DM_ENG_FREE(encSysVal);
            free(sType);
         }
      }
      _writeCrc(fp);
      fclose(fp);
      fp = fopen(_parametersObjFile, "r");
      if (fp)
      {
         fstat(fileno(fp), &prop);
         _lastModifData = prop.st_mtime;
         fclose(fp);
      }
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
   DBG("DM_ENG_ParameterData_getPeriodicInformTime - periodicInformTime = %ld sec (for 1970.1.1)", periodicInformTime);
   return periodicInformTime;
}

void DM_ENG_ParameterData_setPeriodicInformTime(time_t t)
{
  DBG("DM_ENG_ParameterData_setPeriodicInformTime - periodicInformTime = %ld sec (for 1970.1.1)", periodicInformTime);
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

   _lastTransferId++;
   tr->transferId = _lastTransferId;

   if (transferRequests == NULL)
   {
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
      _writeParameters();
      DM_ENG_Parameter_setDataChanged(false);
   }
}

void DM_ENG_ParameterData_restore()
{
   _lastModifData = 0;
   _readParameters();
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
      _readParameters(); // N.B. Le fichier est lu seulement si modifié depuis la dernière lecture
      _readDmControl(); // Id.
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

static bool _isCrcOK()
{
   return _crcOK;
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
