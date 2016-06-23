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
 * File        : DM_ENG_ComputedExp.c
 *
 * Created     : 26/07/2010
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
 * @file DM_ENG_ComputedExp.c
 *
 * @brief Module dedicated to the parsing and the processing of the expressions of the COMPUTED parameters 
 *
 **/

#include "DM_ENG_ComputedExp.h"
#include <ctype.h>
#include <stdio.h>

DM_ENG_ComputedExp* DM_ENG_newComputedExp(DM_ENG_ComputedExp_Operator op, DM_ENG_ComputedExp* l, DM_ENG_ComputedExp* r);
DM_ENG_ComputedExp* DM_ENG_newLitComputedExp(DM_ENG_ComputedExp_Operator op, char* lit);
DM_ENG_ComputedExp* DM_ENG_newNumComputedExp(unsigned int i);
void _displayExp(DM_ENG_ComputedExp* exp);

static DM_ENG_ComputedExp* _parseList();
static DM_ENG_ComputedExp* _parseGuard();
static DM_ENG_ComputedExp* _parseCons();
static DM_ENG_ComputedExp* _parseConcat(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op);
static DM_ENG_ComputedExp* _parseOr(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op);
static DM_ENG_ComputedExp* _parseAnd(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op);
static DM_ENG_ComputedExp* _parseCond(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op);
static DM_ENG_ComputedExp* _parseSum(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op);
static DM_ENG_ComputedExp* _parseProd(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op);
static DM_ENG_ComputedExp* _parsePrim();

static const char* OP_CHAR = "!%*+-/:<=>?^|~"; // caractères pouvant être combinés pour former des opérateurs. Ex : "+=" 

static const char* _INTERNAL_SEPARATOR   = "!";

// Fct préféfinies. A compléter...

static const char* _CLASS_FCT = "Class";
static const char* _DIFF_TIME_FCT = "DiffTime";

bool DM_ENG_ComputedExp_isPredefinedFunction(char* name)
{
   return (strcmp(name, _CLASS_FCT) == 0);
}

static char* _sExp = NULL;
static char* _sToken = NULL;

static void _nextToken()
{
   if (_sToken != NULL) { free(_sToken); _sToken = NULL; }
}

static void _initTokens(char* exp)
{
   _nextToken();
   _sExp = exp;
}

static char* _getToken()
{
   if ((_sToken == NULL) && (_sExp != NULL) && (*_sExp != '\0'))
   {
      while (isspace(*_sExp)) { _sExp++; }
      const char* deb = _sExp;

      while (isalnum(*_sExp) || (*_sExp == '.')|| (*_sExp == '_')) { _sExp++; } // ident ou nombre

      if (_sExp == deb) { while ((*_sExp != '\0') && (strchr(OP_CHAR, *_sExp) != NULL)) { _sExp++; } } // opérateur

      if ((_sExp == deb) && ((*_sExp == '"') || (*_sExp == '\''))) // chaîne ou ident quoté
      {
         bool echap = false;
         char delim = *_sExp;
         while (true)
         {
            _sExp++;
            if ((*_sExp == '\0') || (!echap && (*_sExp == delim))) break;
            echap = !echap && (*_sExp == '\\');
         }
         if (*_sExp == delim) { _sExp++; }
      }

      if ((_sExp == deb) && (*_sExp != '\0')) { _sExp++; } // ponctuation (opérateur d'1 caractère)

      size_t len = _sExp - deb;
      if (len != 0)
      {
         _sToken = (char*)malloc(len+1);
         strncpy(_sToken, deb, len);
         _sToken[len] = '\0';
      }
   }
   return _sToken;
}

/*static char* _getNextToken()
{
   _nextToken();
   return _getToken();
}*/

DM_ENG_ComputedExp* DM_ENG_ComputedExp_parse(char* sExp)
{
   _initTokens(sExp);
   DM_ENG_ComputedExp* res = _parseList();
   if (_getToken() != NULL) // l'expression n'est pas (entièrement) reconnue -> échec de l'analyse
   {
      if (res != NULL)
      {
         DM_ENG_deleteComputedExp(res);
         res = NULL;
      }
   }
   return res;
}

static DM_ENG_ComputedExp* _parseList()
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* s = _parseGuard();
   if (s != NULL)
   {
      char* tok = _getToken();
      if ((tok != NULL) && (strcmp(tok, ",") == 0))
      {
         _nextToken();
         DM_ENG_ComputedExp* rExp = _parseList();
         if (rExp != NULL)
         {
            res = DM_ENG_newComputedExp(DM_ENG_ComputedExp_LIST, s, rExp); // associativité de dr à g !
         }
         else // c'est une erreur
         {
            DM_ENG_deleteComputedExp(s);
            s = NULL;
         }
      }
      else
      {
         res = s;
      }
   }
   return res;
}

static DM_ENG_ComputedExp* _parseGuard()
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* g = _parseCons();
   if (g != NULL)
   {
      char* tok = _getToken();
      if ((tok != NULL) && (strcmp(tok, "?") == 0))
      {
         _nextToken();
         DM_ENG_ComputedExp* rExp = _parseGuard();
         if (rExp != NULL)
         {
            res = DM_ENG_newComputedExp(DM_ENG_ComputedExp_GUARD, g, rExp); // associativité de dr à g !
         }
         else // c'est une erreur
         {
            DM_ENG_deleteComputedExp(g);
            g = NULL;
         }
      }
      else
      {
         res = g;
      }
   }
   return res;
}

static DM_ENG_ComputedExp* _parseCons()
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* concat = _parseConcat(NULL, 0);
   if (concat != NULL)
   {
      char* tok = _getToken();
      if ((tok != NULL) && (strcmp(tok, ":") == 0))
      {
         _nextToken();
         DM_ENG_ComputedExp* rExp = _parseCons();
         if (rExp != NULL)
         {
            res = DM_ENG_newComputedExp(DM_ENG_ComputedExp_CONS, concat, rExp); // associativité de dr à g !
         }
         else // c'est une erreur
         {
            DM_ENG_deleteComputedExp(concat);
            concat = NULL;
         }
      }
      else
      {
         res = concat;
      }
   }
   return res;
}

static DM_ENG_ComputedExp* _parseConcat(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op)
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* or_ = _parseOr(NULL, 0);
   if (or_ != NULL)
   {
      if (lExp != NULL)
      {
         or_ = DM_ENG_newComputedExp(op, lExp, or_); // associativité de g à dr
      }

      op = DM_ENG_ComputedExp_UNDEF;
      char* tok = _getToken();
      if (tok != NULL)
      {
         if (strcmp(tok, "++") == 0)
         {
            op = DM_ENG_ComputedExp_CONCAT;
            _nextToken();
            res = _parseConcat(or_, op); // à charge de cette fonction de détruire or_ en cas d'erreur
         }
      }
      if (op == DM_ENG_ComputedExp_UNDEF)
      {
         res = or_;
      }
   }
   else if (lExp != NULL) // c'est une erreur
   {
      DM_ENG_deleteComputedExp(lExp);
      lExp = NULL;
   }
   return res;
}

static DM_ENG_ComputedExp* _parseOr(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op)
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* and_ = _parseAnd(NULL, 0);
   if (and_ != NULL)
   {
      if (lExp != NULL)
      {
         and_ = DM_ENG_newComputedExp(op, lExp, and_); // associativité de g à dr
      }

      op = DM_ENG_ComputedExp_UNDEF;
      char* tok = _getToken();
      if (tok != NULL)
      {
         if (strcmp(tok, "|") == 0)
         {
            op = DM_ENG_ComputedExp_OR;
            _nextToken();
            res = _parseOr(and_, op); // à charge de cette fonction de détruire and_ en cas d'erreur
         }
      }
      if (op == DM_ENG_ComputedExp_UNDEF)
      {
         res = and_;
      }
   }
   else if (lExp != NULL) // c'est une erreur
   {
      DM_ENG_deleteComputedExp(lExp);
      lExp = NULL;
   }
   return res;
}

static DM_ENG_ComputedExp* _parseAnd(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op)
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* cond = _parseCond(NULL, 0);
   if (cond != NULL)
   {
      if (lExp != NULL)
      {
         cond = DM_ENG_newComputedExp(op, lExp, cond); // associativité de g à dr
      }

      op = DM_ENG_ComputedExp_UNDEF;
      char* tok = _getToken();
      if (tok != NULL)
      {
         if (strcmp(tok, "&") == 0)
         {
            op = DM_ENG_ComputedExp_AND;
            _nextToken();
            res = _parseAnd(cond, op); // à charge de cette fonction de détruire cond en cas d'erreur
         }
      }
      if (op == DM_ENG_ComputedExp_UNDEF)
      {
         res = cond;
      }
   }
   else if (lExp != NULL) // c'est une erreur
   {
      DM_ENG_deleteComputedExp(lExp);
      lExp = NULL;
   }
   return res;
}

static DM_ENG_ComputedExp* _parseCond(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op)
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* sum = _parseSum(NULL, 0);
   if (sum != NULL)
   {
      if (lExp != NULL)
      {
         sum = DM_ENG_newComputedExp(op, lExp, sum); // associativité de g à dr
      }

      op = DM_ENG_ComputedExp_UNDEF;
      char* tok = _getToken();
      if (tok != NULL)
      {
         if (strcmp(tok, "<") == 0) { op = DM_ENG_ComputedExp_LT; }
         else if (strcmp(tok, "<=") == 0) { op = DM_ENG_ComputedExp_LE; }
         else if (strcmp(tok, ">") == 0) { op = DM_ENG_ComputedExp_GT; }
         else if (strcmp(tok, ">=") == 0) { op = DM_ENG_ComputedExp_GE; }
         else if (strcmp(tok, "==") == 0) { op = DM_ENG_ComputedExp_EQ; }
         else if (strcmp(tok, "!=") == 0) { op = DM_ENG_ComputedExp_NE; }
         if (op != DM_ENG_ComputedExp_UNDEF)
         {
            _nextToken();
            res = _parseCond(sum, op); // à charge de cette fonction de détruire sum en cas d'erreur
         }
      }
      if (op == DM_ENG_ComputedExp_UNDEF)
      {
         res = sum;
      }
   }
   else if (lExp != NULL) // c'est une erreur
   {
      DM_ENG_deleteComputedExp(lExp);
      lExp = NULL;
   }
   return res;
}

static DM_ENG_ComputedExp* _parseSum(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op)
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* prod = _parseProd(NULL, 0);
   if (prod != NULL)
   {
      if (lExp != NULL)
      {
         prod = DM_ENG_newComputedExp(op, lExp, prod); // associativité de g à dr
      }

      op = DM_ENG_ComputedExp_UNDEF;
      char* tok = _getToken();
      if (tok != NULL)
      {
         if (strcmp(tok, "+") == 0) { op = DM_ENG_ComputedExp_PLUS; }
         else if (strcmp(tok, "-") == 0) { op = DM_ENG_ComputedExp_MINUS; }
         if (op != DM_ENG_ComputedExp_UNDEF)
         {
            _nextToken();
            res = _parseSum(prod, op); // à charge de cette fonction de détruire prod en cas d'erreur
         }
      }
      if (op == DM_ENG_ComputedExp_UNDEF)
      {
         res = prod;
      }
   }
   else if (lExp != NULL) // c'est une erreur
   {
      DM_ENG_deleteComputedExp(lExp);
      lExp = NULL;
   }
   return res;
}

static DM_ENG_ComputedExp* _parseProd(DM_ENG_ComputedExp* lExp, DM_ENG_ComputedExp_Operator op)
{
   DM_ENG_ComputedExp* res = NULL;
   DM_ENG_ComputedExp* prim = _parsePrim();
   if (prim != NULL)
   {
      if (lExp != NULL)
      {
         prim = DM_ENG_newComputedExp(op, lExp, prim); // associativité de g à dr
      }

      op = DM_ENG_ComputedExp_UNDEF;
      char* tok = _getToken();
      if (tok != NULL)
      {
         if (strcmp(tok, "*") == 0) { op = DM_ENG_ComputedExp_TIMES; }
         else if (strcmp(tok, "/") == 0) { op = DM_ENG_ComputedExp_DIV; }
         if (op != DM_ENG_ComputedExp_UNDEF)
         {
            _nextToken();
            res = _parseProd(prim, op); // à charge de cette fonction de détruire prim en cas d'erreur
         }
      }
      if (op == DM_ENG_ComputedExp_UNDEF)
      {
         res = prim;
      }
   }
   else if (lExp != NULL) // c'est une erreur
   {
      DM_ENG_deleteComputedExp(lExp);
      lExp = NULL;
   }
   return res;
}

static DM_ENG_ComputedExp* _parsePrim()
{
   DM_ENG_ComputedExp* res = NULL;
   char* tok = _getToken();
   if (tok != NULL)
   {
      if (strcmp(tok, "(") == 0)
      {
         _nextToken();
         res = _parseList();
         if (res != NULL)
         {
            tok = _getToken();
            if (strcmp(tok, ")") == 0)
            {
               _nextToken();
            }
            else // erreur
            {
               DM_ENG_deleteComputedExp(res);
               res = NULL;
            }
         }
      }
      else if (strcmp(tok, "!") == 0)
      {
         _nextToken();
         res = _parsePrim();
         if (res != NULL)
         {
            res = DM_ENG_newComputedExp(DM_ENG_ComputedExp_NOT, res, NULL);
         }
      }
      else if ((strcmp(tok, "+") == 0) || (strcmp(tok, "-") == 0))
      {
         DM_ENG_ComputedExp_Operator op = (tok[0] == '+' ? DM_ENG_ComputedExp_U_PLUS : DM_ENG_ComputedExp_U_MINUS);
         _nextToken();
         res = _parsePrim();
         if (res != NULL)
         {
            res = DM_ENG_newComputedExp(op, res, NULL); // + ou - unaires
         }
      }
      else if ((*tok == '"') || (*tok == '\''))
      {
         char delim = *tok;
         char* str = (char*) malloc(strlen(tok));
         char* ch1 = tok;
         char* ch2 = str;
         do
         {
            ch1++;
            if ((*ch1 == '\\') || (*ch1 == delim)) { ch1++; } // pour passer l'escape et le delimiteur final
            *ch2 = *ch1;
            ch2++;
         }
         while (*ch1 != '\0');
         res = DM_ENG_newLitComputedExp((delim == '"' ? DM_ENG_ComputedExp_STRING : DM_ENG_ComputedExp_QUOTE), str);
         free(str);
         _nextToken();
      }
      else
      {
         unsigned int i = 0;
         if (DM_ENG_stringToUint(tok, &i)) // c'est un nombre (UINT)
         {
            res = DM_ENG_newNumComputedExp(i);
            _nextToken();
         }
         else if (isalpha(*tok) || (*tok == '.') || (*tok == '_')) // c'est un ident
         {
            res = DM_ENG_newLitComputedExp(DM_ENG_ComputedExp_IDENT, tok);
            _nextToken();
            tok = _getToken();
            if ((tok != NULL) && (strcmp(tok, "(") == 0)) // c'est un appel de fct
            {
               _nextToken();
               DM_ENG_ComputedExp* args = _parseList();
               tok = _getToken();
               if ((args != NULL) && (strcmp(tok, ")") == 0)) // c'est bien un appel de fct, on transforme l'expression
               {
                  _nextToken();
                  res->oper = DM_ENG_ComputedExp_CALL;
                  res->left = args;
               }
               else // erreur !
               {
                  if (args != NULL) { DM_ENG_deleteComputedExp(args); }
                  DM_ENG_deleteComputedExp(res);
                  res = NULL;
               }             
            }
         }
         else
         {
            res = DM_ENG_newComputedExp(DM_ENG_ComputedExp_NIL, NULL, NULL);
         }
      }
   }
   else
   {
      res = DM_ENG_newComputedExp(DM_ENG_ComputedExp_NIL, NULL, NULL);
   }
   return res;
}

DM_ENG_ComputedExp* DM_ENG_newComputedExp(DM_ENG_ComputedExp_Operator op, DM_ENG_ComputedExp* l, DM_ENG_ComputedExp* r)
{
   DM_ENG_ComputedExp* res = (DM_ENG_ComputedExp*) malloc(sizeof(DM_ENG_ComputedExp));
   res->oper = op;
   res->left = l;
   res->right = r;
   res->num = 0;
   res->str = NULL;
   return res;
}

DM_ENG_ComputedExp* DM_ENG_newLitComputedExp(DM_ENG_ComputedExp_Operator op, char* lit)
{
   DM_ENG_ComputedExp* res = (DM_ENG_ComputedExp*) malloc(sizeof(DM_ENG_ComputedExp));
   res->oper = op;
   res->left = NULL;
   res->right = NULL;
   res->num = 0;
   res->str = strdup(lit);
   return res;
}

DM_ENG_ComputedExp* DM_ENG_newNumComputedExp(unsigned int i)
{
   DM_ENG_ComputedExp* res = (DM_ENG_ComputedExp*) malloc(sizeof(DM_ENG_ComputedExp));
   res->oper = DM_ENG_ComputedExp_NUM;
   res->left = NULL;
   res->right = NULL;
   res->num = i;
   res->str = NULL;
   return res;
}

void DM_ENG_deleteComputedExp(DM_ENG_ComputedExp* exp)
{
   if (exp->left != NULL) { DM_ENG_deleteComputedExp(exp->left); }
   if (exp->right != NULL) { DM_ENG_deleteComputedExp(exp->right); }
   if (exp->str != NULL) { free(exp->str); }
   free(exp);
}

void _displayExp(DM_ENG_ComputedExp* exp)
{
   if (exp != NULL)
   {
      switch (exp->oper)
      {
         case DM_ENG_ComputedExp_LIST :
            printf("{");
            _displayExp(exp->left);
            printf(",");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_GUARD :
            printf("{");
            _displayExp(exp->left);
            printf("?");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_CONS :
            printf("{");
            _displayExp(exp->left);
            printf(":");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_CONCAT :
            printf("{");
            _displayExp(exp->left);
            printf("++");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_PLUS :
            printf("{");
            _displayExp(exp->left);
            printf("+");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_MINUS :
            printf("{");
            _displayExp(exp->left);
            printf("-");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_TIMES :
            printf("{");
            _displayExp(exp->left);
            printf("*");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_DIV :
            printf("{");
            _displayExp(exp->left);
            printf("/");
            _displayExp(exp->right);
            printf("}");
            break;
         case DM_ENG_ComputedExp_U_PLUS :
            printf("{");
            printf("+");
            _displayExp(exp->left);
            printf("}");
            break;
         case DM_ENG_ComputedExp_U_MINUS :
            printf("{");
            printf("-");
            _displayExp(exp->left);
            printf("}");
            break;
         case DM_ENG_ComputedExp_IDENT :
         case DM_ENG_ComputedExp_STRING :
         case DM_ENG_ComputedExp_QUOTE :
            printf("%s", exp->str);
            break;
         case DM_ENG_ComputedExp_NUM :
            printf("%d", exp->num);
            break;
         case DM_ENG_ComputedExp_CALL :
            printf("%s(", exp->str);
            _displayExp(exp->left);
            printf(")");
            break;
         case DM_ENG_ComputedExp_NIL :
            printf("NIL");
            break;
         default:
            printf("***");
            break;
      }
   }
   else
   {
      printf("???");
   }
}

static DM_ENG_F_GET_VALUE _getValue = NULL;

static bool _eval(DM_ENG_ComputedExp* exp, const char* data, char** pRes);
static bool _evalUint(DM_ENG_ComputedExp* exp, const char* data, unsigned int* pVal);

static bool _evalClass(unsigned int val, DM_ENG_ComputedExp* exp, const char* data, unsigned int* pVal)
{
   bool res = true;
   unsigned int c = 0;
   unsigned int borne;
   while (res && (exp != NULL))
   {
      borne = UINT_MAX;
      if (exp->oper != DM_ENG_ComputedExp_NIL)
      {
         if (exp->oper == DM_ENG_ComputedExp_CONS)
         {
            if ((exp->left == NULL) || (exp->left->oper != DM_ENG_ComputedExp_NIL))
            {
               res = _evalUint(exp->left, data, &borne);
            }
            exp = exp->right;
         }
         else
         {
            res = _evalUint(exp, data, &borne);
            exp = NULL;
         }
      }
      if (val <= borne) break;
      c++;
   }
   if (res) { *pVal = c; } 
   return res;
}

static bool _evalDiffTime(DM_ENG_ComputedExp* arg1, const char* data, unsigned int* pVal)
{
   bool res = false;
   time_t t1 = 0;
   time_t t2 = 0;
   DM_ENG_ComputedExp* arg2 = NULL; // arg1 => t1, arg2 => t2
   if (arg1->oper == DM_ENG_ComputedExp_LIST)
   {
      arg2 = arg1->left;
      arg1 = arg1->right;
   }
   if ((arg2 == NULL) || (arg2->oper == DM_ENG_ComputedExp_NIL)) // 1 seul argument ou 1er argument vide => t2 = now
   {
      t2 = time(NULL);
      res = true;
   }
   else
   {
      char* str = NULL;
      res = _eval(arg2, data, &str);
      if (res && (str != NULL))
      {
         res = DM_ENG_dateStringToTime(str, &t2);
         free(str);
      }
   }
   if (res)
   {
      char* str = NULL;
      res = _eval(arg1, data, &str);
      if (res && (str != NULL))
      {
         res = DM_ENG_dateStringToTime(str, &t1);
         free(str);
      }
      if (res)
      {
         if (t2 < t1)
         {
            res = false;
         }
         else
         {
            *pVal = t2-t1;
         }
      }
   }
   return res;
}

static bool _evalUint(DM_ENG_ComputedExp* exp, const char* data, unsigned int* pVal)
{
   bool res = false;
   unsigned int left;
   unsigned int right;
   if (exp != NULL)
   {
      switch (exp->oper)
      {
         case DM_ENG_ComputedExp_LIST :
         case DM_ENG_ComputedExp_CONS :
            // to do
            break;
         case DM_ENG_ComputedExp_PLUS :
         case DM_ENG_ComputedExp_MINUS :
         case DM_ENG_ComputedExp_TIMES :
         case DM_ENG_ComputedExp_DIV :
            res = _evalUint(exp->left, data, &left) && _evalUint(exp->right, data, &right);
            if (res)
            {
               if (exp->oper == DM_ENG_ComputedExp_PLUS) { *pVal = left+right; }
               else if (exp->oper == DM_ENG_ComputedExp_MINUS) { *pVal = left-right; }
               else if (exp->oper == DM_ENG_ComputedExp_TIMES) { *pVal = left*right; }
               else if (exp->oper == DM_ENG_ComputedExp_DIV)
               {
                  if (right == 0) { res = false; }
                  else { *pVal = left/right; }
               }
            }
            break;
         case DM_ENG_ComputedExp_U_PLUS :
            res = _evalUint(exp->left, data, pVal);
            break;
         case DM_ENG_ComputedExp_U_MINUS :
            res = _evalUint(exp->left, data, &left);
            if (res) { *pVal = -left; }
            break;
         case DM_ENG_ComputedExp_IDENT :
         case DM_ENG_ComputedExp_QUOTE :
            if (_getValue != NULL) { res = DM_ENG_stringToUint(_getValue(exp->str, data), pVal); }
            break;
         case DM_ENG_ComputedExp_STRING :
            // to do
            break;
         case DM_ENG_ComputedExp_NUM :
            *pVal = exp->num;
            res = true;
            break;
         case DM_ENG_ComputedExp_CALL :
            {
               DM_ENG_ComputedExp* arg1 = exp->left;
               if ((strcmp(exp->str, _CLASS_FCT) == 0) && (arg1->oper == DM_ENG_ComputedExp_LIST) && (arg1->left != NULL))
               {
                  res = _evalUint(arg1->left, data, &left);
                  if (res) { res = _evalClass(left, arg1->right, data, pVal); }
               }
               else if (strcmp(exp->str, _DIFF_TIME_FCT) == 0)
               {
                  res = _evalDiffTime(arg1, data, pVal);
               }
               else
               {
                  if ((arg1->oper == DM_ENG_ComputedExp_LIST) && (arg1->left != NULL)) { arg1 = arg1->left; }
                  if (arg1->oper == DM_ENG_ComputedExp_IDENT)
                  {
                     char* prmName = (char*) malloc(strlen(arg1->str)+strlen(exp->str)+2);
                     strcpy(prmName, arg1->str);
                     strcat(prmName, _INTERNAL_SEPARATOR);
                     strcat(prmName, exp->str);
                     if (_getValue != NULL) { res = DM_ENG_stringToUint(_getValue(prmName, data), pVal); }
                     free(prmName);
                  }
               }
            }
            break;
         case DM_ENG_ComputedExp_NIL :
         default:
            // erreur !!
            break;
      }
   }
   return res;
}

static bool _evalBool(DM_ENG_ComputedExp* exp, const char* data, bool* pVal)
{
   bool res = false;
   if (exp != NULL)
   {
      switch (exp->oper)
      {
         case DM_ENG_ComputedExp_OR :
         case DM_ENG_ComputedExp_AND :
            {
               bool left = false;
               bool right = false;
               res = _evalBool(exp->left, data, &left) && _evalBool(exp->right, data, &right);
               if (res)
               {
                  *pVal = (exp->oper == DM_ENG_ComputedExp_OR ? left|right : left&right);
               }
            }
            break;
         case DM_ENG_ComputedExp_LT :
         case DM_ENG_ComputedExp_LE :
         case DM_ENG_ComputedExp_GT :
         case DM_ENG_ComputedExp_GE :
         case DM_ENG_ComputedExp_EQ :
         case DM_ENG_ComputedExp_NE :
            {
               unsigned int left;
               unsigned int right;
               res = _evalUint(exp->left, data, &left) && _evalUint(exp->right, data, &right);
               if (res)
               {
                  if (exp->oper == DM_ENG_ComputedExp_LT) { *pVal = left<right; }
                  else if (exp->oper == DM_ENG_ComputedExp_LE) { *pVal = left<=right; }
                  else if (exp->oper == DM_ENG_ComputedExp_GT) { *pVal = left>right; }
                  else if (exp->oper == DM_ENG_ComputedExp_GE) { *pVal = left>=right; }
                  else if (exp->oper == DM_ENG_ComputedExp_EQ) { *pVal = left==right; }
                  else if (exp->oper == DM_ENG_ComputedExp_NE) { *pVal = left!=right; }
               }
               else // sinon on compare chaîne à chaîne
               {
                  char* sLeft = NULL;
                  char* sRight = NULL;
                  _eval(exp->left, data, &sLeft);
                  _eval(exp->right, data, &sRight);
                  if ((sLeft != NULL) && (sRight != NULL))
                  {
                     int cmp = strcmp(sLeft, sRight);
                     if (exp->oper == DM_ENG_ComputedExp_LT) { *pVal = cmp<0; }
                     else if (exp->oper == DM_ENG_ComputedExp_LE) { *pVal = cmp<=0; }
                     else if (exp->oper == DM_ENG_ComputedExp_GT) { *pVal = cmp>0; }
                     else if (exp->oper == DM_ENG_ComputedExp_GE) { *pVal = cmp>=0; }
                     else if (exp->oper == DM_ENG_ComputedExp_EQ) { *pVal = cmp==0; }
                     else if (exp->oper == DM_ENG_ComputedExp_NE) { *pVal = cmp!=0; }
                     res = true;
                  }
                  DM_ENG_FREE(sLeft);
                  DM_ENG_FREE(sRight);
               }
            }
            break;
         case DM_ENG_ComputedExp_NOT :
            {
               bool left;
               res = _evalBool(exp->left, data, &left);
               if (res)
               {
                  *pVal = !left;
               }
            }
            break;
         case DM_ENG_ComputedExp_IDENT :
            if (_getValue != NULL) { res = DM_ENG_stringToBool(_getValue(exp->str, data), pVal); }
            break;
         case DM_ENG_ComputedExp_CALL :
            {
               DM_ENG_ComputedExp* arg1 = exp->left;
               if ((arg1->oper == DM_ENG_ComputedExp_LIST) && (arg1->left != NULL)) { arg1 = arg1->left; }
               if (arg1->oper == DM_ENG_ComputedExp_IDENT)
               {
                  char* prmName = (char*) malloc(strlen(arg1->str)+strlen(exp->str)+2);
                  strcpy(prmName, arg1->str);
                  strcat(prmName, _INTERNAL_SEPARATOR);
                  strcat(prmName, exp->str);
                  if (_getValue != NULL) { res = DM_ENG_stringToBool(_getValue(prmName, data), pVal); }
                  free(prmName);
               }
            }
            break;
         case DM_ENG_ComputedExp_NIL :
         default:
            // erreur !!
            break;
      }
   }
   return res;
}

static bool _eval(DM_ENG_ComputedExp* exp, const char* data, char** pRes)
{
   bool res = false;
   unsigned int left;
   unsigned int right;
   if (exp != NULL)
   {
      switch (exp->oper)
      {
         case DM_ENG_ComputedExp_LIST :
            // to do
            break;
         case DM_ENG_ComputedExp_GUARD : // expression gardée
            {
               bool guard = false;
               res = _evalBool(exp->left, data, &guard);
               if (res) { res = guard; }
               if (res) { res = _eval(exp->right, data, pRes); }
            }
            break;
         case DM_ENG_ComputedExp_CONS :
         case DM_ENG_ComputedExp_CONCAT :
            {
               char* sLeft = NULL;
               char* sRight = NULL;
               if (_eval(exp->left, data, &sLeft))
               {
                  _eval(exp->right, data, &sRight);
               }
               if ((sLeft != NULL) && (sRight != NULL))
               {
                  *pRes = (char*) malloc(strlen(sLeft)+strlen(sRight)+2);
                  strcpy(*pRes, sLeft);
                  if (exp->oper == DM_ENG_ComputedExp_CONS) { strcat(*pRes, ":"); }
                  strcat(*pRes, sRight);
                  res = true;
               }
               DM_ENG_FREE(sLeft);
               DM_ENG_FREE(sRight);
            }
            break;
         case DM_ENG_ComputedExp_OR :
         case DM_ENG_ComputedExp_AND :
         case DM_ENG_ComputedExp_LT :
         case DM_ENG_ComputedExp_LE :
         case DM_ENG_ComputedExp_GT :
         case DM_ENG_ComputedExp_GE :
         case DM_ENG_ComputedExp_EQ :
         case DM_ENG_ComputedExp_NE :
         case DM_ENG_ComputedExp_NOT :
            {
               bool val = false;
               res = _evalBool(exp, data, &val);
               if (res) { *pRes = strdup(val ? DM_ENG_TRUE_STRING : DM_ENG_FALSE_STRING); }
            }
            break;
         case DM_ENG_ComputedExp_PLUS :
         case DM_ENG_ComputedExp_MINUS :
         case DM_ENG_ComputedExp_TIMES :
         case DM_ENG_ComputedExp_DIV :
            res = _evalUint(exp->left, data, &left) && _evalUint(exp->right, data, &right);
            if (res)
            {
               unsigned int val = 0;
               if (exp->oper == DM_ENG_ComputedExp_PLUS) { val = left+right; }
               else if (exp->oper == DM_ENG_ComputedExp_MINUS) { val = left-right; }
               else if (exp->oper == DM_ENG_ComputedExp_TIMES) { val = left*right; }
               else if (exp->oper == DM_ENG_ComputedExp_DIV)
               {
                  if (right == 0) { res = false; }
                  else { val = left/right; }
               }
               *pRes = DM_ENG_uintToString(val);
            }
            break;
         case DM_ENG_ComputedExp_U_PLUS :
            res = _evalUint(exp->left, data, &left);
            if (res) { *pRes = DM_ENG_uintToString(left); }
            break;
         case DM_ENG_ComputedExp_U_MINUS :
            res = _evalUint(exp->left, data, &left);
            if (res) { *pRes = DM_ENG_uintToString(-left); }
            break;
         case DM_ENG_ComputedExp_IDENT :
         case DM_ENG_ComputedExp_QUOTE :
            if (_getValue != NULL)
            {
               char* sVal = _getValue(exp->str, data);
               if (sVal != NULL)
               {
                  *pRes = strdup(sVal);
                  res = true;
               }
            }
            break;
         case DM_ENG_ComputedExp_STRING :
            *pRes = strdup(exp->str);
            res = true;
            break;
         case DM_ENG_ComputedExp_NUM :
            *pRes = DM_ENG_uintToString(exp->num);
            res = true;
            break;
         case DM_ENG_ComputedExp_CALL :
            {
               DM_ENG_ComputedExp* arg1 = exp->left;
               if ((strcmp(exp->str, _CLASS_FCT) == 0) && (arg1->oper == DM_ENG_ComputedExp_LIST) && (arg1->left != NULL))
               {
                  res = _evalUint(arg1->left, data, &left);
                  if (res)
                  {
                     unsigned int val = 0;
                     res = _evalClass(left, arg1->right, data, &val);
                     if (res) { *pRes = DM_ENG_uintToString(val); }
                  }
               }
               else if (strcmp(exp->str, _DIFF_TIME_FCT) == 0)
               {
                  unsigned int val;
                  res = _evalDiffTime(arg1, data, &val);
                  if (res) { *pRes = DM_ENG_uintToString(val); }
               }
               else
               {
                  if ((arg1->oper == DM_ENG_ComputedExp_LIST) && (arg1->left != NULL)) { arg1 = arg1->left; }
                  if (arg1->oper == DM_ENG_ComputedExp_IDENT)
                  {
                     char* prmName = (char*) malloc(strlen(arg1->str)+strlen(exp->str)+2);
                     strcpy(prmName, arg1->str);
                     strcat(prmName, _INTERNAL_SEPARATOR);
                     strcat(prmName, exp->str);
                     if (_getValue != NULL)
                     {
                        char* sVal = _getValue(prmName, data);
                        if (sVal != NULL)
                        {
                           *pRes = strdup(sVal);
                           res = true;
                        }
                     }
                     free(prmName);
                  }
               }
            }
            break;
         case DM_ENG_ComputedExp_NIL :
         default:
            // erreur !!
            break;
      }
   }
   return res;
}

bool DM_ENG_ComputedExp_eval(char* sExp, DM_ENG_F_GET_VALUE getValue, const char* data, char** pRes)
{
   bool resOK = false;
   DM_ENG_ComputedExp* exp = DM_ENG_ComputedExp_parse(sExp);
   if (exp != NULL)
   {
      _getValue = getValue;
      resOK = _eval(exp, data, pRes);
      DM_ENG_deleteComputedExp(exp);
   }
   return resOK;
}

////////////////////////////////////////////////////////////////////

static DM_ENG_F_IS_VALUE_CHANGED _isValueChanged = NULL;

static bool _isChanged(DM_ENG_ComputedExp* exp, char* data, bool* pChanged, bool* pPushed)
{
   bool res = false;
   if (exp != NULL)
   {
      switch (exp->oper)
      {
         case DM_ENG_ComputedExp_LIST :
         case DM_ENG_ComputedExp_CONS :
         case DM_ENG_ComputedExp_PLUS :
         case DM_ENG_ComputedExp_MINUS :
         case DM_ENG_ComputedExp_TIMES :
         case DM_ENG_ComputedExp_DIV :
         case DM_ENG_ComputedExp_LT :
         case DM_ENG_ComputedExp_LE :
         case DM_ENG_ComputedExp_GT :
         case DM_ENG_ComputedExp_GE :
         case DM_ENG_ComputedExp_EQ :
         case DM_ENG_ComputedExp_NE :
         case DM_ENG_ComputedExp_OR :
         case DM_ENG_ComputedExp_AND :
         case DM_ENG_ComputedExp_CONCAT :
            res = _isChanged(exp->left, data, pChanged, pPushed);
            if (res && !(*pChanged && *pPushed))
            {
               bool changed = false;
               bool pushed = false;
               res = _isChanged(exp->right, data, &changed, &pushed);
               *pChanged |= changed;
               *pPushed |= pushed;
            }
            break;
         case DM_ENG_ComputedExp_U_PLUS :
         case DM_ENG_ComputedExp_U_MINUS :
         case DM_ENG_ComputedExp_NOT :
            res = _isChanged(exp->left, data, pChanged, pPushed);
            break;
         case DM_ENG_ComputedExp_IDENT :
            if (_isValueChanged != NULL)
            {
               *pChanged = _isValueChanged(exp->str, data, pPushed);
               res = true;
            }
            break;
         case DM_ENG_ComputedExp_CALL :
            {
               DM_ENG_ComputedExp* arg1 = exp->left;
               if (DM_ENG_ComputedExp_isPredefinedFunction(exp->str))
               {
                  res = _isChanged(arg1, data, pChanged, pPushed);
               }
               else
               {
                  if ((arg1->oper == DM_ENG_ComputedExp_LIST) && (arg1->left != NULL)) { arg1 = arg1->left; }
                  if (arg1->oper == DM_ENG_ComputedExp_IDENT)
                  {
                     char* prmName = (char*) malloc(strlen(arg1->str)+strlen(exp->str)+2);
                     strcpy(prmName, arg1->str);
                     strcat(prmName, _INTERNAL_SEPARATOR);
                     strcat(prmName, exp->str);
                     if (_isValueChanged != NULL)
                     {
                        *pChanged = _isValueChanged(prmName, data, pPushed);
                        res = true;
                     }
                     free(prmName);
                  }
               }
            }
            break;
         case DM_ENG_ComputedExp_GUARD : // expression gardée
            {
               bool guard = false;
               res = _evalBool(exp->left, data, &guard);
               if (res)
               {
                  if (!guard) { res = false; }
                  else { res = _isChanged(exp->right, data, pChanged, pPushed); }
               }
            }
            break;
         default: // false dans les autres cas
               *pChanged = false;
               *pPushed = false;
               res = true;
            break;
      }
   }
   return res;
}

bool DM_ENG_ComputedExp_isChanged(char* sExp, DM_ENG_F_IS_VALUE_CHANGED isValueChanged, DM_ENG_F_GET_VALUE getValue, char* data, bool* pChanged, bool* pPushed)
{
   bool res = false;
   DM_ENG_ComputedExp* exp = DM_ENG_ComputedExp_parse(sExp);
   if (exp != NULL)
   {
      _isValueChanged = isValueChanged;
      _getValue = getValue;
      res = _isChanged(exp, data, pChanged, pPushed);
      DM_ENG_deleteComputedExp(exp);
   }
   return res;
}
