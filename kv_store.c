// 파일명: kv_store.c

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/slab.h>       // kmalloc 및 kfree를 위해
#include <linux/uaccess.h>    // copy_to_user 및 copy_from_user를 위해
#include <linux/device.h>     // class_create, device_create 등을 위해
#include <linux/mutex.h>      // 동기화를 위한 mutex 사용 (선택 사항)

#define DEVICE_NAME "kv_store"
#define CLASS_NAME "kv"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("키-값 저장소 문자 디바이스 드라이버");
MODULE_VERSION("0.1");

static int majorNumber;
static struct class* kvClass = NULL;
static struct device* kvDevice = NULL;

// 키-값 쌍 구조체
typedef struct kv_pair {
    char key[256];
    char value[256];
    struct kv_pair* next;
} kv_pair_t;

static kv_pair_t* head = NULL; // 링크드 리스트의 헤드

// 동기화를 위한 mutex (선택 사항)
static DEFINE_MUTEX(kv_mutex);

// 함수 프로토타입
static int dev_open(struct inode*, struct file*);
static int dev_release(struct inode*, struct file*);
static ssize_t dev_read(struct file*, char*, size_t, loff_t*);
static ssize_t dev_write(struct file*, const char*, size_t, loff_t*);

// 파일 연산 구조체
static struct file_operations fops =
{
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

// 디바이스 열기
static int dev_open(struct inode* inodep, struct file* filep){
    printk(KERN_INFO "kv_store: 디바이스 열림\n");
    return 0;
}

// 디바이스 닫기
static int dev_release(struct inode* inodep, struct file* filep){
    printk(KERN_INFO "kv_store: 디바이스 닫힘\n");
    return 0;
}

// 디바이스 읽기 (키로 값 가져오기)
static ssize_t dev_read(struct file* filep, char* buffer, size_t len, loff_t* offset){
    char key[256];
    kv_pair_t* curr = head;
    ssize_t bytes_read = 0;

    // 사용자 공간에서 키 복사
    if (len >= 256) {
        printk(KERN_ALERT "kv_store: 키 길이가 너무 깁니다\n");
        return -EINVAL;
    }

    if (copy_from_user(key, buffer, len)){
        printk(KERN_ALERT "kv_store: 사용자로부터 키를 가져오는데 실패\n");
        return -EFAULT;
    }
    key[len] = '\0';

    // 동기화 시작
    mutex_lock(&kv_mutex);

    // 키 검색
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            size_t value_len = strlen(curr->value);

            if (copy_to_user(buffer, curr->value, value_len)) {
                printk(KERN_ALERT "kv_store: 사용자에게 값을 보내는데 실패\n");
                mutex_unlock(&kv_mutex);
                return -EFAULT;
            }
            bytes_read = value_len;
            printk(KERN_INFO "kv_store: 키 '%s', 값 '%s' 읽음\n", key, curr->value);

            // 동기화 종료
            mutex_unlock(&kv_mutex);
            return bytes_read;
        }
        curr = curr->next;
    }

    // 동기화 종료
    mutex_unlock(&kv_mutex);

    // 키를 찾지 못함
    printk(KERN_INFO "kv_store: 키 '%s'를 찾지 못함\n", key);
    return 0;
}

// 디바이스 쓰기 (키-값 쌍 설정)
static ssize_t dev_write(struct file* filep, const char* buffer, size_t len, loff_t* offset){
    char input[512];
    char key[256];
    char value[256];
    kv_pair_t* curr = head;
    kv_pair_t* new_pair;

    if (len >= 512) {
        printk(KERN_ALERT "kv_store: 입력이 너무 깁니다\n");
        return -EINVAL;
    }

    // 사용자 공간에서 입력 복사
    if (copy_from_user(input, buffer, len)){
        printk(KERN_ALERT "kv_store: 사용자로부터 데이터를 가져오는데 실패\n");
        return -EFAULT;
    }
    input[len] = '\0';

    // 입력을 "key=value"로 파싱
    char* delim_pos = strchr(input, '=');
    if (delim_pos == NULL) {
        printk(KERN_ALERT "kv_store: 잘못된 입력 형식, 'key=value' 형식이어야 합니다\n");
        return -EINVAL;
    }

    size_t key_len = delim_pos - input;
    size_t value_len = len - key_len - 1;

    if (key_len >= 256 || value_len >= 256) {
        printk(KERN_ALERT "kv_store: 키 또는 값이 너무 깁니다\n");
        return -EINVAL;
    }

    strncpy(key, input, key_len);
    key[key_len] = '\0';
    strncpy(value, delim_pos + 1, value_len);
    value[value_len] = '\0';

    // 동기화 시작
    mutex_lock(&kv_mutex);

    // 키가 이미 존재하는지 확인
    while (curr != NULL) {
        if (strcmp(curr->key, key) == 0) {
            // 값 업데이트
            strcpy(curr->value, value);
            printk(KERN_INFO "kv_store: 키 '%s'를 값 '%s'로 업데이트\n", key, value);

            // 동기화 종료
            mutex_unlock(&kv_mutex);
            return len;
        }
        curr = curr->next;
    }

    // 키를 찾지 못함, 새로운 키-값 쌍 생성
    new_pair = kmalloc(sizeof(kv_pair_t), GFP_KERNEL);
    if (!new_pair) {
        printk(KERN_ALERT "kv_store: 메모리 할당에 실패\n");
        mutex_unlock(&kv_mutex);
        return -ENOMEM;
    }
    strcpy(new_pair->key, key);
    strcpy(new_pair->value, value);
    new_pair->next = head;
    head = new_pair;

    printk(KERN_INFO "kv_store: 키 '%s'와 값 '%s' 추가\n", key, value);

    // 동기화 종료
    mutex_unlock(&kv_mutex);
    return len;
}

// 모듈 초기화
static int __init kv_store_init(void){
    printk(KERN_INFO "kv_store: LKM 초기화 중\n");

    // 동적으로 주 번호 할당 시도
    majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if (majorNumber<0){
        printk(KERN_ALERT "kv_store: 주 번호 등록에 실패\n");
        return majorNumber;
    }
    printk(KERN_INFO "kv_store: 주 번호 %d로 등록됨\n", majorNumber);

    // 디바이스 클래스 등록
    kvClass = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(kvClass)){                // 오류 체크 및 정리
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "kv_store: 디바이스 클래스 등록에 실패\n");
        return PTR_ERR(kvClass);          // 포인터에서 오류 반환
    }
    printk(KERN_INFO "kv_store: 디바이스 클래스가 정상적으로 등록됨\n");

    // 디바이스 드라이버 등록
    kvDevice = device_create(kvClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
    if (IS_ERR(kvDevice)){               // 오류 발생 시 정리
        class_destroy(kvClass);
        unregister_chrdev(majorNumber, DEVICE_NAME);
        printk(KERN_ALERT "kv_store: 디바이스 생성에 실패\n");
        return PTR_ERR(kvDevice);
    }
    printk(KERN_INFO "kv_store: 디바이스가 정상적으로 생성됨\n"); // 성공적으로 초기화됨

    // mutex 초기화
    mutex_init(&kv_mutex);

    return 0;
}

// 모듈 종료
static void __exit kv_store_exit(void){
    kv_pair_t* curr = head;
    kv_pair_t* tmp;

    // 링크드 리스트 정리
    while (curr != NULL) {
        tmp = curr;
        curr = curr->next;
        kfree(tmp);
    }

    // mutex 해제
    mutex_destroy(&kv_mutex);

    device_destroy(kvClass, MKDEV(majorNumber, 0));     // 디바이스 제거
    class_unregister(kvClass);                          // 디바이스 클래스 등록 해제
    class_destroy(kvClass);                             // 디바이스 클래스 제거
    unregister_chrdev(majorNumber, DEVICE_NAME);        // 주 번호 등록 해제
    printk(KERN_INFO "kv_store: LKM 종료\n");
}

module_init(kv_store_init);
module_exit(kv_store_exit);
