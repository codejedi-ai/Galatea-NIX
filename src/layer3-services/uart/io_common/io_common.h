#ifndef IO_COMMON_H
#define IO_COMMON_H

#include <stdint.h>

#define QUEUELENGTH 100
#define GETC 32
#define PUTC 33
#define CTS 34

struct intFun
{
	uint8_t tid;
	uint8_t type;
	uint8_t channel;
	uint8_t char_ch;
	uint8_t char_ch2;
};

struct fi_list
{
	struct intFun call[QUEUELENGTH];
	int size;
	uint8_t begin;
	uint8_t end;
};

#endif
