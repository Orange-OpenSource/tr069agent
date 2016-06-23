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
 * File        : DM_ENG_Parameter.h
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
 * @file DM_ENG_Parameter.h
 *
 * @brief Object representing one parameter with all relevant data
 *
 **/

#ifndef _DM_ENG_PARAMETER_H_
#define _DM_ENG_PARAMETER_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_ParameterType.h"
#include "DM_ENG_NotificationMode.h"

typedef enum _DM_ENG_StorageMode
{
   DM_ENG_StorageMode_DM_ONLY = 0,
   DM_ENG_StorageMode_SYSTEM_ONLY,
   DM_ENG_StorageMode_MIXED,
   DM_ENG_StorageMode_COMPUTED

} DM_ENG_StorageMode;

typedef struct _DM_ENG_Parameter
{
	char* name;
	DM_ENG_ParameterType type;
   int minValue;
   int maxValue;
	DM_ENG_StorageMode storageMode;
	bool writable;
	int immediateChanges;
	char** accessList;
	DM_ENG_NotificationMode notification;
	bool mandatoryNotification;
	bool activeNotificationDenied;
   int loadingMode; // bit 0 (loadingMode & 1) : 0 => DM ONLY ou donnée indépendante, 1 => données groupées (et SYSTEM ou MIXED)
                    // bit 1 (loadingMode & 2) : 1 <=> sous-arborescence évolutive (s'applique aux noeuds, utile à la racine de données groupées et aux objets STATISTICS)
   int state;
	char* backValue;
   char* value;
   char* configKey;
   char* definition;
   struct _DM_ENG_Parameter* next;

} __attribute((packed)) DM_ENG_Parameter;

DM_ENG_Parameter* DM_ENG_newParameter(char* attr[], bool ini);
DM_ENG_Parameter* DM_ENG_newInstanceParameter(char* name, DM_ENG_Parameter* param);
DM_ENG_Parameter* DM_ENG_newDefinedParameter(char* name, DM_ENG_ParameterType type, char* def, bool hidden);
void DM_ENG_deleteParameter(DM_ENG_Parameter* param);
bool DM_ENG_Parameter_isNode(char* paramName);
bool DM_ENG_Parameter_isNodeInstance(char* paramName);
bool DM_ENG_Parameter_isValuable(char* paramName);
char* DM_ENG_Parameter_getLongName(char* prmName, const char* destName);
bool DM_ENG_Parameter_isDiagnosticsParameter(const char* paramName, char** pObjName);
bool DM_ENG_Parameter_isDiagnosticsState(const char* paramName);

bool DM_ENG_Parameter_isHidden(DM_ENG_Parameter* param);
//void DM_ENG_Parameter_setHidden(DM_ENG_Parameter* param);

void DM_ENG_Parameter_addConfigKey(DM_ENG_Parameter* param, char* configKey);
bool DM_ENG_Parameter_removeConfigKey(DM_ENG_Parameter* param, char* configKey);

int DM_ENG_Parameter_setAttributes(DM_ENG_Parameter* param, int notification, char* accessList[]);
int DM_ENG_Parameter_checkBeforeSetValue(DM_ENG_Parameter* param, char* value);
int DM_ENG_Parameter_setValue(DM_ENG_Parameter* param, char* value);
int DM_ENG_Parameter_getValue(DM_ENG_Parameter* param, OUT char** pValue);
char* DM_ENG_Parameter_getFilteredValue(DM_ENG_Parameter* param);

void DM_ENG_Parameter_initState(DM_ENG_Parameter* param);
bool DM_ENG_Parameter_isValueChanged(DM_ENG_Parameter* param);
void DM_ENG_Parameter_commit(DM_ENG_Parameter* param);
bool DM_ENG_Parameter_cancelChange(DM_ENG_Parameter* param);

void DM_ENG_Parameter_updateValue(DM_ENG_Parameter* param, char* value, bool newValue);
void DM_ENG_Parameter_resetValue(DM_ENG_Parameter* param);
void DM_ENG_Parameter_markLoaded(DM_ENG_Parameter* param, bool withValues);
void DM_ENG_Parameter_clearTemporaryValue(DM_ENG_Parameter* param, int level);
bool DM_ENG_Parameter_isLoaded(DM_ENG_Parameter* param, bool withValues);

void DM_ENG_deleteAllParameter(DM_ENG_Parameter** pParam);
void DM_ENG_addParameter(DM_ENG_Parameter** pParam, DM_ENG_Parameter* newParam);

bool DM_ENG_Parameter_isDataChanged();
void DM_ENG_Parameter_setDataChanged(bool changed);

void DM_ENG_Parameter_markBeingEvaluated(DM_ENG_Parameter* param);
void DM_ENG_Parameter_unmarkBeingEvaluated(DM_ENG_Parameter* param);
bool DM_ENG_Parameter_isBeingEvaluated(DM_ENG_Parameter* param);

void DM_ENG_Parameter_markPushed(DM_ENG_Parameter* param);
void DM_ENG_Parameter_unmarkPushed(DM_ENG_Parameter* param);
bool DM_ENG_Parameter_isPushed(DM_ENG_Parameter* param);

#endif
