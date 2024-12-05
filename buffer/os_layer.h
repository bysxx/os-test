// os_layer.h

#ifndef OS_LAYER_H
#define OS_LAYER_H

#include "buffer_cache.h"

void os_read(int block_number, char *buffer);
void os_write(int block_number, const char *buffer);
void os_init(ReplacementPolicy policy);
void os_destroy();

#endif // OS_LAYER_H
