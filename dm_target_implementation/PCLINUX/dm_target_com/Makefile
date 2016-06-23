#*---------------------------------------------------------------------------
#* Project     : TR69 Generic Agent
#* Sub-Project : CWMP - TR069
#*
#* Copyright (C) 2014 Orange
#*
#* This software is distributed under the terms and conditions of the 'Apache-2.0'
#* license which can be found in the file 'LICENSE.txt' in this package distribution
#* or at 'http://www.apache.org/licenses/LICENSE-2.0'.
#*
#*---------------------------------------------------------------------------
#* File        : Makefile
#*
#* Created     : 18/04/2008
#* Author      : 
#*
#*---------------------------------------------------------------------------
#*/


INCS = $(INCLUDE_DIR)

REP_OBJ = $(LOCAL_DIR)/obj


SOURCES = src/DM_COM_HttpServerInterface.c    \
	  src/DM_COM_HttpClientInterface.c    \
	  src/DM_COM_DomXmlParserInterface.c  \
	  src/DM_COM_ConnectionSecurity.c
	  
OBJETS  = $(REP_OBJ)/DM_COM_HttpServerInterface.o    \
	  $(REP_OBJ)/DM_COM_HttpClientInterface.o    \
	  $(REP_OBJ)/DM_COM_DomXmlParserInterface.o  \
	  $(REP_OBJ)/DM_COM_ConnectionSecurity.o
	  
all: $(OBJETS)

$(REP_OBJ)/DM_COM_HttpServerInterface.o: src/DM_COM_HttpServerInterface.c
	$(CC) -o $(REP_OBJ)/DM_COM_HttpServerInterface.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_COM_HttpServerInterface.c
	
$(REP_OBJ)/DM_COM_HttpClientInterface.o: src/DM_COM_HttpClientInterface.c
	$(CC) -o $(REP_OBJ)/DM_COM_HttpClientInterface.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_COM_HttpClientInterface.c

$(REP_OBJ)/DM_COM_DomXmlParserInterface.o: src/DM_COM_DomXmlParserInterface.c
	$(CC) -o $(REP_OBJ)/DM_COM_DomXmlParserInterface.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_COM_DomXmlParserInterface.c

$(REP_OBJ)/DM_COM_ConnectionSecurity.o: src/DM_COM_ConnectionSecurity.c
	$(CC) -o $(REP_OBJ)/DM_COM_ConnectionSecurity.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_COM_ConnectionSecurity.c

clean:
	rm -f $(REP_OBJ)/*.o src/*~ inc/*~
	
	


