// 파일명: kv_store_app.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define DEVICE "/dev/kv_store"

void set_kv_pair(const char* key, const char* value) {
    int fd = open(DEVICE, O_WRONLY);
    if (fd < 0) {
        perror("디바이스 열기에 실패 (쓰기 모드)");
        return;
    }

    char buffer[512];
    snprintf(buffer, sizeof(buffer), "%s=%s", key, value);
    ssize_t ret = write(fd, buffer, strlen(buffer));
    if (ret < 0) {
        perror("디바이스에 쓰기 실패");
    }
    close(fd);
}

void get_value(const char* key) {
    int fd = open(DEVICE, O_RDWR);
    if (fd < 0) {
        perror("디바이스 열기에 실패 (읽기 모드)");
        return;
    }

    // 키를 디바이스에 쓰기
    ssize_t ret = write(fd, key, strlen(key));
    if (ret < 0) {
        perror("디바이스에 키 쓰기 실패");
        close(fd);
        return;
    }

    // 디바이스로부터 값 읽기
    char value[256];
    ret = read(fd, value, sizeof(value));
    if (ret < 0) {
        perror("디바이스로부터 읽기 실패");
    } else if (ret == 0) {
        printf("키 '%s'를 찾지 못함\n", key);
    } else {
        value[ret] = '\0';
        printf("키: %s, 값: %s\n", key, value);
    }

    close(fd);
}

int main() {
    printf("키1 설정: key1 = value1\n");
    set_kv_pair("key1", "value1");

    printf("키2 설정: key2 = value2\n");
    set_kv_pair("key2", "value2");

    printf("키1 가져오기\n");
    get_value("key1");

    printf("키2 가져오기\n");
    get_value("key2");

    printf("존재하지 않는 키3 가져오기\n");
    get_value("key3");

    return 0;
}
