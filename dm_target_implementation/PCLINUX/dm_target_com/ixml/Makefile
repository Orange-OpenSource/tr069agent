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

TMP_LOCAL_DIR = $(CURDIR)
INCS = $(INCLUDE_DIR) -I$(TMP_LOCAL_DIR) -I$(TMP_LOCAL_DIR)/inc/ -I$(TMP_LOCAL_DIR)/src/inc/

REP_OBJ = $(LOCAL_DIR)/obj

SOURCES = src/attr.c         \
	  src/document.c     \
	  src/element.c      \
	  src/ixml.c         \
	  src/ixmlmembuf.c   \
	  src/ixmlparser.c   \
	  src/namedNodeMap.c \
	  src/node.c         \
	  src/nodeList.c
	  
OBJETS  = $(REP_OBJ)/attr.o         \
          $(REP_OBJ)/document.o     \
          $(REP_OBJ)/element.o      \
          $(REP_OBJ)/ixml.o         \
          $(REP_OBJ)/ixmlmembuf.o   \
          $(REP_OBJ)/ixmlparser.o   \
          $(REP_OBJ)/namedNodeMap.o \
          $(REP_OBJ)/node.o         \
          $(REP_OBJ)/nodeList.o

all: $(OBJETS)

$(REP_OBJ)/attr.o: src/attr.c
	$(CC) -o $(REP_OBJ)/attr.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/attr.c

$(REP_OBJ)/document.o: src/document.c
	$(CC) -o $(REP_OBJ)/document.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/document.c

$(REP_OBJ)/element.o: src/element.c
	$(CC) -o $(REP_OBJ)/element.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/element.c

$(REP_OBJ)/ixml.o: src/ixml.c
	$(CC) -o $(REP_OBJ)/ixml.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/ixml.c

$(REP_OBJ)/ixmlmembuf.o: src/ixmlmembuf.c
	$(CC) -o $(REP_OBJ)/ixmlmembuf.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/ixmlmembuf.c

$(REP_OBJ)/ixmlparser.o: src/ixmlparser.c
	$(CC) -o $(REP_OBJ)/ixmlparser.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/ixmlparser.c

$(REP_OBJ)/namedNodeMap.o: src/namedNodeMap.c
	$(CC) -o $(REP_OBJ)/namedNodeMap.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/namedNodeMap.c

$(REP_OBJ)/node.o: src/node.c
	$(CC) -o $(REP_OBJ)/node.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/node.c

$(REP_OBJ)/nodeList.o: src/nodeList.c
	$(CC) -o $(REP_OBJ)/nodeList.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/nodeList.c


clean:
	rm -f $(REP_OBJ)/*.o
