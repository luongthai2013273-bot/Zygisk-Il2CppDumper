// utils.h
#pragma once
#include <jni.h>
#include <string>

std::string readPackageFile(const char* path = "/storage/emulated/0/packages/pkg.txt");
std::string getCurrentPackage(JNIEnv* env);
void hideLibrary();
