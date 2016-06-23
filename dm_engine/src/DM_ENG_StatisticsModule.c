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
 * File        : DM_ENG_StatisticsModule.c
 *
 * Created     : 02/04/2010
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
 * @file DM_ENG_StatisticsModule.c
 *
 * @brief This module processes the raw collected sample data and computes the derived parameters
 *
 */

#include "DM_ENG_StatisticsModule.h"
#include "DM_ENG_ParameterData.h"
#include "DM_ENG_ParameterManager.h"
#include "DM_ENG_ComputedExp.h"
#include "DM_ENG_Device.h"
#include "DM_CMN_Thread.h"
#include "CMN_Trace.h"

static const char* _INTERNAL_SEPARATOR   = "!";

static const char* _STATS_ENABLE_NAME    = "Enable";
static const char* _STATS_STATUS_NAME    = "Status";
static const char* _LAST_TIMESTAMP_NAME  = "LastTimeStamp";
static const char* _SUB_SAMPLE_INTERVAL_NAME = "SubSampleInterval";
static const char* _DELTA_SUFFIX         = "!Delta";

static const char* _TOTAL_NODE           = "Total.";
static const char* _STATS_RESET_NAME     = "Reset";
static const char* _RESET_TIME_NAME      = "ResetTime";

static const char* _SAMPLE_SET_REF_NAME  = "SampleSetRef";
static const char* _SAMPLE_STATUS_NAME   = "SampleStatus";
static const char* _FORCE_SAMPLE_NAME    = "ForceSample";

static const char* _READING_NODE         = ""; //"Reading.";

static const char* _CURRENT_PERIOD_NODE  = "CurrentSample.";

static const char* _SAMPLES_REPORT_NODE  = "SamplesReport.";

static const char* _SLIDING_MODE_NAME    = "SlidingMode";
static const char* _TIMESTAMP_CSV_NAME   = "TimestampCsv";
static const char* _SUB_SAMPLES_NAME     = "SubSamples";
static const char* _DURATION_CSV_NAME    = "DurationCsv";
static const char* _SUSPECT_DATA_CSV_NAME = "SuspectDataCsv";
static const char* _DURATION_NAME        = "Duration";
static const char* _SUSPECT_DATA_NAME    = "SuspectData";
static const char* _LAST_VALUE_SUFFIX    = "!LastValue";
static const char* _VALUE_SUFFIX         = "!Value";
static const char* _SUM_SUFFIX           = "!Sum";
static const char* _MIN_SUFFIX           = "!Min";
static const char* _MIN_TIME_SUFFIX      = "!MinTime";
static const char* _MAX_SUFFIX           = "!Max";
static const char* _MAX_TIME_SUFFIX      = "!MaxTime";
static const char* _COUNT_SUFFIX         = "!Count";
static const char* _AVERAGE_SUFFIX       = "!Average";
static const char* _CSV_SUFFIX           = "!Csv";
static const char* _OR_SUFFIX            = "!Or";
static const char* _HISTOGRAM_SUFFIX     = "!Histogram";
static const char* _TIMESTAMP_SUFFIX     = "!Timestamp";
static const char* _DELAY_HISTOGRAM_SUFFIX = "!DelayHistogram";

static const char* _SAMPLE_ENABLE_NAME   = "SampleEnable";
static const char* _TIME_REFERENCE_NAME  = "TimeReference";
static const char* _SAMPLE_INTERVAL_NAME = "SampleInterval";
static const char* _REPORT_SAMPLES_NAME  = "ReportSamples";
static const char* _FETCH_SAMPLES_NAME   = "FetchSamples";
static const char* _START_TIME_NAME      = "StartTime";
static const char* _END_TIME_NAME        = "EndTime";

static const char* _STATUS_DISABLED = "Disabled";
static const char* _STATUS_ENABLED  = "Enabled";
static const char* _STATUS_TRIGGER  = "Trigger";
static const char* _STATUS_ERROR    = "Error";

static unsigned int _SUB_SAMPLE_INTERVAL_DEFAULT = 3600; // 1 heure
static unsigned int _SAMPLE_INTERVAL_DEFAULT     = 3600; // 1 heure (cf. TR-157)
static unsigned int _REPORT_SAMPLES_DEFAULT      = 24; // (cf. TR-157)
static unsigned int _INITIAL_RETRY_DELAY         = 2; // 2 secondes

static const int _MAX_INTERVALS = 20; // Nb d'intervalles max des histogrammes

static DM_ENG_ParameterValueStruct* statsToPoll = NULL;
static DM_CMN_ThreadId_t _pollingThreadId = 0;
static DM_CMN_Mutex_t _pollingMutex = NULL;
static DM_CMN_Cond_t _pollingCond = NULL;

static DM_ENG_F_GET_VALUE _getValue = NULL;
static DM_ENG_F_CREATE_PARAMETER _createParam = NULL;

enum _Status
{
   _INITIAL = 0,
   _STARTING,
   _RUNNING,
   _EXITING

} _samplingStatus = 0;

static void _signalPolling();

static DM_ENG_Parameter* _getStatParameter(const char* statObject, const char* paramName)
{
   char* longName = (char*) malloc(strlen(statObject)+strlen(paramName)+1);
   strcpy(longName, statObject);
   strcat(longName, paramName);
   DM_ENG_Parameter* prm = DM_ENG_ParameterData_getParameter(longName);
   free(longName);
   return prm;
}

static char* _simpleGetValue(DM_ENG_Parameter* prm) // simple redirection permise
{
   if ( ((prm->value == NULL) || (*prm->value == '\0')) 
     && ((prm->storageMode == DM_ENG_StorageMode_COMPUTED) &&  (prm->definition != NULL)) )
   {
      char* targetName = DM_ENG_Parameter_getLongName(prm->definition, prm->name);
      prm = DM_ENG_ParameterData_getParameter(targetName);
      DM_ENG_FREE(targetName);
   }
   return (prm == NULL ? NULL : prm->value);
}

static void _updateNewValue(DM_ENG_Parameter* prm, char* val)
{
   DM_ENG_Parameter_updateValue(prm, (val == NULL ? (char*)DM_ENG_EMPTY : val), true);
}

static void _updateValueIfChanged(DM_ENG_Parameter* prm, char* val)
{
   if ((prm != NULL) && ((prm->value == NULL) || (strcmp(prm->value, val) != 0)))
   {
      DM_ENG_Parameter_updateValue(prm, val, true);
   }
}

static void _getSampleSettings(const char* statObject, time_t* pTimeRef, unsigned int* pSampleInterval, unsigned int* pReportSamples, unsigned int* pFetchSamples)
{
   char* sampleSetName = (char*)statObject;
   DM_ENG_Parameter* prm = _getStatParameter(statObject, _SAMPLE_SET_REF_NAME);
   if ((prm != NULL) && (prm->value != NULL) && (*prm->value != '\0'))
   {
      sampleSetName = DM_ENG_Parameter_getLongName(prm->value, statObject);
   }

   if (pTimeRef != NULL)
   {
      *pTimeRef = 0;
      prm = _getStatParameter(sampleSetName, _TIME_REFERENCE_NAME);
      if (prm != NULL) { DM_ENG_dateStringToTime(_simpleGetValue(prm), pTimeRef); }
   }

   if (pSampleInterval != NULL)
   {
      *pSampleInterval = _SAMPLE_INTERVAL_DEFAULT;
      prm = _getStatParameter(sampleSetName, _SAMPLE_INTERVAL_NAME);
      if (prm != NULL) { DM_ENG_stringToUint(_simpleGetValue(prm), pSampleInterval); }
   }

   if (pReportSamples != NULL)
   {
      *pReportSamples = _REPORT_SAMPLES_DEFAULT;
      prm = _getStatParameter(sampleSetName, _REPORT_SAMPLES_NAME);
      if (prm != NULL) { DM_ENG_stringToUint(_simpleGetValue(prm), pReportSamples); }
   }

   if (pFetchSamples != NULL)
   {
      *pFetchSamples = 0;
      prm = _getStatParameter(sampleSetName, _FETCH_SAMPLES_NAME);
      if (prm != NULL) { DM_ENG_stringToUint(_simpleGetValue(prm), pFetchSamples); }
   }

   if (sampleSetName != statObject) { free(sampleSetName); }
}

static int _startSampling(const char* objName, char* def)
{
   int res = 0;
   char* data = NULL;
   DM_ENG_ComputedExp_eval(def, _getValue, objName, &data);

   unsigned int sampleInterval = 0;
   DM_ENG_Parameter* prm = _getStatParameter(objName, _SUB_SAMPLE_INTERVAL_NAME);
   if (prm != NULL) { DM_ENG_stringToUint(prm->value, &sampleInterval); }

   time_t timeRef = 0;
   if (sampleInterval == 0) // si SubSampleInterval est défini, on le garde
   {
      _getSampleSettings(objName, &timeRef, &sampleInterval, NULL, NULL);
   }
   else
   {
      _getSampleSettings(objName, &timeRef, NULL, NULL, NULL);
   }

   res = DM_ENG_Device_startSampling(objName, timeRef, sampleInterval, data);
   DM_ENG_FREE(data);

   if (res == 0) // Initialisation des ResetTime si nuls
   {
      prm = _getStatParameter(objName, _RESET_TIME_NAME);
      if (prm != NULL)
      {
         time_t resetTime = 0;
         DM_ENG_dateStringToTime(prm->value, &resetTime);
         if (resetTime == 0)
         {
            char* sResetTime = DM_ENG_dateTimeToString(time(NULL));
            _updateNewValue(prm, sResetTime);
            free(sResetTime);
         }
      }
      prm = _getStatParameter(objName, _TOTAL_NODE);
      if (prm != NULL)
      {
         prm = _getStatParameter(prm->name, _RESET_TIME_NAME);
         if (prm != NULL)
         {
            time_t resetTime = 0;
            DM_ENG_dateStringToTime(prm->value, &resetTime);
            if (resetTime == 0)
            {
               char* sResetTime = DM_ENG_dateTimeToString(time(NULL));
               _updateNewValue(prm, sResetTime);
               free(sResetTime);
            }
         }
      }
   }

   return res;
}

static char* _createInternalParams(DM_ENG_ComputedExp* exp, char* destName, DM_ENG_ParameterType type, char* configKey)
{
   char* root = NULL;
   if (exp != NULL)
   {
      switch (exp->oper)
      {
         case DM_ENG_ComputedExp_IDENT :
            root = exp->str;
            break;
         case DM_ENG_ComputedExp_LIST :
         case DM_ENG_ComputedExp_OR :
         case DM_ENG_ComputedExp_AND :
         case DM_ENG_ComputedExp_LT :
         case DM_ENG_ComputedExp_LE :
         case DM_ENG_ComputedExp_GT :
         case DM_ENG_ComputedExp_GE :
         case DM_ENG_ComputedExp_EQ :
         case DM_ENG_ComputedExp_NE :
         case DM_ENG_ComputedExp_PLUS :
         case DM_ENG_ComputedExp_MINUS :
         case DM_ENG_ComputedExp_TIMES :
         case DM_ENG_ComputedExp_DIV :
            root = _createInternalParams(exp->left,destName, type, configKey);
            _createInternalParams(exp->right, destName, type, configKey);
            break;
         case DM_ENG_ComputedExp_NOT :
         case DM_ENG_ComputedExp_U_PLUS :
         case DM_ENG_ComputedExp_U_MINUS :
            root = _createInternalParams(exp->left, destName, type, configKey);
            break;
         case DM_ENG_ComputedExp_CALL :
            if (exp->left != NULL)
            {
               char* suffix = exp->str;
               char* args = NULL;
               if (exp->left->oper == DM_ENG_ComputedExp_LIST)
               {
                  root = _createInternalParams(exp->left->left, destName, type, configKey); // garder le même type ? !!
                  if (exp->left->right != NULL)
                  {
                     if ((exp->left->right->oper == DM_ENG_ComputedExp_STRING)
                      || (exp->left->right->oper == DM_ENG_ComputedExp_IDENT)) // élargir à tte expression !!
                     {
                        args = strdup(exp->left->right->str);
                     }
                  }
               }
               else
               {
                  root = _createInternalParams(exp->left, destName, type, configKey); // garder le même type ? !!
               }
               if ((root != NULL) && (suffix != NULL) && !DM_ENG_ComputedExp_isPredefinedFunction(suffix))
               {
                  char* internalName = (char*) malloc(strlen(root)+strlen(suffix)+2);
                  strcpy(internalName, root);
                  strcat(internalName, _INTERNAL_SEPARATOR);
                  strcat(internalName, suffix);

                  bool created = _createParam(internalName, destName, type, args, configKey);
                  free(internalName);
                  if (created) // vérification des dépendances. Pas nécessaire pour la partie Report !!
                  {
                     if (strcmp(suffix, _MIN_TIME_SUFFIX+1) == 0) // vérifier que le min est créé
                     {
                        internalName = (char*) malloc(strlen(root)+strlen(_MIN_SUFFIX)+1);
                        strcpy(internalName, root);
                        strcat(internalName, _MIN_SUFFIX);
                        _createParam(internalName, destName, DM_ENG_ParameterType_UINT, NULL, configKey);
                        free(internalName);
                     }
                     else if (strcmp(suffix, _MAX_TIME_SUFFIX+1) == 0) // vérifier que le max est créé
                     {
                        internalName = (char*) malloc(strlen(root)+strlen(_MAX_SUFFIX)+1);
                        strcpy(internalName, root);
                        strcat(internalName, _MAX_SUFFIX);
                        _createParam(internalName, destName, DM_ENG_ParameterType_UINT, NULL, configKey);
                        free(internalName);
                     }
                     else if (strcmp(suffix, _AVERAGE_SUFFIX+1) == 0) // vérifier que le count est créé
                     {
                        internalName = (char*) malloc(strlen(root)+strlen(_COUNT_SUFFIX)+1);
                        strcpy(internalName, root);
                        strcat(internalName, _COUNT_SUFFIX);
                        _createParam(internalName, destName, DM_ENG_ParameterType_UINT, NULL, configKey);
                        free(internalName);
                     }
                     else if (strcmp(suffix, _DELAY_HISTOGRAM_SUFFIX+1) == 0) // vérifier que le Timestamp dans la branche Reading est créé
                     {
                        const char* ch1 = _TOTAL_NODE;                  // point à améliorer !!
                        char* ch2 = strstr(destName, ch1);
                        char* destDestName = destName;
                        if (ch2 == NULL) { ch1 = _CURRENT_PERIOD_NODE; ch2 = strstr(destName, ch1); }
                        if (ch2 != NULL) // on subsitue ch1 par _READING_NODE
                        {
                           destDestName = (char*) malloc(strlen(destName)+strlen(_READING_NODE)+1);
                           strcpy(destDestName, destName);
                           strcpy(destDestName+(ch2-destName), _READING_NODE);
                           strcat(destDestName, ch2+strlen(ch1));
                        }
                        internalName = (char*) malloc(strlen(root)+strlen(_TIMESTAMP_SUFFIX)+1);
                        strcpy(internalName, root);
                        strcat(internalName, _TIMESTAMP_SUFFIX);
                        _createParam(internalName, destDestName, DM_ENG_ParameterType_DATE, NULL, configKey);
                        free(internalName);
                        if (destDestName != destName ) { free(destDestName); }
                     }
                     // on crée le !Csv si c'est le CurrentSample et et on a SlidingMode à true
                     char* sCurr = strstr(destName, _CURRENT_PERIOD_NODE);
                     if ((sCurr != NULL) && (strchr(sCurr+strlen(_CURRENT_PERIOD_NODE), '.') == NULL))
                     {
                        char* prmName = (char*) malloc(strlen(destName)+strlen(_SLIDING_MODE_NAME)+1);
                        strcpy(prmName, destName);
                        strcpy(prmName+(sCurr-destName)+strlen(_CURRENT_PERIOD_NODE), _SLIDING_MODE_NAME);
                        DM_ENG_Parameter* prm = DM_ENG_ParameterData_getParameter(prmName);
                        free(prmName);
                        if (prm != NULL)
                        {
                           bool slidingMode = false;
                           DM_ENG_stringToBool(prm->value, &slidingMode);
                           if (slidingMode)
                           {
                              internalName = (char*) malloc(strlen(root)+strlen(_CSV_SUFFIX)+1);
                              strcpy(internalName, root);
                              strcat(internalName, _CSV_SUFFIX);
                              _createParam(internalName, destName, DM_ENG_ParameterType_STRING, NULL, configKey);
                              free(internalName);
                           }
                        }
                     }
                  }
                  if (args != NULL) { free(args); }
               }
            }
            break;
         default :
            // rien à faire
            break;
      }
   }
   return root;
}

static void _fillStatsToPoll()
{
   // 1ère boucle pour retenir toutes les stats, "pollables" ou non
   DM_ENG_Parameter* prm;
   for (prm = DM_ENG_ParameterData_getFirst(); prm!=NULL; prm = DM_ENG_ParameterData_getNext())
   {
      if (DM_ENG_Parameter_isNode(prm->name) && (prm->type == DM_ENG_ParameterType_STATISTICS))
      {
         char* data = NULL;
         DM_ENG_ComputedExp_eval(prm->definition, _getValue, prm->name, &data);
         DM_ENG_addParameterValueStruct(&statsToPoll, DM_ENG_newParameterValueStruct(prm->name, prm->minValue, data));
         DM_ENG_FREE(data);
      }
      int lenCurr = strlen(_CURRENT_PERIOD_NODE);
      int iCurr = strlen(prm->name) - lenCurr - strlen(_SLIDING_MODE_NAME);
      if ((iCurr > 1) && (strcmp(prm->name + iCurr + lenCurr, _SLIDING_MODE_NAME) == 0) && (strncmp(prm->name + iCurr, _CURRENT_PERIOD_NODE, lenCurr) == 0))
      {
         bool slidingMode = false;
         DM_ENG_stringToBool(prm->value, &slidingMode);
         if (slidingMode)
         {
            char* prmName = strdup(prm->name);
            char* configKey = strdup(prm->configKey);
            // création du TimeStampCsv s'il n'existe pas
            _createParam((char*)_TIMESTAMP_CSV_NAME, prmName, DM_ENG_ParameterType_STRING, NULL, configKey);
            // création du DurationCsv s'il n'existe pas
            _createParam((char*)_DURATION_CSV_NAME, prmName, DM_ENG_ParameterType_STRING, NULL, configKey);
            // création du SuspectDataCsv s'il n'existe pas
            _createParam((char*)_SUSPECT_DATA_CSV_NAME, prmName, DM_ENG_ParameterType_STRING, NULL, configKey);
            free(prmName);
            free(configKey);
         }
      }
      if ((!prm->writable) && (prm->immediateChanges == 2)) // signature d'une variable stat cumulative -> créer les !Delta
      {
         // création du Delta s'il n'existe pas
         char* internalName = (char*) malloc(strlen(prm->name)+strlen(_DELTA_SUFFIX)+1);
         strcpy(internalName, prm->name);
         strcat(internalName, _DELTA_SUFFIX);
         _createParam(internalName, NULL, DM_ENG_ParameterType_UINT, NULL, prm->configKey);
         free(internalName);
      }
      if ((prm->storageMode == DM_ENG_StorageMode_COMPUTED) && (prm->definition != NULL))
      {
         DM_ENG_ComputedExp* exp = DM_ENG_ComputedExp_parse(prm->definition);
         if (exp != NULL)
         {
            char* resumeName = strdup(prm->name);
            _createInternalParams(exp, resumeName, prm->type, prm->configKey);
            DM_ENG_ParameterManager_resumeParameterLoopFrom(resumeName);
            free(resumeName);
            DM_ENG_deleteComputedExp(exp);
         }
      }
   }
}

static void _clearStatsToPoll()
{
   DM_ENG_ParameterValueStruct* stp = statsToPoll;
   while (stp != NULL)
   {
      DM_ENG_Device_stopSampling(stp->parameterName);
      statsToPoll = stp->next;
      DM_ENG_deleteParameterValueStruct(stp);
      stp = statsToPoll;
   }
}

void DM_ENG_StatisticsModule_init(DM_ENG_F_CREATE_PARAMETER createParam, DM_ENG_F_GET_VALUE getValue)
{
   DM_CMN_Thread_initMutex(&_pollingMutex);
   DM_CMN_Thread_initCond(&_pollingCond);
   DM_CMN_Thread_lockMutex(_pollingMutex);

   _createParam = createParam;
   _getValue = getValue;

   _fillStatsToPoll();

   _samplingStatus = _INITIAL;
   if (statsToPoll != NULL)
   {
      _signalPolling();
   }
   DM_CMN_Thread_unlockMutex(_pollingMutex);
}

/*
 * Recherche si le sampling est actif
 * puis, si sampleStatusToUpdate == true, met à jour le SampleStatus
 */
static bool _isSampleEnabled(const char* objName, bool sampleStatusToUpdate)
{
   bool sampleEnabled = false;
   DM_ENG_Parameter* prm = _getStatParameter(objName, _SAMPLE_ENABLE_NAME);
   if (prm != NULL) // On se réfère au SampleEnable local si présent
   {
      DM_ENG_stringToBool(prm->value, &sampleEnabled);
   }
   if (sampleEnabled || (prm == NULL))
   {
      DM_ENG_Parameter* prm = _getStatParameter(objName, _SAMPLE_SET_REF_NAME);
      if ((prm != NULL) && (prm->value != NULL) && (strlen(prm->value) > 0))
      {
         char* sampleSetName = DM_ENG_Parameter_getLongName(prm->value, objName);
         prm = _getStatParameter(sampleSetName, _SAMPLE_ENABLE_NAME);
         if (prm == NULL) { sampleEnabled = true; } // activé par défaut
         else { DM_ENG_stringToBool(_simpleGetValue(prm), &sampleEnabled); }
         free(sampleSetName);
      }
   }

   if (sampleStatusToUpdate)
   {
      char* statusValue = (char*)_STATUS_DISABLED;
      if (sampleEnabled)
      {
         statusValue = (char*)_STATUS_ENABLED;
         prm = _getStatParameter(objName, _STATS_STATUS_NAME);
         if ((prm != NULL) && (prm->value != NULL))
         {
            if (strcmp(prm->value, _STATUS_DISABLED) == 0) { statusValue = (char*)_STATUS_DISABLED; }
            else if (strcmp(prm->value, _STATUS_ERROR) == 0) { statusValue = (char*)_STATUS_ERROR; }
         }
      }
      prm = _getStatParameter(objName, _SAMPLE_STATUS_NAME);
      if (prm != NULL)
      {
         _updateValueIfChanged(prm, statusValue);
      }
   }

   return sampleEnabled;
}

static void _startAllSamplings()
{
   // 2ème boucle sur les stats pour démarrer celles qui sont activées et supprimer celles qui ne le sont pas
   // puis on ne retient que les stats pollables
   DM_ENG_ParameterValueStruct* stp = statsToPoll;
   DM_ENG_ParameterValueStruct* prev = NULL;
   while (stp != NULL)
   {
      DM_ENG_Parameter* prm = _getStatParameter(stp->parameterName, _STATS_ENABLE_NAME);
      bool bEnable = true; // par défaut, les stats sont activées
      char* definition = NULL;
      if (prm != NULL)
      {
         DM_ENG_stringToBool(prm->value, &bEnable);
         definition = prm->definition;
      }

      if (bEnable)
      {
         int startCode = _startSampling(stp->parameterName, definition);
         char* statusValue = (char*)_STATUS_ENABLED;
         if (startCode != 0) { statusValue = (char*)_STATUS_ERROR; }

         prm = _getStatParameter(stp->parameterName, _STATS_STATUS_NAME);
         _updateValueIfChanged(prm, statusValue);

         _isSampleEnabled(stp->parameterName, true);
      }
      if (bEnable && (stp->type == 1))
      {
         stp->type = 0;
         prev = stp;
         stp = stp->next;
      }
      else
      {
         if (prev == NULL) { statsToPoll = stp->next; }
         else { prev->next = stp->next; }
         DM_ENG_deleteParameterValueStruct(stp);
         stp = (prev == NULL ? statsToPoll : prev->next);      
      }
   }   
}

void DM_ENG_StatisticsModule_exit()
{
   DM_CMN_Thread_lockMutex(_pollingMutex);
   _samplingStatus = _EXITING;

   _clearStatsToPoll();

   if (_pollingThreadId != 0)
   {
      DM_CMN_Thread_signalCond(_pollingCond);
   }
   DM_CMN_Thread_unlockMutex(_pollingMutex);
}

void DM_ENG_StatisticsModule_restart()
{
   DM_CMN_Thread_lockMutex(_pollingMutex);

   _clearStatsToPoll();
   _fillStatsToPoll();
   if (_samplingStatus == _RUNNING)
   {
      DBG("Restart sampling - Begin");
      _samplingStatus = _STARTING; // pour redémarrer la collecte dans _runPolling
   }

   if (statsToPoll != NULL)
   {
      _signalPolling();
   }
   DM_CMN_Thread_unlockMutex(_pollingMutex);
}

/**
 * Permet la synchronisation sur l'arrêt du thread statistics
 */
void DM_ENG_StatisticsModule_join()
{
   if (_pollingThreadId != 0)
   {
      DM_CMN_Thread_join(_pollingThreadId);
   }
}

typedef enum _T_PUSH_CASE
{
   _BEFORE_ADDING_SAMPLE_DATA,
   _AFTER_ADDING_SAMPLE_DATA,
   _FORCE_SAMPLE

} T_PUSH_CASE;

/**
 * Reverse les données du CurrentSample dans le SamplesReport, appelé soit en fin de Sample, soit par ForceSample
 *
 * @param statObject
 * @param pushCase On distingue les 3 types d'appels pour des traitements particuliers ponctuels
 * @param argTime Argument de type time_t interprété de différentes façons selon les cas
 */
static void _pushSample(const char* statObject, T_PUSH_CASE pushCase, time_t argTime, bool slidingMode)
{
   time_t timeRef = 0;
   unsigned int sampleInterval = 0;
   unsigned int reportSamples = 0;
   unsigned int fetchSamples = 0;
   time_t timestamp = 0;

   DM_ENG_Parameter* param = _getStatParameter(statObject, _LAST_TIMESTAMP_NAME);
   if ((param == NULL) || !DM_ENG_dateStringToTime(param->value, &timestamp)) { return; }

   _getSampleSettings(statObject, &timeRef, &sampleInterval, &reportSamples, &fetchSamples);

   time_t boundTime = 0; // borne de sample atteinte
   if (timestamp >= timeRef) { boundTime = sampleInterval * ((timestamp-timeRef) / sampleInterval) + timeRef; }
   else { boundTime = timeRef - sampleInterval * ((timeRef-timestamp) / sampleInterval + 1); }

   if ((pushCase == _BEFORE_ADDING_SAMPLE_DATA) && (argTime < (time_t)(boundTime + 2 * sampleInterval))) { return; } // rien à faire avant

   char* samplesNodeName = (char*) malloc(strlen(statObject)+strlen(_SAMPLES_REPORT_NODE)+1);
   strcpy(samplesNodeName, statObject);
   strcat(samplesNodeName, _SAMPLES_REPORT_NODE);

   time_t endTime = argTime;
   param = _getStatParameter(samplesNodeName, _END_TIME_NAME);
   if (param != NULL)
   {
      DM_ENG_dateStringToTime(param->value, &endTime);

      if ((pushCase == _AFTER_ADDING_SAMPLE_DATA) && (endTime == 0) && (timestamp > boundTime)) // on considère alors que endTime = boundTime
      {
         endTime = boundTime;
         char* sEndReport = DM_ENG_dateTimeToString(endTime);
         _updateNewValue(param, sEndReport);
         free(sEndReport);
      }
   }

   if (((pushCase == _AFTER_ADDING_SAMPLE_DATA) && (boundTime <= endTime)) || (timestamp <= endTime)) { free(samplesNodeName); return; } // pas de donnée nouvelle à traiter

   if (param != NULL)
   {
      char* sEndReport = DM_ENG_dateTimeToString(timestamp);
      _updateNewValue(param, sEndReport);
      free(sEndReport);
   }

   if (pushCase != _AFTER_ADDING_SAMPLE_DATA) { boundTime += sampleInterval; }  // forceSample : on affecte l'intervalle en cours (au lieu de celui qui vient de se terminer)

   char* currentNodeName = (char*) malloc(strlen(statObject)+strlen(_CURRENT_PERIOD_NODE)+1);
   strcpy(currentNodeName, statObject);
   strcat(currentNodeName, _CURRENT_PERIOD_NODE);

   unsigned int samplesExpected = reportSamples; // sauf modification
   unsigned int samplesToDrop   = 0; // (0 par défaut)

   param = _getStatParameter(samplesNodeName, _START_TIME_NAME);
   if (param != NULL)
   {
      time_t startTime = 0;
      time_t startReport = boundTime - reportSamples * sampleInterval;

      DM_ENG_dateStringToTime(param->value, &startTime);
      if (startReport > startTime)
      {
         if (startTime == 0) // aucune donnée encore enregistrée
         {
            samplesExpected = 1;
            startReport = boundTime - sampleInterval;
         }
         else
         {
            samplesToDrop = (startReport - startTime + sampleInterval - 1) / sampleInterval;
         }
         char* sStartReport = DM_ENG_dateTimeToString(startReport);
         _updateNewValue(param, sStartReport);
         free(sStartReport);
         startTime = startReport;
      }
      else // startReport <= startTime
      {
         samplesExpected = (boundTime - startTime + sampleInterval - 1) / sampleInterval;
      }
   }

   if ((pushCase == _AFTER_ADDING_SAMPLE_DATA) && (fetchSamples > 0) && (fetchSamples <= reportSamples))
   {
      time_t fetchTime = 0;
      time_t fetchInterval = fetchSamples * sampleInterval;
      if (boundTime >= timeRef) { fetchTime = fetchInterval * ((boundTime-timeRef) / fetchInterval) + timeRef; }
      else { fetchTime = timeRef - fetchInterval * ((timeRef-boundTime) / fetchInterval + 1); }
      if ((fetchTime > endTime) && (fetchTime <= boundTime)) // Notif à déclencher
      {
         param = _getStatParameter(statObject, _SAMPLE_STATUS_NAME);
         if (param != NULL)
         {
            _updateNewValue(param, (char*)_STATUS_TRIGGER);
         }
      }
   }

   // puis boucler sur tous les paramètres de CurrentPeriod pour reporter les valeurs et faire les raz
   char* prmName;
   for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
   {
      if ((strlen(prmName) > strlen(currentNodeName)) && (strncmp(prmName, currentNodeName, strlen(currentNodeName)) == 0)
         && (strcmp(prmName+strlen(currentNodeName), _START_TIME_NAME) != 0) // peut-être pas de StartTime
         && (strcmp(prmName+strlen(currentNodeName), _SLIDING_MODE_NAME) != 0)
         && (strcmp(prmName+strlen(currentNodeName), _SUB_SAMPLES_NAME) != 0))
      {
         char* resumeName = strdup(prmName);

         param = DM_ENG_ParameterData_getCurrent();
         if ((param->storageMode == DM_ENG_StorageMode_COMPUTED) || (param->storageMode == DM_ENG_StorageMode_SYSTEM_ONLY) || (param->storageMode == DM_ENG_StorageMode_MIXED))
         {
            DM_ENG_ParameterManager_loadLeafParameterValue(param, true);
            param = DM_ENG_ParameterData_getParameter(resumeName); // on recharge le param courant
         }
         char* sCurrentValue = (param->value == NULL ? NULL : strdup(param->value));
         if ((pushCase != _FORCE_SAMPLE) && !slidingMode) { DM_ENG_Parameter_updateValue(param, (char*)DM_ENG_EMPTY, false); } // sinon on ne raz pas les valeurs

         char* shortName = resumeName+strlen(currentNodeName);
         param = _getStatParameter(samplesNodeName, shortName);
         if ((param != NULL) && (param->storageMode != DM_ENG_StorageMode_COMPUTED))
         {
            unsigned int prmSamplesStored = 1;
            if (param->value != NULL)
            {
               int k;
               for (k=0; param->value[k]!='\0'; k++)
               {
                  if (param->value[k] == ',') { prmSamplesStored++; }
               }
            }

            unsigned int prmSamplesToDrop = samplesToDrop;
            unsigned int prmSamplesToAdd = 0; // samples manquants + nouveau sample (0 par défaut)
            if (prmSamplesStored < samplesToDrop)
            {
               prmSamplesToDrop = prmSamplesStored;
               prmSamplesToAdd = samplesExpected;
            }
            else
            {
               unsigned int extent = samplesToDrop + samplesExpected;
               if (prmSamplesStored <= extent)
               {
                  prmSamplesToAdd = extent - prmSamplesStored;
               }
               else
               {
                  prmSamplesToDrop = prmSamplesStored - samplesExpected;
               }
            }

            const char* sOldValue = DM_ENG_EMPTY;
            if (param->value != NULL)
            {
               if (prmSamplesToDrop == 0)
               {
                  sOldValue = param->value;
               }
               else if (prmSamplesToDrop < prmSamplesStored)
               {
                  unsigned int p = 0;
                  int k;
                  for (k=0; param->value[k]!='\0'; k++)
                  {
                     if (param->value[k] == ',')
                     {
                        p++;
                        if (p == prmSamplesToDrop)
                        {
                           sOldValue = param->value + k+1;
                           break;
                        }
                     }
                  }
               }
            }

            int lenOld = strlen(sOldValue);
            int lenCurr = (sCurrentValue == NULL ? 0 : strlen(sCurrentValue));
            char* sNewValue = (char*) malloc(lenOld+prmSamplesToAdd+lenCurr+1);
            strcpy(sNewValue, sOldValue);

            if ((prmSamplesToAdd == 0) && (lenOld>0)) // cas particulier, suite à un forceSample, où on remplace la dernière valeur
            {
               int k = lenOld-1;
               while ((k>=0) && (sNewValue[k]!=',')) { sNewValue[k] = '\0'; k--; }
            }
            else
            {
               unsigned int i = 0;
               if (prmSamplesToDrop >= prmSamplesStored) { i++; } // la chaîne vide compte pour un élément
               while (i<prmSamplesToAdd) { strcat(sNewValue, ","); i++; }
            }

            if (sCurrentValue != NULL) { strcat(sNewValue, sCurrentValue); }
            _updateNewValue(param, sNewValue);
            free(sNewValue);
         }

         DM_ENG_FREE(sCurrentValue);

         DM_ENG_ParameterManager_resumeParameterLoopFrom(resumeName);
         free(resumeName);
      }
   }

   free(currentNodeName);
   free(samplesNodeName);
}

void DM_ENG_StatisticsModule_performParameterSetting(const char* objName, const char* paramName)
{
   size_t iSuffix = strlen(objName);
   if ((strlen(paramName) <= iSuffix) || (strncmp(paramName, objName, iSuffix) != 0)) return;

   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(paramName);
   DM_ENG_Parameter* prm = NULL;
   bool isResetParam = (strcmp(paramName+iSuffix, _STATS_RESET_NAME) == 0);
   size_t jSuffix = iSuffix+strlen(_TOTAL_NODE);
   if (!isResetParam
    && (strlen(paramName) > jSuffix) && (strncmp(paramName+iSuffix, _TOTAL_NODE, strlen(_TOTAL_NODE)) == 0) && (strcmp(paramName+jSuffix, _STATS_RESET_NAME) == 0))
   {
      isResetParam = true;
      iSuffix = jSuffix;
      jSuffix = 0;
   }
   if (isResetParam && (param->type == DM_ENG_ParameterType_BOOLEAN))
   {
      bool bReset = false;
      DM_ENG_stringToBool(param->value, &bReset);
      _updateNewValue(param, (char*)DM_ENG_EMPTY);
      if (bReset)
      {
         // effacement des valeurs des paramètres de stats à resetter
         char* prmName;
         for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
         {
            if ((strlen(prmName) > iSuffix) && (strncmp(prmName, paramName, iSuffix) == 0))
            {
               prm = DM_ENG_ParameterData_getCurrent();
               if (!prm->writable) // Reset ne s'applique qu'au param RO
               {
                  if ((prm->type == DM_ENG_ParameterType_DATE)
                     && ((strcmp(prm->name+iSuffix, _RESET_TIME_NAME) == 0)
                        || ((jSuffix >0) && (strlen(prm->name) > jSuffix) && (strncmp(prm->name+iSuffix, _TOTAL_NODE, strlen(_TOTAL_NODE)) == 0) && (strcmp(prm->name+jSuffix, _RESET_TIME_NAME) == 0))))
                  // Param XxxStats.ResetTime et XxxStats.Total.ResetTime
                  {
                     char* sResetTime = DM_ENG_dateTimeToString(time(NULL));
                     _updateNewValue(prm, sResetTime);
                     free(sResetTime);
                  }
                  else if (strcmp(prm->name+iSuffix, _STATS_STATUS_NAME) != 0) // on ne réinitialise pas le status 
                  {
                     DM_ENG_Parameter_clearTemporaryValue(prm, 2);
                  }
               }
            }
         }
      }
   }
   else if ((strcmp(paramName+iSuffix, _STATS_ENABLE_NAME) == 0) && (param->type == DM_ENG_ParameterType_BOOLEAN))
   {
      DM_CMN_Thread_lockMutex(_pollingMutex);
      if (_samplingStatus != _EXITING)
      {
         bool bEnable = false;
         DM_ENG_stringToBool(param->value, &bEnable);
         char* statusValue = (char*)_STATUS_ERROR;
         bool toPoll = false;
         if (bEnable)
         {
            if (_startSampling(objName, param->definition) == 0)
            {
               statusValue = (char*)_STATUS_ENABLED;
   
               prm = DM_ENG_ParameterData_getParameter(objName);
               if ((prm != NULL) && (prm->minValue == 1))
               {
                  DM_ENG_ParameterValueStruct* stp = statsToPoll;
                  while ((stp != NULL) && (strcmp(stp->parameterName, objName) != 0)) { stp = stp->next; }
                  if (stp == NULL)
                  {
                     stp = DM_ENG_newParameterValueStruct(objName, 0, NULL);
                     DM_ENG_addParameterValueStruct(&statsToPoll, stp);
                     toPoll = true;
                  }
               }
            }
         }
         else
         {
            if (DM_ENG_Device_stopSampling(objName) == 0)
            {
               statusValue = (char*)_STATUS_DISABLED;
            }
   
            DM_ENG_ParameterValueStruct* stp = statsToPoll;
            DM_ENG_ParameterValueStruct* prev = NULL;
            while ((stp != NULL) && (strcmp(stp->parameterName, objName) != 0)) { prev = stp; stp = stp->next; }
            if (stp != NULL)
            {
               if (prev == NULL) { statsToPoll = stp->next; }
               else { prev->next = stp->next; }
               DM_ENG_deleteParameterValueStruct(stp);
            }
         }

         // affection du Status
         prm = _getStatParameter(objName, _STATS_STATUS_NAME);
         if (prm != NULL)
         {
            _updateNewValue(prm, statusValue);
         }

         // mise à jour nécessaire du SampleStatus
         _isSampleEnabled(objName, true);

         if (toPoll)
         {
            _signalPolling();
         }
      }
      DM_CMN_Thread_unlockMutex(_pollingMutex);
   }
   else if (strcmp(paramName+iSuffix, _SAMPLE_SET_REF_NAME) == 0)
   {
      _isSampleEnabled(objName, true);

      // Raz des parties CurrentSample et SamplesReport
      char* prmName;
      for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
      {
         if ((strlen(prmName) > iSuffix) && (strncmp(prmName, objName, strlen(objName)) == 0)
          && ((strncmp(prmName+iSuffix, _CURRENT_PERIOD_NODE, strlen(_CURRENT_PERIOD_NODE)) == 0) || (strncmp(prmName+iSuffix, _SAMPLES_REPORT_NODE, strlen(_SAMPLES_REPORT_NODE)) == 0)) )
         {
            prm = DM_ENG_ParameterData_getCurrent();
            if (!prm->writable) // Reset ne s'applique qu'au param RO
            {
               DM_ENG_Parameter_clearTemporaryValue(prm, 2);
            }
         }
      }
   }
   else if (strcmp(paramName+iSuffix, _SAMPLE_ENABLE_NAME) == 0)
   {
      _isSampleEnabled(objName, true);
   }
   else if (strcmp(paramName+iSuffix, _FORCE_SAMPLE_NAME) == 0)
   {
      _pushSample(objName, _FORCE_SAMPLE, 0, false);
      param = DM_ENG_ParameterData_getParameter(paramName); // on recharge, c'est plus sûr
      _updateNewValue(param, (char*)DM_ENG_FALSE_STRING);
   }
}

/*
 * Vérifie que le paramètre Enable est bien à true. Utilisé lors de la remontée correcte de données.
 * Et, si oui, s'assure que Status est bien à "Enabled", en le positionnant au besoin
 */
static bool _checkStatEnabled(const char* statObject)
{
   bool enabled = false;

   DM_ENG_Parameter* param = _getStatParameter(statObject, _STATS_ENABLE_NAME);

   if (param == NULL) { enabled = true; } // à défaut de ce paramètre, on considère que les stats sont activées 
   else { DM_ENG_stringToBool(param->value, &enabled); }

   if (enabled)
   {
      param = _getStatParameter(statObject, _STATS_STATUS_NAME);
      if ((param != NULL) && ((param->value == NULL) || (strcmp(param->value, _STATUS_ENABLED) != 0)))
      {
         _updateNewValue(param, (char*)_STATUS_ENABLED);
      }
   }
   return enabled;
}

/*
 * Fournit la valeur de type unsigned int du prochain argument
 * args est une chaîne de la forme "arg1:arg2:..." où arg1 est soit directement un entier soit une nom de param dont la valeur fournit le résultat entier
 * pVal reçoit la valeur entière de l'argument
 * pNextArg pointe sur une chaîne recopiant args à partir du 2ème argument ("arg2:..."). N'est pas affecté s'il n'y en a pas.
 * 
 * retourne un booléen indiquant si la valeur a bien été obtenue
 * 
 * Attention, effet de bord : cette fonction change le current parameter !
 */
static bool _getUIntArgValue(char* args, unsigned int* pVal, char** pNextArg, const char* destName)
{
   bool res = false;
   if (args == NULL) return false;

   char* arg = strdup(args);
   char* separ = strchr(arg, ':');
   if (separ != NULL)
   {
      *separ = '\0';
      if (pNextArg != NULL) { *pNextArg = strdup(separ+1); }
   }
   res = DM_ENG_stringToUint(arg, pVal);
   if (!res) // si conversion OK, *pVal est ainsi obtenue
   {
      // arg est un nom de param donnant la valeur attendue
      char* longName = DM_ENG_Parameter_getLongName(arg, destName);
      char* sVal = NULL;
      if (DM_ENG_ParameterManager_getParameterValue(longName, &sVal) == 0) // OK
      {
         res = DM_ENG_stringToUint(sVal, pVal);
         DM_ENG_FREE(sVal);
      }
      free(longName);
   }
   free(arg);

   return res;
}

static void _computeHisto(const char* nameHisto, unsigned int value)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(nameHisto);
   if (param != NULL)
   {
      if ((param->definition != NULL) && (strlen(param->definition) > 0))
      {
         char* sIntervals = NULL;

         // redirection vers le paramètre définissant les intervalles
         char* longName = DM_ENG_Parameter_getLongName(param->definition, nameHisto);
         if (DM_ENG_ParameterManager_getParameterValue(longName, &sIntervals) != 0) // autoriser une expression !! -> DM_ENG_ComputedExp_eval(param->definition, _getValue, nameHisto, &sIntervals);
         {
            sIntervals = strdup(param->definition);
         }
         free(longName);

         if (sIntervals != NULL)
         {
            // on lit les valeurs des bornes
            unsigned int intervals[_MAX_INTERVALS];
            intervals[0] = 0;
            int i;
            for (i=1; i<_MAX_INTERVALS; i++) { intervals[i] = UINT_MAX; }
            int nbIntervals = 0;
            while ((sIntervals != NULL) && (nbIntervals<_MAX_INTERVALS))
            {
               char* next = NULL;
               _getUIntArgValue(sIntervals, &intervals[nbIntervals], &next, nameHisto);
               free(sIntervals);
               sIntervals = next;
               nbIntervals++;
            }
            if (sIntervals != NULL) { free(sIntervals); }

            // on lit les valeurs de l'histogramme
            param = DM_ENG_ParameterData_getParameter(nameHisto); // on recharge le param courant
            unsigned int histogram[_MAX_INTERVALS];
            for (i=0; i<nbIntervals; i++) { histogram[i] = 0; }
            i=0;
            char* sHisto = param->value;
            while ((sHisto != NULL) && (i<nbIntervals))
            {
               char* next = NULL;
               _getUIntArgValue(sHisto, &histogram[i], &next, nameHisto);
               if (i != 0) { free(sHisto); }
               sHisto = next;
               i++;
            }
            if (sHisto != NULL) { free(sHisto); } // rq: nbIntervals>=1 donc sHisto != param->value

            // on incrémente la bonne colonne de l'histogramme
            i = 0;
            while ((i<nbIntervals) && (value > intervals[i])) { i++; }
            if (i<nbIntervals) { histogram[i]++; }

            size_t sizeMaxHisto = nbIntervals*2;
            if (param->value != NULL) { sizeMaxHisto += strlen(param->value); } // majoration sûre de la taille
            sHisto = (char*) malloc(sizeMaxHisto+1);
            for (i=0; i<nbIntervals; i++)
            {
               char* sVal = DM_ENG_uintToString(histogram[i]);
               if (i==0) { strcpy(sHisto, sVal); }
               else { strcat(sHisto, ":"); strcat(sHisto, sVal); }
               free(sVal);
            }
            _updateNewValue(param, sHisto);
            free(sHisto);
         }
      }
   }
}

static void _computeSlidingHisto(const char* nameHisto UNUSED, const char* csv UNUSED)
{
   // TO DO !!
}

static void _computeStats(const char* name, unsigned int newValue, char* sNewValue, time_t timestamp)
{
   // affection du paramètre !LastValue
   DM_ENG_Parameter* param = _getStatParameter(name, _LAST_VALUE_SUFFIX);
   if (param != NULL)
   {
      _updateNewValue(param, sNewValue);
   }

   // affection du !Sum
   if (newValue > 0) // sinon c'est pas la peine !
   {
      param = _getStatParameter(name, _SUM_SUFFIX);
      if (param == NULL) { param = _getStatParameter(name, _VALUE_SUFFIX); } // 2 nommages possibles
      if (param != NULL)
      {
         unsigned int sum = 0;
         DM_ENG_stringToUint(param->value, &sum);
         sum += newValue;
         char* sValue = DM_ENG_uintToString(sum);
         _updateNewValue(param, sValue);
         free(sValue);
      }
   }

   // affection du !Min
   param = _getStatParameter(name, _MIN_SUFFIX);
   if (param != NULL)
   {
      unsigned int oldMin = UINT_MAX;
      DM_ENG_stringToUint(param->value, &oldMin);
      if (newValue < oldMin)
      {
         _updateNewValue(param, sNewValue);
         // affectation du MinTime
         param = _getStatParameter(name, _MIN_TIME_SUFFIX);
         if (param != NULL)
         {
            char* sValue = DM_ENG_dateTimeToString(timestamp);
            _updateNewValue(param, sValue);
            free(sValue);
         }
      }
   }

   // affection du !Max
   param = _getStatParameter(name, _MAX_SUFFIX);
   if (param != NULL)
   {
      unsigned int oldMax = 0;
      DM_ENG_stringToUint(param->value, &oldMax);
      if (newValue > oldMax)
      {
         _updateNewValue(param, sNewValue);
         // affectation du MaxTime
         param = _getStatParameter(name, _MAX_TIME_SUFFIX);
         if (param != NULL)
         {
            char* sValue = DM_ENG_dateTimeToString(timestamp);
            _updateNewValue(param, sValue);
            free(sValue);
         }
      }
   }

   // affection du !Count
   param = _getStatParameter(name, _COUNT_SUFFIX);
   if (param != NULL)
   {
      unsigned int count = 0;
      DM_ENG_stringToUint(param->value, &count);
      count++;
      char* sValue = DM_ENG_uintToString(count);
      _updateNewValue(param, sValue);
      free(sValue);
      // affectation du !Average
      param = _getStatParameter(name, _AVERAGE_SUFFIX);
      if (param != NULL)
      {
         unsigned int avg = 0;
         DM_ENG_stringToUint(param->value, &avg);
         avg = ((count-1)*avg + newValue) / count; // à revoir avec _Average décimale !!
         char* sValue = DM_ENG_uintToString(avg);
         _updateNewValue(param, sValue);
         free(sValue);
      }                                   
   }

   // affection du !Csv
   param = _getStatParameter(name, _CSV_SUFFIX);
   if (param != NULL)
   {
      unsigned int maxValues = 20; // valeur par défaut;
      _getUIntArgValue(param->definition, &maxValues, NULL, name);
      if (maxValues > 0)
      {
         char* sCsvValue = NULL;
         param = _getStatParameter(name, _CSV_SUFFIX); // on recharge le param Csv
         if ((param->value==NULL) || (maxValues == 1))
         {
            sCsvValue = strdup(sNewValue);
         }
         else
         {
            unsigned int nbVal = 1;
            int k = strlen(param->value);
            while (k>0)
            {
               k--;
               if (param->value[k] == ',')
               {
                  nbVal++;
                  if (nbVal >= maxValues) { k++; break; }
               }
            }
            sCsvValue = (char*) malloc( strlen(param->value)-k + strlen(sNewValue) + 2);
            strcpy(sCsvValue, param->value+k);
            strcat(sCsvValue, ",");  
            strcat(sCsvValue, sNewValue);
         }
         _updateNewValue(param, sCsvValue);

         free(sCsvValue);
      }
   }

   // affection du !Or
   param = _getStatParameter(name, _OR_SUFFIX);
   if (param != NULL)
   {
      bool oldVal = false; 
      bool newVal = false;
      DM_ENG_stringToBool(param->value, &oldVal);
      DM_ENG_stringToBool(sNewValue, &newVal);
      if (!oldVal && newVal)
      {
         _updateNewValue(param, (char*)DM_ENG_TRUE_STRING);
      }
   }

   // affection du !Histogram
   char* nameHisto = (char*) malloc(strlen(name)+strlen(_HISTOGRAM_SUFFIX)+1);
   strcpy(nameHisto, name);
   strcat(nameHisto, _HISTOGRAM_SUFFIX);
   _computeHisto(nameHisto, newValue);
   free(nameHisto);

   // autre stats...
}

static void _addSlidingValue(DM_ENG_Parameter* param, char* sValue, unsigned int valuesToSave)
{
   // PRE : param != NULL
   char* sOldValue = param->value;
   if ((sOldValue != NULL) && (*sOldValue != '\0'))
   {
      sOldValue += strlen(sOldValue);
      if (valuesToSave > 0)
      {
         sOldValue--;
         unsigned int k = 0;
         while (sOldValue != param->value)
         {
            if (*sOldValue == ',')
            {
               k++;
               if (k == valuesToSave) { sOldValue++; break; }
            }
            sOldValue--;
         }
      }
   }
   if ((sOldValue == NULL) || (*sOldValue == '\0'))
   {
      _updateNewValue(param, sValue);
   }
   else
   {
      char* sNewValue = (char*) malloc(strlen(sOldValue)+ (sValue == NULL ? 0 : strlen(sValue)) +2);
      strcpy(sNewValue, sOldValue);
      strcat(sNewValue, ",");
      if (sValue != NULL) { strcat(sNewValue, sValue); }
      _updateNewValue(param, sNewValue);
      free(sNewValue);
   }
}

static unsigned int _countCsv(const char* csv)
{
   unsigned int res = 0;
   if (csv != NULL)
   {
      char* ch0 = strdup(csv);
      char* ch1 = ch0;
      while (*ch1 != '\0')
      {
         char* ch2 = ch1;
         while ((*ch2 != '\0') && (*ch2 != ',')) { ch2++; }
         if (*ch2 == ',') { *ch2 = '\0'; ch2++; }
         if (DM_ENG_stringToUint(ch1, NULL)) { res++; }
         ch1 = ch2;
      }  
      free(ch0);
   }
   return res;
}

static unsigned int _sumCsv(const char* csv)
{
   unsigned int res = 0;
   if (csv != NULL)
   {
      char* ch0 = strdup(csv);
      char* ch1 = ch0;
      while (*ch1 != '\0')
      {
         char* ch2 = ch1;
         while ((*ch2 != '\0') && (*ch2 != ',')) { ch2++; }
         if (*ch2 == ',') { *ch2 = '\0'; ch2++; }
         unsigned int i = 0;
         DM_ENG_stringToUint(ch1, &i);
         res += i;
         ch1 = ch2;
      }  
      free(ch0);
   }
   return res;
}

static unsigned int _minCsv(const char* csv, int* index)
{
   unsigned int res = UINT_MAX;
   if (csv != NULL)
   {
      int i = 0;
      char* ch0 = strdup(csv);
      char* ch1 = ch0;
      while (*ch1 != '\0')
      {
         char* ch2 = ch1;
         while ((*ch2 != '\0') && (*ch2 != ',')) { ch2++; }
         if (*ch2 == ',') { *ch2 = '\0'; ch2++; }
         unsigned int v = 0;
         if (DM_ENG_stringToUint(ch1, &v) && (v<res)) { res = v; *index = i; }
         ch1 = ch2;
         i++;
      }  
      free(ch0);
   }
   return res;
}

static unsigned int _maxCsv(const char* csv, int* index)
{
   unsigned int res = 0;
   if (csv != NULL)
   {
      int i = 0;
      char* ch0 = strdup(csv);
      char* ch1 = ch0;
      while (*ch1 != '\0')
      {
         char* ch2 = ch1;
         while ((*ch2 != '\0') && (*ch2 != ',')) { ch2++; }
         if (*ch2 == ',') { *ch2 = '\0'; ch2++; }
         unsigned int v = 0;
         if (DM_ENG_stringToUint(ch1, &v) && (v>res)) { res = v; *index = i; }
         ch1 = ch2;
         i++;
      }
      free(ch0);
   }
   return res;
}

static char* _getCsvItem(const char* csv, int index)
{
   char* res = NULL;
   const char* ch = csv;
   int i = 0;
   while ((i < index) && (ch != NULL))
   {
      ch = strchr(ch, ',');
      if (ch != NULL) { ch++; }
      i++;
   }
   if (ch != NULL)
   {
      res = strdup(ch);
      char* virg = strchr(res, ',');
      if (virg != NULL) { *virg = '\0'; }
   }
   return res;
}

static bool _orCsv(const char* csv)
{
   bool res = false;
   if (csv != NULL)
   {
      char* ch0 = strdup(csv);
      char* ch1 = ch0;
      while ((*ch1 != '\0') && (!res))
      {
         char* ch2 = ch1;
         while ((*ch2 != '\0') && (*ch2 != ',')) { ch2++; }
         if (*ch2 == ',') { *ch2 = '\0'; ch2++; }
         DM_ENG_stringToBool(ch1, &res);
         ch1 = ch2;
      }
      free(ch0);
   }
   return res;
}

static void _computeSlidingStats(const char* name, char* sNewValue, const char* currentNodeName, unsigned int valuesToSave)
{
   char* sCsvValue = NULL;
   DM_ENG_Parameter* param = NULL;

   if (sNewValue != NULL)
   {
      // affection du paramètre !LastValue
      param = _getStatParameter(name, _LAST_VALUE_SUFFIX);
      if (param != NULL)
      {
         _updateNewValue(param, sNewValue);
      }
   }

   // affection du paramètre !Csv
   param = _getStatParameter(name, _CSV_SUFFIX);
   if (param != NULL)
   {
      _addSlidingValue(param, sNewValue, valuesToSave);
      sCsvValue = strdup(param->value);
   }

   param = _getStatParameter(name, _SUM_SUFFIX);
   if (param == NULL) { param = _getStatParameter(name, _VALUE_SUFFIX); } // 2 nommages possibles
   if (param != NULL)
   {
      unsigned int sum = _sumCsv(sCsvValue);
      char* sValue = DM_ENG_uintToString(sum);
      _updateNewValue(param, sValue);
      free(sValue);
   }

   // affection du !Min
   param = _getStatParameter(name, _MIN_SUFFIX);
   if (param != NULL)
   {
      int index = 0;
      unsigned int min = _minCsv(sCsvValue, &index);
      char* sValue = DM_ENG_uintToString(min);
      _updateNewValue(param, sValue);
      free(sValue);
      // affectation du MinTime
      param = _getStatParameter(name, _MIN_TIME_SUFFIX);
      if (param != NULL)
      {
         param = _getStatParameter(currentNodeName, _TIMESTAMP_CSV_NAME);
         if (param != NULL)
         {
            char* sValue = _getCsvItem(param->value, index);
            param = _getStatParameter(name, _MIN_TIME_SUFFIX); // on recharge
            _updateNewValue(param, sValue);
            free(sValue);
         }
      }
   }

   // affection du !Max
   param = _getStatParameter(name, _MAX_SUFFIX);
   if (param != NULL)
   {
      int index = 0;
      unsigned int max = _maxCsv(sCsvValue, &index);
      char* sValue = DM_ENG_uintToString(max);
      _updateNewValue(param, sValue);
      free(sValue);
      // affectation du MaxTime
      param = _getStatParameter(name, _MAX_TIME_SUFFIX);
      if (param != NULL)
      {
         param = _getStatParameter(currentNodeName, _TIMESTAMP_CSV_NAME);
         if (param != NULL)
         {
            char* sValue = _getCsvItem(param->value, index);
            param = _getStatParameter(name, _MAX_TIME_SUFFIX); // on recharge
            _updateNewValue(param, sValue);
            free(sValue);
         }
      }
   }

   // affection du !Count
   param = _getStatParameter(name, _COUNT_SUFFIX);
   if (param != NULL)
   {
      unsigned int count = _countCsv(sCsvValue);
      char* sValue = DM_ENG_uintToString(count);
      _updateNewValue(param, sValue);
      free(sValue);
   }

   // affection du !Average
   param = _getStatParameter(name, _AVERAGE_SUFFIX);
   if (param != NULL)
   {
      unsigned int sum = _sumCsv(sCsvValue);
      unsigned int count = _countCsv(sCsvValue);
      char* sValue = NULL;
      if (count != 0) { sValue = DM_ENG_uintToString(sum / count); }
      _updateNewValue(param, sValue);
      free(sValue);
   }

   // affection du !Or
   param = _getStatParameter(name, _OR_SUFFIX);
   if (param != NULL)
   {
      bool or_ = _orCsv(sCsvValue);
      _updateNewValue(param, (or_ ? (char*)DM_ENG_TRUE_STRING: (char*)DM_ENG_FALSE_STRING));
   }

   // affection du !Histogram
   char* nameHisto = (char*) malloc(strlen(name)+strlen(_HISTOGRAM_SUFFIX)+1);
   strcpy(nameHisto, name);
   strcat(nameHisto, _HISTOGRAM_SUFFIX);
   _computeSlidingHisto(nameHisto, sCsvValue);
   free(nameHisto);

   // autre stats...
   if (sCsvValue != NULL) { free(sCsvValue); }
}

static void _processOneData(char* name, char* sNewValue, DM_ENG_SampleDataStruct* sd, const char* readingNodeName, const char* totalNodeName, const char* currentNodeName,
   bool slidingMode, unsigned int valuesToSave)
{
   DM_ENG_Parameter* param = DM_ENG_ParameterData_getParameter(name);
   if (param != NULL)
   {
      char* readingName = NULL;
      char* totalName = NULL;
      char* currentName = NULL;
      if ((strlen(name) > strlen(sd->statObject)) && (strncmp(name, sd->statObject, strlen(sd->statObject)) == 0))
      {
         char* shortName = name+strlen(sd->statObject);

         readingName = (char*) malloc(strlen(readingNodeName)+strlen(shortName)+1);
         strcpy(readingName, readingNodeName);
         strcat(readingName, shortName);

         totalName = (char*) malloc(strlen(totalNodeName)+strlen(shortName)+1);
         strcpy(totalName, totalNodeName);
         strcat(totalName, shortName);

         if (currentNodeName != NULL)
         {
            currentName = (char*) malloc(strlen(currentNodeName)+strlen(shortName)+1);
            strcpy(currentName, currentNodeName);
            strcat(currentName, shortName);
         }
      }

      if (sNewValue != NULL)
      {
         unsigned int oldValue = 0;
         unsigned int newValue = 0;

         bool withDelta = false;
         bool withValidDelta = false;

         if (param->type == DM_ENG_ParameterType_UINT)
         {
            if (!DM_ENG_stringToUint(param->value, &oldValue)) { sd->continued=false; } // pas d'ancienne valeur, d'office, on considère qu'il n'y a pas continuité 
            DM_ENG_stringToUint(sNewValue, &newValue);                  
         }
         DM_ENG_Parameter_markPushed(param);
         _updateValueIfChanged(param, sNewValue);

         if ((param->type == DM_ENG_ParameterType_UINT) || (param->type == DM_ENG_ParameterType_BOOLEAN))
         {
            if (param->type == DM_ENG_ParameterType_UINT)
            {
               // affectation du _Delta
               param = _getStatParameter(name, _DELTA_SUFFIX);
               withDelta = (param != NULL);

               if (withDelta) // la valeur remontée est un cumul, il faut calculer le delta
                              // dans la suite, on utilise le delta et non la valeur brute remontée
               {
                  withValidDelta = sd->continued && (newValue >= oldValue);
                  newValue = (withValidDelta ? newValue-oldValue : 0);
                  sNewValue = DM_ENG_uintToString(newValue);
                  _updateNewValue(param, sNewValue);
               }
            }

            if (totalName != NULL) // sinon c'est qu'il y a un problème sur le nom de stat remonté, on ne fait pas d'agrégation
            {
               if (!withDelta || withValidDelta)
               {
                  _computeStats(totalName, newValue, sNewValue, sd->timestamp);

                  if (currentName != NULL)
                  {
                     if (slidingMode)
                     {
                        _computeSlidingStats(currentName, sNewValue, currentNodeName, valuesToSave);
                     }
                     else
                     {
                        _computeStats(currentName, newValue, sNewValue, sd->timestamp);
                     }
                  }
               }
               else if (slidingMode) // dans ce cas, il faut tt de même insérer une valeur vide dans le csv
               {
                  _computeSlidingStats(currentName, NULL, currentNodeName, valuesToSave);
               }
            }

            if (withDelta)
            {
               free(sNewValue);
            }
         }

         // Affectation du timestamp et calcul de la distance
         param = _getStatParameter(name, _TIMESTAMP_SUFFIX);
         if (param != NULL)
         {
            time_t oldTimestamp = 0;
            DM_ENG_dateStringToTime(param->value, &oldTimestamp);
            char* sValue = DM_ENG_dateTimeToString(sd->timestamp);
            _updateNewValue(param, sValue);
            free(sValue);
            if (sd->continued && (oldTimestamp != 0) && (sd->timestamp >= oldTimestamp) && (totalName != NULL))
            {
               unsigned int distance = sd->timestamp - oldTimestamp;

               // affection du Total.Xxx!DelayHistogram
               char* nameHisto = (char*) malloc(strlen(totalName)+strlen(_DELAY_HISTOGRAM_SUFFIX)+1);
               strcpy(nameHisto, totalName);
               strcat(nameHisto, _DELAY_HISTOGRAM_SUFFIX);
               _computeHisto(nameHisto, distance);
               free(nameHisto);

               if (currentName != NULL)
               {
                  // affection du CurrentSample.Xxx!DelayHistogram
                  nameHisto = (char*) malloc(strlen(currentName)+strlen(_DELAY_HISTOGRAM_SUFFIX)+1);
                  strcpy(nameHisto, currentName);
                  strcat(nameHisto, _DELAY_HISTOGRAM_SUFFIX);
                  _computeHisto(nameHisto, distance);
                  free(nameHisto);
               }
            }
         }
      }
      else if (slidingMode && (totalName != NULL)) // dans ce cas, on insére une valeur vide dans le csv
      {
         _computeSlidingStats(currentName, NULL, currentNodeName, valuesToSave);
      }

      if (readingName != NULL) { free(readingName); }
      if (totalName != NULL) { free(totalName); }
      if (currentName != NULL) { free(currentName); }
   }
}

void DM_ENG_StatisticsModule_processSampleData(DM_ENG_SampleDataStruct* sd)
{
   if ((sd->statObject != NULL) && (sd->parameterList != NULL))
   {
      DM_ENG_ParameterManager_convertNamesToLong(sd->statObject, sd->parameterList);
      DM_ENG_Parameter* param = NULL; // N.B. Pointeur réutilisé car l'accès aux données paramètres (ParameterData) peut restreindre l'accès à un seul à la fois

      if (_checkStatEnabled(sd->statObject)) // si le status n'est pas Enabled, on ignore les données
      {
         param = DM_ENG_ParameterData_getParameter(sd->statObject);
         if ((param != NULL) && ((param->loadingMode & 2) == 2))
         {
            DM_ENG_ParameterManager_updateParameterNames(sd->statObject, sd->parameterList);
         }

         bool slidingMode = false;
         unsigned int sampleInterval = _SAMPLE_INTERVAL_DEFAULT;
         unsigned int valuesToSave = 0; // nb de valeurs à garder dans la fenêtre glissante

         char* readingNodeName = (char*) malloc(strlen(sd->statObject)+strlen(_READING_NODE)+1);
         strcpy(readingNodeName, sd->statObject);
         strcat(readingNodeName, _READING_NODE);

         char* totalNodeName = (char*) malloc(strlen(sd->statObject)+strlen(_TOTAL_NODE)+1);
         strcpy(totalNodeName, sd->statObject);
         strcat(totalNodeName, _TOTAL_NODE);

         char* currentNodeName = NULL;
         if (_isSampleEnabled(sd->statObject, false))
         {
            _getSampleSettings(sd->statObject, NULL, &sampleInterval, NULL, NULL);

            currentNodeName = (char*) malloc(strlen(sd->statObject)+strlen(_CURRENT_PERIOD_NODE)+1);
            strcpy(currentNodeName, sd->statObject);
            strcat(currentNodeName, _CURRENT_PERIOD_NODE);

            param = _getStatParameter(currentNodeName, _SLIDING_MODE_NAME);
            if (param != NULL) { DM_ENG_stringToBool(param->value, &slidingMode); }

            _pushSample(sd->statObject, _BEFORE_ADDING_SAMPLE_DATA, sd->timestamp, slidingMode);
         }

         unsigned int duration = 0; // Peut servir pour des moyennes pondérées, si duration et deltaSeconds tous deux non nuls
         unsigned int deltaSeconds = 0;
         bool suspectData = false;

         time_t lastTimestamp = 0;

         param = _getStatParameter(readingNodeName, _LAST_TIMESTAMP_NAME);
         if (param != NULL)
         {
            bool lastTimestampKnown = DM_ENG_dateStringToTime(param->value, &lastTimestamp);
            char* sValue = DM_ENG_dateTimeToString(sd->timestamp);
            _updateNewValue(param, sValue);
            free(sValue);

            if (lastTimestampKnown && sd->continued && (sd->timestamp > lastTimestamp))
            {
               deltaSeconds = sd->timestamp-lastTimestamp;

               param = _getStatParameter(totalNodeName, _DURATION_NAME);
               if (param != NULL)
               {
                  DM_ENG_stringToUint(param->value, &duration);

                  duration += deltaSeconds;
                  char* sValue = DM_ENG_uintToString(duration);
                  _updateNewValue(param, sValue);
                  free(sValue);
               }
            }
         }

         if (sd->suspectData)
         {
            param = _getStatParameter(totalNodeName, _SUSPECT_DATA_NAME);
            if (param != NULL)
            {
               _updateNewValue(param, (char*)DM_ENG_TRUE_STRING);
            }
         }

         if (currentNodeName != NULL)
         {
            param = _getStatParameter(currentNodeName, _TIMESTAMP_CSV_NAME);
            if (param != NULL)
            {
               if (param->value != NULL)
               {
                  // calcul de valuesToSave
                  char* timestamps = strdup(param->value);
                  char* ts = timestamps;
                  while (*ts != '\0')
                  {
                     bool endList = false;
                     char* ch = ts;
                     while ((*ch != '\0') && (*ch != ',')) { ch++; }
                     if (*ch == ',') { *ch = '\0'; ch++; }
                     else { endList = true; }
                     time_t t = 0;
                     DM_ENG_dateStringToTime(ts, &t);
                     ts = ch;
                     if ((time_t)(t + sampleInterval) > sd->timestamp )
                     {
                        if (endList)
                        {
                           valuesToSave = 1;
                        }
                        else
                        {
                           valuesToSave = 2;
                           while (*ts != '\0')
                           {
                              if (*ts == ',') { valuesToSave++; }
                              ts++;
                           }
                        }
                        break;
                     }
                  }
                  free(timestamps);
               }

               char* sValue = DM_ENG_dateTimeToString(sd->timestamp);
               _addSlidingValue(param, sValue, valuesToSave);
               free(sValue);
            }

            param = _getStatParameter(currentNodeName, _DURATION_CSV_NAME);
            if (param != NULL)
            {
               char* sValue = DM_ENG_uintToString(deltaSeconds);
               _addSlidingValue(param, sValue, valuesToSave);
               free(sValue);
               duration = _sumCsv(param->value);
            }

            param = _getStatParameter(currentNodeName, _SUSPECT_DATA_CSV_NAME);
            if (param != NULL)
            {
               _addSlidingValue(param, (char*)(sd->suspectData ? DM_ENG_TRUE_STRING : DM_ENG_FALSE_STRING), valuesToSave);
               suspectData = _orCsv(param->value);
            }

            param = _getStatParameter(currentNodeName, _DURATION_NAME);
            if (param != NULL)
            {
               if (!slidingMode)
               {
                  duration = 0;
                  DM_ENG_stringToUint(param->value, &duration);
                  duration += deltaSeconds;                  
               }
               char* sValue = DM_ENG_uintToString(duration);
               _updateNewValue(param, sValue);
               free(sValue);
            }

            param = _getStatParameter(currentNodeName, _SUSPECT_DATA_NAME);
            if (param != NULL)
            {
               bool prev = false;
               DM_ENG_stringToBool(param->value, &prev);
               if (slidingMode)
               {
                  if (suspectData != prev)
                  {
                     _updateNewValue(param, (char*)(sd->suspectData ? DM_ENG_TRUE_STRING : DM_ENG_FALSE_STRING));
                  }
               }
               else
               {
                  if (sd->suspectData && !prev)
                  {
                     _updateNewValue(param, (char*)DM_ENG_TRUE_STRING);
                  }
               }
            }
         }

         // traitement des données remontées
         int j;
         for (j = 0; sd->parameterList[j] != NULL; j++)
         {
            char* name = (char*)sd->parameterList[j]->parameterName;
            if (!DM_ENG_Parameter_isNode(name))
            {
               _processOneData(name, sd->parameterList[j]->value, sd, readingNodeName, totalNodeName, currentNodeName, slidingMode, valuesToSave);
            }
         }

         // traitement des données de stat calculées (à partir des données remontées) + variables non notifiées
         char* prmName;
         for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
         {
            if ((strlen(prmName) > strlen(readingNodeName)) && (strncmp(prmName, readingNodeName, strlen(readingNodeName)) == 0))
            {
               param = DM_ENG_ParameterData_getCurrent();
               if ((!param->writable) && ((param->immediateChanges == 1) || (param->immediateChanges == 2))) // signature de variable de stat
               {
                  char* resumeName = NULL;
                  char* newValue = NULL;
                  bool toProcess = slidingMode;
                  if (param->storageMode == DM_ENG_StorageMode_COMPUTED) // variable de stat calculée
                  {
                     resumeName = strdup(prmName);
                     DM_ENG_ParameterManager_loadLeafParameterValue(param, true);
                     param = DM_ENG_ParameterData_getParameter(resumeName); // on recharge le param courant
                     if (DM_ENG_Parameter_isPushed(param))
                     {
                        newValue = (param->value == NULL ? NULL : strdup(param->value));
                        toProcess = true;
                     }
                  }
                  else if (slidingMode) // variable de stat : si non notifiée, on insère une valeur vide
                  {
                     for (j = 0; toProcess && (sd->parameterList[j] != NULL); j++)
                     {
                        if (strcmp(prmName, sd->parameterList[j]->parameterName) == 0) { toProcess = false; }
                     }
                  }
                  if (toProcess)
                  {
                     if (resumeName == NULL) { resumeName = strdup(prmName); }
                     _processOneData(resumeName, newValue, sd, readingNodeName, totalNodeName, currentNodeName, slidingMode, valuesToSave);
                  }
                  if (newValue != NULL) { free(newValue); }
                  if (resumeName != NULL)
                  {
                     DM_ENG_ParameterManager_resumeParameterLoopFrom(resumeName);
                     free(resumeName);
                  }
               }
            }
         }

         for (prmName = DM_ENG_ParameterData_getFirstName(); prmName!=NULL; prmName = DM_ENG_ParameterData_getNextName())
         {
            if ((strlen(prmName) > strlen(readingNodeName)) && (strncmp(prmName, readingNodeName, strlen(readingNodeName)) == 0))
            {
               param = DM_ENG_ParameterData_getCurrent();
               DM_ENG_Parameter_unmarkPushed(param);
            }
         }

         // si on a atteint la fin d'un SampleInterval, on doit reverser le CurrentSample dans le SamplesReport
         if (currentNodeName != NULL)
         {
            _pushSample(sd->statObject, _AFTER_ADDING_SAMPLE_DATA, lastTimestamp, slidingMode);
         }

         free(totalNodeName);
         free(readingNodeName);
         if (currentNodeName != NULL) { free(currentNodeName); }

         DM_ENG_ParameterData_sync();
      }
   }
   DM_ENG_deleteSampleDataStruct(sd);
}

/*
 *
 */
static void* _runPolling(void* data UNUSED)
{
   DM_ENG_ParameterManager_lockDM();
   DM_CMN_Thread_lockMutex(_pollingMutex);

   if (_samplingStatus == _INITIAL)
   {
      DBG("Start sampling - Begin");
      _samplingStatus = _STARTING;
      while (DM_ENG_Device_startSampling(NULL, 0, 0, NULL) == 1)
      {
         DM_CMN_Thread_unlockMutex(_pollingMutex);
         DM_ENG_ParameterManager_unlock();
         DM_CMN_Thread_msleep(500);
         DM_ENG_ParameterManager_lockDM();
         DM_CMN_Thread_lockMutex(_pollingMutex);
      }
   }

   while (statsToPoll != NULL)
   {
      if (_samplingStatus == _STARTING)
      {
         _startAllSamplings();
         _samplingStatus = _RUNNING;
         DBG("Start sampling - End");
      }

      time_t nextPollingTime = 0;
      DM_ENG_ParameterValueStruct* stp = statsToPoll; // accès sous mutex
      time_t lastTimestamp = 0; // pour le stp courant
      time_t pollingTime = 0;   // id.
      while (stp != NULL)
      {
         // Calcul du prochain pollingTime
         DM_ENG_Parameter* prm = _getStatParameter(stp->parameterName, _LAST_TIMESTAMP_NAME);
         lastTimestamp = 0;
         if (prm != NULL) { DM_ENG_dateStringToTime(prm->value, &lastTimestamp); }

         time_t now = time(NULL);
         pollingTime = stp->timestamp; // la date de la précédente tentative
         if (pollingTime == 0) { pollingTime = lastTimestamp; } // à défaut, on prend le lastTimestamp

         unsigned int subSampleInterval = _SUB_SAMPLE_INTERVAL_DEFAULT;
         prm = _getStatParameter(stp->parameterName, _SUB_SAMPLE_INTERVAL_NAME);
         if (prm != NULL) { DM_ENG_stringToUint(prm->value, &subSampleInterval); }

         time_t newPollingTime = pollingTime + subSampleInterval;

         if ((pollingTime == 0) || (newPollingTime <= now))
         {
            pollingTime = now; // cas où on (re)démarre juste la collecte de stats => polling immédiat et on se cale sur now
         }
         else
         {
            unsigned int retryDelay = (unsigned int)stp->type;
            if (retryDelay > 0) // on est dans une procédure de retry
            {
               if (retryDelay > subSampleInterval) // on le limite à subSampleInterval
               {
                  retryDelay = subSampleInterval;
                  stp->type = (DM_ENG_ParameterType)retryDelay;
               }
               newPollingTime = pollingTime + retryDelay;
            }

            // prise en compte du SampleSet... pour caler si nécessaire les temps de polling sur les SampleIntervals
            time_t timeRef = 0;
            unsigned int sampleInterval = 0;
            _getSampleSettings(stp->parameterName, &timeRef, &sampleInterval, NULL, NULL);
            // calage sur les SampleIntervals
            time_t base = ((lastTimestamp == 0) || (retryDelay > 0) ? pollingTime : lastTimestamp);
            time_t sampleTime = base + sampleInterval;
            if (timeRef != base)
            {
               if (timeRef < base) { sampleTime -= (base-timeRef)%sampleInterval; }
               else { sampleTime = base + (timeRef-base)%sampleInterval; }
            }
            if ((sampleTime > pollingTime) && (sampleTime < newPollingTime)) { newPollingTime = sampleTime; }

            pollingTime = newPollingTime;
         }
         if ((nextPollingTime == 0) || (pollingTime < nextPollingTime)) { nextPollingTime = pollingTime; }

         if (pollingTime <= now) break;

         stp = stp->next;
      }

      if (stp != NULL) // on va chercher les données
      {
         DBG("SampleData polling (%s) - Begin", stp->parameterName);
         // on fait le polling
         DM_ENG_SampleDataStruct* sampleData = NULL;

         // on déverrouille pour ne pas bloquer l'accès aux données car l'appel DM_ENG_Device_getSampleData peut être assez long
         DM_CMN_Thread_unlockMutex(_pollingMutex);
         DM_ENG_ParameterManager_unlock();
         DM_CMN_Thread_lockMutex(_pollingMutex);
         if (_samplingStatus != _RUNNING) continue;

         int res = DM_ENG_Device_getSampleData(stp->parameterName, lastTimestamp, stp->value, &sampleData);

         DM_CMN_Thread_unlockMutex(_pollingMutex);
         DM_ENG_ParameterManager_lockDM();
         DM_CMN_Thread_lockMutex(_pollingMutex);
         if (_samplingStatus != _RUNNING)
         {
            if (sampleData != NULL) { DM_ENG_deleteSampleDataStruct(sampleData); }
            continue;
         }

         stp->timestamp = pollingTime;
         if ((res != 0) || (sampleData == NULL)) // pas de donnée obtenue, on configure le mécanisme de retry 
         {
            unsigned int retryDelay = (unsigned int)stp->type;
            if (retryDelay == 0) { retryDelay = _INITIAL_RETRY_DELAY; }
            else { retryDelay *= 2; } // il sera écrêté plus tard s'il dépasse le SubSampleInterval
            stp->type = (DM_ENG_ParameterType)retryDelay;
         }
         else // traitement des données lues
         {
            stp->type = 0; // raz du retryDelay
            DM_ENG_StatisticsModule_processSampleData(sampleData);
         }
         DBG("SampleData polling - End");
      }
      else // on attend le nextPollingTime
      {
         DM_CMN_Thread_unlockMutex(_pollingMutex);
         DM_ENG_ParameterManager_unlock();
         DM_CMN_Thread_lockMutex(_pollingMutex);
         if (_samplingStatus != _RUNNING) continue;

         DM_CMN_Timespec_t timeout;
         timeout.sec = nextPollingTime;
         timeout.msec = 0;
         DM_CMN_Thread_timedWaitCond(_pollingCond, _pollingMutex, &timeout);

         DM_CMN_Thread_unlockMutex(_pollingMutex);
         DM_ENG_ParameterManager_lockDM();
         DM_CMN_Thread_lockMutex(_pollingMutex);
      }
   }
   _pollingThreadId = 0;

   DM_CMN_Thread_unlockMutex(_pollingMutex);

   if (_samplingStatus == _EXITING)
   {
      DM_CMN_Thread_destroyCond(_pollingCond); _pollingCond = NULL;
      DM_CMN_Thread_destroyMutex(_pollingMutex); _pollingMutex = NULL;
      _samplingStatus = _INITIAL;
   }

   DM_ENG_ParameterManager_unlock();
   return 0;
}

static void _signalPolling()
{
   if (_pollingThreadId == 0)
   {
      DM_CMN_Thread_create(_runPolling, NULL, true, &_pollingThreadId);
   }
   else
   {
      DM_CMN_Thread_signalCond(_pollingCond);
   }
}
