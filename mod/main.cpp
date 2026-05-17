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
    if (!env) { logf("[MM] ERROR: env null"); return nullptr; }

    jobject ctx = aml->GetAppContextObject();
    if (!ctx) { logf("[MM] ERROR: ctx null"); return nullptr; }
    logf("[MM] ctx: %p", ctx);

    // Fix double slash
    const char* cfgPath = aml->GetConfigPath();
    char cleanCfg[256];
    strncpy(cleanCfg, cfgPath, sizeof(cleanCfg)-1);
    int cfgLen = strlen(cleanCfg);
    if (cfgLen > 0 && cleanCfg[cfgLen-1] == '/') cleanCfg[cfgLen-1] = '\0';

    char dexPath[256], optPath[256];
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", cleanCfg);
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  cleanCfg);
    logf("[MM] dex: %s", dexPath);

    FILE* f = fopen(dexPath, "r");
    if (!f) { logf("[MM] ERROR: dex tidak ada"); return nullptr; }
    fclose(f);

    // Ambil ClassLoader langsung dari object ctx
    // ctx adalah jobject — cukup panggil getClassLoader() via reflection
    jclass    clsObj  = env->FindClass("java/lang/Object");
    jclass    clsCtx  = env->FindClass("android/content/Context");
    if (!clsCtx || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass Context gagal");
        return nullptr;
    }
    logf("[MM] FindClass Context OK");

    jmethodID midGCL = env->GetMethodID(clsCtx, "getClassLoader",
                           "()Ljava/lang/ClassLoader;");
    if (!midGCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: getClassLoader method null");
        return nullptr;
    }
    logf("[MM] getClassLoader method OK");

    jobject parentCL = env->CallObjectMethod(ctx, midGCL);
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: getClassLoader() threw exception");
        return nullptr;
    }
    if (!parentCL) {
        logf("[MM] WARN: parentCL null, coba Thread.currentThread().getContextClassLoader()");
        // Fallback: ambil dari Thread saat ini
        jclass    clsThread = env->FindClass("java/lang/Thread");
        jmethodID midCT     = env->GetStaticMethodID(clsThread,
                                  "currentThread", "()Ljava/lang/Thread;");
        jmethodID midGCCL   = env->GetMethodID(clsThread,
                                  "getContextClassLoader",
                                  "()Ljava/lang/ClassLoader;");
        jobject curThread = env->CallStaticObjectMethod(clsThread, midCT);
        parentCL = env->CallObjectMethod(curThread, midGCCL);
        logf("[MM] fallback parentCL: %p", parentCL);
        if (!parentCL) { logf("[MM] ERROR: parentCL tetap null"); return nullptr; }
    }
    logf("[MM] parentCL OK: %p", parentCL);

    // DexClassLoader
    jclass clsDCL = env->FindClass("dalvik/system/DexClassLoader");
    if (!clsDCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass DexClassLoader gagal");
        return nullptr;
    }
    jmethodID midDCLi = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (!midDCLi || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: DCL.<init> gagal");
        return nullptr;
    }
    logf("[MM] DCL ready");

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
    jmethodID midLC  = env->GetMethodID(clsDCL, "loadClass",
                           "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jClsName = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass  cls      = (jclass)env->CallObjectMethod(dcl, midLC, jClsName);
    env->DeleteLocalRef(jClsName);

    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: loadClass gagal");
        return nullptr;
    }
    logf("[MM] loadClass OK");

    aml->ShowToast(false, "[ModMenu] DEX loaded OK!");
    return nullptr;
}

ON_MOD_PRELOAD() {
    remove(LOGFILE);
    logf("[MM] OnModPreLoad OK");
}

ON_MOD_LOAD() {
    logf("[MM] OnModLoad");
    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid);
}
