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
 * File        : DM_ENG_StatisticsModule.h
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
 * @file DM_ENG_StatisticsModule.h
 *
 * @brief This module processes the raw collected sample data and computes the derived parameters
 *
 */

#ifndef _DM_ENG_STATISTICS_MODULE_H_
#define _DM_ENG_STATISTICS_MODULE_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_SampleDataStruct.h"
#include "DM_ENG_ComputedExp.h"

typedef bool (*DM_ENG_F_CREATE_PARAMETER)(char* newName, char* destName, DM_ENG_ParameterType type, char* args, char* configKey);

void DM_ENG_StatisticsModule_init(DM_ENG_F_CREATE_PARAMETER createParam, DM_ENG_F_GET_VALUE getValue);
void DM_ENG_StatisticsModule_exit();
void DM_ENG_StatisticsModule_join();
void DM_ENG_StatisticsModule_restart();

void DM_ENG_StatisticsModule_performParameterSetting(const char* objName, const char* paramName);

void DM_ENG_StatisticsModule_processSampleData(DM_ENG_SampleDataStruct* sd);

#endif /*_DM_ENG_STATISTICS_MODULE_H_*/
