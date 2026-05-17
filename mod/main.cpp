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
    logf("[MM] env OK: %p", env);

    jobject ctx = aml->GetAppContextObject();
    if (!ctx) { logf("[MM] ERROR: ctx null"); return nullptr; }
    logf("[MM] ctx OK: %p", ctx);

    // Fix double slash - trim trailing slash dari GetConfigPath
    char dexPath[256], optPath[256];
    const char* cfgPath = aml->GetConfigPath();
    int cfgLen = strlen(cfgPath);
    // Hapus trailing slash jika ada
    char cleanCfg[256];
    strncpy(cleanCfg, cfgPath, sizeof(cleanCfg)-1);
    if (cfgLen > 0 && cleanCfg[cfgLen-1] == '/')
        cleanCfg[cfgLen-1] = '\0';
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", cleanCfg);
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  cleanCfg);
    logf("[MM] dex: %s", dexPath);

    FILE* f = fopen(dexPath, "r");
    if (!f) { logf("[MM] ERROR: dex tidak ada"); return nullptr; }
    fclose(f);
    logf("[MM] dex ada");

    // Ambil ClassLoader via ActivityThread langsung
    // (lebih reliable dari ctx.getClassLoader())
    jclass    clsAT    = env->FindClass("android/app/ActivityThread");
    if (!clsAT || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass ActivityThread gagal");
        return nullptr;
    }
    jmethodID midCurAT = env->GetStaticMethodID(clsAT,
                             "currentActivityThread",
                             "()Landroid/app/ActivityThread;");
    if (!midCurAT || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: GetStaticMethodID currentActivityThread gagal");
        return nullptr;
    }
    jobject at = env->CallStaticObjectMethod(clsAT, midCurAT);
    if (!at || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: currentActivityThread() null");
        return nullptr;
    }
    logf("[MM] ActivityThread OK: %p", at);

    jmethodID midGCL = env->GetMethodID(clsAT, "getSystemContext",
                           "()Landroid/app/ContextImpl;");
    if (!midGCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] getSystemContext gagal, coba getApplication...");
        // Fallback: getApplication
        midGCL = env->GetMethodID(clsAT, "getApplication",
                     "()Landroid/app/Application;");
        if (!midGCL || env->ExceptionCheck()) {
            env->ExceptionClear();
            logf("[MM] ERROR: getApplication juga gagal");
            return nullptr;
        }
    }
    jobject appCtx = env->CallObjectMethod(at, midGCL);
    if (!appCtx || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: appCtx null");
        return nullptr;
    }
    logf("[MM] appCtx OK: %p", appCtx);

    // Ambil ClassLoader dari appCtx
    jclass    clsCtxObj = env->GetObjectClass(appCtx);
    jmethodID midGCL2   = env->GetMethodID(clsCtxObj, "getClassLoader",
                              "()Ljava/lang/ClassLoader;");
    if (!midGCL2 || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: getClassLoader dari appCtx gagal");
        return nullptr;
    }
    jobject parentCL = env->CallObjectMethod(appCtx, midGCL2);
    if (!parentCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: parentCL null");
        return nullptr;
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
        logf("[MM] ERROR: GetMethodID DCL.<init> gagal");
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

    aml->ShowToast(false, "[ModMenu] DEX loaded OK");
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
