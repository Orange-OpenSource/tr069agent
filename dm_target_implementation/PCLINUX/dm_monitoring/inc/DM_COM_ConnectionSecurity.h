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
 * File        : DM_COM_ConnectionSecurity.c
 *
 * Created     : 29/12/2008
 * Author      : 
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */

#ifndef _DM_COM_CONNECTION_SECURITY_H_
#define _DM_COM_CONNECTION_SECURITY_H_

#define MAXCONNECTIONREQUESTARRAYSIZE (100)

void initConnectionFreqArray(time_t*);
void newConnectionRequestAllowed(time_t*);
bool maxConnectionPerPeriodReached(time_t*);

#endif
