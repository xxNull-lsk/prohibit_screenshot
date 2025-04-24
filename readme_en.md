# Background
Implement anti-screenshot functionality under the X11 environment.

# Implementation
1. Monitor XGetImage in X11 and replace the screenshot with a specified image.
2. Monitor XShmGetImage in X11 and replace the screenshot with a specified image.
3. Disable XShmCreatePixmap.

# Features
- **lock_window**
  - Lock window/Unlock window/Reset lock/Change lock image/View list of locked windows.

- **screenshot -s**
  - Take and save screenshots via XGetImage.

- **screenshot -shm**
  - Take and save screenshots via XShmGetImage.

- **screenshot -shm-pixmap**
  - Take and save screenshots via XShmCreatePixmap.

Notes:
1. Need to replace `libX11.so.6.3.0` and `libXext.so.6.4.0`.
2. Multi-monitor setups are not considered.
3. Window state changes (e.g., window destruction, minimization) are not considered.

Keywords for modifications: `XGetImage`, `XShmGetImage`, `XShmCreatePixmap`, `isProhibitScreenshot`

[Demo Video](./20250424000911.mp4)