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
 * File        : DM_ENG_Common.c
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
 * @file DM_ENG_Common.c
 *
 * @brief Common definitions and all-purpose functions
 *
 **/

#include "DM_ENG_Common.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

static const int _MAX_FAULT_MESSAGES = 20; // Index du dernier élément dans le tableau _FAULT_MESSAGES (Vendor specific fault)

static const char* _FAULT_MESSAGES[] =
{
   "Method not supported",                             // 9000
   "Request denied (no reason specified)",             // 9001
   "Internal error",                                   // 9002
   "Invalid arguments",                                // 9003
   "Resources exceeded",                               // 9004
   "Invalid parameter name",                           // 9005
   "Invalid parameter type",                           // 9006
   "Invalid parameter value",                          // 9007
   "Attempt to set a non-writable parameter",          // 9008
   "Notification request rejected",                    // 9009
   "Download failure",                                 // 9010
   "Upload failure",                                   // 9011
   "File transfer server authentication failure",      // 9012
   "Unsupported protocol for file transfer",           // 9013
   "Download failure: unable to join multicast group", // 9014
   "Download failure: unable to contact file server",  // 9015
   "Download failure: unable to access file",          // 9016
   "Download failure: unable to complete download",    // 9017
   "Download failure: file corrupted",                 // 9018
   "Download failure: file authentication failure",    // 9019
   "Vendor specific fault"                             // 9800 - 9899
};

const char  DM_ENG_FALSE_CHAR   = '0';
const char  DM_ENG_TRUE_CHAR    = '1';
const char* DM_ENG_FALSE_STRING = "false";
const char* DM_ENG_TRUE_STRING  = "true";

const char* DM_ENG_EMPTY = "";
const char* DM_ENG_UNKNOWN_TIME = "0001-01-01T00:00:00Z";

#ifndef UINT_MAX_DECIMAL_DIGITS
#define UINT_MAX_DECIMAL_DIGITS 20
#endif

/**
 * @param ch Un caractère représentant un chiffre
 * @return L'entier compris entre 0 et 9 ou -1 si le caractère n'est pas un chiffre
 */
int DM_ENG_charToInt(char ch)
{
    return ((ch>='0') && (ch<='9') ? ch-'0' : -1);
}

/**
 * @param ch Un chaîne représentant un entier (limité aux int)
 * @param pRes Pointeur sur un int devant accueillir le résultat
 * @return Booléen indiquant si la chaîne complète est dans le format attendu.
 */
bool DM_ENG_stringToInt(char* ch, int* pRes)
{
   if ((ch == NULL) || (strlen(ch) == 0))  { return false; }
   char* end = NULL;
   errno = 0;
   long val = strtol(ch, &end, 10);
   if (errno == ERANGE) { return false; }
   while ((end != NULL) && (isspace(*end))) { end++; }
   if ( ((end != NULL) && (*end != '\0')) || (val<INT_MIN) || (val>INT_MAX) ) { return false; }
   if (pRes!=NULL) { *pRes = (int)val; }
   return true;
}

/**
 * @param ch Un chaîne représentant un entier (limité aux unsigned int)
 * @param pRes Pointeur sur un unsigned int devant accueillir le résultat
 * @return Booléen indiquant si la chaîne complète est dans le format attendu.
 */
bool DM_ENG_stringToUint(char* ch, unsigned int* pRes)
{
   if ((ch == NULL) || (strlen(ch) == 0))  { return false; }
   while (isspace(*ch)) { ch++; }
   if (!isdigit(*ch)) { return false; }
   char* end = NULL;
   errno = 0;
   unsigned long val = strtoul(ch, &end, 10);
   if (errno == ERANGE) { return false; }
   while ((end != NULL) && (isspace(*end))) { end++; }
   if ( ((end != NULL) && (*end != '\0')) || (val>UINT_MAX)) { return false; }
   if (pRes!=NULL) { *pRes = (unsigned int)val; }
   return true;
}

/**
 * @param ch Un chaîne représentant un entier (limité aux long)
 * @param pRes Pointeur sur un long devant accueillir le résultat
 * @return Booléen indiquant si la chaîne est dans le format attendu.
 */
bool DM_ENG_stringToLong(char* ch, long* pRes)
{
   if ((ch == NULL) || (strlen(ch) == 0))  { return false; }
   char* end = NULL;
   errno = 0;
   long val = strtol(ch, &end, 10);
   if (errno == ERANGE) { return false; }
   while ((end != NULL) && (isspace(*end))) { end++; }
   if ((end != NULL) && (*end != '\0')) { return false; }
   if (pRes!=NULL) { *pRes = (long)val; }
   return true;
}

/**
 * @param ch Un chaîne représentant un boolean : "0", "1", "false" ou "true"
 * @param pRes Pointeur sur un bool devant accueillir le résultat
 * @return Booléen indiquant si la chaîne est dans le format attendu.
 */
bool DM_ENG_stringToBool(char* ch, bool* pRes)
{
   if ((ch == NULL) || (strlen(ch) == 0))  { return false; }
   bool val = false;
   while (isspace(*ch)) { ch++; }
   if (strncasecmp(ch, DM_ENG_FALSE_STRING, strlen(DM_ENG_FALSE_STRING)) == 0) { /*val = false;*/ ch += strlen(DM_ENG_FALSE_STRING); }
   else if (strncasecmp(ch, DM_ENG_TRUE_STRING, strlen(DM_ENG_TRUE_STRING)) == 0) { val = true; ch += strlen(DM_ENG_TRUE_STRING); }
   else if (*ch == DM_ENG_FALSE_CHAR) { /*val = false;*/ ch++; }
   else if (*ch == DM_ENG_TRUE_CHAR) { val = true; ch++; }
   else { return false; }
   while (isspace(*ch)) { ch++; }
   if (pRes!=NULL) { *pRes = val; }
   return (*ch == '\0');
}

/**
 * @param ch Un chaîne représentant une date et une heure
 * @return La valeur entière (long) correspondant à cette date-heure ou -1 si la chaîne n'a pas le format attendu.
 * La chaîne "0001-01-01T00:00:00.000Z" est reconnue comme "Unknown Time" et alors la valeur retournée est 0
 */
bool DM_ENG_dateStringToTime(char* ch, time_t* pT)
{
   if ((ch == NULL) || (strlen(ch) == 0)) { return false; }
   time_t res = -1;

   struct tm ts;
   memset(&ts, 0, sizeof(struct tm));
   char* left = NULL;
   char* right = strdup("000000000");
   char* frac = strchr(ch, '.');
   if (frac != 0)
   {
      left = DM_ENG_strndup(ch, frac-ch);
      frac++;
      int i;
      for (i=0; (i<9) && (*frac >= '0') && (*frac <= '9'); i++)
      {
         right[i] = *frac;
         frac++;
      }
   }
   if ( sscanf( (left == NULL ? ch : left), "%d-%d-%dT%d:%d:%d", &ts.tm_year, &ts.tm_mon, &ts.tm_mday, &ts.tm_hour, &ts.tm_min, &ts.tm_sec ) == 6)
   {
      if ((ts.tm_year==1) && (ts.tm_mon==1) && (ts.tm_mday==1) && (ts.tm_hour==0) && (ts.tm_min==0) && (ts.tm_sec==0)) // Unknown Time
      {
         res = 0;
      }
      else
      {
         ts.tm_year = ts.tm_year-1900;
         ts.tm_mon = ts.tm_mon-1;

         res  = mktime( &ts );
         if (res != -1)
         {
            // truc pour annuler le décalage horaire introduit par mktime qui suppose ts en localtime
            time_t t0 = 86400; // valeur arbitraire (Epoch + 1 jour)
            res += t0 - mktime( gmtime(&t0) ); // delta localtime
         }
      }
      //long nsec = strtol(right, NULL, 10);
   }
   DM_ENG_FREE(left);
   DM_ENG_FREE(right);
   if (res == -1) { return false; }
   if (pT != NULL) { *pT = res; }
   return true;
}

/**
 * @param i Un int
 * @return La chaîne représentant l'entier i
 */
char* DM_ENG_intToString(int i)
{
   int lg = (i<0 ? 2 : 1);
   int k = i;
   while ((k/=10) != 0) { lg++; }
   char* res = (char*)malloc(lg+1);
   sprintf(res, "%d", i);
   return res;
}

/**
 * @param i Un unsigned int
 * @return La chaine representant l'entier i non signe
 */
char* DM_ENG_uintToString(unsigned int i)
{
   char buf[UINT_MAX_DECIMAL_DIGITS+1];
   char *p = buf+sizeof(buf);

   *--p = '\0';
   do
   {
      *--p = '0' + (i % 10);
   } while ((i /= 10) != 0);
   
   return( strdup(p) );
}

/**
 * @param l Un long
 * @return La chaîne représentant l'entier l
 */
char* DM_ENG_longToString(long l)
{
   int lg = (l<0 ? 2 : 1);
   long k = l;
   while ((k/=10) != 0) { lg++; }
   char* res = (char*)malloc(lg+1);
   sprintf(res, "%ld", l);
   return res;
}

static const int _MAX_DATE_SIZE = 25;

/**
 * @param t Date time
 * @return String representing the date time in UTC. Format "0000-00-00T00:00:00Z"
 */
char* DM_ENG_dateTimeToString(time_t t)
{
   char res[_MAX_DATE_SIZE+1];
   struct tm  *pT = gmtime( &t );

   if (t <= 0) { return strdup(DM_ENG_UNKNOWN_TIME); }

   sprintf( res, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2dZ",
      1900+pT->tm_year,
      1+pT->tm_mon,
      pT->tm_mday,
      pT->tm_hour,
      pT->tm_min,
      pT->tm_sec );
   return (strdup(res));
}

/**
 * @param list Tableau de pointeurs terminé par null
 * @return La longueur du tableau
 */
int DM_ENG_tablen(void* list[])
{
   int len = 0;
   if (list != NULL)
   {
      while (list[len]!=NULL) len++;
   }
   return len;
}

/**
 * Copies an array of strings
 * 
 * @param tStr An array pointer
 * 
 * @return An array pointer
 */
char** DM_ENG_copyTabString(char* tCh[])
{
   int size = 0;
   if (tCh == NULL) return NULL;
   while (tCh[size] != NULL) { size++; }
   char** newTab = (char**)calloc(size+1, sizeof(char*));
   int i=0;
   while (tCh[i] != NULL) { newTab[i] = strdup(tCh[i]); i++; }
   return newTab; 
}

/**
 * Releases the space allocated for an array of strings
 * 
 * @param tCh An array pointer
 */
void DM_ENG_deleteTabString(char* tCh[])
{
   int i=0;
   while (tCh[i] != NULL) { free(tCh[i++]); }
   free(tCh);
}

/**
 * Splits a comma-separated list string to an array of strings.
 * N.B. The spaces at the beginning or at the end of items are removed.
 * 
 * @param ch A comma-separated list
 * 
 * @return An array of strings
 */
char** DM_ENG_splitCommaSeparatedList(char* ch)
{
   int size = 1;
   if (ch == NULL) return NULL;
   char* iCh;
   for (iCh = ch; *iCh!='\0'; iCh++)
   {
      if (*iCh == ',') size++;
   }
   char** newTab = (char**)calloc(size+1, sizeof(char*));

   iCh = ch;
   int i;
   for (i=0; i<size; i++)
   {
      while (*iCh==' ') iCh++; // on passe les espaces
      char* deb = iCh; // 1er car non espace
      char* fin = iCh; // fin de string
      while ((*iCh!='\0') && (*iCh!=',')) // on cherche le délimiteur
      {
         if (*iCh!=' ') fin = iCh+1; // après le dernier caractère autre que espace
         iCh++;
      }
      newTab[i] = (char*)calloc(fin-deb+1, sizeof(char));
      strncpy(newTab[i], deb, fin-deb);
      iCh++;
   }
   return newTab;
}

/**
 * Builds a comma-separated list string from a given array of strings
 * 
 * @param tCh An array of strings
 * 
 * @return A comma-separated list string
 */
char* DM_ENG_buildCommaSeparatedList(char* tCh[])
{
   if ((tCh == NULL) || (tCh[0] == NULL)) return NULL;
   int size=0;
   int i;
   for (i=0; tCh[i]!=NULL; i++)
   {
      size += strlen(tCh[i])+1;
   }

   char* list = (char*)calloc(size, sizeof(char));

   char* p = list;
   for (i=0; tCh[i]!= NULL; i++)
   {
      if (i>0) { *p=','; p++; }
      sprintf(p, "%s", tCh[i]);
      p += strlen(tCh[i]);
   }
   return list;
}

bool DM_ENG_belongsToCommaSeparatedList(char* csl, char* ch)
{
   bool res = false;
   if ((csl != NULL) && (ch != NULL))
   {
      char** list = DM_ENG_splitCommaSeparatedList(csl); // N.B. list != NULL car csl != NULL
      int i;
      for (i=0; !res && (list[i]!=NULL); i++)
      {
         if (strcmp(list[i], ch) == 0) { res = true; }
      }
      DM_ENG_deleteTabString(list);
   }
   return res;
}

void DM_ENG_addToCommaSeparatedList(char** pCsl, char* ch)
{
   if (!DM_ENG_belongsToCommaSeparatedList(*pCsl, ch))
   {
      char* oldCsl = (*pCsl == NULL ? (char*)DM_ENG_EMPTY : *pCsl);
      char* newCh = (ch == NULL ? (char*)DM_ENG_EMPTY : ch);
      *pCsl = (char*)malloc(strlen(oldCsl)+strlen(newCh)+2);
      strcpy(*pCsl, oldCsl);
      strcat(*pCsl, ",");
      strcat(*pCsl, newCh);
      if (oldCsl != DM_ENG_EMPTY) { free(oldCsl); }
   }
}

// Supprime 1 élément et rend true ssi il a été trouvé
bool DM_ENG_deleteFromCommaSeparatedList(char** pCsl, char* ch)
{
   bool trouve = false;
   if ((*pCsl != NULL) && (ch != NULL))
   {
      char** list = DM_ENG_splitCommaSeparatedList(*pCsl); // N.B. list != NULL car csl != NULL
      int i;
      for (i=0; (list[i]!=NULL); i++)
      {
         if (trouve)
         {
            list[i-1] = list[i];
            list[i] = NULL;
         }
         else if (strcmp(list[i], ch) == 0)
         {
            trouve = true;
            free(list[i]);
            list[i] = NULL;
         }
      }
      if (trouve)
      {
         if (*pCsl != DM_ENG_EMPTY) { free(*pCsl); }
         *pCsl = DM_ENG_buildCommaSeparatedList(list);
      }
      DM_ENG_deleteTabString(list);
   }
   return trouve;
}

/**
 * @param code Fault code
 * @return The corresponding fault string (type const char* => do not free the result)
 */
const char* DM_ENG_getFaultString(int code)
{
   const char* res = NULL;
   int i = code - 9000;
   if ((i>=0) && (i<_MAX_FAULT_MESSAGES))
   {
      res = _FAULT_MESSAGES[i];
   }
   else if ((i>=800) && (i<=899))
   {
      res = _FAULT_MESSAGES[_MAX_FAULT_MESSAGES];
   }
   return res;
}

static char* _SPECIAL_CHARS = "%\\;\"<";

/**
 * Encode a string value if needed to avoid problem in reading/writing in csv files, config files and in XML format
 *
 * @param str String to encode
 * @return Encoded string IF str needs to be encoded (it contains special chars or chars whose ascii code is less than 32)
 *                        ELSE NULL 
 */
char* DM_ENG_encodeValue(char* str)
{
   if (str == NULL) return NULL;

   int nbSpe = 0;
   int i;
   for (i=0; str[i] != '\0'; i++)
   {
      if ((str[i]<' ') || (strchr(_SPECIAL_CHARS, str[i]) != 0)) nbSpe++;
   }
   if (nbSpe == 0) return NULL;

   char* newStr = (char*)calloc(strlen(str)+2*nbSpe+1, sizeof(char));
   int j=0;
   for (i=0; str[i] != '\0'; i++)
   {
      if ((str[i]<' ') || (strchr(_SPECIAL_CHARS, str[i]) != 0))
      {
         sprintf(newStr+j, "%%%.2X", str[i]);
         j += 3;
      }
      else
      {
         newStr[j++] = str[i];
      }
   }
   return newStr;
}

/**
 * Decode a string value if it contains coded chars (%XX)
 *
 * @param str String to decode
 * @return Decoded string IF str needs to be decoded ELSE NULL 
 */
char* DM_ENG_decodeValue(char* str)
{
   if (str == NULL) return NULL;

   int nbSpe = 0;
   int i;
   for (i=0; str[i] != '\0'; i++)
   {
      if (str[i]=='%') nbSpe++;
   }
   if (nbSpe == 0) return NULL;

   char* newStr = (char*)calloc(strlen(str)-2*nbSpe+1, sizeof(char));
   int j=0;
   int echap = 0;
   for (i=0; str[i] != '\0'; i++)
   {
      if (str[i]=='%')
      {
         echap = 1;
      }
      else if (echap == 1)
      {
         unsigned int v = 0;
         sscanf(str+i, "%2X", &v);
         newStr[j++] = v;
         echap = 2;
      }
      else if (echap == 2)
      {
         echap = 0;
      }
      else
      {
         newStr[j++] = str[i];
      }
   }
   return newStr;
}

/**
 * @brief Private strndup implementation
 *
 * @param IN A pointer on the string to duplicate
 *        IN The maximum number of char to duplicate (note: n est set to '\0')
 *
 * @Return A pointer on the duplicated string
 *
 * Note: The returned pointer must be free
 */
char* DM_ENG_strndup(const char* src, size_t size)
{
   size_t len = strlen(src);
   if (len < size) { size = len; }

   char* dest = (char*)malloc(size+1);

   memcpy(dest, src, size);
   dest[size] = '\0';

   return dest;
}
