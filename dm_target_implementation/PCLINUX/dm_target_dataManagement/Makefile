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

SOURCES = src/DM_DAT1_ParameterData.c

OBJETS  = $(REP_OBJ)/DM_DAT1_ParameterData.o
	  
	  
all: $(OBJETS)


$(REP_OBJ)/DM_DAT1_ParameterData.o: src/DM_DAT1_ParameterData.c
	$(CC) -o $(REP_OBJ)/DM_DAT1_ParameterData.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) $(INCS_FTX) -c src/DM_DAT1_ParameterData.c 


clean:
	rm -f $(REP_OBJ)/*.o

