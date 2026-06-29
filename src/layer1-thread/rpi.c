#include "rpi.h"
#include "util.h"
#include "mmio_config.h"
#include <stdarg.h>

static char* const  MMIO_BASE = (char*)           CONFIG_MMIO_BASE;

/*********** GPIO CONFIGURATION ********************************/

static char* const GPIO_BASE = (char*)(CONFIG_GPIO_BASE);
static const uint32_t GPFSEL_OFFSETS[6] = {0x00, 0x04, 0x08, 0x0c, 0x10, 0x14};
static const uint32_t GPIO_PUP_PDN_CNTRL_OFFSETS[4] = {0xe4, 0xe8, 0xec, 0xf0};

#define GPFSEL_REG(reg) (*(uint32_t*)(GPIO_BASE + GPFSEL_OFFSETS[reg]))
#define GPIO_PUP_PDN_CNTRL_REG(reg) (*(uint32_t*)(GPIO_BASE + GPIO_PUP_PDN_CNTRL_OFFSETS[reg]))




// function control settings for GPIO pins
static const uint32_t GPIO_INPUT  = 0x00;
static const uint32_t GPIO_OUTPUT = 0x01;
static const uint32_t GPIO_ALTFN0 = 0x04;
static const uint32_t GPIO_ALTFN1 = 0x05;
static const uint32_t GPIO_ALTFN2 = 0x06;
static const uint32_t GPIO_ALTFN3 = 0x07;
static const uint32_t GPIO_ALTFN4 = 0x03;
static const uint32_t GPIO_ALTFN5 = 0x02;

// pup/pdn resistor settings for GPIO pins
static const uint32_t GPIO_NONE = 0x00;
static const uint32_t GPIO_PUP  = 0x01;
static const uint32_t GPIO_PDP  = 0x02;

static void setup_gpio(uint32_t pin, uint32_t setting, uint32_t resistor) {
  uint32_t reg   =  pin / 10;
  uint32_t shift = (pin % 10) * 3;
  uint32_t status = GPFSEL_REG(reg);   // read status
  status &= ~(7u << shift);              // clear bits
  status |=  (setting << shift);         // set bits
  GPFSEL_REG(reg) = status;

  reg   =  pin / 16;
  shift = (pin % 16) * 2;
  status = GPIO_PUP_PDN_CNTRL_REG(reg); // read status
  status &= ~(3u << shift);              // clear bits
  status |=  (resistor << shift);        // set bits
  GPIO_PUP_PDN_CNTRL_REG(reg) = status; // write back
}

/*********** UART CONTROL ************************ ************/

// Use configurable addresses from mmio_config.h
// Supports both RPi4 hardware and QEMU virt machine
static char* const UART0_BASE = (char*)(CONFIG_UART0_BASE);
static char* const UART3_BASE = (char*)(CONFIG_UART3_BASE);

// line_uarts[] maps the each serial line on the RPi hat to the UART that drives it
// currently:
//   * there is no line 0
//   * line 1 (console) is driven by UART0
//   * line 2 (train control) is driven by UART3
static char* const line_uarts[] = { NULL, UART0_BASE, UART3_BASE };


// All UART register offsets and flag definitions now provided by mmio_config.h
#define UART_REG(line, offset) (*(volatile uint32_t*)(line_uarts[line] + offset))

// UART initialization, to be called before other UART functions
// Nothing to do for UART0, for which GPIO is configured during boot process
// For UART3 (line 2 on the RPi hat), we need to configure the GPIO to route
// the uart control and data signals to the GPIO pins expected by the hat
void uart_init() {
#if TARGET_QEMU_VIRT == 0
  // GPIO setup only needed for RPi4 hardware
  setup_gpio(4, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(5, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(6, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(7, GPIO_ALTFN4, GPIO_NONE);
  setup_gpio(14, GPIO_ALTFN0, GPIO_NONE);
  setup_gpio(15, GPIO_ALTFN0, GPIO_NONE);
#endif
  // QEMU virt doesn't need GPIO configuration
}

uint32_t get_CTS(size_t line) {
  return (UART_REG(line, UART_FR) & UART_FR_CTS);
}

volatile uint32_t* get_RIS(uint32_t line) {
  return &UART_REG(line, UART_RIS);
}

volatile uint32_t* get_ICR(size_t line) {
  return &UART_REG(line, UART_ICR);
}

static const uint32_t UARTCLK = 48000000;
void enable_RX_and_TX() {
  // enable things for the console
  
  uint32_t line = 1;
  // enable RTIM
  // UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 6);

  // enable TXIC
  // UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 5);
  
  // enable RXIM
  // UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 4);

  // enable CTSMIM
  UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 1);

  line = 2;
  // Enable RTIM
  // UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 6);
  
  // enable TXIC
  UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 5);
  
  // enable RXIM
  UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 4);

  // enable CTSMIM
  UART_REG(line, UART_IMSC) = UART_REG(line, UART_IMSC) | (1 << 1);
}

void print_digits_int(uint32_t target) {
  for (int i = 31; i >= 0; i--){
    if (target & (0x01 << i)){
          uart_printf(CONSOLE, "1");
      } else {
          uart_printf(CONSOLE, "0");
      }
  }
}


// Configure the line properties (e.g, parity, baud rate) of a UART
// and ensure that it is enabled
void uart_config_and_enable(size_t line, uint32_t baudrate) {
  uint32_t cr_state;
  // to avoid floating point, this computes 64 times the required baud divisor
  uint32_t baud_divisor = (uint32_t)((((uint64_t)UARTCLK)*4)/baudrate);

  // line control registers should not be changed while the UART is enabled, so disable it
  cr_state = UART_REG(line, UART_CR);
  UART_REG(line, UART_CR) = cr_state & ~UART_CR_UARTEN;
  // set the baud rate
  if (line == MARKLIN) {
    // set the line control registers: 8 bit, no parity, 2 stop bit, FIFOs enabled
    UART_REG(line, UART_LCRH) = UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW | UART_LCRH_FEN | UART_LCRH_STP2;
  } else if (line == CONSOLE){
    // set the line control registers: 8 bit, no parity, 1 stop bit, FIFOs enabled
    UART_REG(line, UART_LCRH) = UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW | UART_LCRH_FEN;
  }
  UART_REG(line, UART_IBRD) = baud_divisor >> 6;
  UART_REG(line, UART_FBRD) = baud_divisor & 0x3f;
  // re-enable the UART
  // enable both transmit and receive regardless of previous state
  UART_REG(line, UART_CR) = cr_state | UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}

void uart_config_and_enable_marklin() {
  uint32_t cr_state;
  uint32_t baudrate;
  baudrate = 2400;
  // to avoid floating point, this computes 64 times the required baud divisor
  uint32_t baud_divisor = (uint32_t)((((uint64_t)UARTCLK)*4)/baudrate);

  // line control registers should not be changed while the UART is enabled, so disable it
  cr_state = UART_REG(MARKLIN, UART_CR);
  UART_REG(MARKLIN, UART_CR) = cr_state & ~UART_CR_UARTEN;
  // ========= BEGIN UART_PATCH ==============================================================================
  // = The line control register (LCRH) needs to be set *after* the baud rate registers (IBRD and FBRD).
  // =========================================================================================================
    // set the baud rate
  UART_REG(MARKLIN, UART_IBRD) = baud_divisor >> 6;
  UART_REG(MARKLIN, UART_FBRD) = baud_divisor & 0x3f;
  // set the line control registers: 8 bit, no parity, 2 stop bits, FIFOs enabled
  UART_REG(MARKLIN, UART_LCRH) = UART_LCRH_WLEN_HIGH | UART_LCRH_WLEN_LOW | UART_LCRH_STP2;// | UART_LCRH_FEN;
  // ======== END UART_PATCH =================================================================================
// re-enable the UART
  // enable both transmit and receive regardless of previous state
  UART_REG(MARKLIN, UART_CR) = cr_state | UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
}
unsigned char uart_getc_queue(size_t line){
  if (UART_REG(line, UART_FR) & UART_FR_RXFE) return 0;
  return 1;
}
unsigned char uart_getc_modified(size_t line) {
  unsigned char ch;
  /* wait for data if necessary */
  if (UART_REG(line, UART_FR) & UART_FR_RXFE) return 0;
  ch = UART_REG(line, UART_DR);
  return(ch);
}
unsigned char uart_getc(size_t line) {
  unsigned char ch;
  /* wait for data if necessary */
  while(UART_REG(line, UART_FR) & UART_FR_RXFE);
  ch = UART_REG(line, UART_DR);
  return(ch);
}

int uart_rxc(size_t line) { // Checks if there is data to recieve
  int x = !( UART_REG(line, UART_FR) & UART_FR_RXFE);
  return x;
  // returns 0 if it is empty, returns 1 if it is not empty
}

int uart_cts(size_t line) {
  int x = UART_REG(line, UART_FR) & UART_FR_CTS;
  return x; // returns the CTS value

}

void uart_putc(size_t line, unsigned char c) {
  // QEMU virt: just write directly without checking TXFF
  // On real hardware, we would check: while(UART_REG(line, UART_FR) & UART_FR_TXFF);
  UART_REG(line, UART_DR) = c;
}

void uart_putl(size_t line, const char* buf, size_t blen) {
  uint32_t i;
  for(i=0; i < blen; i++) {
    uart_putc(line, *(buf+i));
  }
}

void uart_puts(size_t line, const char* buf) {
  while (*buf) {
    uart_putc(line, *buf);
    buf++;
  }
}

// printf-style printing, with limited format support
static void uart_format_print (size_t line, char *fmt, va_list va ) {
	char bf[12];
	char ch;

  while ( ( ch = *(fmt++) ) ) {
		if ( ch != '%' )
			uart_putc(line, ch );
		else {
			ch = *(fmt++);
			switch( ch ) {
			case 'u':
				ui2a( va_arg( va, unsigned int ), 10, bf );
				uart_puts( line, bf );
				break;
			case 'd':
				i2a( va_arg( va, int ), bf );
				uart_puts( line, bf );
				break;
			case 'x':
				ui2a( va_arg( va, unsigned int ), 16, bf );
				uart_puts( line, bf );
				break;
			case 's':
				uart_puts( line, va_arg( va, char* ) );
				break;
      case 'b':
        ui2a( va_arg( va, unsigned int ), 2, bf );
        uart_puts( line, bf );
        break;
      case 'c':
				uart_putc( line, ch );
				break;
			case '%':
				uart_putc( line, ch );
				break;
      case '\0':
        return;
			}
		}
	}
}

void uart_printf( size_t line, char *fmt, ... ) {
	va_list va;

	va_start(va,fmt);
	uart_format_print( line, fmt, va );
	va_end(va);
}
