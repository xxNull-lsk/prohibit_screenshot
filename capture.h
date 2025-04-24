#ifndef CAPTURE_H
#define CAPTURE_H

#include <X11/Xlib.h>
unsigned char* capture_screen_normal(Display *dpy, Window root, int *width, int *height);
unsigned char* capture_screen_shm(Display* display, Window root, int* width, int* height);
unsigned char* capture_screen_shm_pixmap(Display* display, Window root, int* width, int* height);
#endif