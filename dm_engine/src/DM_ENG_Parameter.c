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
 * File        : DM_ENG_Parameter.c
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
 * @file DM_ENG_Parameter.c
 *
 * @brief Object representing one parameter with all relevant data
 *
 * LOGIQUE
 * 1) Pour les param SYSTEM ou MIXED
 * value != NULL ssi param chargé (avec les valeurs). Si objet => les param (et instances) en dessous sont chargés.
 * pour les objets, value == NULL et backValue != NULL => les param sont chargés sans les valeurs (les instances sont créées)
 *
 * 2)
 * (!param->writable) && (param->immediateChanges == 1) : signature d'une variable statistique
 * (!param->writable) && (param->immediateChanges == 2) : signature d'une variable statistique à valeur cumulative
 * (!param->writable) && (param->immediateChanges == 3) : signature d'un param à valeur drapeau (aussitôt notifiée, on l'efface). Ex : Trigger
 **/

#include "DM_ENG_Error.h"
#include "DM_ENG_Global.h"
#include "DM_ENG_InformMessageScheduler.h"
#include "DM_ENG_DiagnosticsLauncher.h"
#include <ctype.h>

#define _FLAG_TRIGGER 3

static const char* _WRITABLE = "W";
//static const char* _READONLY = "R";

static const char* _YES = "Y";
//static const char* _NO = "N";

static const char* _NONE = "_";

// Bits de l'attribut d'état d'un paramètre, state = (<change requested>, <value changed>)
enum
{
   _INITIAL = 0,
   _CHANGE_REQUESTED = 2,
   _BEING_EVALUATED = 4,
   _LOADED = 8,
   _PUSHED = 16,
   _VALUE_CHANGED = 1
};

static bool _dataChanged = false;

bool DM_ENG_Parameter_isDataChanged()
{
   return _dataChanged;
}

void DM_ENG_Parameter_setDataChanged(bool changed)
{
   _dataChanged = changed;
}

DM_ENG_Parameter* DM_ENG_newParameter(char* attr[], bool ini)
{
   DM_ENG_Parameter* res = (DM_ENG_Parameter*) malloc(sizeof(DM_ENG_Parameter));
   res->name = (char*) malloc(strlen(attr[0])+strlen(DM_ENG_PARAMETER_PREFIX)+1);
   strcpy( res->name, DM_ENG_PARAMETER_PREFIX );
   strcat( res->name, attr[0] );

   res->minValue = 0; // par défaut
   res->maxValue = 0; // par défaut
   res->type = DM_ENG_stringToType(attr[1], &res->minValue, &res->maxValue);

   // 0 : DM_ENG_StorageMode_DM_ONLY, 1 : DM_ENG_StorageMode_SYSTEM_ONLY, 2 : DM_ENG_StorageMode_MIXED
   res->storageMode = (DM_ENG_StorageMode)DM_ENG_charToInt(*attr[2]);
   if ((int)res->storageMode == -1) { res->storageMode = DM_ENG_StorageMode_DM_ONLY; }

   // 2 codages possibles : codage chiffré ou littéral (R/W)
   int iAttr = DM_ENG_charToInt(*attr[3]);
   res->writable = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr[3], _WRITABLE)==0) );

   // 2 codages possibles : codage chiffré ou littéral (Y/N)
   iAttr = DM_ENG_charToInt(*attr[4]);
   res->immediateChanges = ( iAttr != -1 ? iAttr : (strcmp(attr[4], _YES)==0 ? 1 : 0) );

   // 0 : pas de notification, 1 : notif passive, 2 : notif active
   res->notification = (DM_ENG_NotificationMode)DM_ENG_charToInt(*attr[5]);
   if ((int)res->notification == -1) { res->notification = DM_ENG_NotificationMode_OFF; }

   // 2 codages possibles : codage chiffré ou littéral (Y/N)
   iAttr = DM_ENG_charToInt(*attr[6]);
   res->mandatoryNotification = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr[6], _YES)==0) );

   // 2 codages possibles : codage chiffré ou littéral (Y/N)
   iAttr = DM_ENG_charToInt(*attr[7]);
   res->activeNotificationDenied = ( iAttr != -1 ? (bool)iAttr : (strcmp(attr[7], _YES)==0) );

   // copie si présent sinon NULL
   res->accessList = (*attr[8]==0 ? (ini ? (char**)DM_ENG_DEFAULT_ACCESS_LIST : NULL)
                                  : (strcmp(attr[8], DM_ENG_DEFAULT_ACCESS_LIST[0])==0 ? (char**)DM_ENG_DEFAULT_ACCESS_LIST : DM_ENG_splitCommaSeparatedList(attr[8])));

   // copie si présent sinon NULL
   // si présent dans le fichier d'init, c'est la valeur par défaut
   res->value = (*attr[9]==0 ? NULL : strdup(attr[9]));

   // Les 3 attributs suivants ne sont pas présents dans le fichier d'initialisation

   // codé sinon 0
   iAttr = DM_ENG_charToInt(*attr[10]);
   res->loadingMode = ( iAttr != -1 ? iAttr : 0 );

   // codé sinon 0
   iAttr = DM_ENG_charToInt(*attr[11]);
   res->state = ( iAttr != -1 ? iAttr : 0 );

   // copie si présent sinon NULL
   res->backValue = (*attr[12]==0 ? NULL : strdup(attr[12]));

   // copie si présent sinon NULL
   res->configKey = (*attr[13]==0 ? NULL : strdup(attr[13]));

   // copie si présent sinon NULL
   res->definition = (*attr[14]==0 ? NULL : strdup(attr[14]));

   res->next = NULL;

   return res;
}

DM_ENG_Parameter* DM_ENG_newInstanceParameter(char* name, DM_ENG_Parameter* proto)
{
   DM_ENG_Parameter* res = (DM_ENG_Parameter*) malloc(sizeof(DM_ENG_Parameter));
   res->name = strdup(name);
   res->type = (proto->type == DM_ENG_ParameterType_STATISTICS ? DM_ENG_ParameterType_ANY : proto->type);
   res->minValue = (proto->type == DM_ENG_ParameterType_STATISTICS ? 0 : proto->minValue);
   res->maxValue = proto->maxValue;
   res->storageMode = proto->storageMode;
   res->writable = proto->writable;
   res->immediateChanges = proto->immediateChanges;
   res->accessList = proto->accessList;
   res->notification = proto->notification;
   res->mandatoryNotification = proto->mandatoryNotification;
   res->activeNotificationDenied = proto->activeNotificationDenied;
   res->loadingMode = (proto->loadingMode & 1);
   res->state = 0;
   res->backValue = (proto->backValue==NULL ? NULL : strdup(proto->backValue));
   res->value = (proto->value==NULL ? NULL : strdup(proto->value));
   res->configKey = (proto->configKey==NULL ? NULL : strdup(proto->configKey));
   res->definition = (proto->definition==NULL ? NULL : strdup(proto->definition));
   res->next = NULL;
   return res;
}

DM_ENG_Parameter* DM_ENG_newDefinedParameter(char* name, DM_ENG_ParameterType type, char* def, bool hidden)
{
   DM_ENG_Parameter* res = (DM_ENG_Parameter*) malloc(sizeof(DM_ENG_Parameter));
   res->name = strdup(name);
   res->type = type;
   res->minValue = 0;
   res->maxValue = UINT_MAX;
   res->storageMode = DM_ENG_StorageMode_DM_ONLY;
   res->writable = false;
   res->immediateChanges = 0;
   res->accessList = (hidden ? DM_ENG_splitCommaSeparatedList((char*)_NONE) : (char**)DM_ENG_DEFAULT_ACCESS_LIST);
   res->notification = DM_ENG_NotificationMode_OFF;
   res->mandatoryNotification = false;
   res->activeNotificationDenied = false;
   res->loadingMode = 0;
   res->state = 0;
   res->backValue = NULL;
   res->value = NULL;
   res->configKey = NULL;
   res->definition = (def==NULL ? NULL : strdup(def));
   res->next = NULL;
   return res;
}

void DM_ENG_deleteParameter(DM_ENG_Parameter* param)
{
   free(param->name);
   if ((param->accessList != NULL) && (param->accessList != (char**)DM_ENG_DEFAULT_ACCESS_LIST)) { DM_ENG_deleteTabString(param->accessList); }
   if ((param->value != NULL) && (param->value != DM_ENG_EMPTY)) { free(param->value); }
   if ((param->backValue != NULL) && (param->backValue != DM_ENG_EMPTY)) { free(param->backValue); }
   if ((param->configKey != NULL) && (param->configKey != DM_ENG_EMPTY)) { free(param->configKey); }
   if ((param->definition != NULL) && (param->definition != DM_ENG_EMPTY)) { free(param->definition); }
   free(param);
}

void DM_ENG_deleteAllParameter(DM_ENG_Parameter** pParam)
{
   DM_ENG_Parameter* param = *pParam;
   while (param != NULL)
   {
      DM_ENG_Parameter* pa = param;
      param = param->next;
      DM_ENG_deleteParameter(pa);
   }
   *pParam = NULL;
}

void DM_ENG_addParameter(DM_ENG_Parameter** pParam, DM_ENG_Parameter* newParam)
{
   DM_ENG_Parameter* param = *pParam;
   if (param==NULL)
   {
      *pParam = newParam;
   }
   else
   {
      while (param->next != NULL) { param = param->next; }
      param->next = newParam;
   }
}

bool DM_ENG_Parameter_isNode(char* paramName)
{
   return ( paramName[strlen(paramName)-1] == '.');
}

/*
 * Détermine si c'est le nom d'un objet instance, c'est à dire de la forme "Xxx...Zzz.999."
 * où 999 représente une liste d'au moins un chiffre 
 */
bool DM_ENG_Parameter_isNodeInstance(char* prmName)
{
   int k = strlen(prmName)-2;
   bool isNodeInstance = (prmName[k+1] == '.');
   do
   {
      isNodeInstance &= (k>1) && (prmName[k] >= '0') && (prmName[k] <= '9');
      k--;
   }
   while (isNodeInstance && (prmName[k] != '.'));
   return isNodeInstance;
}

/**
 * @return Returns true if it is a final paramater which can get a value.
 */
bool DM_ENG_Parameter_isValuable(char* paramName)
{
	return (!DM_ENG_Parameter_isNode(paramName) && (strstr(paramName, "..")==0));
}

/*
 * Retrouve le nom long à partir du nom fourni qui peut être relatif ou absolu
 *
 * @prmNam Nom du paramètre recherché. Il est fourni
 *  - soit en absolu. Il commence alors par le préfixe. Ex: 'Device.Aaa.Bbb.Ccc'.
 *  - soit en relatif (s'il ne commence pas par le préfixe) par rapport au niveau occupé par destName,
 *    avec possibilité de remonter les niveaux en commençant pas '..', '...' etc.
 *  Ex: 'Ccc' donnera 'Device.Aaa.Bbb.Ccc'. '.Ccc' donnera la même chose. '..Eee' donnera 'Device.Aaa.Eee'
 * @destName Nom (long) du paramètre (ou noeud) en train d'être calculé ou défini. Ex: Device.Aaa.Bbb.Ddd.
 *
 * Rq1 : '.' désigne le noeud courant (soit destName, soit le noeud père de destName, si destName est une feuille
 * Rq2 : '.Device' désignerait un paramètre ou un objet nommé "Device" sous le noeud courant sans faire référence
 *       à la racine du data model (nommage à éviter) 
 */
char* DM_ENG_Parameter_getLongName(char* prmName, const char* destName)
{
   char* longName = NULL;
   size_t len = (destName == NULL ? 0 : strlen(destName));
   if ((len == 0) || (strncmp(prmName, DM_ENG_PARAMETER_PREFIX, strlen(DM_ENG_PARAMETER_PREFIX)) == 0))
   {
      longName = strdup(prmName);
   }
   else
   {
      int pts = 0;
      while (*prmName == '.') { prmName++; pts++; } // on passe et on compte les points du début
      if (pts == 0) { pts = 1; } // aucun point ou un point, c'est la même chose 
      longName = (char*) malloc(len+strlen(prmName)+1);
      strcpy(longName, destName);
      do
      {
         len--;
         if (longName[len] == '.') { longName[len+1] = '\0'; pts--; }
      }
      while ((len > 0) && (pts > 0));
      strcat(longName, prmName);
   }
   return longName;
}

/**
 * @param param Current parameter
 * @param notification The notification to set. This attribute is not set if the value is DM_ENG_NotificationMode.UNDEFINED.
 * @param accessList The accessList to set. This attribute is not set if the value is NULL.
 */
int DM_ENG_Parameter_setAttributes(DM_ENG_Parameter* param, int notification, char* accessList[])
{
   if (notification != DM_ENG_NotificationMode_UNDEFINED)
   {
		if ((notification == DM_ENG_NotificationMode_ACTIVE) && param->activeNotificationDenied)
		{
         return DM_ENG_NOTIFICATION_REQUEST_REJECTED;
		}
		param->notification = (DM_ENG_NotificationMode)notification;
   }
   if (accessList != NULL)
   {
      if ((param->accessList != NULL) && (param->accessList != (char**)DM_ENG_DEFAULT_ACCESS_LIST))
      {
         DM_ENG_deleteTabString(param->accessList);
      }
      param->accessList = (accessList == (char**)DM_ENG_DEFAULT_ACCESS_LIST ? (char**)DM_ENG_DEFAULT_ACCESS_LIST : DM_ENG_copyTabString(accessList));
   }
   _dataChanged = true;
   return 0;
}

/**
 * @return Get the value and returns it if pValue non NULL.
 *
 * N.B. Ni param, ni pValue ne doivent être NULL 
 */
int DM_ENG_Parameter_getValue(DM_ENG_Parameter* param, OUT char** pValue)
{
   if ((param->type == DM_ENG_ParameterType_DATE) && ((param->value==NULL) || param->value==DM_ENG_EMPTY))
   {
      param->value = strdup(DM_ENG_UNKNOWN_TIME);
      _dataChanged = true;
   }
	*pValue = (param->value==NULL ? NULL : strdup(param->value));
	return 0;
}

/*
 * Sert à filter les valeurs remontées à l'ACS. 3 cas : Password, ConnectionRequestPassword, STUNPassword remontés comme chaînes vides.
 */
char* DM_ENG_Parameter_getFilteredValue(DM_ENG_Parameter* param)
{
   return ( ((strcmp(param->name, DM_TR106_ACS_PASSWORD) == 0)
          || (strcmp(param->name, DM_TR106_CONNECTIONREQUESTPASSWORD) == 0)
          || (strcmp(param->name, DM_TR106_STUN_PASSWORD) == 0))
      ? (char*)DM_ENG_EMPTY
      : param->value);
}

void DM_ENG_Parameter_commit(DM_ENG_Parameter* param);

// param prend la propriété de newVal
static void _setNewValue(DM_ENG_Parameter* param, char* newVal)
{
   if ((param->value != NULL) && (param->value != DM_ENG_EMPTY)) { free(param->value); }
   param->value = newVal;
}

/**
 * Sets the state attribute to _INITIAL
 */
void DM_ENG_Parameter_initState(DM_ENG_Parameter* param)
{
   if (param->state != _INITIAL)
   {
      param->state = _INITIAL;
      if ((!param->writable) && (param->immediateChanges == _FLAG_TRIGGER)) // signature d'un param à valeur drapeau (aussitôt notifiée, on l'efface)
      {
         if (param->backValue == NULL)
         {
            _setNewValue(param, NULL);
         }
         else
         {
            // si param->backValue de la forme "Default/Trigger", on ne remplace la valeur (par "Default") que si on avait "Trigger"
            char* separ = strchr(param->backValue, '/');
            if (separ != NULL)
            {
               *separ = '\0';
               if (strcmp(param->value, separ+1) == 0) // on avait bien la valeur "Trigger" (sinon on ne change rien)
               {
                  _setNewValue(param, strdup(param->backValue));
               }
               *separ = '/';
            }
            else
            {
               _setNewValue(param, strdup(param->backValue));
            }
         }
      }
      _dataChanged = true;
   }
}

/**
 * Sets the value provided by the system and commit to notify.
 * This function is called from low level (system notification).
 * 
 * @param value The new value. Clears the value if NULL
 * @param newValue True if it is a new pushed value, false if the value is just loaded 
 */
void DM_ENG_Parameter_updateValue(DM_ENG_Parameter* param, char* value, bool newValue)
{
   _setNewValue(param, ((value==NULL) || (value==(char*)DM_ENG_EMPTY) ? value : strdup(value)));
   _dataChanged = true;
   if (newValue)
   {
      param->state |= _VALUE_CHANGED | _PUSHED;
      DM_ENG_Parameter_commit(param);
   }
}

void DM_ENG_Parameter_resetValue(DM_ENG_Parameter* param)
{
   char* newVal = NULL;
   if (DM_ENG_Parameter_isDiagnosticsState(param->name))
   {
      newVal = strdup(DM_ENG_NONE_STATE);
   }
   // ajouter ici les resets particuliers
   _setNewValue(param, newVal);
}

void DM_ENG_Parameter_markLoaded(DM_ENG_Parameter* param, bool withValues)
{
   if (withValues) // il faut (param->value != NULL)
   {
      if (param->value == NULL)
      {
         param->value = (char*)DM_ENG_EMPTY;
         _dataChanged = true;
      }
      if ((param->storageMode == DM_ENG_StorageMode_COMPUTED) && ((param->state & _LOADED) == 0))
      {
         param->state |= _LOADED;
         _dataChanged = true;
      }
   }
   else // il faut (param->value != NULL) || (param->backValue != NULL)
   if ((param->value == NULL) && (param->backValue == NULL))
   {
      param->backValue = (char*)DM_ENG_EMPTY;
      _dataChanged = true;
   }
}

/**
 * Clear the temporary value in the database
 * 
 * @param param The parameter
 * @param level Level to choose the parameters which must be cleared, defined as follows :
 *   0 : only COMPUTED
 *   1 : COMPUTED + SYSTEM ONLY (when ParameterManager_unlock is called)
 *   2 : all writable parameters
 *
 * (*) Au level 1, les noeuds sont dé-marqués pour permettre de recharger les param SYSTEM_ONLY qu'ils peuvent englober
 *     sauf s'ils sont déclarés MIXED, cas où tous les descendants doivent aussi être MIXED ou COMPUTED (non SYSTEM_ONLY)
 */
void DM_ENG_Parameter_clearTemporaryValue(DM_ENG_Parameter* param, int level)
{
   if ((level == 2)
   || ((level == 1) && (((param->name[strlen(param->name)-1] == '.') && (param->storageMode != DM_ENG_StorageMode_MIXED)) // Voir (*)
                     || (param->storageMode == DM_ENG_StorageMode_SYSTEM_ONLY))))
   {
      if ((param->backValue != NULL) && (param->writable || (param->immediateChanges != _FLAG_TRIGGER))) // exclut cas (!param->writable) && (param->immediateChanges == _FLAG_TRIGGER) où backup mémorise la valeur drapeau par défaut
      {
         if (param->backValue != DM_ENG_EMPTY) { free(param->backValue); }
         param->backValue = NULL;
         _dataChanged = true;
      }
      if ((param->value != NULL) && (((param->state & _VALUE_CHANGED) == 0) || (level == 2)))
         // optimisation : on n'efface pas la valeur qui vient d'être remontée (par un dataNewValue, ...) et n'a pas encore été notifiée 
      {
         if (param->value != DM_ENG_EMPTY) { free(param->value); }
         param->value = NULL;
         param->state = _INITIAL;
         _dataChanged = true;
      }
   }
   else if (param->storageMode == DM_ENG_StorageMode_COMPUTED)
   {
      if ((param->value != NULL) && (param->definition != NULL))
      {
         char* def = param->definition;
         while (*def == ' ') { def++; } // on passe les espaces de début
         while (isalnum(*def) || (*def == '.') || (*def == '_')) { def++; } // on passe le nom ou le nombre
         while (*def == ' ') { def++; } // on passe les espaces de fin
         if (*def == 0) // cas d'un nombre ou d'un nom (simple redirection), on efface. Exemple : redirection sur un param Trigger
                        // sinon c'est un cas avec expression. En gardant la valeur calculée, on peut savoir ensuite si la valeur a changé.
                        // Exemple : calcul d'une alarme sur dépassement de seuil
         {
            if (param->value != DM_ENG_EMPTY) { free(param->value); }
            param->value = NULL;
            _dataChanged = true;
         }
      }
      int stateToClear = _LOADED | _PUSHED;
      if ((param->state & stateToClear) != 0)
      {
         param->state &= ~stateToClear;
         _dataChanged = true;
      }
   }
}

// N'a de sens que pour les noeuds et les paramètres système
// Si withValues == false, n'a de sens que sur les noeuds
/*bool DM_ENG_Parameter_isLoaded(DM_ENG_Parameter* param, bool withValues)
{
   return ((param->value != NULL) || (!withValues && (param->backValue != NULL)));
}*/

bool DM_ENG_Parameter_isLoaded(DM_ENG_Parameter* param, bool withValues)
{
   bool isNode = (param->name[strlen(param->name)-1] == '.');
   if (!isNode && ((param->type == DM_ENG_ParameterType_DATE) || (param->type == DM_ENG_ParameterType_BOOLEAN))
     && ((param->value==DM_ENG_EMPTY) || ((param->storageMode==DM_ENG_StorageMode_DM_ONLY) && (param->value==NULL))))
   {
      param->value = strdup(param->type == DM_ENG_ParameterType_DATE ? DM_ENG_UNKNOWN_TIME : DM_ENG_FALSE_STRING);
      _dataChanged = true;
   }
   return (((param->value != NULL) && ((param->storageMode!=DM_ENG_StorageMode_COMPUTED) || ((param->state & _LOADED) != 0)))
        || (!isNode && (param->storageMode == DM_ENG_StorageMode_DM_ONLY)) // storageMode n'a de sens que sur les feuilles
        || (!withValues && (!isNode || (param->backValue != NULL)))); // cas !withValues : les feuilles sont nécessairement à jour, les noeuds le sont si backValue != NULL
}

/**
 * Returns true if the value changed
 */
bool DM_ENG_Parameter_isValueChanged(DM_ENG_Parameter* param)
{
   return ((param->state & _VALUE_CHANGED) != 0);
}

int DM_ENG_Parameter_checkBeforeSetValue(DM_ENG_Parameter* param, char* value)
{
   if ((param->state & _CHANGE_REQUESTED) != 0)
   {
      return DM_ENG_INVALID_ARGUMENTS; // Paramètre apparaissant plus d'une fois dans une requête SetParameterValues
   }

   bool typeOk = true;
   switch (param->type)
   {
      case DM_ENG_ParameterType_INT :
      {
         int iVal;
         typeOk = (DM_ENG_stringToInt(value, &iVal) && (iVal>=param->minValue) && (iVal<=param->maxValue));
         break;
      }
      case DM_ENG_ParameterType_UINT :
      {
         unsigned int uVal;
         typeOk = (DM_ENG_stringToUint(value, &uVal) && (uVal>=(unsigned int)param->minValue) && (uVal<=(unsigned int)param->maxValue));
         break;
      }
      case DM_ENG_ParameterType_LONG :
         typeOk = DM_ENG_stringToLong(value, NULL);
         break;
      case DM_ENG_ParameterType_BOOLEAN :
         typeOk = DM_ENG_stringToBool(value, NULL);
         break;
      case DM_ENG_ParameterType_DATE :
         typeOk = DM_ENG_dateStringToTime(value, NULL);
         break;
      case DM_ENG_ParameterType_STRING :
         typeOk = (value != NULL) && ((param->minValue == -1) || ((int)strlen(value) <= param->minValue));
         break;
      default : // DM_ENG_ParameterType_ANY, DM_ENG_ParameterType_STATISTICS, DM_ENG_ParameterType_UNDEFINED => OK
         break;
   }

   return (typeOk ? 0 : DM_ENG_INVALID_PARAMETER_VALUE);
}

/**
 * Sets the parameter value. Only called by RPC methods.
 *
 * @param value The value to set.
 *
 * @return Returns 1 for SYSTEM or MIXED parameters, 0 for DM_ONLY and COMPUTED
 */
int DM_ENG_Parameter_setValue(DM_ENG_Parameter* param, char* value)
{
   int res = 0;

   if ((param->backValue != NULL) && (param->backValue != DM_ENG_EMPTY)) { free(param->backValue); }
   param->backValue = param->value;
   param->value = NULL;

   _setNewValue(param, (value==NULL ? NULL : strdup(value)));
	param->state |= _CHANGE_REQUESTED;
   _dataChanged = true;

   if ((param->storageMode == DM_ENG_StorageMode_SYSTEM_ONLY) || (param->storageMode == DM_ENG_StorageMode_MIXED))
   {
      res = 1;
   }
   param->state |= _VALUE_CHANGED;
	return res;
}

bool DM_ENG_Parameter_isDiagnosticsState(const char* paramName)
{
   int iSuffix = strlen(paramName) - strlen(DM_ENG_DIAGNOSTICS_STATE_SUFFIX);
   return (iSuffix > 0) && (strcmp(paramName+iSuffix, DM_ENG_DIAGNOSTICS_STATE_SUFFIX) == 0);
}

bool DM_ENG_Parameter_isDiagnosticsParameter(const char* paramName, char** pObjName)
{
   int i=0;
   char* next = NULL;
   while ((next = strstr(paramName+i, DM_ENG_DIAGNOSTICS_OBJECT_SUFFIX)) != NULL)
   {
      i = (next-paramName)+strlen(DM_ENG_DIAGNOSTICS_OBJECT_SUFFIX);
   }
   if ((i!=0) && (pObjName != NULL))
   {
      (*pObjName) = strdup((char*)paramName);
      (*pObjName)[i] = '\0';
   }
   return (i!=0);  
}

bool DM_ENG_Parameter_isHidden(DM_ENG_Parameter* param)
{
   return (param->accessList != NULL) && (param->accessList[0] != NULL) && (strcmp(param->accessList[0], _NONE) == 0);
}

//void DM_ENG_Parameter_setHidden(DM_ENG_Parameter* param)
//{
//   if ((param->accessList != NULL) && (param->accessList != (char**)DM_ENG_DEFAULT_ACCESS_LIST)) { DM_ENG_deleteTabString(param->accessList); }   
//   param->accessList = DM_ENG_splitCommaSeparatedList((char*)_NONE);
//   _dataChanged = true;
//}

void DM_ENG_Parameter_addConfigKey(DM_ENG_Parameter* param, char* configKey)
{
   if ((configKey != NULL) && (param->configKey != NULL)) // on ne modifie pas les param prédéfinis (pour lesquels param->configKey == NULL)
   {
      DM_ENG_addToCommaSeparatedList(&param->configKey, configKey);
      _dataChanged = true;
   }
}

bool DM_ENG_Parameter_removeConfigKey(DM_ENG_Parameter* param, char* configKey)
{
   bool res = DM_ENG_deleteFromCommaSeparatedList(&param->configKey, configKey);
   _dataChanged |= res;
   return res;
}

/**
 * Validates the change and notify if necessary
 */
void DM_ENG_Parameter_commit(DM_ENG_Parameter* param)
{
	param->state &= ~_CHANGE_REQUESTED;
	if ((param->backValue != NULL) && param->writable)
   {
      if (param->backValue != DM_ENG_EMPTY) { free(param->backValue); }
      param->backValue = NULL;
   }
	if (param->state != 0)
   {
      if ( DM_ENG_Parameter_isDiagnosticsState(param->name)
        && (param->value != NULL) && (strcmp(param->value, DM_ENG_NONE_STATE) != 0) && (strcmp(param->value, DM_ENG_REQUESTED_STATE) != 0))
      {
         DM_ENG_InformMessageScheduler_diagnosticsComplete();
      }

      if (((param->notification == DM_ENG_NotificationMode_ACTIVE) && DM_ENG_Parameter_isValuable(param->name))
        || (strcmp(param->name, DM_ENG_DEVICE_STATUS_PARAMETER_NAME)==0))
   	{
   		DM_ENG_InformMessageScheduler_parameterValueChanged(param);
   	}
   }
   _dataChanged = true;
}

/**
 * Cancels the change (returns to the previous value)
 */
bool DM_ENG_Parameter_cancelChange(DM_ENG_Parameter* param)
{
   bool changeWasRequested = ((param->state & _CHANGE_REQUESTED) != 0);
   if (changeWasRequested)
   {
      _setNewValue(param, param->backValue);
      param->backValue = NULL;
      param->state = _INITIAL;
      _dataChanged = true;
   }
   return changeWasRequested;
}

/**
 * Sets the being evalued state and commit.
 */
void DM_ENG_Parameter_markBeingEvaluated(DM_ENG_Parameter* param)
{
   param->state |= _BEING_EVALUATED;
   _dataChanged = true;
}

/**
 * Unsets the being evalued state and commit.
 */
void DM_ENG_Parameter_unmarkBeingEvaluated(DM_ENG_Parameter* param)
{
   param->state &= ~_BEING_EVALUATED;
   _dataChanged = true;
}

/**
 * Tests if being evalued.
 */
bool DM_ENG_Parameter_isBeingEvaluated(DM_ENG_Parameter* param)
{
   return ((param->state & _BEING_EVALUATED) != 0);
}

/**
 * Sets the pushed state and commit.
 */
void DM_ENG_Parameter_markPushed(DM_ENG_Parameter* param)
{
   param->state |= _PUSHED;
   _dataChanged = true;
}

/**
 * Unsets the pushed state and commit.
 */
void DM_ENG_Parameter_unmarkPushed(DM_ENG_Parameter* param)
{
   if ((param->state & _PUSHED) != 0)
   {
      param->state &= ~_PUSHED;
      _dataChanged = true;
   }
}

/**
 * Tests if pushed.
 */
bool DM_ENG_Parameter_isPushed(DM_ENG_Parameter* param)
{
   return ((param->state & _PUSHED) != 0);
}
