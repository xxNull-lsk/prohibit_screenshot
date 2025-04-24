#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xdamage.h>
#include <vector>
using namespace std;
#include <fcntl.h>
#include <sys/mman.h>

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


char* get_window_title(Display* dpy, Window window) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char* prop;

    // Get the _NET_WM_NAME property
    if (XGetWindowProperty(dpy, window, XInternAtom(dpy, "_NET_WM_NAME", False),
        0, (~0L), False, AnyPropertyType,
        &actual_type, &actual_format, &nitems,
        &bytes_after, &prop) == Success && prop != NULL) {
        char* title = strdup((char*)prop);
        XFree(prop);
        return title;
    }

    // Fallback to WM_NAME property if _NET_WM_NAME is not available
    if (XGetWindowProperty(dpy, window, XInternAtom(dpy, "WM_NAME", False),
        0, (~0L), False, AnyPropertyType,
        &actual_type, &actual_format, &nitems,
        &bytes_after, &prop) == Success && prop != NULL) {
        char* title = strdup((char*)prop);
        XFree(prop);
        return title;
    }

    return NULL;
}

vector<Window> getTopLevelWindows(Display* dpy) {

    Atom netClList = XInternAtom(dpy, "_NET_CLIENT_LIST", 1);
    Atom actualType;
    int format;
    unsigned long num, bytes;
    Window* data = 0;
    vector<Window> windows;

    for (int i = 0; i < ScreenCount(dpy); ++i) {
        Window rootWin = RootWindow(dpy, i);

        int status = XGetWindowProperty(dpy, rootWin, netClList, 0L,
            ~0L, 0, AnyPropertyType,
            &actualType, &format, &num,
            &bytes, (unsigned char**)&data);

        if (status != Success) {
            fprintf(stderr, "Failed getting root window properties\n");
            continue;
        }

        for (unsigned long i = 0; i < num; ++i) {
            Window win = data[i];
            char* title = get_window_title(dpy, win);
            if (title) {
                fprintf(stderr, "%d: Window: 0x%lx %ld, Title: %s\n", i + 1, win, win, title);
                XFree(title);
            }
            windows.push_back(win);
        }

        XFree(data);
    }
    return windows;
}


int ListLockWindow()
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
    int count = 0;
    int lock = lock_shm();
    if (shm_data->magic == LOCK_IMAGES_MAGIC) {
        count = shm_data->count;
        for (int i = 0; i < count; i++) {
            fprintf(stderr, "%d: %d %s\n", i, shm_data->data[i], get_window_title(XOpenDisplay(NULL), shm_data->data[i]));
        }
    }
    unlock_shm(lock);
    munmap(shm_data, sizeof(LOCK_IMAGES));
    close(shm_fd);

	return count;
}
int ResetLockWindow()
{
	// 从共享内存中获取加锁的窗口列表
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        fprintf(stderr, "shm_open failed!\n");
        return 0;
    }
    LOCK_IMAGES* shm_data = (LOCK_IMAGES*)mmap(NULL, sizeof(LOCK_IMAGES), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed!\n");
        close(shm_fd);
        return 0;
    }
    int lock = lock_shm();

    int count = 0;
    if (shm_data->magic == LOCK_IMAGES_MAGIC) {
        shm_data->lock_image_id = 0;
        shm_data->count = 0;
        memset(shm_data->data, 0, sizeof(shm_data->data));
    }
    unlock_shm(lock);
    munmap(shm_data, sizeof(LOCK_IMAGES));
    close(shm_fd);

	return count;
}
int ChangedBackground()
{
	// 从共享内存中获取加锁的窗口列表
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        fprintf(stderr, "shm_open failed!\n");
        return 0;
    }
    LOCK_IMAGES* shm_data = (LOCK_IMAGES*)mmap(NULL, sizeof(LOCK_IMAGES), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed!\n");
        close(shm_fd);
        return 0;
    }
    
    int lock = lock_shm();
    if (shm_data->magic == LOCK_IMAGES_MAGIC) {
        shm_data->lock_image_id++;
    }
    unlock_shm(lock);
    munmap(shm_data, sizeof(LOCK_IMAGES));
    close(shm_fd);

	return 1;
}
int LockWindow(Window wid, bool doLock) {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        shm_fd = shm_open(SHM_NAME, O_RDWR|O_CREAT, 0666);
        if (shm_fd == -1) {
            fprintf(stderr, "shm_open failed!\n");
            return 0;
        }
        ftruncate(shm_fd, sizeof(LOCK_IMAGES));
    }

    int lock = lock_shm();
    LOCK_IMAGES* shm_data = (LOCK_IMAGES*)mmap(NULL, sizeof(LOCK_IMAGES), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
        fprintf(stderr, "mmap failed!\n");
        close(shm_fd);
        unlock_shm(lock);
        return 0;
    }
    shm_data->magic = LOCK_IMAGES_MAGIC;
    int ret = 1;
    if (shm_data->magic == LOCK_IMAGES_MAGIC) {
        if (doLock) {
            if (shm_data->count >= MAX_LOCK_IMAGES_SIZE) {
                fprintf(stderr, "LOCK_IMAGES is full\n");
                ret = 0;
            }else{
                shm_data->data[shm_data->count] = wid;
                shm_data->count++;
            }
        }else{
            for (int i = 0; i < shm_data->count; i++) {
                if (shm_data->data[i] == wid) {
                    shm_data->data[i] = shm_data->data[shm_data->count - 1];
                    shm_data->count--;
                    break;
                }
            }
        }
    }
    unlock_shm(lock);
    munmap(shm_data, sizeof(LOCK_IMAGES));
    close(shm_fd);
    return ret;
}

int main(int argc, char* argv[]) {
    bool doLock = false;
    if (argc > 1) {
        if (strcmp(argv[1], "lock") == 0) {
            doLock = true;
        }
        if (strcmp(argv[1], "list") == 0) {
            int count = ListLockWindow();
            fprintf(stderr, "locked window count=%d\n", count);
            return 0;
        }
        if (strcmp(argv[1], "reset") == 0) {
            ResetLockWindow();
            return 0;
        }
        if (strcmp(argv[1], "changed_bg") == 0) {
            ChangedBackground();
            return 0;
        }
    }
    else {
        fprintf(stderr, "Usage: %s lock/unlock/list/reset/changed_bg\n", argv[0]);
        return 1;
    }
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Failed to open display\n");
        return 1;
    }

    Window root = DefaultRootWindow(dpy);
    fprintf(stderr, "0: Screen: 0x%lx %ld 屏幕\n", root, root);
    auto windows = getTopLevelWindows(dpy);
    windows.insert(windows.begin(), root);
    // 从控制台输入窗口ID
    int index = 0;
    printf("请输入窗口Index: ");
    scanf("%d", &index);
    printf("窗口ID有效, index=%d 0x%lx\n", index, windows[index]);
    if (LockWindow(windows[index], doLock)) {
        printf("%s成功\n", doLock ? "锁定窗口" : "解锁窗口");
    }
    else {
        printf("%s失败\n", doLock ? "锁定窗口" : "解锁窗口");
    }


    XCloseDisplay(dpy);
    return 0;
}