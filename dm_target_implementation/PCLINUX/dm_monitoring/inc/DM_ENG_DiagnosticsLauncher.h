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
 * File        : DM_ENG_DiagnosticsLauncher.h
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
 * @file DM_ENG_DiagnosticsLauncher.h
 *
 * @brief 
 *
 **/

#ifndef _DM_ENG_DIAGNOSTICS_LAUNCHER_H_
#define _DM_ENG_DIAGNOSTICS_LAUNCHER_H_

extern char* DM_ENG_NONE_STATE;
extern char* DM_ENG_REQUESTED_STATE;
extern char* DM_ENG_COMPLETE_STATE;
extern char* DM_ENG_ERROR_STATE;
extern char* DM_ENG_ERROR_CANNOT_RESOLVE_STATE;
extern char* DM_ENG_ERROR_MAX_HOP_STATE;
extern char* DM_ENG_ERROR_INTERNAL_STATE;
extern char* DM_ENG_ERROR_OTHER_STATE;

extern char* DM_ENG_DIAGNOSTICS_OBJECT_SUFFIX;
extern char* DM_ENG_DIAGNOSTICS_STATE_SUFFIX;

void DM_ENG_Diagnostics_initDiagnostics(const char* name);

#endif
