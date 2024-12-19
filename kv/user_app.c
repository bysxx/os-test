#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "kv_store_ioctl.h"

int main() {
    int fd = open("/dev/kv_store", O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return 1;
    }

    struct kv_pair kv;

    // Insert key:value
    kv.key = 42;
    strcpy(kv.value, "Hello, Kernel!");
    if (ioctl(fd, KV_INSERT, &kv) < 0) {
        perror("KV_INSERT failed");
    }

    // Search key:value
    kv.key = 42;
    if (ioctl(fd, KV_SEARCH, &kv) == 0) {
        printf("Found key %u: value = %s\n", kv.key, kv.value);
    } else {
        perror("KV_SEARCH failed");
    }

    close(fd);
    return 0;
}