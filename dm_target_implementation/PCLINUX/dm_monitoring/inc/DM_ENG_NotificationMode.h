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
 * File        : DM_ENG_NotificationMode.h
 *
 * Created     : 25/05/2009
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
 * @file DM_ENG_NotificationMode.h
 *
 * @brief Notification modes
 *
 */

#ifndef _DM_ENG_NOTIFICATION_MODE_H_
#define _DM_ENG_NOTIFICATION_MODE_H_

/**
 * Notification Mode of the parameter value changes
 */
typedef enum _DM_ENG_NotificationMode
{
   DM_ENG_NotificationMode_OFF = 0,
   DM_ENG_NotificationMode_PASSIVE,
   DM_ENG_NotificationMode_ACTIVE,
   DM_ENG_NotificationMode_UNDEFINED

} DM_ENG_NotificationMode;

#endif

