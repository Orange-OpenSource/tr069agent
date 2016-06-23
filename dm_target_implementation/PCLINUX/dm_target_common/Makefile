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

SOURCES = src/DM_GlobalUtils.c  \
          src/DM_CMN_Thread.c

OBJETS  = $(REP_OBJ)/DM_GlobalUtils.o \
          $(REP_OBJ)/DM_CMN_Thread.o
	  
all: $(OBJETS)


$(REP_OBJ)/DM_GlobalUtils.o: src/DM_GlobalUtils.c
	$(CC) -o $(REP_OBJ)/DM_GlobalUtils.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_GlobalUtils.c
	
$(REP_OBJ)/DM_CMN_Thread.o: src/DM_CMN_Thread.c
	$(CC) -o $(REP_OBJ)/DM_CMN_Thread.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_CMN_Thread.c
	
clean:
	rm -f $(REP_OBJ)/*.o
	
	


