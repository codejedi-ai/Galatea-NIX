// Newlib stubs for bare-metal environment
// These functions are required by newlib but not needed in our kernel

#include <sys/stat.h>
#include <sys/types.h>

// Define errno for bare-metal environment
int errno = 0;

// Define errno constants
#define EBADF  9   // Bad file number
#define EINVAL 22  // Invalid argument  
#define ENOMEM 12  // Out of memory

// Define stat mode constants if not available
#ifndef S_IFCHR
#define S_IFCHR 0x2000  // Character device
#endif

// Close a file - not implemented
int _close(int file) {
    (void)file;
    errno = EBADF;
    return -1;
}

// Query file status - not implemented
int _fstat(int file, struct stat *st) {
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

// Get process ID - return dummy value
int _getpid(void) {
    return 1;
}

// Query whether a file descriptor is a terminal - always return true for stdout/stderr
int _isatty(int file) {
    (void)file;
    return 1;
}

// Send a signal - not implemented
int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

// Seek to position in file - not implemented
int _lseek(int file, int offset, int whence) {
    (void)file;
    (void)offset;
    (void)whence;
    return 0;
}

// Read from file - not implemented
int _read(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

// Increase program data space - not implemented (kernel overrides malloc/free)
void *_sbrk(int incr) {
    (void)incr;
    errno = ENOMEM;
    return (void *)-1;
}

// Write to file - not implemented (using custom UART functions)
int _write(int file, char *ptr, int len) {
    (void)file;
    (void)ptr;
    (void)len;
    return len;
}

// Exit program - infinite loop
void _exit(int status) {
    (void)status;
    while (1) {
        // Infinite loop
    }
}

// Stub for printf - not implemented (use uart_printf instead)
int printf(const char *format, ...) {
    (void)format;
    return 0;
}
