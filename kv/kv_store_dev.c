#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>           // 파일 오퍼레이션 구조체를 위해 필요
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/cdev.h>
#include <linux/slab.h>         // kmalloc과 kfree를 위해 필요
#include <linux/uaccess.h>      // copy_to_user와 copy_from_user를 위해 필요
#include <linux/mutex.h>        // mutex를 위해 필요
#include <linux/string.h>       // 문자열 처리 함수들을 위해 필요
#include <linux/list.h>         // 연결 리스트를 위해 필요

#define KV_STORE_DEV_MAJOR 240         // 주 번호 정의
#define KV_STORE_DEV_NAME "kvstore"    // 디바이스 이름 정의

struct kv_pair {
    char *key;
    char *value;
    struct list_head list;
};

static LIST_HEAD(kv_list);             // 키-값 리스트의 헤드 초기화
static DEFINE_MUTEX(kv_mutex);         // 리스트 접근을 보호하기 위한 뮤텍스

static int kv_open(struct inode *inode, struct file *filp);
static int kv_release(struct inode *inode, struct file *filp);
static ssize_t kv_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t kv_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

static struct file_operations kv_fops = {
    .owner = THIS_MODULE,
    .open = kv_open,
    .release = kv_release,
    .read = kv_read,
    .write = kv_write,
};

static int kv_open(struct inode *inode, struct file *filp) {
    // 특별한 설정이 필요하지 않음
    return 0;
}

static int kv_release(struct inode *inode, struct file *filp) {
    // private_data가 할당되어 있다면 해제
    if (filp->private_data) {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }
    return 0;
}

static ssize_t kv_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos) {
    char *kbuf;
    ssize_t ret = 0;
    char *cmd, *key, *value;
    size_t key_len, value_len;
    struct kv_pair *kvp;
    struct list_head *pos, *q;
    int found = 0;

    // 커널 버퍼 할당
    kbuf = kmalloc(count + 1, GFP_KERNEL);
    if (!kbuf) {
        ret = -ENOMEM;
        goto out;
    }

    // 사용자 공간에서 데이터 복사
    if (copy_from_user(kbuf, buf, count)) {
        ret = -EFAULT;
        goto out_kfree;
    }
    kbuf[count] = '\0'; // 문자열 종료

    // 명령어 파싱
    if (strncmp(kbuf, "SET ", 4) == 0) {
        // SET 명령어 처리
        cmd = kbuf + 4;
        key = cmd;
        value = strchr(cmd, ' ');
        if (!value) {
            ret = -EINVAL;
            goto out_kfree;
        }
        *value = '\0';
        value++;

        // 키와 값에서 개행 문자 제거
        key_len = strlen(key);
        if (key_len > 0 && key[key_len - 1] == '\n')
            key[key_len - 1] = '\0';

        value_len = strlen(value);
        if (value_len > 0 && value[value_len - 1] == '\n')
            value[value_len - 1] = '\0';

        // 키-값 저장
        mutex_lock(&kv_mutex);
        // 키가 이미 존재하는지 확인
        list_for_each(pos, &kv_list) {
            kvp = list_entry(pos, struct kv_pair, list);
            if (strcmp(kvp->key, key) == 0) {
                // 기존 값 업데이트
                kfree(kvp->value);
                kvp->value = kstrdup(value, GFP_KERNEL);
                found = 1;
                break;
            }
        }
        if (!found) {
            // 새로운 키-값 쌍 생성
            kvp = kmalloc(sizeof(struct kv_pair), GFP_KERNEL);
            if (!kvp) {
                mutex_unlock(&kv_mutex);
                ret = -ENOMEM;
                goto out_kfree;
            }
            kvp->key = kstrdup(key, GFP_KERNEL);
            kvp->value = kstrdup(value, GFP_KERNEL);
            if (!kvp->key || !kvp->value) {
                kfree(kvp->key);
                kfree(kvp->value);
                kfree(kvp);
                mutex_unlock(&kv_mutex);
                ret = -ENOMEM;
                goto out_kfree;
            }
            list_add(&kvp->list, &kv_list);
        }
        mutex_unlock(&kv_mutex);
        ret = count;

    } else if (strncmp(kbuf, "GET ", 4) == 0) {
        // GET 명령어 처리
        cmd = kbuf + 4;
        key_len = strlen(cmd);
        if (key_len > 0 && cmd[key_len - 1] == '\n')
            cmd[key_len - 1] = '\0';

        // filp->private_data에 키 저장
        if (filp->private_data) {
            kfree(filp->private_data);
            filp->private_data = NULL;
        }
        filp->private_data = kstrdup(cmd, GFP_KERNEL);
        if (!filp->private_data) {
            ret = -ENOMEM;
            goto out_kfree;
        }
        *f_pos = 0; // 파일 포인터 초기화
        ret = count;

    } else if (strncmp(kbuf, "DEL ", 4) == 0) {
        // DEL 명령어 처리
        cmd = kbuf + 4;
        key_len = strlen(cmd);
        if (key_len > 0 && cmd[key_len - 1] == '\n')
            cmd[key_len - 1] = '\0';

        mutex_lock(&kv_mutex);
        // 키 삭제
        list_for_each_safe(pos, q, &kv_list) {
            kvp = list_entry(pos, struct kv_pair, list);
            if (strcmp(kvp->key, cmd) == 0) {
                list_del(pos);
                kfree(kvp->key);
                kfree(kvp->value);
                kfree(kvp);
                found = 1;
                break;
            }
        }
        mutex_unlock(&kv_mutex);
        ret = count;

    } else {
        // 알 수 없는 명령어
        ret = -EINVAL;
        goto out_kfree;
    }

out_kfree:
    kfree(kbuf);
out:
    return ret;
}

static ssize_t kv_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos) {
    char *key;
    struct kv_pair *kvp;
    struct list_head *pos;
    char *value;
    size_t len;
    ssize_t ret = 0;
    int found = 0;

    // 키가 설정되었는지 확인
    if (!filp->private_data) {
        ret = 0; // EOF
        goto out;
    }

    key = filp->private_data;

    mutex_lock(&kv_mutex);
    list_for_each(pos, &kv_list) {
        kvp = list_entry(pos, struct kv_pair, list);
        if (strcmp(kvp->key, key) == 0) {
            // 키를 찾음
            value = kvp->value;
            len = strlen(value);
            if (*f_pos >= len) {
                // EOF
                ret = 0;
                goto out_unlock;
            }
            if (count > len - *f_pos)
                count = len - *f_pos;

            if (copy_to_user(buf, value + *f_pos, count)) {
                ret = -EFAULT;
                goto out_unlock;
            }
            *f_pos += count;
            ret = count;
            found = 1;
            break;
        }
    }
    mutex_unlock(&kv_mutex);
    if (!found) {
        // 키를 찾지 못함
        ret = 0; // EOF
    }
    // 저장된 키 해제
    kfree(filp->private_data);
    filp->private_data = NULL;
    return ret;

out_unlock:
    mutex_unlock(&kv_mutex);
out:
    return ret;
}

static int __init kv_init(void) {
    int result;

    // 문자 디바이스 등록
    result = register_chrdev(KV_STORE_DEV_MAJOR, KV_STORE_DEV_NAME, &kv_fops);
    if (result < 0) {
        printk(KERN_WARNING "kvstore: can't get major number %d\n", KV_STORE_DEV_MAJOR);
        return result;
    }

    printk(KERN_INFO "kvstore: registered with major number %d\n", KV_STORE_DEV_MAJOR);
    return 0;
}

static void __exit kv_exit(void) {
    struct kv_pair *kvp;
    struct list_head *pos, *q;

    // 키-값 리스트 정리
    mutex_lock(&kv_mutex);
    list_for_each_safe(pos, q, &kv_list) {
        kvp = list_entry(pos, struct kv_pair, list);
        list_del(pos);
        kfree(kvp->key);
        kfree(kvp->value);
        kfree(kvp);
    }
    mutex_unlock(&kv_mutex);

    // 문자 디바이스 해제
    unregister_chrdev(KV_STORE_DEV_MAJOR, KV_STORE_DEV_NAME);

    printk(KERN_INFO "kvstore: unregistered major number %d\n", KV_STORE_DEV_MAJOR);
}

module_init(kv_init);
module_exit(kv_exit);

MODULE_LICENSE("Dual BSD/GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Key-Value Store Character Device Driver");
