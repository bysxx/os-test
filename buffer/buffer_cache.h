// buffer_cache.h

#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include <pthread.h>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 1000
#define BUFFER_SIZE 10  // N: Total buffers managed

typedef enum { FIFO, LRU, LFU } ReplacementPolicy;

typedef struct Buffer {
    int block_number;
    char data[BLOCK_SIZE];
    int is_dirty;
    int is_locked;
    int access_count;      // For LFU
    struct Buffer *prev;   // For LRU
    struct Buffer *next;   // For LRU
} Buffer;

typedef struct BufferCache {
    Buffer buffers[BUFFER_SIZE];
    ReplacementPolicy policy;
    pthread_mutex_t lock;
    // Pointers for LRU implementation
    Buffer *lru_head;
    Buffer *lru_tail;
} BufferCache;

void init_buffer_cache(BufferCache *cache, ReplacementPolicy policy);
void destroy_buffer_cache(BufferCache *cache);
Buffer* read_block(BufferCache *cache, int block_number);
void write_block(BufferCache *cache, int block_number, const char *data);
void start_flush_thread(BufferCache *cache);
void stop_flush_thread();
int get_cache_hits();
int get_cache_misses();

#endif // BUFFER_CACHE_H
