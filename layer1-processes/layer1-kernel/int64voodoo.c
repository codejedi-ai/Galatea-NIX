#include "rpi.h"
#include "util.h"
#include "int64voodoo.h"
// turn byint- two integers into int64
void int64_to_byint(uint64_t charint, int *a, int *b){
  *a = charint >> 32;
  *b = charint & 0xFFFFFFFF;
}
// return int64 that encodes two integers
uint64_t byint_to_int64(int a, int b){
  uint64_t ret = a;
  ret = ret << 32;
  ret = ret | b;
  return ret;
}
// an octochar is a char array of length 8 that is encoded as a int64
uint64_t octochar_to_int64(char *str){
  uint64_t ret = *(uint64_t*)str;
  return ret;
}
// return 8 characters in place from a int64
void int64_to_octochar(uint64_t charint, char *str){
  *(uint64_t*)str = charint;
}