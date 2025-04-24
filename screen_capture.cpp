#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xdamage.h>
#include <png.h>
#include <vector>
using namespace std;

#define WIDTH 800
#define HEIGHT 600

void save_png(const char* filename, unsigned char* data, int width, int height) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        perror("fopen");
        return;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fclose(fp);
        return;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_write_struct(&png_ptr, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        return;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, width, height,
        8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < height; y++) {
        png_bytep row_pointer = data + y * width * 3;
        png_write_row(png_ptr, row_pointer);
    }

    png_write_end(png_ptr, NULL);

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

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
    Display* dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "Cannot open display\n");
        exit(1);
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

    Damage damage = XDamageCreate(dpy, root, XDamageReportDeltaRectangles);
    int damage_event, damage_error;
    if (!XDamageQueryExtension(dpy, &damage_event, &damage_error)) {
        fprintf(stderr, "XDamage扩展不可用\n");
        XCloseDisplay(dpy);
        return 1;
    }


    XEvent event;
    while (1) {
        fprintf(stderr, "Waiting for damage event...\n");
        XNextEvent(dpy, &event);
        fprintf(stderr, "Received damage event, type: %d damage_event:%d\n", event.type, damage_event);
        if (event.type == damage_event + XDamageNotify) {
            XDamageNotifyEvent* de = (XDamageNotifyEvent*)&event;
            XserverRegion region = XFixesCreateRegion(dpy, NULL, 0);
            XDamageSubtract(dpy, de->damage, None, region);
            int count;
            XRectangle* area = XFixesFetchRegion(dpy, region, &count);
            if (!area) {
                fprintf(stderr, "XFixesFetchRegion失败\n");
                XDamageSubtract(dpy, damage, None, None);
                continue;
            }
            for (int i = 0; i < count; i++) {
                XRectangle rect = area[i];
                fprintf(stderr, "捕获区域: count=%d, x=%d, y=%d, width=%d, height=%d\n", count, rect.x, rect.y, rect.width, rect.height);
                XImage* image = XGetImage(dpy, root, rect.x, rect.y, rect.width, rect.height, AllPlanes, ZPixmap);
                if (!image) {
                    fprintf(stderr, "XGetImage失败\n");
                    XFree(area);
                    continue;
                }
                unsigned char* data = (unsigned char*)malloc(rect.width * rect.height * 3);
                for (int y = 0; y < rect.height; y++) {
                    for (int x = 0; x < rect.width; x++) {
                        long pixel = image->f.get_pixel(image, x, y);
                        data[(y * rect.width + x) * 3 + 0] = (pixel >> 16) & 0xFF;
                        data[(y * rect.width + x) * 3 + 1] = (pixel >> 8) & 0xFF;
                        data[(y * rect.width + x) * 3 + 2] = (pixel >> 0) & 0xFF;
                    }
                }

                struct timeval tv;
                gettimeofday(&tv, NULL);
                char filename[256];
                snprintf(filename, sizeof(filename), "screenshot_%ld%06ld.png", tv.tv_sec, tv.tv_usec);
                save_png(filename, data, rect.width, rect.height);

                free(data);
                XDestroyImage(image);
                XFree(area);
            }
            XFixesDestroyRegion(dpy, region);
            XDamageSubtract(dpy, damage, None, None);
        }
    }

    XCloseDisplay(dpy);
    return 0;
}



