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


LDFLAGS = $(CWMP_USED_LIBRARY_FLAGS)
STD=

APPLI_NAME = $(CWMP_APPLICATION_NAME)
APPLI_DIR  = $(LOCAL_DIR)


OBJETS  = $(REP_OBJ)/dm_main.o
OBJETS_GEN = $(wildcard $(REP_OBJ)/*.o)


all: $(OBJETS)
	$(CC) -o $(APPLI_DIR)/$(APPLI_NAME) $(OBJETS_GEN) $(LIBS) $(LDFLAGS) 


$(REP_OBJ)/dm_main.o: src/dm_main.c
	$(CC) -o $(REP_OBJ)/dm_main.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/dm_main.c


install:

	
clean:
	rm -f $(REP_OBJ)/*.o
	rm -f $(APPLI_DIR)/$(APPLI_NAME)



