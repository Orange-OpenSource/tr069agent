#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <netinet/in.h>  /* for IPPROTO_TCP / IPPROTO_UDP */
#include <ctype.h>
#include <sys/time.h>				/* */
#include <arpa/inet.h>				/* For inet_aton            */
#include <assert.h>				/* */
#include <stdbool.h>				/* Need for the bool type   */
#include <unistd.h>       /*for sleep function*/

#include "DM_GlobalDefs.h"			/* */
#include "DM_ENG_Parameter.h"			/* */
#include "DM_ENG_ParameterData.h"
#include "DM_ENG_ParameterBackInterface.h" /*DM_ENG_ParameterManager_init is defined in this file*/
#include "DM_ENG_ParameterManager.h"   /*DM_ENG_ParameterManager_ related functions are defined in this file (except DM_ENG_ParameterManager_init)*/

int main(int argc, char const *argv[]) {
  char* datapath = "./rsc";
  bool factoryReset = true;
  int res = DM_ENG_ParameterManager_init(datapath, factoryReset);
  if (res == 0) {printf("%s\n", "init success!!!");}
  DM_ENG_Parameter* newParam = malloc(sizeof(DM_ENG_Parameter));
  newParam = DM_ENG_ParameterManager_getFirstParameter();
  printf("%s\n", newParam->name);
  char* parameterurlname = "Device.ManagementServer.URL";
  char* pvalueurl = NULL;
  int i = DM_ENG_ParameterManager_getParameterValue(parameterurlname, &pvalueurl);
  if (i == 0) {
    printf("%s\n", pvalueurl);
  }
  int nbparam = DM_ENG_ParameterData_getNumberOfParameters();
  printf("%d\n", nbparam);
  char* attr[15] = {"DeviceInfo.newParametername", "STRING", "0", "1", "1", "2", "0", "0", "" , "newParameterName", "0", "0", "", "", ""};
  newParam = DM_ENG_newParameter(attr, true);
  DM_ENG_ParameterData_addParameter(newParam);
  nbparam = DM_ENG_ParameterData_getNumberOfParameters();
  printf("%d\n", nbparam);
  char* newParametername = "Device.DeviceInfo.newParametername";
  i = DM_ENG_ParameterManager_getParameterValue(newParametername, &pvalueurl);
  if (i==0) {
    printf("%s\n", pvalueurl);
  }
  DM_ENG_ParameterManager_dataNewValue(newParametername,pvalueurl);
  //DM_ENG_Parameter_setValue();
  //DM_ENG_ParameterManager_dataNewValue(DM_ENG_DEVICE_STATUS_PARAMETER_NAME, DM_ENG_DEVICE_STATUS_UP);
  free(newParam);
  return 0;
}
