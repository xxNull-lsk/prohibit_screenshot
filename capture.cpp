#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <errno.h>

#include "capture.h"

unsigned char* capture_screen_normal(Display *dpy, Window root, int *width, int *height) {
    XWindowAttributes attributes;
    XGetWindowAttributes(dpy, root, &attributes);
    *width = attributes.width;
    *height = attributes.height;

    XImage *image = XGetImage(dpy, root, 0, 0, *width, *height, AllPlanes, ZPixmap);
    if (!image) {
        fprintf(stderr, "Failed to get image\n");
        return NULL;
    }

    unsigned char *data = (unsigned char *)malloc(*width * *height * 4);
    if (!data) {
        image->f.destroy_image(image);
        fprintf(stderr, "Failed to allocate memory\n");
        return NULL;
    }

    for (int y = 0; y < *height; ++y) {
        for (int x = 0; x < *width; ++x) {
            long pixel = image->f.get_pixel(image, x, y);
            data[(y * *width + x) * 4 + 0] = (pixel >> 16) & 0xFF; // Red
            data[(y * *width + x) * 4 + 1] = (pixel >> 8) & 0xFF;  // Green
            data[(y * *width + x) * 4 + 2] = pixel & 0xFF;         // Blue
            data[(y * *width + x) * 4 + 3] = 0xFF;                  // Alpha
        }
    }

    image->f.destroy_image(image);
    return data;
}


unsigned char* capture_screen_shm(Display* display, Window root, int* width, int* height) {
    XWindowAttributes attributes;
    int ret = XGetWindowAttributes(display, root, &attributes);
    if (ret < 0) {
        fprintf(stderr, "Failed to get window attributes, ret=%d errno=%d\n", ret, errno);
        return 0;
    }
    *width = attributes.width;
    *height = attributes.height;
    XShmSegmentInfo shminfo;
    shminfo.shmid = -1;
    shminfo.shmaddr = NULL;
    shminfo.readOnly = False;

    XImage* image = XShmCreateImage(display, attributes.visual, attributes.depth, ZPixmap, 0, &shminfo,
        attributes.width, attributes.height);
    if (image == NULL) {
        fprintf(stderr, "Failed to create XImage\n");
        return 0;
    }
    shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0600);
    shminfo.shmaddr = (char*)shmat(shminfo.shmid, 0, 0);
    image->data = shminfo.shmaddr;

    XShmAttach(display, &shminfo);
    XSync(display, False);

    XShmGetImage(display, root, image, 0, 0, AllPlanes);
    unsigned char* data = (unsigned char*)malloc(*width * *height * 4);
    if (!data) {
        image->f.destroy_image(image);
        fprintf(stderr, "Failed to allocate memory\n");
    }
    else {
        for (int y = 0; y < *height; ++y) {
            for (int x = 0; x < *width; ++x) {
                long pixel = image->f.get_pixel(image, x, y);
                data[(y * *width + x) * 4 + 0] = (pixel >> 16) & 0xFF; // Red
                data[(y * *width + x) * 4 + 1] = (pixel >> 8) & 0xFF;  // Green
                data[(y * *width + x) * 4 + 2] = pixel & 0xFF;         // Blue
                data[(y * *width + x) * 4 + 3] = 0xFF;                  // Alpha
            }
        }
    }
    image->f.destroy_image(image);

    XShmDetach(display, &shminfo);
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, NULL);
    return data;
}

unsigned char* capture_screen_shm_pixmap(Display* display, Window root, int* width, int* height) {
    int major, minor;
    Bool supports_shm, sharedPixmaps;
    supports_shm = XShmQueryExtension(display);
    if (!supports_shm || !XShmQueryVersion(display, &major, &minor, &sharedPixmaps)) {
        // 处理错误，例如打印错误信息或退出程序
        fprintf(stderr, "X server does not support the MIT-SHM extension.\n");
        return 0;
    }
    else {
        fprintf(stderr, "X server supports the MIT-SHM extension.major:%d minor:%d sharedPixmaps:%d\n", major, minor, sharedPixmaps);
    }

    XWindowAttributes attributes;
    int ret = XGetWindowAttributes(display, root, &attributes);
    if (ret < 0) {
        fprintf(stderr, "Failed to get window attributes, ret=%d errno=%d\n", ret, errno);
        return 0;
    }
    *width = attributes.width;
    *height = attributes.height;
    XShmSegmentInfo shminfo;
    shminfo.shmid = -1;
    shminfo.shmaddr = NULL;
    shminfo.readOnly = False;

    XImage* image = XShmCreateImage(display, attributes.visual, attributes.depth, ZPixmap, 0, &shminfo,
        attributes.width, attributes.height);
    if (image == NULL) {
        fprintf(stderr, "Failed to create XImage\n");
        return 0;
    }
    shminfo.shmid = shmget(IPC_PRIVATE, image->bytes_per_line * image->height, IPC_CREAT | 0600);
    shminfo.shmaddr = (char*)shmat(shminfo.shmid, 0, 0);
    image->data = shminfo.shmaddr;

    XShmAttach(display, &shminfo);
    XSync(display, False);

    Pixmap shm_pixmap_ = XShmCreatePixmap(display, root, image->data, &shminfo, image->width, image->height, attributes.depth);
    if (shm_pixmap_ == 0) {
        fprintf(stderr, "Failed to create XShmPixmap\n");
        image->f.destroy_image(image);
        XShmDetach(display, &shminfo);
        shmdt(shminfo.shmaddr);
        shmctl(shminfo.shmid, IPC_RMID, NULL);
        return 0;
    }
    XSync(display, False);

    XGCValues shm_gc_values;
    shm_gc_values.subwindow_mode = IncludeInferiors;
    shm_gc_values.graphics_exposures = False;
    GC shm_gc_ = XCreateGC(display, root,
                        GCSubwindowMode | GCGraphicsExposures, &shm_gc_values);
    XSync(display, False);

    XCopyArea(display, root, shm_pixmap_, shm_gc_,
        0, 0, image->width, image->height, 0, 0);
    XSync(display, False);

    unsigned char* data = (unsigned char*)malloc(*width * *height * 4);
    if (!data) {
        image->f.destroy_image(image);
        fprintf(stderr, "Failed to allocate memory\n");
    }
    else {
        for (int y = 0; y < *height; ++y) {
            for (int x = 0; x < *width; ++x) {
                long pixel = image->f.get_pixel(image, x, y);
                data[(y * *width + x) * 4 + 0] = (pixel >> 16) & 0xFF; // Red
                data[(y * *width + x) * 4 + 1] = (pixel >> 8) & 0xFF;  // Green
                data[(y * *width + x) * 4 + 2] = pixel & 0xFF;         // Blue
                data[(y * *width + x) * 4 + 3] = 0xFF;                  // Alpha
            }
        }
    }
    image->f.destroy_image(image);

    XFreePixmap(display, shm_pixmap_);
    XShmDetach(display, &shminfo);
    shmdt(shminfo.shmaddr);
    shmctl(shminfo.shmid, IPC_RMID, NULL);
    return data;
}


