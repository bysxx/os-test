// buffer_cache.c

#include "buffer_cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

static pthread_t flush_thread_id;
static int flush_thread_running = 0;
static int cache_hits = 0;
static int cache_misses = 0;

void init_buffer_cache(BufferCache *cache, ReplacementPolicy policy) {
    cache->policy = policy;
    pthread_mutex_init(&cache->lock, NULL);
    cache->lru_head = NULL;
    cache->lru_tail = NULL;

    for (int i = 0; i < BUFFER_SIZE; i++) {
        cache->buffers[i].block_number = -1; // Indicates empty buffer
        cache->buffers[i].is_dirty = 0;
        cache->buffers[i].is_locked = 0;
        cache->buffers[i].access_count = 0;
        cache->buffers[i].prev = NULL;
        cache->buffers[i].next = NULL;
    }
}

void destroy_buffer_cache(BufferCache *cache) {
    pthread_mutex_destroy(&cache->lock);
}

static Buffer* find_buffer(BufferCache *cache, int block_number) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        if (cache->buffers[i].block_number == block_number) {
            return &cache->buffers[i];
        }
    }
    return NULL;
}

static void move_to_head(BufferCache *cache, Buffer *buffer) {
    // For LRU policy
    if (cache->lru_head == buffer) {
        return;
    }
    // Remove buffer from its current position
    if (buffer->prev) {
        buffer->prev->next = buffer->next;
    }
    if (buffer->next) {
        buffer->next->prev = buffer->prev;
    }
    if (cache->lru_tail == buffer) {
        cache->lru_tail = buffer->prev;
    }
    // Insert buffer at the head
    buffer->next = cache->lru_head;
    buffer->prev = NULL;
    if (cache->lru_head) {
        cache->lru_head->prev = buffer;
    }
    cache->lru_head = buffer;
    if (cache->lru_tail == NULL) {
        cache->lru_tail = buffer;
    }
}

static Buffer* select_victim(BufferCache *cache) {
    Buffer *victim = NULL;
    switch (cache->policy) {
        case FIFO:
            // For simplicity, FIFO is treated same as LRU in this implementation
            victim = cache->lru_tail;
            break;
        case LRU:
            victim = cache->lru_tail;
            break;
        case LFU:
            // Find buffer with least access_count
            victim = &cache->buffers[0];
            for (int i = 1; i < BUFFER_SIZE; i++) {
                if (cache->buffers[i].access_count < victim->access_count) {
                    victim = &cache->buffers[i];
                }
            }
            break;
    }
    return victim;
}

static void write_back(Buffer *buffer) {
    if (buffer->is_dirty) {
        int fd = open("diskfile", O_WRONLY);
        lseek(fd, buffer->block_number * BLOCK_SIZE, SEEK_SET);
        write(fd, buffer->data, BLOCK_SIZE);
        close(fd);
        buffer->is_dirty = 0;
    }
}

Buffer* read_block(BufferCache *cache, int block_number) {
    pthread_mutex_lock(&cache->lock);
    Buffer *buffer = find_buffer(cache, block_number);
    if (buffer) {
        // Buffer Hit
        cache_hits++;
        buffer->access_count++;
        if (cache->policy == LRU) {
            move_to_head(cache, buffer);
        }
        pthread_mutex_unlock(&cache->lock);
        return buffer;
    } else {
        // Buffer Miss
        cache_misses++;
        // Find a free buffer
        Buffer *victim = NULL;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (cache->buffers[i].block_number == -1) {
                victim = &cache->buffers[i];
                break;
            }
        }
        if (!victim) {
            // No free buffer, select a victim
            victim = select_victim(cache);
            while (victim->is_locked) {
                // Wait or select another victim
                // For simplicity, we'll unlock and wait
                pthread_mutex_unlock(&cache->lock);
                usleep(1000); // Sleep for 1ms
                pthread_mutex_lock(&cache->lock);
                victim = select_victim(cache);
            }
            if (victim->is_dirty) {
                // Write back to disk
                write_back(victim);
            }
            // Remove from LRU list
            if (victim->prev) {
                victim->prev->next = victim->next;
            }
            if (victim->next) {
                victim->next->prev = victim->prev;
            }
            if (cache->lru_head == victim) {
                cache->lru_head = victim->next;
            }
            if (cache->lru_tail == victim) {
                cache->lru_tail = victim->prev;
            }
        }

        // Read from disk
        int fd = open("diskfile", O_RDONLY);
        lseek(fd, block_number * BLOCK_SIZE, SEEK_SET);
        read(fd, victim->data, BLOCK_SIZE);
        close(fd);

        // Update buffer metadata
        victim->block_number = block_number;
        victim->is_dirty = 0;
        victim->access_count = 1;
        victim->is_locked = 0;

        // Add to LRU list
        victim->prev = NULL;
        victim->next = cache->lru_head;
        if (cache->lru_head) {
            cache->lru_head->prev = victim;
        }
        cache->lru_head = victim;
        if (cache->lru_tail == NULL) {
            cache->lru_tail = victim;
        }

        pthread_mutex_unlock(&cache->lock);
        return victim;
    }
}

void write_block(BufferCache *cache, int block_number, const char *data) {
    pthread_mutex_lock(&cache->lock);
    Buffer *buffer = find_buffer(cache, block_number);
    if (!buffer) {
        // Need to allocate a buffer
        Buffer *victim = NULL;
        for (int i = 0; i < BUFFER_SIZE; i++) {
            if (cache->buffers[i].block_number == -1) {
                victim = &cache->buffers[i];
                break;
            }
        }
        if (!victim) {
            victim = select_victim(cache);
            while (victim->is_locked) {
                // Wait or select another victim
                pthread_mutex_unlock(&cache->lock);
                usleep(1000); // Sleep for 1ms
                pthread_mutex_lock(&cache->lock);
                victim = select_victim(cache);
            }
            if (victim->is_dirty) {
                write_back(victim);
            }
            // Remove from LRU list
            if (victim->prev) {
                victim->prev->next = victim->next;
            }
            if (victim->next) {
                victim->next->prev = victim->prev;
            }
            if (cache->lru_head == victim) {
                cache->lru_head = victim->next;
            }
            if (cache->lru_tail == victim) {
                cache->lru_tail = victim->prev;
            }
        }
        buffer = victim;
        buffer->block_number = block_number;
        buffer->access_count = 0;
    }

    // Copy data to buffer
    memcpy(buffer->data, data, BLOCK_SIZE);
    buffer->is_dirty = 1;
    buffer->access_count++;

    if (cache->policy == LRU) {
        move_to_head(cache, buffer);
    }

    pthread_mutex_unlock(&cache->lock);
}

static void* flush_thread_func(void *arg) {
    BufferCache *cache = (BufferCache *)arg;
    while (flush_thread_running) {
        pthread_mutex_lock(&cache->lock);
        for (int i = 0; i < BUFFER_SIZE; i++) {
            Buffer *buffer = &cache->buffers[i];
            if (buffer->is_dirty && !buffer->is_locked) {
                buffer->is_locked = 1; // Lock the buffer
                pthread_mutex_unlock(&cache->lock);

                // Write back to disk
                write_back(buffer);

                pthread_mutex_lock(&cache->lock);
                buffer->is_locked = 0; // Unlock the buffer
            }
        }
        pthread_mutex_unlock(&cache->lock);
        sleep(5); // Flush every 5 seconds
    }
    return NULL;
}

void start_flush_thread(BufferCache *cache) {
    flush_thread_running = 1;
    pthread_create(&flush_thread_id, NULL, flush_thread_func, (void *)cache);
}

void stop_flush_thread() {
    flush_thread_running = 0;
    pthread_join(flush_thread_id, NULL);
}

int get_cache_hits() {
    return cache_hits;
}

int get_cache_misses() {
    return cache_misses;
}
