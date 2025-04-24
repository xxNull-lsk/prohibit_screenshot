#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>

#include <sys/mman.h>
#include <fcntl.h>


#define SHM_NAME "/lock_image"
#define LOCK_FILE_NAME "/tmp/lock_image"
#define MAX_LOCK_IMAGES_SIZE 1000
#define LOCK_IMAGES_MAGIC 0x4E2A6D8D
#pragma pack(1)
typedef struct {
    uint32_t magic;  // LOCK_IMAGES_MAGIC
    uint32_t lock_image_id;
    uint32_t ununsed[21];
    uint16_t count;
    uint16_t size;   // MAX_LOCK_IMAGES_SIZE
    uint32_t data[MAX_LOCK_IMAGES_SIZE];
}LOCK_IMAGES;
#pragma pack()

int lock_shm(){
    int fd = open(LOCK_FILE_NAME, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        return -1;
    }

    // 加锁
    struct flock lock;
    lock.l_type = F_WRLCK;  // 写锁
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return -1;
    }
    return fd;
}

int unlock_shm(int fd){
    if (fd == -1) {
        return -1;
    }
    // 解锁
    struct flock lock;
    lock.l_type = F_UNLCK;  // 解锁
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 0;
    if (fcntl(fd, F_SETLKW, &lock) == -1) {
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}


int isProhibitScreenshot()
{
	// 从共享内存中获取加锁的窗口列表
    int shm_fd = shm_open(SHM_NAME, O_RDONLY, 0666);
    if (shm_fd == -1) {
        fprintf(stderr, "shm_open failed!\n");
        return 0;
    }
    LOCK_IMAGES* shm_data = (LOCK_IMAGES*)mmap(NULL, sizeof(LOCK_IMAGES), PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed!\n");
        close(shm_fd);
        return 0;
    }
    
    int lock = lock_shm();

    int count = 0;
    if (shm_data->magic == LOCK_IMAGES_MAGIC) {
        count = shm_data->count;
        for (int i = 0; i < count; i++) {
            fprintf(stderr, "lock image: %d\n", shm_data->data[i]);
        }
    }
    unlock_shm(lock);
    munmap(shm_data, sizeof(LOCK_IMAGES));
    close(shm_fd);

	return count;
}

int main(int argc, char* argv[]) {
    while(1){
        sleep(1);
        if (isProhibitScreenshot()){
            printf("prohibit screenshot\n");
        }else{
            printf("allow screenshot\n");
        }
    }
    return 0;
}