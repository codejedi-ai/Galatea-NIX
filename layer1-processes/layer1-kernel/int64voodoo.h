#include "rpi.h"
#include "util.h"

/*
I found out that int64 takes up 64 bits whih is 8 bytes.
What is even more dissapointing is that when passing such into parameters, the compiler
would pass in a 64bit integer no matter what. 
I find it very wasteful to use 64 bits to pass int parameters of characters

Thus I made different datatypes
byint - two integers that are 4 bytes each encoded into 8 bytes
octochar - 8 characters encoded into 8 bytes, this is a string of 7 chars + escape

An octochar is 8 characters that is to be returned inplace from a int64
One can even make a string by initiating an array of 9 characters and setting the last one to 0

*/
// turn byint- two integers into int64
void int64_to_byint(uint64_t byint_charint, int *a, int *b);
// return int64 that encodes two integers
uint64_t byint_to_int64(int a, int b);
// an octochar is a char array of length 8 that is encoded as a int64
uint64_t octochar_to_int64(char *str);
// return 8 characters in place from a int64
void int64_to_octochar(uint64_t charint, char *str);