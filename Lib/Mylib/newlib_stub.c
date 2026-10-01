#include <sys/stat.h>
#include "common_defs.h"

__attribute__((weak)) int _getpid_r(void) {
  return 1;
}

__attribute__((weak)) void _kill_r(int UNUSED(pid)) { 
    while(1) ; 
    }

__attribute__((weak)) int _isatty(int UNUSED(fd)){
    return 0;
}
 
__attribute__((weak)) int _close(int UNUSED(fd)){
    return -1;
}
 
__attribute__((weak)) int _lseek(int UNUSED(fd), int UNUSED(ptr), int UNUSED(dir)){
    return -1;
}
 
__attribute__((weak)) int _fstat(int UNUSED(fd), struct stat *UNUSED(st)){
    return 0;
}

__attribute__((weak)) int _read(int UNUSED(file), char *UNUSED(ptr), int UNUSED(len)){
    return -1;
}
 
__attribute__((weak)) int _write(int UNUSED(file), char *UNUSED(ptr), int UNUSED(len)){
    return -1;
}

void __attribute__ ((weak)) _init(void)  {}
