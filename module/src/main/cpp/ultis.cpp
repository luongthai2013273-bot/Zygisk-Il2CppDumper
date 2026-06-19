// utils.cpp
#include "utils.h"
#include <android/log.h>
#include <fstream>
#include <string>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "MenuUI", __VA_ARGS__)

std::string readPackageFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        LOGD("Cannot open package file: %s", path);
        return "";
    }
    std::string pkg;
    std::getline(f, pkg);
    pkg.erase(0, pkg.find_first_not_of(" \t\n\r"));
    pkg.erase(pkg.find_last_not_of(" \t\n\r") + 1);
    LOGD("Read target package: %s", pkg.c_str());
    return pkg;
}

std::string getCurrentPackage(JNIEnv* env) {
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

void hideLibrary() {
    LOGD("Hide library called (placeholder)");
}
