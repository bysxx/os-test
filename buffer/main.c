// main.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "library.h"
#include "buffer_cache.h"

#define TEST_BLOCKS 10000

void generate_access_sequence(int *sequence, int size, int pattern) {
    if (pattern == 0) {
        // Sequential access
        for (int i = 0; i < size; i++) {
            sequence[i] = i % TOTAL_BLOCKS;
        }
    } else if (pattern == 1) {
        // Random access
        for (int i = 0; i < size; i++) {
            sequence[i] = rand() % TOTAL_BLOCKS;
        }
    } else if (pattern == 2) {
        // Zipfian distribution
        double sum = 0.0;
        double c = 1.0;
        for (int i = 1; i <= TOTAL_BLOCKS; i++) {
            sum += 1.0 / (i * i);
        }
        double k = c / sum;
        for (int i = 0; i < size; i++) {
            double rnd = (double)rand() / RAND_MAX;
            double cumulative = 0.0;
            for (int j = 1; j <= TOTAL_BLOCKS; j++) {
                cumulative += k / (j * j);
                if (rnd <= cumulative) {
                    sequence[i] = j - 1;
                    break;
                }
            }
        }
    }
}

int main() {
    int access_sequence[TEST_BLOCKS];
    char buffer[BLOCK_SIZE];
    int policy_option;
    int pattern_option;

    srand(time(NULL));

    printf("Select Replacement Policy (0: FIFO, 1: LRU, 2: LFU): ");
    scanf("%d", &policy_option);

    printf("Select Access Pattern (0: Sequential, 1: Random, 2: Zipfian): ");
    scanf("%d", &pattern_option);

    ReplacementPolicy policy = FIFO;
    if (policy_option == 1) {
        policy = LRU;
    } else if (policy_option == 2) {
        policy = LFU;
    }

    lib_init(policy);

    // Generate a block access sequence
    generate_access_sequence(access_sequence, TEST_BLOCKS, pattern_option);

    for (int i = 0; i < TEST_BLOCKS; i++) {
        int block_number = access_sequence[i];
        memset(buffer, 0, BLOCK_SIZE);
        lib_read(block_number, buffer);
    }

    int hits = get_cache_hits();
    int misses = get_cache_misses();
    double hit_rate = (double)hits / (hits + misses) * 100.0;

    printf("Cache Hits: %d\n", hits);
    printf("Cache Misses: %d\n", misses);
    printf("Hit Rate: %.2f%%\n", hit_rate);

    lib_destroy();
    return 0;
}
