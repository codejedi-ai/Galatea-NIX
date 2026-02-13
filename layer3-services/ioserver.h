#include "rpi.h"
void io_TXIC_MARKLIN_server();
void io_RXIC_MARKLIN_server();
void io_CTS_MARKLIN_server();
void io_notifier();

/*
int Getc(int tid, int channel)
returns the next un-returned character from the given channel. 
The first argument is the task id of the appropriate I/O server. 
How communication errors are handled is implementation-dependent. 
Getc() is actually a wrapper for a send to the appropriate server.
Return Value
>=0	new character from the given UART.
-1	tid is not a valid uart server task.
*/
// have the server look out for the most recent interrupt that is the RXIC on the marklin
int Getc(int tid, int channel);
/*
int Putc(int tid, int channel, unsigned char ch)
queues the given character for transmission by the given UART. 
On return the only guarantee is that the character has been queued. 
Whether it has been transmitted or received is not guaranteed. 
How communication errors are handled is implementation-dependent. 
Putc() is actually a wrapper for a send to the appropriate server.
Return Value
0	success.
-1	tid is not a valid uart server task.
*/
// Either the queue is empty or the server needs to wait for the TXIC interrupt to be triggered
int Putc(int tid, int channel, unsigned char ch);
// int Put2c (int tid, int channel, unsigned char ch1, unsigned char ch2);
// DO NOT USE await CTS
int awaitCTS(int tid, int channel, uint8_t val);