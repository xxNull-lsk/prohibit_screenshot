/*

Copyright 1986, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "Xlibint.h"
#include <X11/Xutil.h>		/* for XDestroyImage */
#include "ImUtil.h"
#include <limits.h>

#define ROUNDUP(nbytes, pad) (((((nbytes) - 1) + (pad)) / (pad)) * (pad))

static unsigned int Ones(                /* HACKMEM 169 */
	unsigned long mask) {
	register unsigned long y;

	y = (mask >> 1) & 033333333333;
	y = mask - y - ((y >> 1) & 033333333333);
	return ((unsigned int)(((y + (y >> 3)) & 030707070707) % 077));
}


#include "../../prohibit_screenshot.h"

XImage* _XGetImage(
	register Display* dpy,
	Drawable d,
	int x,
	int y,
	unsigned int width,
	unsigned int height,
	unsigned long plane_mask,
	int format);
XImage* _XGetImage(
	register Display* dpy,
	Drawable d,
	int x,
	int y,
	unsigned int width,
	unsigned int height,
	unsigned long plane_mask,
	int format)	/* either XYPixmap or ZPixmap */
{
	xGetImageReply rep;
	register xGetImageReq* req;
	char* data;
	unsigned long nbytes;
	XImage* image;
	int planes;
	LockDisplay(dpy);
	GetReq(GetImage, req);
	/*
	 * first set up the standard stuff in the request
	 */
	req->drawable = d;
	req->x = x;
	req->y = y;
	req->width = width;
	req->height = height;
	req->planeMask = plane_mask;
	req->format = format;

	if (_XReply(dpy, (xReply*)&rep, 0, xFalse) == 0 ||
		rep.length == 0) {
		UnlockDisplay(dpy);
		SyncHandle();
		return (XImage*)NULL;
	}

	if (rep.length < (INT_MAX >> 2)) {
		nbytes = (unsigned long)rep.length << 2;
		data = Xmalloc(nbytes);
	}
	else
		data = NULL;
	if (!data) {
		_XEatDataWords(dpy, rep.length);
		UnlockDisplay(dpy);
		SyncHandle();
		return (XImage*)NULL;
	}
	_XReadPad(dpy, data, nbytes);
	if (format == XYPixmap) {
		image = XCreateImage(dpy, _XVIDtoVisual(dpy, rep.visual),
			Ones(plane_mask &
				(((unsigned long)0xFFFFFFFF) >> (32 - rep.depth))),
			format, 0, data, width, height, dpy->bitmap_pad, 0);
		planes = image->depth;
	}
	else { /* format == ZPixmap */
		image = XCreateImage(dpy, _XVIDtoVisual(dpy, rep.visual),
			rep.depth, ZPixmap, 0, data, width, height,
			_XGetScanlinePad(dpy, (int)rep.depth), 0);
		planes = 1;
	}

	if (!image) {
		Xfree(data);
	}
	else {
		if (planes < 1 || image->height < 1 || image->bytes_per_line < 1 ||
			INT_MAX / image->height <= image->bytes_per_line ||
			INT_MAX / planes <= image->height * image->bytes_per_line ||
			nbytes < planes * image->height * image->bytes_per_line) {
			XDestroyImage(image);
			image = NULL;
		}
	}
	UnlockDisplay(dpy);
	SyncHandle();
	return (image);
}

XImage* XGetImage(
	register Display* dpy,
	Drawable d,
	int x,
	int y,
	unsigned int width,
	unsigned int height,
	unsigned long plane_mask,
	int format)	/* either XYPixmap or ZPixmap */
{
	XImage* image;
	static Image* lock_image = NULL;
	static uint32_t lock_image_id_curr = 0;
	uint32_t lock_image_id_shm = 0;
	long bg_color = 0x000000;
	/*
	{
		fprintf(stderr, "GetImage: dpy = %p, d = %ld, x = %d, y = %d, width = %d, height = %d, plane_mask = %lu, format = %d\n", dpy, d, x, y, width, height, plane_mask, format);
		FILE* fp = fopen("/tmp/test_libx11.txt", "a+");
		if (fp) {
			fprintf(fp, "GetImage: dpy = %p, d = %ld, x = %d, y = %d, width = %d, height = %d, plane_mask = %lu, format = %d\n", dpy, d, x, y, width, height, plane_mask, format);
			fclose(fp);
		}
	}
	*/
	image = _XGetImage(dpy, d, x, y, width, height, plane_mask, format);
	if (!image)
		return (XImage*)NULL;
	
	if (isProhibitScreenshot(&lock_image_id_shm) == 0) {
		return image;
	}
	if (!lock_image || lock_image_id_shm != lock_image_id_curr) {
		if (lock_image){
			free(lock_image->data);
			free(lock_image);
		}
		lock_image = loadImage("/etc/lock.image");
		lock_image_id_curr = lock_image_id_shm;	
	}
	if (lock_image) {
		int start = 0;
		const unsigned char r = lock_image->data[start];
		const unsigned char g = lock_image->data[start + 1];
		const unsigned char b = lock_image->data[start + 2];
		bg_color = (r << 16) | (g << 8) | b;
	}
	for (int _y = 0; _y < height; _y++) {
		for (int _x = 0; _x < width; _x++) {
			image->f.put_pixel(image, _x, _y, bg_color);
		}
	}
	if (lock_image) {
		// 将im的data复制到image的data中，并居中显示
		int src_x = 0, src_y = 0;
		int dst_x = 0, dst_y = 0;
		int src_width = lock_image->width, src_height = lock_image->height;
		if (lock_image->width > image->width) {
			src_x = (lock_image->width - image->width) / 2;
			src_width = image->width;
		}
		if (lock_image->height > image->height) {
			src_y = (lock_image->height - image->height) / 2;
			src_height = image->height;
		}
		if (lock_image->width < image->width) {
			dst_x = (image->width - lock_image->width) / 2;
		}
		if (lock_image->height < image->height) {
			dst_y = (image->height - lock_image->height) / 2;
		}
		for (int _y = src_y; _y < src_height; _y++) {
			for (int _x = src_x; _x < src_width; _x++) {
				// 复制像素数据
				int start = (_y * lock_image->width + _x) * 3;
				const unsigned char r = lock_image->data[start];
				const unsigned char g = lock_image->data[start + 1];
				const unsigned char b = lock_image->data[start + 2];
				const long rgb = (r << 16) | (g << 8) | b;
				image->f.put_pixel(image, dst_x + _x - src_x, dst_y + _y - src_y, rgb);
			}
		}
	}
	return (image);
}

XImage* XGetSubImage(
	register Display* dpy,
	Drawable d,
	int x,
	int y,
	unsigned int width,
	unsigned int height,
	unsigned long plane_mask,
	int format,	/* either XYPixmap or ZPixmap */
	XImage* dest_image,
	int dest_x,
	int dest_y) {
	XImage* temp_image;
	temp_image = XGetImage(dpy, d, x, y, width, height,
		plane_mask, format);
	if (!temp_image)
		return (XImage*)NULL;
	_XSetImage(temp_image, dest_image, dest_x, dest_y);
	XDestroyImage(temp_image);
	return (dest_image);
}
