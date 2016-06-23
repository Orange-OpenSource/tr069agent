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

SOURCES = src/dm_com.c         \
	  src/dm_com_rpc_acs.c \
	  src/dm_com_digest.c  \
	  src/md5.c            \
	  src/dm_com_utils.c
	  
OBJETS  = $(REP_OBJ)/dm_com.o         \
	  $(REP_OBJ)/dm_com_rpc_acs.o \
	  $(REP_OBJ)/dm_com_digest.o  \
	  $(REP_OBJ)/md5.o            \
	  $(REP_OBJ)/dm_com_utils.o


all: $(OBJETS)

$(REP_OBJ)/dm_com.o: src/dm_com.c
	$(CC) -o $(REP_OBJ)/dm_com.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/dm_com.c

$(REP_OBJ)/dm_com_rpc_acs.o: src/dm_com_rpc_acs.c
	$(CC) -o $(REP_OBJ)/dm_com_rpc_acs.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/dm_com_rpc_acs.c
	
$(REP_OBJ)/dm_com_digest.o: src/dm_com_digest.c
	$(CC) -o $(REP_OBJ)/dm_com_digest.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/dm_com_digest.c

$(REP_OBJ)/md5.o: src/md5.c
	$(CC) -o $(REP_OBJ)/md5.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/md5.c
	
$(REP_OBJ)/dm_com_utils.o: src/dm_com_utils.c
	$(CC) -o $(REP_OBJ)/dm_com_utils.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/dm_com_utils.c

clean:
	rm -f $(REP_OBJ)/*.o
	
	


