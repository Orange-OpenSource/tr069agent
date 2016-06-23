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
 * File        : dm_com_utils.h
 *
 * Created     : 2008/06/05
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
 * @file dm_com_utils.h
 *
 * @brief dm_com utilities
 *
 *
 */
 
#ifndef _DM_COM_UTILS_H
#define _DM_COM_UTILS_H

#include "DM_ENG_Common.h"


 /**
 * @brief Private routine used to generate a random stream
 *
 * @param A pointer on the stream to generate (the memory must be 
 *        allocated by the calling routine)
          The size of the random stream to generate
 *
 * @Return TRUE if the stream is generated
 *         FALSE otherwise
 *
 */
bool _generateRandomString(char * strToGenerate, int strSizeToGenerate);


#endif /* _DM_COM_UTILS_H */
