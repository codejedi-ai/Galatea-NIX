#ifndef _util_h_
#define _util_h_ 1

#include <stddef.h>
// Directly Copied From A0 util.h code

// conversions
int a2d(char ch);
char a2i( char ch, char **src, int base, int *nump );
void ui2a( unsigned int num, unsigned int base, char *bf );
void i2a( int num, char *bf );

// memory
void *memset(void *s, int c, size_t n);
void* memcpy(void* restrict dest, const void* restrict src, size_t n);

// helper functions // may be moved to a different file later on // may not be needed
void print_time(long unsigned int time, unsigned int row, unsigned int column, size_t line);
void print_mstime(long unsigned int time, unsigned int id, size_t line, int offset);

// Sensor stuff should be deleted
void sensor_push(int sen, int arr[], size_t alen);
void print_sensors(int arr[], size_t alen, unsigned int column, unsigned int row, size_t line);

void print_switch(int s, char c, size_t line);
void print_tspeed(int arr[], size_t alen, size_t line);
//

// Might 

#endif /* util.h */
