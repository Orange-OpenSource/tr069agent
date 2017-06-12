#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char const *argv[]) {
  char* attr[12] = {"DeviceInfo.newParametername", "STRING", "0", "1", "1", "2", "0", "0", "" , "", "0", "0"};

  for (int i=0; i<12; i++) {
      printf("%s\n", attr[i]);
  }
  return 0;
}
