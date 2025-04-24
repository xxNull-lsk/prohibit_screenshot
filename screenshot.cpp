#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <png.h>
#include <vector>
using namespace std;

#include "capture.h"
#include "save_png.h"

char* get_window_title(Display *dpy, Window window) {
    Atom actual_type;
    int actual_format;
    unsigned long nitems;
    unsigned long bytes_after;
    unsigned char *prop;

    // Get the _NET_WM_NAME property
    if (XGetWindowProperty(dpy, window, XInternAtom(dpy, "_NET_WM_NAME", False),
                           0, (~0L), False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems,
                           &bytes_after, &prop) == Success && prop != NULL) {
        char *title = strdup((char *)prop);
        XFree(prop);
        return title;
    }

    // Fallback to WM_NAME property if _NET_WM_NAME is not available
    if (XGetWindowProperty(dpy, window, XInternAtom(dpy, "WM_NAME", False),
                           0, (~0L), False, AnyPropertyType,
                           &actual_type, &actual_format, &nitems,
                           &bytes_after, &prop) == Success && prop != NULL) {
        char *title = strdup((char *)prop);
        XFree(prop);
        return title;
    }

    return NULL;
}

vector<Window> getTopLevelWindows(Display* dpy)
{

    Atom netClList = XInternAtom(dpy, "_NET_CLIENT_LIST", 1);
    Atom actualType;
    int format;
    unsigned long num, bytes;
    Window *data = 0;
    vector<Window> windows;

    for (int i = 0; i < ScreenCount(dpy); ++i) {
        Window rootWin = RootWindow(dpy, i);

        int status = XGetWindowProperty(dpy, rootWin, netClList, 0L,
                        ~0L, 0, AnyPropertyType,
                        &actualType, &format, &num,
                        &bytes, (unsigned char **)&data);

        if (status != Success) {
            fprintf(stderr, "Failed getting root window properties\n");
            continue;
        }

        for (unsigned long i = 0; i < num; ++i)
        {
            Window win = data[i];
            char* title = get_window_title(dpy, win);
            if (title) {
                fprintf(stderr, "%d: Window: 0x%lx %ld, Title: %s\n", i+1, win, win, title);
                XFree(title);
            }
            windows.push_back(win);
        }

        XFree(data);
    }
    return windows;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [-s|-shm|-shm-pixmap] [output_path]\n", argv[0]);
        return EXIT_FAILURE;
    }
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        return EXIT_FAILURE;
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
    root = windows[index];
    
    int width, height;
    unsigned char* data;
    const char* output_path = "screenshot.png";
    if (strcmp(argv[1], "-s") == 0) {
        output_path = "screenshot_normal.png";
        data = capture_screen_normal(dpy, root, &width, &height);
    }
    else if (strcmp(argv[1], "-shm") == 0) {
        output_path = "screenshot_shm.png";
        data = capture_screen_shm(dpy, root, &width, &height);
    }
    else if (strcmp(argv[1], "-shm-pixmap") == 0) {
        output_path = "screenshot_shm_pixmap.png";
        data = capture_screen_shm_pixmap(dpy, root, &width, &height);
    } else {
        fprintf(stderr, "Usage: %s [-s|-shm|-shm-pixmap] [output_path]\n", argv[0]);
        XCloseDisplay(dpy);
        return EXIT_FAILURE;
    }
    if (!data) {
        XCloseDisplay(dpy);
        return EXIT_FAILURE;
    }
    if (argc > 2) {
        output_path = argv[2];
    }
    save_png(output_path, width, height, data);
    free(data);
    printf("Screenshot saved to %s\n", output_path);
    XCloseDisplay(dpy);
    return EXIT_SUCCESS;
}



