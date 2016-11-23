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

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# Makefile Usage
#
# Mandatory Parameter:
#	Target: The Target Name must be specified
#
# make Optional Parameters:
#	DEBUG: By default no no CC debug flag is tun ON. To add debug info: DEBUG=Y
#	TRACE_LEVEL: By default no trace is displayed. To add trace: TRACE_LEVEL=n   with n = 0..7
#       DEVICE_TYPE    : Define the device type (InternetGatewayDevice or Device) Can be set to IGD or D (Default value is D for Device)
#       STUN_ENABLE    : STUN_ENABLE=Y / N (Default N)
#       WITHOUT_UI     : WITHOUT_UI=Y / N (Default N)
#       USE_SIMU       : USE_SIMU=Y / N (Default N)
#       DBUS_IPC_ENABLE: DBUS_IPC_ENABLE=Y / N (Default N)
#       USE_DBUS_TEST  : USE_DBUS_TEST=Y / N (Default N)
#
#
#   Exemple:
#   make Target=TargetName TRACE_LEVEL=7 DEBUG=Y
#   make clean Target=TargetName
#
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# ---------------------   TARGET CONFIGURATION - BEGIN  ---------------------
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------

# COMPILATOR DECLARATION
CC = gcc -fno-stack-protector

# NAME OF THE GENERATED APPLICATION
CWMP_APPLICATION_NAME = cwmpd

# OBJ DIR LOCATION
REP_OBJ = $(LOCAL_DIR)/obj


# DECLARE ALL THE INCLUDE PATH FOR USED LIBRARY HEADERS(cwmp application headers)
ifeq ($(DBUS_IPC_ENABLE), Y)
  LIB_HEADER_INC = -I/usr/local/include $(shell pkg-config --cflags-only-I dbus-1)
else
  LIB_HEADER_INC = -I/usr/local/include
endif

# DECLARE ALL THE UDSED LIBRARY
# CWMP_USED_LIBRARY_FLAGS = -lpthread -lcurl -lefence
# CWMP_USED_LIBRARY_FLAGS = -lpthread -lcurl -lqdbm
# CWMP_USED_LIBRARY_FLAGS = -lpthread -lcurl -lm
ifeq ($(DBUS_IPC_ENABLE), Y)
  CWMP_USED_LIBRARY_FLAGS = -lpthread -lcurl -ldbus-1
else
  CWMP_USED_LIBRARY_FLAGS = -lpthread -lcurl
endif

# Debug Flag. Default value is NO DEBUG and optimization set to -Os
# To turn on debug: make Target=xxxx DEBUG=Y
DEFAULT_DEBUG_FLAG    = N
DEBUG                 = $(DEFAULT_DEBUG_FLAG(DBG))$(DBG)
ifeq ($(DEBUG), Y)
  CWMP_DEBUG_FLAG = -ggdb -O0
else
  CWMP_OPTIM_FLAG=-Os -fno-strict-aliasing
endif

ifeq ($(DEVICE_TYPE), IGD)
  CWMP_DEVICE_TYPE = -D_InternetGatewayDevice_
endif

ifeq ($(WITHOUT_UI), Y)
  CWMP_WITHOUT_UI = -D_WithoutUI_
endif

ifeq ($(USE_SIMU), Y)
  CWMP_USE_SIMU = -D_UseSimu_
endif

ifeq ($(DBUS_IPC_ENABLE), Y)
  CWMP_DBUS_IPC_ENABLE = -D_DBusIpcEnabled_
endif

# CFLAGS -Wno-unused 
#CWMP_CFLAGS = -W -Wall -Werror $(CWMP_DEVICE_TYPE) $(CWMP_WITHOUT_UI) $(CWMP_USE_SIMU) $(CWMP_DBUS_IPC_ENABLE)
CWMP_CFLAGS = -W -Wall  $(CWMP_DEVICE_TYPE) $(CWMP_WITHOUT_UI) $(CWMP_USE_SIMU) $(CWMP_DBUS_IPC_ENABLE)

# Trace Level used for the Trace System (0..7) 0--> No Trace, ..., 7 All the traces are generated into CWMP trace file
# The Default Trace level is 0. To add trace used: make Target=xxxx TRACE_LEVEL=N  (with N = 0..7)
DEFAULT_TRACE_LEVEL     = 0
TRACE_LEVEL             = $(DEFAULT_TRACE_LEVEL$(TRACELEVEL))$(TRACELEVEL)

DEFAULT_STUN_ENABLE = N
STUN_ENABLE         = $(DEFAULT_STUN_ENABLE$(STUNENABLE))$(STUNENABLE)

# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------
# ----------------------   TARGET CONFIGURATION - END  ----------------------
# ---------------------------------------------------------------------------
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
#               PRIVATE CONFIGURATION - BEGIN (NO NEED TO CHANGE)
# ---------------------------------------------------------------------------


TargetName = $(Target)

LOCAL_DIR = $(CURDIR)

# DECLARE ALL THE INCLUDE PATH (cwmp application headers)
CWMP_INC  = -I$(LOCAL_DIR) -I$(LOCAL_DIR)/dm_inc/ -I$(LOCAL_DIR)/dm_com/inc
CWMP_INC  += -I$(LOCAL_DIR)/dm_engine/inc -I$(LOCAL_DIR)/dm_main/inc
CWMP_INC  += -I$(LOCAL_DIR)/dm_target_implementation/$(TargetName)/dm_target_com/inc
CWMP_INC  += -I$(LOCAL_DIR)/dm_target_implementation/$(TargetName)/dm_target_deviceAdapter/inc
CWMP_INC  += -I$(LOCAL_DIR)/dm_target_implementation/$(TargetName)/dm_target_dataManagement/inc
CWMP_INC  += -I$(LOCAL_DIR)/dm_target_implementation/$(TargetName)/dm_target_common/inc
CWMP_INC  += -I$(LOCAL_DIR)/dm_target_implementation/$(TargetName)/dm_target_com/ixml/inc/
ifeq ($(DBUS_IPC_ENABLE), Y)
  CWMP_INC  += -I$(LOCAL_DIR)/dm_ipc/inc
endif


# INCLUDE DIR
INCLUDE_DIR = $(CWMP_INC) $(LIB_HEADER_INC)

# CWMP_CPP_FLAGS
# CWMP_CPP_FLAGS = -DFTX_TRACE_LEVEL=$(TRACE_LEVEL) -DX86
CWMP_CPP_FLAGS = -DFTX_TRACE_LEVEL=$(TRACE_LEVEL)

# CWMP_C_FLAGS
CWMP_C_FLAGS = $(CWMP_OPTIM_FLAG) $(CWMP_DEBUG_FLAG) $(CWMP_CFLAGS)

ifeq ($(STUN_ENABLE), Y)
  CWMP_C_FLAGS += -DSTUN_ENABLED_ON_TR069_AGENT
  CWMP_INC  += -I$(LOCAL_DIR)/dm_target_implementation/$(TargetName)/dm_target_nat/stun/inc
endif

# ---------------------------------------------------------------------------
#               PRIVATE CONFIGURATION - END (NO NEED TO CHANGE)
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
#                          EXPORT ENVIRONMENT VARIABLES
# ---------------------------------------------------------------------------
export CC
export CWMP_C_FLAGS
export CWMP_CPP_FLAGS
export CWMP_APPLICATION_NAME
export CWMP_USED_LIBRARY_FLAGS
export INCLUDE_DIR
export LOCAL_DIR

# Build target dependant path
dm_target_common_path         = dm_target_implementation/$(TargetName)/dm_target_common/
dm_target_com_path            = dm_target_implementation/$(TargetName)/dm_target_com/
dm_target_ixml_path           = dm_target_implementation/$(TargetName)/dm_target_com/ixml
dm_target_deviceAdapter_path  = dm_target_implementation/$(TargetName)/dm_target_deviceAdapter/
dm_target_dataManagement_path = dm_target_implementation/$(TargetName)/dm_target_dataManagement/
dm_target_main_path           = dm_target_implementation/$(TargetName)/dm_target_main/

dm_target_nat_stun            = dm_target_implementation/$(TargetName)/dm_target_nat/stun/

all:	$(CWMP_APPLICATION_NAME)
	
$(CWMP_APPLICATION_NAME):
	mkdir -p $(REP_OBJ)
	echo $(dm_target_com_path)
	(cd dm_com && $(MAKE))
ifeq ($(DBUS_IPC_ENABLE), Y)
	(cd dm_ipc && $(MAKE))
endif
	(cd $(dm_target_common_path) && $(MAKE))	
	(cd $(dm_target_com_path) && $(MAKE))
	(cd $(dm_target_ixml_path) && $(MAKE))
	(cd $(dm_target_deviceAdapter_path) && $(MAKE))
	(cd $(dm_target_dataManagement_path) && $(MAKE))
	(cd dm_engine && $(MAKE))
ifeq ($(STUN_ENABLE), Y)
	($(MAKE) -C $(dm_target_nat_stun) all)
endif
	(cd $(dm_target_main_path) && $(MAKE))

install:
	(cd dm_main && $(MAKE) install)

clean :
	@echo "Cleaning..."
	@(cd dm_com && $(MAKE) $@)
	@(cd dm_ipc && $(MAKE) $@)
	(cd $(dm_target_common_path) && $(MAKE) $@)	
	(cd $(dm_target_com_path) && $(MAKE) $@)
	(cd $(dm_target_ixml_path) && $(MAKE) $@)
	(cd $(dm_target_deviceAdapter_path) && $(MAKE) $@)
	(cd $(dm_target_dataManagement_path) && $(MAKE) $@)
	@(cd dm_engine && $(MAKE) $@)
	@(cd $(dm_target_main_path) && $(MAKE) $@)
ifeq ($(STUN_ENABLE), Y)
	($(MAKE) -C $(dm_target_nat_stun) $@)
endif
	rm -rf $(REP_OBJ)

distclean :
	@(cd dm_com && $(MAKE) $@)
	@(cd dm_ipc && $(MAKE) $@)
	(cd $(dm_target_common_path) && $(MAKE) $@)
	(cd $(dm_target_com_path) && $(MAKE) $@)
	(cd $(dm_target_ixml_path) && $(MAKE) $@)
	(cd $(dm_target_deviceAdapter_path) && $(MAKE) $@)
	(cd $(dm_target_dataManagement_path) && $(MAKE) $@)
	@(cd dm_engine && $(MAKE) $@)
	@(cd $(dm_target_main_path) && $(MAKE) $@)
