#include <jni.h>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <thread>
#include <chrono>
#include <string>
#include <fstream>
#include <dlfcn.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <cmath>
#include "utils.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "MenuUI", __VA_ARGS__)

// ====================================================================
// 2. NATIVE WINDOW + UI
// ====================================================================
static ANativeWindow* g_window = nullptr;
static int screenW = 1080, screenH = 1920;

// Trạng thái menu
static bool menuVisible = true;
static bool espEnabled = false;
static bool aimbotEnabled = false;
static bool wallhackEnabled = false;

// Vị trí menu
static float menuX = 20.0f;
static float menuY = 80.0f;
static float menuW = 260.0f;
static float menuH = 380.0f;

// Kéo thả
static bool isDragging = false;
static float dragOffsetX = 0.0f;
static float dragOffsetY = 0.0f;

// ====================================================================
// 3. VẼ UI BẰNG PIXEL BUFFER
// ====================================================================
static uint32_t* g_pixels = nullptr;
static int g_pitch = 0;

static void drawPixel(int x, int y, uint32_t color) {
    if (!g_pixels || x < 0 || y < 0 || x >= screenW || y >= screenH) return;
    g_pixels[y * g_pitch + x] = color;
}

static void drawRoundRect(int left, int top, int right, int bottom, int radius, uint32_t color) {
    if (left > right || top > bottom) return;
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            bool inCorner = false;
            if (x - left < radius && y - top < radius) {
                if ((x - left) * (x - left) + (y - top) * (y - top) > radius * radius) inCorner = true;
            } else if (right - x < radius && y - top < radius) {
                if ((right - x) * (right - x) + (y - top) * (y - top) > radius * radius) inCorner = true;
            } else if (x - left < radius && bottom - y < radius) {
                if ((x - left) * (x - left) + (bottom - y) * (bottom - y) > radius * radius) inCorner = true;
            } else if (right - x < radius && bottom - y < radius) {
                if ((right - x) * (right - x) + (bottom - y) * (bottom - y) > radius * radius) inCorner = true;
            }
            if (!inCorner) {
                drawPixel(x + (int)menuX, y + (int)menuY, color);
            }
        }
    }
}

static void drawText(int x, int y, const char* text, uint32_t color) {
    if (!text) return;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        for (int dy = 0; dy < 12; dy++) {
            for (int dx = 0; dx < 8; dx++) {
                if (c == ' ') continue;
                drawPixel(x + i * 10 + dx, y + dy, color);
            }
        }
    }
}

static void drawToggle(int x, int y, int w, int h, bool state, const char* label) {
    uint32_t bgColor = state ? 0xFF00E676 : 0xFFFF1744;
    drawRoundRect(x, y, x + w, y + h, 8, bgColor);
    drawText(x + 12, y + 12, label, 0xFFFFFFFF);
    drawText(x + w - 40, y + 12, state ? "ON" : "OFF", 0xDD000000);
}

static void drawUI() {
    if (!g_window || !menuVisible) return;
    
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(g_window, &buffer, nullptr) < 0) return;
    
    g_pixels = (uint32_t*)buffer.bits;
    g_pitch = buffer.stride;
    screenW = buffer.width;
    screenH = buffer.height;
    
    // Nền menu
    drawRoundRect(4, 4, (int)menuW, (int)menuH, 16, 0x22000000);
    drawRoundRect(0, 0, (int)menuW, (int)menuH, 16, 0xDD1A1A2E);
    drawRoundRect(1, 1, (int)menuW - 1, (int)menuH - 1, 16, 0xFF4A4A8A);
    
    // Thanh tiêu đề
    drawRoundRect(4, 4, (int)menuW - 4, 44, 12, 0xCC4A4A8A);
    drawText(16, 22, "⚡ Z-MENU", 0xFFB8B8FF);
    drawText((int)menuW - 32, 22, "✕", 0xFFFF1744);
    
    // Các toggle
    const char* items[] = {"ESP", "Aimbot", "Wallhack"};
    bool* states[] = {&espEnabled, &aimbotEnabled, &wallhackEnabled};
    float yPos = 60.0f;
    for (int i = 0; i < 3; i++) {
        drawToggle(12, (int)yPos, 200, 36, *states[i], items[i]);
        yPos += 48.0f;
    }
    
    // Footer
    drawText(12, (int)menuH - 16, "Drag title to move", 0x66FFFFFF);
    
    ANativeWindow_unlockAndPost(g_window);
}

// ====================================================================
// 4. TẠO NATIVE WINDOW
// ====================================================================
static void createNativeWindow(JNIEnv* env) {
    jclass atCls = env->FindClass("android/app/ActivityThread");
    jmethodID curAct = env->GetStaticMethodID(atCls, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject thread = env->CallStaticObjectMethod(atCls, curAct);
    jmethodID getApp = env->GetMethodID(atCls, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(thread, getApp);
    
    jclass svCls = env->FindClass("android/view/SurfaceView");
    jmethodID svInit = env->GetMethodID(svCls, "<init>", "(Landroid/content/Context;)V");
    jobject sv = env->NewObject(svCls, svInit, context);
    
    jmethodID getHolder = env->GetMethodID(svCls, "getHolder", "()Landroid/view/SurfaceHolder;");
    jobject holder = env->CallObjectMethod(sv, getHolder);
    
    jclass holderCls = env->GetObjectClass(holder);
    jmethodID getSurface = env->GetMethodID(holderCls, "getSurface", "()Landroid/view/Surface;");
    jobject surface = env->CallObjectMethod(holder, getSurface);
    
    g_window = ANativeWindow_fromSurface(env, surface);
    if (!g_window) {
        LOGD("Failed to create native window");
        return;
    }
    
    // Thêm SurfaceView vào WindowManager
    jclass wmCls = env->FindClass("android/view/WindowManager");
    jmethodID getWm = env->GetMethodID(env->GetObjectClass(context), "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    jstring wmStr = env->NewStringUTF("window");
    jobject wm = env->CallObjectMethod(context, getWm, wmStr);
    env->DeleteLocalRef(wmStr);
    
    jclass lpCls = env->FindClass("android/view/WindowManager$LayoutParams");
    jmethodID lpInit = env->GetMethodID(lpCls, "<init>", "(III)V");
    jobject params = env->NewObject(lpCls, lpInit, (int)menuW, (int)menuH, 2038);
    
    jfieldID flagsField = env->GetFieldID(lpCls, "flags", "I");
    env->SetIntField(params, flagsField, 8);
    
    jfieldID gravityField = env->GetFieldID(lpCls, "gravity", "I");
    env->SetIntField(params, gravityField, 0x33);
    
    jfieldID xField = env->GetFieldID(lpCls, "x", "I");
    jfieldID yField = env->GetFieldID(lpCls, "y", "I");
    env->SetIntField(params, xField, (int)menuX);
    env->SetIntField(params, yField, (int)menuY);
    
    jmethodID addView = env->GetMethodID(wmCls, "addView", "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V");
    env->CallVoidMethod(wm, addView, sv, params);
    
    LOGD("Native window created!");
}

// ====================================================================
// 5. VÒNG LẶP UI
// ====================================================================
static void uiLoop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (menuVisible) {
            drawUI();
        }
    }
}

// ====================================================================
// 6. ENTRY POINT ZYGISK
// ====================================================================
extern "C" void zygisk_initialize(void* handle, JNIEnv* env) {
    hideLibrary();
    LOGD("Zygisk initialize");
}

extern "C" void zygisk_post_app_specialize(JNIEnv* env, jclass cls) {
    std::string target = readPackageFile();
    if (target.empty()) target = "com.dts.freefireth";
    
    std::string cur = getCurrentPackage(env);
    if (cur != target) {
        LOGD("Skip: %s", cur.c_str());
        return;
    }
    
    LOGD("Target: %s. Waiting 5s...", cur.c_str());
    
    std::thread([env]() {
        for (int i = 0; i < 50; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        LOGD("Creating native window...");
        createNativeWindow(env);
        uiLoop();
    }).detach();
}   // <--- DẤU NGOẶC NÀY LÀ QUAN TRỌNG NHẤT
