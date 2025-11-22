#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include "uart.h"  // optional, for printf redirection

int _write(int fd, const void *buf, int len) {
    const char *data = buf;
    for (int i = 0; i < len; i++) {
        uart_out_char(3, data[i]);
    }
    return len;
}

int _read(int file, char *ptr, int len) {
    return 0;
}

int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat *st) {
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

void *_sbrk(ptrdiff_t incr) {
    return 0;
}

int _kill(int pid, int sig) {
    return -1;
}

int _getpid(void) {
    return 0;
}

void _exit(int status) {
    while (1);
}