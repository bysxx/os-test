#ifndef KV_STORE_IOCTL_H
#define KV_STORE_IOCTL_H

#include <linux/ioctl.h>

#define KV_IOCTL_MAGIC 'K'

#define KV_INSERT _IOW(KV_IOCTL_MAGIC, 1, struct kv_pair)
#define KV_SEARCH _IOR(KV_IOCTL_MAGIC, 2, struct kv_pair)

struct kv_pair {
    unsigned int key;
    char value[20];
};

#endif // KV_STORE_IOCTL_H