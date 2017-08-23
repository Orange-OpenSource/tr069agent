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
 * File        : dm_upnpigd.c
 *
 * Created     : 30/05/2017
 * Author      : Ying ZENG
 *
 *---------------------------------------------------------------------------
 * $Id$
 *
 *---------------------------------------------------------------------------
 * $Log$
 *
 */


#ifndef DM_IGD_H

#define DM_IGD_H


/**
 * @brief the DM_IGD thread
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
void* DM_IGD_upnpigdThread();
int DM_IGD_buildConnectionRequestUrl(char** pResult);

#endif
