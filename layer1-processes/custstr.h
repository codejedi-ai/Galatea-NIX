#include "util.h"
#include "syscall.h"
#include <stdlib.h>
int8_t is_hex(char *switch_number);

int64_t atoi_64(char *str);

int64_t str_to_hex(char *str);

void strcmp_inpace(int* ret, char* s1, char* s2);
// return non-zero if s1 and s2 are equal
int strcmp_ret(char* s1, char* s2);
// this fiunction would
int parse_char_arr(char *arr, char **num, int num_size);


int cust_strcpy(char *dest, int lenDes, char *src, int lenSrc);

void strflush(char* msg, uint8_t msglen);