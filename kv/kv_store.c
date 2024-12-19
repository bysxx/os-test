#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cdev.h>
#include "kv_store_ioctl.h"

#define DEVICE_NAME "kv_store"
#define HASH_TABLE_SIZE 256

struct kv_entry {
    unsigned int key;
    char value[20];
    struct kv_entry *next;
};

static struct kv_entry *hash_table[HASH_TABLE_SIZE];
static spinlock_t kv_lock;
static struct cdev kv_cdev;
static dev_t kv_dev;

// Hash function
static unsigned int hash_key(unsigned int key) {
    return key % HASH_TABLE_SIZE;
}

// Insert key:value pair
static int insert_kv(unsigned int key, const char *value) {
    unsigned int index = hash_key(key);
    struct kv_entry *entry, *new_entry;

    spin_lock(&kv_lock);

    entry = hash_table[index];
    while (entry) {
        if (entry->key == key) {
            strncpy(entry->value, value, sizeof(entry->value) - 1);
            entry->value[sizeof(entry->value) - 1] = '\0';
            spin_unlock(&kv_lock);
            return 0;
        }
        entry = entry->next;
    }

    new_entry = kmalloc(sizeof(*new_entry), GFP_KERNEL);
    if (!new_entry) {
        spin_unlock(&kv_lock);
        return -ENOMEM;
    }

    new_entry->key = key;
    strncpy(new_entry->value, value, sizeof(new_entry->value) - 1);
    new_entry->value[sizeof(new_entry->value) - 1] = '\0';
    new_entry->next = hash_table[index];
    hash_table[index] = new_entry;

    spin_unlock(&kv_lock);
    return 0;
}

// Search key:value pair
static int search_kv(unsigned int key, char *value) {
    unsigned int index = hash_key(key);
    struct kv_entry *entry;

    spin_lock(&kv_lock);

    entry = hash_table[index];
    while (entry) {
        if (entry->key == key) {
            strncpy(value, entry->value, sizeof(entry->value));
            spin_unlock(&kv_lock);
            return 0;
        }
        entry = entry->next;
    }

    spin_unlock(&kv_lock);
    return -ENOENT;
}

// Ioctl handler
static long kv_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct kv_pair kv;
    int ret;

    switch (cmd) {
    case KV_INSERT:
        if (copy_from_user(&kv, (void __user *)arg, sizeof(kv)))
            return -EFAULT;
        return insert_kv(kv.key, kv.value);

    case KV_SEARCH:
        if (copy_from_user(&kv, (void __user *)arg, sizeof(kv)))
            return -EFAULT;
        ret = search_kv(kv.key, kv.value);
        if (ret)
            return ret;
        if (copy_to_user((void __user *)arg, &kv, sizeof(kv)))
            return -EFAULT;
        return 0;

    default:
        return -EINVAL;
    }
}

// File operations
static struct file_operations kv_fops = {
    .unlocked_ioctl = kv_ioctl,
    .owner = THIS_MODULE,
};

// Module initialization
static int __init kv_init(void) {
    int ret;

    spin_lock_init(&kv_lock);

    ret = alloc_chrdev_region(&kv_dev, 0, 1, DEVICE_NAME);
    if (ret)
        return ret;

    cdev_init(&kv_cdev, &kv_fops);
    ret = cdev_add(&kv_cdev, kv_dev, 1);
    if (ret) {
        unregister_chrdev_region(kv_dev, 1);
        return ret;
    }

    pr_info("kv_store: Device initialized\n");
    return 0;
}

// Module cleanup
static void __exit kv_exit(void) {
    int i;
    struct kv_entry *entry, *tmp;

    cdev_del(&kv_cdev);
    unregister_chrdev_region(kv_dev, 1);

    for (i = 0; i < HASH_TABLE_SIZE; i++) {
        entry = hash_table[i];
        while (entry) {
            tmp = entry;
            entry = entry->next;
            kfree(tmp);
        }
    }

    pr_info("kv_store: Device removed\n");
}

module_init(kv_init);
module_exit(kv_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Key:Value Store using Char Device Driver");
