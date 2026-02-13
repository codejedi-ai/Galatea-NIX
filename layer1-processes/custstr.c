#include "custstr.h"
#include "rpi.h"
#include "util.h"
#include <string.h>
int8_t is_empty(char *str){
  return (*str == '\0');
}
int8_t is_hex(char *switch_number){
	if (switch_number[0] == '\0') return 0;
	if (switch_number[1] == '\0') return 0;
	return(switch_number[0] == '0' && switch_number[1] == 'x');
}

int64_t atoi_64(char *str){
    uint8_t is_neg = 0;
    if (str[0] == '-') {
      is_neg = 1;
      str++;
    }
    uint64_t ret = 0; 
    while (*str != '\0') { // loop until the end of the array
        ret = 10 * ret;
        ret += a2d(*str);
        str++; // move to the next character
    }
    return ret;
}

int64_t str_to_hex(char *str){
    uint64_t ret = 0; 
    str++;
    str++;
    while (*str != '\0') { // loop until the end of the array
        ret = 16 * ret;
        ret += a2d(*str);
        str++; // move to the next character
    }
    return ret;
}
// with inplace return again
void strcmp_inpace(int* ret, char* s1, char* s2){
	while(*s1 && *s2){
		if(*s1 != *s2){
			*ret = 0;
      return;
		}
		s1++;
		s2++;
    *ret = (*s1 == *s2);
	}



	*ret = (*s1 == *s2);
}
// with inplace return again
int strcmp_ret(char* s1, char* s2){
	while(*s1 && *s2){
		if(*s1 != *s2){
      return 0;
		}
		s1++;
		s2++;
	}
	return (*s1 == *s2);
}
int strcat_cust(char* dest, const char* src) {
	int newsz = 0;
    while (*dest) {

        dest++;
		newsz++;
    }
    while (*src) {
        *dest = *src;

        dest++;
        src++;
		newsz++;
       
    }
    *dest = '\0';
	return newsz;
}

int parse_char_arr(char *arr, char **num, int num_size){
  char *ptr; // pointer to traverse the array
  int i = 1; // index for the array
  num[0] = arr;
  ptr = arr; // point to the first element of the array
  while (*ptr != '\0') { // loop until the end of the array
    if (*ptr == ' ') { // check if the character is a space
      *ptr = 0;
      
      num[i] = ptr + 1; // store the value in the array
      // // uart_printf(CONSOLE, "num[%d] = %s\r\n", i, num[i]);
      i++;
      if (i >= num_size) {return i;}
      // increment the index
    }
    ptr++; // move to the next character
  }
  // // uart_printf(CONSOLE, "i = %d\r\n", i);
  return i;
}
// this fiunction would
// return strings in place
// dest string, dest str size str and the part in which you want


int cust_strcpy(char *dest, int lenDes, char *src, int lenSrc){
	int i = 0;
	while (*src) {
		if (i >= lenSrc) {break;}
		if (i >= lenDes) {break;}
		*dest = *src;
		dest++;
		src++;
		i++;
	}
	*dest = 0;
	return i;
}

void strflush(char* msg, uint8_t msglen){
    for (int i = 0; i < msglen; i++){
    if (msg[i] != '\0'){
        // print mem address
        // uart_printf(CONSOLE, "strflush: msg[%x] = ", i, msg[i]);
        uart_putc(CONSOLE, msg[i]);
        // uart_printf(CONSOLE, "\r\n");
    }else
        break;
    }
}