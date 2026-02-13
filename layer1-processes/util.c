#include "util.h"
#include "rpi.h"


// ascii digit to integer
int a2d( char ch ) {
	if( ch >= '0' && ch <= '9' ) return ch - '0';
	if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

// unsigned int to ascii string
void ui2a( unsigned int num, unsigned int base, char *bf ) {
	int n = 0;
	int dgt;
	unsigned int d = 1;

	while( (num / d) >= base ) d *= base;
	while( d != 0 ) {
		dgt = num / d;
		num %= d;
		d /= base;
		if( n || dgt > 0 || d == 0 ) {
			*bf++ = dgt + ( dgt < 10 ? '0' : 'a' - 10 );
			++n;
		}
	}
	*bf = 0;
}

// signed int to ascii string
void i2a( int num, char *bf ) {
	if( num < 0 ) {
		num = -num;
		*bf++ = '-';
	}
	ui2a( num, 10, bf );
}

// define our own memset to avoid SIMD instructions emitted from the compiler
void *memset(void *s, int c, size_t n) {
  for (char* it = (char*)s; n > 0; --n) *it++ = c;
  return s;
}

// define our own memcpy to avoid SIMD instructions emitted from the compiler
void* memcpy(void* restrict dest, const void* restrict src, size_t n) {
    char* sit = (char*)src;
    char* cdest = (char*)dest;
    for (size_t i = 0; i < n; ++i) *(cdest++) = *(sit++);
    return dest;
}

void print_time(long unsigned int time, unsigned int row, unsigned int column, size_t line) {
  // This functions prints the time in int line
  unsigned int ms = time % 10;
  time = time / 10;
  unsigned int s = time % 60;
  time = time / 60;
  unsigned int m = time % 60;
  time = time / 60;
  unsigned int h = time % 100;
  
  uart_printf(line, "\033[%u;%uH",row, column);
  uart_printf(line, "time %u%u%u:%u%u:%u%u.%u", h/100, (h/10)%10, h%10, m/10, m%10, s/10, s%10, ms);
  
  if (s % 4 == 0) uart_printf(line, " -");
  else if (s % 4 == 1) uart_printf(line, " \\");
  else if (s % 4 == 2) uart_printf(line, " |");
  else if (s % 4 == 3) uart_printf(line, " /");
  
  return;
}

// print_mstime(ttemp, -1, CONSOLE);

void print_mstime(long unsigned int time, unsigned int id, size_t line, int offset) {
  int row = 5;
  int col = 65;
  int rstep = 4;
  int cstep = 11;
  uart_printf(line, "\033[33m");
  
  time = time / 100;
  
  uart_printf(line, "\033[%u;%uH", row + (id % 2) * rstep + offset, col + (id / 2) * cstep);
  
  if (time > 99999) {
    uart_printf(line, "9999.9");
  }
  else {
    uart_printf(line, "%u%u%u%u.%u", time / 10000, (time / 1000) % 10, (time / 100) % 10, (time / 10) % 10, time % 10);
  }
  
  uart_printf(line, "\033[0m");

  return;
}

void print_tspeed(int arr[], size_t alen, size_t line) {
  int row = 14;
  int col = 13;
  int cstep = 16;
  int i = 1;
  for (uint32_t j = 0; j < alen; j++) {
    if (arr[j] != 0) {
      int tv = arr[j];
      uart_printf(line, "\033[%u;%uH", row + (i % 3), col + (i / 3) * cstep);
      if (tv == -1) {
        uart_printf(line, "\033[31m%u%u \033[33m R", j/10, j%10);
      }
      else {
        uart_printf(line, "\033[31m%u%u \033[33m%u%u", j/10, j%10, tv/10, tv%10);
      }
      i++;
    }
  }
  while (i < 9) {
  
    uart_printf(line, "\033[%u;%uH", row + (i % 3), col + (i / 3) * cstep);
    uart_printf(line, "     ");
    i++;
  
  }
  uart_printf(line, "\033[0m");
  return;
}

void print_sensors(int arr[], size_t alen, unsigned int column, unsigned int row, size_t line) {
  uart_printf(line, "\033[33m");
  for (uint32_t i = 0; i < alen; i++) {
    if (arr[i] != 0) {
      uart_printf(line, "\033[%u;%uH",row + i, column);
      char ch = 'A';
      uart_putc(line, ch + (arr[i] / 17));
      uart_printf(line, "%d", arr[i]%17);
      uart_putc(line,' ');
    }
  }
  uart_printf(line, "\033[0m");
  
  return;
}


void sensor_push(int sen, int arr[], size_t alen) {
  int temp;
  for (uint32_t i = 0; i < alen; ++i) {
    temp = arr[i];
    arr[i] = sen;
    sen = temp;
  }
  return;
}

void print_switch(int s, char c, size_t line) {
  int row = 4;
  int col = 10;
  int step = 9;
  int row2 = 10;
  int col2 = 19;
  int step2 = 10;
  
  if (c == 'C') {
    uart_printf(line, "\033[36m");
  }
  else {
    uart_printf(line, "\033[32m");
  }
  
  
  if (s > 100) {
    s = s - 153;
    uart_printf(line, "\033[%u;%uH SW %d:",row2 + (s / 2), col2 + step2 * (s % 2), s + 153);
    uart_putc(line, c);
  
  }
  else {
    s = s - 1;
    uart_printf(line, "\033[%u;%uH SW %d:",row + (s / 4), col + step * (s % 4), s + 1);
    uart_putc(line, c);
  
  }
  uart_printf(line, "\033[0m");


}



void print_error(char *error_msg, int r, int c){
  uart_printf(CONSOLE,"\033[%u;%uH", r, c);
  // uart_printf(CONSOLE,"\033[K");  
  uart_printf(CONSOLE,"\033[31m"); // "\033[31m" Set the shit to white
  // print error_msg
  uart_puts(CONSOLE, error_msg);
  
  uart_puts(CONSOLE, "\r\n"); 
  uart_printf(CONSOLE,"\033[37m"); // "\033[37m"
}
void print_in_coulor(int color, char *msg){


}
void print_green(char *msg){
      //  print in green
    uart_printf(CONSOLE, "\033[32m");
    uart_printf(CONSOLE, msg);
    uart_printf(CONSOLE, "\033[37m");
}
/*

Code	Effect
"\033[2J"	Clear the screen.
"\033[H"	Move the cursor to the upper-left corner of the screen.
"\033[r;cH"	Move the cursor to row r, column c. Note that both the rows and columns are indexed starting at 1.
"\033[?25l"	Hide the cursor.
"\033[K"	Delete everything from the cursor to the end of the line.
These control sequences can help make your program's display more lively.

Code	Effect
"\033[0m"	Reset special formatting (such as colour).
"\033[30m"	Black text.
"\033[31m"	Red text.
"\033[32m"	Green text.
"\033[33m"	Yellow text.
"\033[34m"	Blue text.
"\033[35m"	Magenta text.
"\033[36m"	Cyan text.
"\033[37m"	White text.


*/
void print_box(int r1,int c1, int r2, int c2){
  // move cursor to r1,c1
  uart_printf(CONSOLE,"\033[%u;%uH", r1, c1);
  // print the top line of the box on r1 and r2 from c1 to c2
  uart_printf(CONSOLE,"\033[K");
  for (int i = c1; i < c2; i++){
    uart_printf(CONSOLE,"\033[%u;%uH", r1, i);
    uart_putc(CONSOLE, '-');
    uart_printf(CONSOLE,"\033[%u;%uH", r2, i);
    uart_putc(CONSOLE, '-');
  }
  // print the left and right line of the box on c1 and c2 from r1 to r2
  for (int i = r1; i < r2; i++){
    uart_printf(CONSOLE,"\033[%u;%uH", i, c1);
    uart_putc(CONSOLE, '|');
    uart_printf(CONSOLE,"\033[%u;%uH", i, c2);
    uart_putc(CONSOLE, '|');
  }
}
