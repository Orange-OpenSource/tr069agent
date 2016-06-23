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
 * File        : DM_ENG_ParameterStatus.h
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
 * @file DM_ENG_ParameterStatus.h
 *
 * @brief The parameter status returned in the SetParameterValues AddObject, DeleteObject and Download responses.
 * 
 * The enumerated values are defined as follows :
 * 0 (APPLIED) : the operation has completed and been applied.
 * 1 (READY TO APPLY) : the operation has been validated, but not yet been completed and applied (for example,
 * if a reboot is required before the object can be deleted).
 *
 **/

#ifndef _DM_ENG_PARAMETER_STATUS_H_
#define _DM_ENG_PARAMETER_STATUS_H_

/**
 * Parameter Status integer enumeration
 */
typedef enum DM_ENG_ParameterStatus
{
   DM_ENG_ParameterStatus_UNDEFINED = -1,
   DM_ENG_ParameterStatus_APPLIED = 0,
   DM_ENG_ParameterStatus_READY_TO_APPLY = 1

} DM_ENG_ParameterStatus;

#endif
