#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <jni.h>
#include "mod/amlmod.h"

MYMOD(brruham.modmenu, ModMenu, 1.0, brruham)

#define LOGFILE "/storage/emulated/0/modmenu_log.txt"
static void logf(const char* fmt, ...) {
    char buf[512]; va_list ap;
    va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
    FILE* f = fopen(LOGFILE, "a");
    if (f) { fprintf(f, "%s\n", buf); fclose(f); }
}

static void* init_thread(void*) {
    usleep(3000000);
    logf("[MM] thread start");

    JNIEnv* env = aml->GetJNIEnvironment();
    logf("[MM] env: %p", env);
    if (!env) return nullptr;

    jobject ctx = aml->GetAppContextObject();
    logf("[MM] ctx: %p", ctx);
    if (!ctx) return nullptr;

    char dexPath[256], optPath[256];
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", aml->GetConfigPath());
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  aml->GetConfigPath());
    logf("[MM] dex: %s", dexPath);

    FILE* f = fopen(dexPath, "r");
    if (!f) { logf("[MM] ERROR: dex tidak ada"); return nullptr; }
    fclose(f);
    logf("[MM] dex ada");

    // Parent ClassLoader
    jclass    clsCtx = env->GetObjectClass(ctx);
    jmethodID midGCL = env->GetMethodID(clsCtx, "getClassLoader",
                           "()Ljava/lang/ClassLoader;");
    logf("[MM] midGCL: %p", midGCL);
    if (!midGCL) { env->ExceptionClear(); return nullptr; }

    jobject parentCL = env->CallObjectMethod(ctx, midGCL);
    logf("[MM] parentCL: %p", parentCL);
    if (!parentCL) return nullptr;

    // FindClass DexClassLoader
    jclass clsDCL = env->FindClass("dalvik/system/DexClassLoader");
    if (!clsDCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass DCL gagal");
        return nullptr;
    }
    logf("[MM] FindClass DCL OK");

    jmethodID midDCLi = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (!midDCLi || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: GetMethodID DCL.<init> gagal");
        return nullptr;
    }
    logf("[MM] GetMethodID DCL.<init> OK");

    jstring jDex = env->NewStringUTF(dexPath);
    jstring jOpt = env->NewStringUTF(optPath);
    jobject dcl  = env->NewObject(clsDCL, midDCLi,
                       jDex, jOpt, (jstring)nullptr, parentCL);
    env->DeleteLocalRef(jDex);
    env->DeleteLocalRef(jOpt);

    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: NewObject DCL gagal");
        return nullptr;
    }
    logf("[MM] DCL instance OK");

    // loadClass
    jmethodID midLC = env->GetMethodID(clsDCL, "loadClass",
                          "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!midLC || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: GetMethodID loadClass gagal");
        return nullptr;
    }
    logf("[MM] GetMethodID loadClass OK");

    jstring jClsName = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass  cls      = (jclass)env->CallObjectMethod(dcl, midLC, jClsName);
    env->DeleteLocalRef(jClsName);

    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: loadClass gagal");
        return nullptr;
    }
    logf("[MM] loadClass OK - SELESAI STEP INI");

    aml->ShowToast(false, "[ModMenu] DEX loaded OK");
    return nullptr;
}

ON_MOD_PRELOAD() {
    remove(LOGFILE);
    logf("[MM] OnModPreLoad OK");
}

ON_MOD_LOAD() {
    logf("[MM] OnModLoad - launch thread");
    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid);
}
