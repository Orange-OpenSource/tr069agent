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


# CC=$(CROSS_COMPILE)gcc
SOURCES = src/DM_DeviceAdapter.c


ifeq ($(USE_SIMU), Y)
OBJETS  = $(REP_OBJ)/DM_DeviceAdapter.o $(REP_OBJ)/DM_SystemCalls.o $(REP_OBJ)/DM_ComponentCalls.o $(REP_OBJ)/simu
else
OBJETS  = $(REP_OBJ)/DM_DeviceAdapter.o $(REP_OBJ)/DM_SystemCalls.o $(REP_OBJ)/DM_ComponentCalls.o
endif
	  
all: $(OBJETS)


$(REP_OBJ)/DM_DeviceAdapter.o: src/DM_DeviceAdapter.c
	$(CC) -o $(REP_OBJ)/DM_DeviceAdapter.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) $(INCS_FTX) -c src/DM_DeviceAdapter.c 

$(REP_OBJ)/DM_SystemCalls.o: src/DM_SystemCalls.c
	$(CC) -o $(REP_OBJ)/DM_SystemCalls.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) $(INCS_FTX) -c src/DM_SystemCalls.c 

$(REP_OBJ)/DM_ComponentCalls.o: src/DM_ComponentCalls.c
	$(CC) -o $(REP_OBJ)/DM_ComponentCalls.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) $(INCS_FTX) -c src/DM_ComponentCalls.c 

ifeq ($(USE_SIMU), Y)
$(REP_OBJ)/simu : src/DM_StatSimulator.c
	$(CC) -o $(REP_OBJ)/simu $(INCS) src/DM_StatSimulator.c
endif

clean:
	rm -f $(REP_OBJ)/*.o $(REP_OBJ)/simu

