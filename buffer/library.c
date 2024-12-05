// library.c

#include "library.h"
#include "os_layer.h"

void lib_init(ReplacementPolicy policy) {
    os_init(policy);
}

void lib_destroy() {
    os_destroy();
}

void lib_read(int block_number, char *buffer) {
    os_read(block_number, buffer);
}

void lib_write(int block_number, const char *buffer) {
    os_write(block_number, buffer);
}
