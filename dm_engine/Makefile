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

SOURCES = src/DM_ENG_Common.c \
	  src/DM_ENG_DeviceIdStruct.c \
	  src/DM_ENG_TransferRequest.c \
	  src/DM_ENG_AllQueuedTransferStruct.c \
	  src/DM_ENG_TransferResultStruct.c \
	  src/DM_ENG_EntityType.c \
	  src/DM_ENG_EventStruct.c \
	  src/DM_ENG_InformMessageScheduler.c \
	  src/DM_ENG_NotificationInterface.c \
	  src/DM_ENG_ParameterAttributesStruct.c \
	  src/DM_ENG_Parameter.c \
	  src/DM_ENG_ParameterInfoStruct.c \
	  src/DM_ENG_ParameterManager.c \
	  src/DM_ENG_ParameterType.c \
	  src/DM_ENG_ParameterValueStruct.c \
	  src/DM_ENG_DownloadRequest.c \
	  src/DM_ENG_DiagnosticsLauncher.c \
	  src/DM_ENG_RPCInterface.c \
	  src/DM_ENG_SetParameterValuesFault.c \
	  src/DM_ENG_SystemNotificationStruct.c \
	  src/DM_ENG_TransferCompleteStruct.c \
	  src/DM_ENG_SampleDataStruct.c \
	  src/DM_ENG_StatisticsModule.c \
	  src/DM_ENG_ComputedExp.c \
	  src/DM_ENG_DataModelConfiguration.c

OBJETS  = $(REP_OBJ)/DM_ENG_Common.o \
	  $(REP_OBJ)/DM_ENG_DeviceIdStruct.o \
	  $(REP_OBJ)/DM_ENG_TransferRequest.o \
	  $(REP_OBJ)/DM_ENG_AllQueuedTransferStruct.o \
	  $(REP_OBJ)/DM_ENG_TransferResultStruct.o \
	  $(REP_OBJ)/DM_ENG_EntityType.o \
	  $(REP_OBJ)/DM_ENG_EventStruct.o \
	  $(REP_OBJ)/DM_ENG_InformMessageScheduler.o \
	  $(REP_OBJ)/DM_ENG_NotificationInterface.o \
	  $(REP_OBJ)/DM_ENG_ParameterAttributesStruct.o \
	  $(REP_OBJ)/DM_ENG_Parameter.o \
	  $(REP_OBJ)/DM_ENG_ParameterInfoStruct.o \
	  $(REP_OBJ)/DM_ENG_ParameterManager.o \
	  $(REP_OBJ)/DM_ENG_ParameterType.o \
	  $(REP_OBJ)/DM_ENG_ParameterValueStruct.o \
	  $(REP_OBJ)/DM_ENG_DownloadRequest.o \
	  $(REP_OBJ)/DM_ENG_DiagnosticsLauncher.o \
	  $(REP_OBJ)/DM_ENG_RPCInterface.o \
	  $(REP_OBJ)/DM_ENG_SetParameterValuesFault.o \
	  $(REP_OBJ)/DM_ENG_SystemNotificationStruct.o \
	  $(REP_OBJ)/DM_ENG_TransferCompleteStruct.o \
	  $(REP_OBJ)/DM_ENG_SampleDataStruct.o \
	  $(REP_OBJ)/DM_ENG_StatisticsModule.o \
	  $(REP_OBJ)/DM_ENG_ComputedExp.o \
	  $(REP_OBJ)/DM_ENG_DataModelConfiguration.o

ifeq ($(DEBUG_COM_FLAG),1)
	CFLAGS+= -DTESTU_COM_FLAG -g
	LIBS += -L$(FTX_ROOT_SRC_DIR)/$(FTX_PKG_PREFIX_DIR)/lib -L$(FTX_ROOT_SRC_DIR)/$(FTX_PKG_PREFIX_DIR)/usr/lib
endif

all: $(OBJETS)


$(REP_OBJ)/DM_ENG_Common.o: src/DM_ENG_Common.c
	$(CC) -o $(REP_OBJ)/DM_ENG_Common.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_Common.c

$(REP_OBJ)/DM_ENG_DeviceIdStruct.o: src/DM_ENG_DeviceIdStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_DeviceIdStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_DeviceIdStruct.c
	
$(REP_OBJ)/DM_ENG_TransferRequest.o: src/DM_ENG_TransferRequest.c
	$(CC) -o $(REP_OBJ)/DM_ENG_TransferRequest.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_TransferRequest.c
	
$(REP_OBJ)/DM_ENG_AllQueuedTransferStruct.o: src/DM_ENG_AllQueuedTransferStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_AllQueuedTransferStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_AllQueuedTransferStruct.c
	
$(REP_OBJ)/DM_ENG_TransferResultStruct.o: src/DM_ENG_TransferResultStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_TransferResultStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_TransferResultStruct.c
	
$(REP_OBJ)/DM_ENG_EntityType.o: src/DM_ENG_EntityType.c
	$(CC) -o $(REP_OBJ)/DM_ENG_EntityType.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_EntityType.c
	
$(REP_OBJ)/DM_ENG_EventStruct.o: src/DM_ENG_EventStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_EventStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_EventStruct.c
	
$(REP_OBJ)/DM_ENG_InformMessageScheduler.o: src/DM_ENG_InformMessageScheduler.c
	$(CC) -o $(REP_OBJ)/DM_ENG_InformMessageScheduler.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_InformMessageScheduler.c
	
$(REP_OBJ)/DM_ENG_NotificationInterface.o: src/DM_ENG_NotificationInterface.c
	$(CC) -o $(REP_OBJ)/DM_ENG_NotificationInterface.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_NotificationInterface.c

$(REP_OBJ)/DM_ENG_ParameterAttributesStruct.o: src/DM_ENG_ParameterAttributesStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_ParameterAttributesStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_ParameterAttributesStruct.c

$(REP_OBJ)/DM_ENG_Parameter.o: src/DM_ENG_Parameter.c
	$(CC) -o $(REP_OBJ)/DM_ENG_Parameter.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_Parameter.c

$(REP_OBJ)/DM_ENG_ParameterInfoStruct.o: src/DM_ENG_ParameterInfoStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_ParameterInfoStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_ParameterInfoStruct.c

$(REP_OBJ)/DM_ENG_ParameterManager.o: src/DM_ENG_ParameterManager.c
	$(CC) -o $(REP_OBJ)/DM_ENG_ParameterManager.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_ParameterManager.c

$(REP_OBJ)/DM_ENG_ParameterType.o: src/DM_ENG_ParameterType.c
	$(CC) -o $(REP_OBJ)/DM_ENG_ParameterType.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_ParameterType.c

$(REP_OBJ)/DM_ENG_ParameterValueStruct.o: src/DM_ENG_ParameterValueStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_ParameterValueStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_ParameterValueStruct.c

$(REP_OBJ)/DM_ENG_DownloadRequest.o: src/DM_ENG_DownloadRequest.c
	$(CC) -o $(REP_OBJ)/DM_ENG_DownloadRequest.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_DownloadRequest.c

$(REP_OBJ)/DM_ENG_DiagnosticsLauncher.o: src/DM_ENG_DiagnosticsLauncher.c
	$(CC) -o $(REP_OBJ)/DM_ENG_DiagnosticsLauncher.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_DiagnosticsLauncher.c

$(REP_OBJ)/DM_ENG_RPCInterface.o: src/DM_ENG_RPCInterface.c
	$(CC) -o $(REP_OBJ)/DM_ENG_RPCInterface.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_RPCInterface.c

$(REP_OBJ)/DM_ENG_SetParameterValuesFault.o: src/DM_ENG_SetParameterValuesFault.c
	$(CC) -o $(REP_OBJ)/DM_ENG_SetParameterValuesFault.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_SetParameterValuesFault.c

$(REP_OBJ)/DM_ENG_SystemNotificationStruct.o: src/DM_ENG_SystemNotificationStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_SystemNotificationStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_SystemNotificationStruct.c

$(REP_OBJ)/DM_ENG_TransferCompleteStruct.o: src/DM_ENG_TransferCompleteStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_TransferCompleteStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_TransferCompleteStruct.c

$(REP_OBJ)/DM_ENG_SampleDataStruct.o: src/DM_ENG_SampleDataStruct.c
	$(CC) -o $(REP_OBJ)/DM_ENG_SampleDataStruct.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_SampleDataStruct.c

$(REP_OBJ)/DM_ENG_StatisticsModule.o: src/DM_ENG_StatisticsModule.c
	$(CC) -o $(REP_OBJ)/DM_ENG_StatisticsModule.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_StatisticsModule.c

$(REP_OBJ)/DM_ENG_ComputedExp.o: src/DM_ENG_ComputedExp.c
	$(CC) -o $(REP_OBJ)/DM_ENG_ComputedExp.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_ComputedExp.c

$(REP_OBJ)/DM_ENG_DataModelConfiguration.o: src/DM_ENG_DataModelConfiguration.c
	$(CC) -o $(REP_OBJ)/DM_ENG_DataModelConfiguration.o $(CWMP_C_FLAGS) $(CWMP_CPP_FLAGS) $(INCS) -c src/DM_ENG_DataModelConfiguration.c

clean:
	rm -f $(REP_OBJ)/*.o
