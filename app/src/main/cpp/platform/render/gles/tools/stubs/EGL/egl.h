// Minimal stub of <EGL/egl.h> for desktop `g++ -fsyntax-only` checks ONLY. The real NDK header is used in the app build.
#pragma once
#include <android/native_window.h>
typedef int EGLint; typedef unsigned int EGLBoolean; typedef void* EGLDisplay; typedef void* EGLSurface; typedef void* EGLContext;
typedef void* EGLConfig; typedef void* EGLNativeDisplayType;
#define EGL_NO_DISPLAY ((EGLDisplay)0)
#define EGL_NO_SURFACE ((EGLSurface)0)
#define EGL_NO_CONTEXT ((EGLContext)0)
#define EGL_DEFAULT_DISPLAY ((EGLNativeDisplayType)0)
#define EGL_NONE 0x3038
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_SURFACE_TYPE 0x3033
#define EGL_WINDOW_BIT 0x0004
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_ALPHA_SIZE 0x3021
#define EGL_DEPTH_SIZE 0x3025
#define EGL_STENCIL_SIZE 0x3026
#define EGL_NATIVE_VISUAL_ID 0x302E
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#define EGL_WIDTH 0x3057
#define EGL_HEIGHT 0x3056
#define EGL_CONTEXT_LOST 0x300E
#define EGL_BAD_SURFACE 0x300D
#define EGL_BAD_NATIVE_WINDOW 0x300B
EGLDisplay eglGetDisplay(EGLNativeDisplayType); EGLBoolean eglInitialize(EGLDisplay, EGLint*, EGLint*); EGLBoolean eglTerminate(EGLDisplay);
EGLBoolean eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
EGLBoolean eglGetConfigAttrib(EGLDisplay, EGLConfig, EGLint, EGLint*);
EGLSurface eglCreateWindowSurface(EGLDisplay, EGLConfig, ANativeWindow*, const EGLint*);
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
EGLBoolean eglDestroyContext(EGLDisplay, EGLContext); EGLBoolean eglDestroySurface(EGLDisplay, EGLSurface);
EGLBoolean eglSwapBuffers(EGLDisplay, EGLSurface); EGLBoolean eglSwapInterval(EGLDisplay, EGLint);
EGLBoolean eglQuerySurface(EGLDisplay, EGLSurface, EGLint, EGLint*); EGLint eglGetError();
