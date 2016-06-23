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
 * File        : DM_stun.h
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
#ifndef DM_STUN_H

#define DM_STUN_H


/**
 * @brief the DM_STUN thread
 *
 * @param none
 *
 * @return return DM_OK is okay else DM_ERR
 *
 */
void* DM_STUN_stunThread();

#endif
