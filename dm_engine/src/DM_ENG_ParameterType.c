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
 * File        : DM_ENG_ParameterType.c
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
 * @file DM_ENG_ParameterType.c
 *
 * @brief Parameter types
 *
 **/

#include "DM_ENG_ParameterType.h"
#include "DM_ENG_Common.h"
#include <stdlib.h>
#include <stdio.h>

static const char* _INT     = "INT";
static const char* _UINT    = "UINT";
static const char* _LONG    = "LONG";
static const char* _B0OLEAN = "BOOLEAN";
static const char* _DATE    = "DATE";
static const char* _STRING  = "STRING";
static const char* _STATISTICS = "STATISTICS";
static const char* _ANY     = "ANY";

DM_ENG_ParameterType DM_ENG_stringToType(char* str, int* pMin, int* pMax)
{
   DM_ENG_ParameterType type = DM_ENG_ParameterType_UNDEFINED;

   // codage du type : <type>:<min>:<max> ou <type>:<min> ou <type>
   char* min = NULL;
   char* max = NULL;
   int i = 0;
   while (str[i] != '\0')
   {
      if (str[i] == ':')
      {
         str[i] = '\0';
         if (min == NULL) { min = str+i+1; }
         else if (max == NULL) { max = str+i+1; }
      }
      i++;
   }
   // 2 codages possibles du type : codage chiffré ou littéral
   if (strlen(str) == 1)
   {
      type = (DM_ENG_ParameterType)DM_ENG_charToInt(*str);
      if ((int)type == -1) { type = DM_ENG_ParameterType_UNDEFINED; }
   }
   else
   {
      type = ( (strcasecmp(str, _INT) == 0) ? DM_ENG_ParameterType_INT
             : (strcasecmp(str, _UINT) == 0) ? DM_ENG_ParameterType_UINT
             : (strcasecmp(str, _LONG) == 0) ? DM_ENG_ParameterType_LONG
             : (strcasecmp(str, _B0OLEAN) == 0) ? DM_ENG_ParameterType_BOOLEAN
             : (strcasecmp(str, _DATE) == 0) ? DM_ENG_ParameterType_DATE
             : (strcasecmp(str, _STRING) == 0) ? DM_ENG_ParameterType_STRING
             : (strcasecmp(str, _STATISTICS) == 0) ? DM_ENG_ParameterType_STATISTICS
             : (strcasecmp(str, _ANY) == 0) ? DM_ENG_ParameterType_ANY
             : DM_ENG_ParameterType_UNDEFINED );
   }

   if (type == DM_ENG_ParameterType_INT)
   {
      if ((min == NULL) || !DM_ENG_stringToInt(min, pMin))
      {
         *pMin = INT_MIN;
      }
      if ((max == NULL) || !DM_ENG_stringToInt(max, pMax))
      {
         *pMax = INT_MAX;
      }
   }
   else if ((type == DM_ENG_ParameterType_UINT)
         || (type == DM_ENG_ParameterType_ANY) // pour les objets, la partie <min> donne le dernier numéro d'instance
         || (type == DM_ENG_ParameterType_STATISTICS)) // pour les objets stats, la partie <min> est à 1 si un mécanisme de polling est nécessaire
   {
      if ((min == NULL) || !DM_ENG_stringToUint(min, (unsigned int*)pMin))
      {
         *pMin = 0;
      }
      if ((max == NULL) || !DM_ENG_stringToUint(max, (unsigned int*)pMax))
      {
         *pMax = (int)UINT_MAX;
      }
   }
   else if (type == DM_ENG_ParameterType_STRING) // N.B. la partie <min> spécifie la taille max des chaînes
   {
      if ((min == NULL) || !DM_ENG_stringToInt(min, pMin))
      {
         *pMin = -1; // signifie : pas de taille max spécifiée
      }
   }

   return type;
}

#define _MAX_STR_TYPE_LEN 30

char* DM_ENG_typeToString(DM_ENG_ParameterType type, int min, int max)
{
   char* res = (char*)malloc(_MAX_STR_TYPE_LEN);
   sprintf(res, "%d", type);
   if (type == DM_ENG_ParameterType_INT)
   {
      if ((min > INT_MIN) || (max < INT_MAX))
      {
         if (max >= INT_MAX) { sprintf(res+strlen(res), ":%d", min); }
         else if (min <= INT_MIN) { sprintf(res+strlen(res), "::%d", max); }
         else { sprintf(res+strlen(res), ":%d:%d", min, max); }
      }
   }
   else if ((type == DM_ENG_ParameterType_UINT) || (type == DM_ENG_ParameterType_ANY) || (type == DM_ENG_ParameterType_STATISTICS))
   {
      unsigned int uMin = (unsigned int)min;
      unsigned int uMax = (unsigned int)max;
      if ((uMin > 0) || (uMax < UINT_MAX))
      {
         if (uMax >= UINT_MAX) { sprintf(res+strlen(res), ":%u", uMin); }
         else if (uMin == 0) { sprintf(res+strlen(res), "::%u", uMax); }
         else { sprintf(res+strlen(res), ":%u:%u", uMin, uMax); }
      }
   }
   else if (type == DM_ENG_ParameterType_STRING)
   {
      if (min > -1) { sprintf(res+strlen(res), ":%d", min); }
   }
   return res;
}
