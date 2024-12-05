// os_layer.c

#include "os_layer.h"
#include <string.h>

static BufferCache cache;

void os_init(ReplacementPolicy policy) {
    init_buffer_cache(&cache, policy);
    start_flush_thread(&cache);
}

void os_destroy() {
    stop_flush_thread();
    destroy_buffer_cache(&cache);
}

void os_read(int block_number, char *buffer) {
    Buffer *buf = read_block(&cache, block_number);
    memcpy(buffer, buf->data, BLOCK_SIZE);
}

void os_write(int block_number, const char *buffer) {
    write_block(&cache, block_number, buffer);
}
