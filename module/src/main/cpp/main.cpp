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
#include <cmath>
#include <vector>
#include <cstring>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "NativeUI", __VA_ARGS__)

// ====================================================================
// 1. UTILITY
// ====================================================================
static std::string readPackageFile(const char* path = "/storage/emulated/0/packages/pkg.txt") {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::string pkg;
    std::getline(f, pkg);
    pkg.erase(0, pkg.find_first_not_of(" \t\n\r"));
    pkg.erase(pkg.find_last_not_of(" \t\n\r") + 1);
    return pkg;
}

static std::string getCurrentPackage(JNIEnv* env) {
    jclass at = env->FindClass("android/app/ActivityThread");
    jmethodID cur = env->GetStaticMethodID(at, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject thread = env->CallStaticObjectMethod(at, cur);
    jmethodID getApp = env->GetMethodID(at, "getApplication", "()Landroid/app/Application;");
    jobject app = env->CallObjectMethod(thread, getApp);
    jclass appCls = env->GetObjectClass(app);
    jmethodID getPkg = env->GetMethodID(appCls, "getPackageName", "()Ljava/lang/String;");
    jstring jpkg = (jstring)env->CallObjectMethod(app, getPkg);
    const char* c = env->GetStringUTFChars(jpkg, nullptr);
    std::string res(c);
    env->ReleaseStringUTFChars(jpkg, c);
    return res;
}

static void hideLibrary() {
    LOGD("Hide library (placeholder)");
}

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
static bool isTouching = false;
static float touchX = 0, touchY = 0;

// ====================================================================
// 3. VẼ UI BẰNG PIXEL BUFFER (không cần Canvas)
// ====================================================================
static uint32_t* g_pixels = nullptr;
static int g_pitch = 0;

// Hàm vẽ điểm ảnh
static void drawPixel(int x, int y, uint32_t color) {
    if (!g_pixels || x < 0 || y < 0 || x >= screenW || y >= screenH) return;
    g_pixels[y * g_pitch + x] = color;
}

// Hàm vẽ hình chữ nhật (có bo góc)
static void drawRoundRect(int left, int top, int right, int bottom, int radius, uint32_t color) {
    if (left > right || top > bottom) return;
    for (int y = top; y < bottom; y++) {
        for (int x = left; x < right; x++) {
            // Kiểm tra bo góc
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

// Hàm vẽ text (đơn giản)
static void drawText(int x, int y, const char* text, uint32_t color) {
    if (!text) return;
    // Font 8x8 đơn giản (chỉ hỗ trợ chữ in hoa và số)
    // Đây là placeholder, bạn có thể dùng font bitmap hoặc stb_truetype
    // Hiện tại vẽ bằng các hình chữ nhật nhỏ
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        // Vẽ ký tự đơn giản (chỉ là khối vuông)
        for (int dy = 0; dy < 12; dy++) {
            for (int dx = 0; dx < 8; dx++) {
                if (c == ' ') continue;
                drawPixel(x + i * 10 + dx, y + dy, color);
            }
        }
    }
}

// Hàm vẽ toggle
static void drawToggle(int x, int y, int w, int h, bool state, const char* label) {
    uint32_t bgColor = state ? 0xFF00E676 : 0xFFFF1744;
    drawRoundRect(x, y, x + w, y + h, 8, bgColor);
    drawText(x + 12, y + 12, label, 0xFFFFFFFF);
    drawText(x + w - 40, y + 12, state ? "ON" : "OFF", 0xDD000000);
}

// ====================================================================
// 4. VẼ TOÀN BỘ UI
// ====================================================================
static void drawUI() {
    if (!g_window || !menuVisible) return;
    
    // Lock window để vẽ
    ANativeWindow_Buffer buffer;
    if (ANativeWindow_lock(g_window, &buffer, nullptr) < 0) {
        LOGD("Failed to lock window");
        return;
    }
    
    g_pixels = (uint32_t*)buffer.bits;
    g_pitch = buffer.stride;
    screenW = buffer.width;
    screenH = buffer.height;
    
    // 1. Nền menu (bo góc, đổ bóng)
    drawRoundRect(4, 4, (int)menuW, (int)menuH, 16, 0x22000000); // Bóng đổ
    drawRoundRect(0, 0, (int)menuW, (int)menuH, 16, 0xDD1A1A2E); // Nền chính
    
    // 2. Viền
    drawRoundRect(1, 1, (int)menuW - 1, (int)menuH - 1, 16, 0xFF4A4A8A);
    
    // 3. Thanh tiêu đề
    drawRoundRect(4, 4, (int)menuW - 4, 44, 12, 0xCC4A4A8A);
    
    // 4. Tiêu đề
    drawText(16, 22, "⚡ Z-MENU", 0xFFB8B8FF);
    
    // 5. Nút đóng X
    drawText((int)menuW - 32, 22, "✕", 0xFFFF1744);
    
    // 6. Các toggle
    const char* items[] = {"ESP", "Aimbot", "Wallhack"};
    bool* states[] = {&espEnabled, &aimbotEnabled, &wallhackEnabled};
    float yPos = 60.0f;
    
    for (int i = 0; i < 3; i++) {
        drawToggle(12, (int)yPos, 200, 36, *states[i], items[i]);
        yPos += 48.0f;
    }
    
    // 7. Footer
    drawText(12, (int)menuH - 16, "Drag title to move", 0x66FFFFFF);
    
    // Unlock window
    ANativeWindow_unlockAndPost(g_window);
}

// ====================================================================
// 5. XỬ LÝ TOUCH (Native)
// ====================================================================
static void handleTouch(float x, float y, int action) {
    if (!menuVisible) return;
    
    float localX = x - menuX;
    float localY = y - menuY;
    
    if (action == 0) { // DOWN
        // Nút đóng
        if (localX > menuW - 40 && localX < menuW - 8 && localY > 8 && localY < 40) {
            menuVisible = false;
            LOGD("Menu closed");
            return;
        }
        // Các toggle
        if (localX > 12 && localX < 212) {
            int idx = -1;
            if (localY > 60 && localY < 96) idx = 0;
            else if (localY > 108 && localY < 144) idx = 1;
            else if (localY > 156 && localY < 192) idx = 2;
            
            if (idx >= 0) {
                bool* states[] = {&espEnabled, &aimbotEnabled, &wallhackEnabled};
                *states[idx] = !(*states[idx]);
                LOGD("Toggle %d: %s", idx, *states[idx] ? "ON" : "OFF");
                return;
            }
        }
        // Bắt đầu kéo
        if (localY > 4 && localY < 44 && localX > 4 && localX < menuW - 44) {
            isDragging = true;
            dragOffsetX = x - menuX;
            dragOffsetY = y - menuY;
        }
    } else if (action == 1) { // MOVE
        if (isDragging) {
            menuX = x - dragOffsetX;
            menuY = y - dragOffsetY;
            if (menuX < 0) menuX = 0;
            if (menuY < 0) menuY = 0;
            if (menuX + menuW > screenW) menuX = screenW - menuW;
            if (menuY + menuH > screenH) menuY = screenH - menuH;
        }
    } else if (action == 2) { // UP
        isDragging = false;
    }
}

// ====================================================================
// 6. VÒNG LẶP CHÍNH + XỬ LÝ SỰ KIỆN TOUCH TỪ /dev/input
// ====================================================================
static void uiLoop() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (menuVisible) {
            drawUI();
        }
    }
}

// Đọc sự kiện touch từ /dev/input (cần root)
static void readTouchEvents() {
    // Mở thiết bị touch (thường là /dev/input/event*)
    // Cần tìm đúng event device của touchscreen
    // Đây là code phức tạp, có thể dùng cách khác:
    // Dùng JNI để gọi MotionEvent (cần ít Java) hoặc dùng socket
    // Hiện tại chỉ dùng vòng lặp giả để chạy UI
}

// ====================================================================
// 7. TẠO NATIVE WINDOW (từ SurfaceView)
// ====================================================================
static void createNativeWindow(JNIEnv* env) {
    // Lấy context
    jclass atCls = env->FindClass("android/app/ActivityThread");
    jmethodID curAct = env->GetStaticMethodID(atCls, "currentActivityThread", "()Landroid/app/ActivityThread;");
    jobject thread = env->CallStaticObjectMethod(atCls, curAct);
    jmethodID getApp = env->GetMethodID(atCls, "getApplication", "()Landroid/app/Application;");
    jobject context = env->CallObjectMethod(thread, getApp);
    
    // Tạo SurfaceView
    jclass svCls = env->FindClass("android/view/SurfaceView");
    jmethodID svInit = env->GetMethodID(svCls, "<init>", "(Landroid/content/Context;)V");
    jobject sv = env->NewObject(svCls, svInit, context);
    
    // Lấy SurfaceHolder
    jmethodID getHolder = env->GetMethodID(svCls, "getHolder", "()Landroid/view/SurfaceHolder;");
    jobject holder = env->CallObjectMethod(sv, getHolder);
    
    // Lấy Surface từ SurfaceHolder
    jclass holderCls = env->GetObjectClass(holder);
    jmethodID getSurface = env->GetMethodID(holderCls, "getSurface", "()Landroid/view/Surface;");
    jobject surface = env->CallObjectMethod(holder, getSurface);
    
    // Chuyển Surface thành ANativeWindow
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
    jobject params = env->NewObject(lpCls, lpInit, (int)menuW, (int)menuH, 2038); // TYPE_APPLICATION_PANEL
    
    jfieldID flagsField = env->GetFieldID(lpCls, "flags", "I");
    env->SetIntField(params, flagsField, 8); // FLAG_NOT_FOCUSABLE
    
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
// 8. ENTRY POINT
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
