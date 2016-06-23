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
 * File        : DM_ENG_InformMessageScheduler.h
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
 * @file DM_ENG_InformMessageScheduler.h
 *
 * @brief 
 *
 **/
 
 
#ifndef _DM_ENG_INFORM_MESSAGE_SCHEDULER_H_
#define _DM_ENG_INFORM_MESSAGE_SCHEDULER_H_

#include "DM_ENG_Common.h"
#include "DM_ENG_Global.h"
#include "DM_ENG_ParameterManager.h"

void DM_ENG_InformMessageScheduler_init(DM_ENG_InitLevel level);
void DM_ENG_InformMessageScheduler_exit();
void DM_ENG_InformMessageScheduler_bootstrapInform();
int DM_ENG_InformMessageScheduler_scheduleInform(unsigned int delay, char* commandKey);
void DM_ENG_InformMessageScheduler_requestConnection();
void DM_ENG_InformMessageScheduler_sessionClosed(bool success);
void DM_ENG_InformMessageScheduler_parameterValueChanged(DM_ENG_Parameter* param);
void DM_ENG_InformMessageScheduler_changeOnPeriodicInform();
void DM_ENG_InformMessageScheduler_diagnosticsComplete();
void DM_ENG_InformMessageScheduler_transferComplete(bool autonomous);
void DM_ENG_InformMessageScheduler_requestDownload();
void DM_ENG_InformMessageScheduler_vendorSpecificEvent(const char* eventCode);
bool DM_ENG_InformMessageScheduler_isSessionOpened(bool openingOK);
bool DM_ENG_InformMessageScheduler_isReadyToClose();
void DM_ENG_InformMessageScheduler_join();

#endif
