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
#* Created     : 12/01/2009
#* Author      :
#*
#*---------------------------------------------------------------------------
#*/

OBJ_DIR = $(LOCAL_DIR)/obj
INCS = $(INCLUDE_DIR)

SRC_LIB_FILES := src/miniwget.c src/minixml.c src/igd_desc_parse.c \
	         src/minisoap.c src/minissdpc.c src/miniupnpc.c \
	         src/upnpreplyparse.c src/upnpcommands.c src/upnperrors.c \
                 src/connecthostport.c src/portlistingparse.c src/receivedata.c \
	         src/upnpdev.c

SRC_THREADS_FILES := $(SRC_LIB_FILES) src/dm_upnpigd.c

OBJ_LIB_FILES := $(subst src,$(OBJ_DIR),$(patsubst %.c,%.o,$(SRC_LIB_FILES)))
OBJ_THREADS_FILES := $(subst src,$(OBJ_DIR),$(patsubst %.c,%.o,$(SRC_THREADS_FILES)))

CWMP_C_FLAGS += -DCONFIG_CRYPTO_INTERNAL


all: $(OBJ_THREADS_FILES)

$(OBJ_DIR)/%.o: src/%.c
	$(CC) $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -o $@ -c $<

clean :
	@rm -f $(OBJ_DIR)/*.o  src/*~ inc/*~ $(LIB_FILE)
