# 背景
在X11环境下，实现防截屏功能。

# 实现
1. 监听X11的XGetImage，将截屏图片替换成指定图片。
2. 监听X11的XShmGetImage，将截屏图片替换成指定图片。
3. 禁用XShmCreatePixmap。

# 功能
- lock_window  
   实现锁定窗口、解锁窗口、重置锁定、更换锁定图片、查看锁定窗口列表等功能。

- screenshot -s
   通过XGetImage实现截屏、保存截屏图片功能。

- screenshot -shm  
   通过XShmGetImage实现截屏、保存截屏图片功能。

- screenshot -shm-pixmap  
   通过XShmCreatePixmap实现截屏、保存截屏图片功能。

注意：  
1. 需要替换`libX11.so.6.3.0`和`libXext.so.6.4.0`。  
2. 没有考虑多屏幕。  
3. 没有考虑窗口状态变化，如窗口销毁、窗口最小化等等。

改动点关键字：`XGetImage`、`XShmGetImage`、`XShmCreatePixmap`、`isProhibitScreenshot`

[演示视频](./20250424000911.mp4)