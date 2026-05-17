#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
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

    // Ambil JavaVM dari libandroid_runtime
    void* librt = dlopen("libandroid_runtime.so", RTLD_NOW | RTLD_NOLOAD);
    if (!librt) { logf("[MM] ERROR: libandroid_runtime null"); return nullptr; }

    auto getVMs = (jint(*)(JavaVM**, jsize, jsize*))
                      dlsym(librt, "JNI_GetCreatedJavaVMs");
    if (!getVMs) { logf("[MM] ERROR: JNI_GetCreatedJavaVMs null"); return nullptr; }

    JavaVM* jvm = nullptr;
    jsize   cnt = 0;
    if (getVMs(&jvm, 1, &cnt) != JNI_OK || cnt == 0 || !jvm) {
        logf("[MM] ERROR: GetCreatedJavaVMs gagal");
        return nullptr;
    }
    logf("[MM] jvm OK: %p", jvm);

    // Attach thread ini ke JVM — dapat env yang valid
    JNIEnv* env = nullptr;
    if (jvm->AttachCurrentThread(&env, nullptr) != JNI_OK || !env) {
        logf("[MM] ERROR: AttachCurrentThread gagal");
        return nullptr;
    }
    logf("[MM] env (attached): %p", env);

    // Ambil Context via ActivityThread (tidak perlu currentActivityThread,
    // cukup pakai AML tapi dengan env ini)
    // Pakai Thread.currentThread().getContextClassLoader() — paling reliable
    jclass    clsThread = env->FindClass("java/lang/Thread");
    jmethodID midCT     = env->GetStaticMethodID(clsThread,
                              "currentThread", "()Ljava/lang/Thread;");
    jmethodID midGCCL   = env->GetMethodID(clsThread,
                              "getContextClassLoader",
                              "()Ljava/lang/ClassLoader;");
    jobject curThread = env->CallStaticObjectMethod(clsThread, midCT);
    jobject parentCL  = env->CallObjectMethod(curThread, midGCCL);
    logf("[MM] parentCL: %p", parentCL);

    if (!parentCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] WARN: getContextClassLoader null, coba bootstrap CL");
        // Fallback: pakai ClassLoader.getSystemClassLoader()
        jclass    clsCL   = env->FindClass("java/lang/ClassLoader");
        jmethodID midSCL  = env->GetStaticMethodID(clsCL,
                                "getSystemClassLoader",
                                "()Ljava/lang/ClassLoader;");
        parentCL = env->CallStaticObjectMethod(clsCL, midSCL);
        logf("[MM] systemCL: %p", parentCL);
        if (!parentCL || env->ExceptionCheck()) {
            env->ExceptionClear();
            logf("[MM] ERROR: parentCL tetap null");
            jvm->DetachCurrentThread();
            return nullptr;
        }
    }

    // Fix path
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
    if (!f) { logf("[MM] ERROR: dex tidak ada"); jvm->DetachCurrentThread(); return nullptr; }
    fclose(f);

    struct stat st;
    if (stat(optPath, &st) != 0) mkdir(optPath, 0755);

    // DexClassLoader
    jclass clsDCL = env->FindClass("dalvik/system/DexClassLoader");
    if (!clsDCL || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: FindClass DCL gagal");
        jvm->DetachCurrentThread();
        return nullptr;
    }
    jmethodID midDCLi = env->GetMethodID(clsDCL, "<init>",
        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/ClassLoader;)V");
    if (!midDCLi || env->ExceptionCheck()) {
        env->ExceptionClear();
        logf("[MM] ERROR: DCL.<init> null");
        jvm->DetachCurrentThread();
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
        jvm->DetachCurrentThread();
        return nullptr;
    }
    logf("[MM] DCL instance OK");

    jmethodID midLC  = env->GetMethodID(clsDCL, "loadClass",
                           "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring jClsName = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass  cls      = (jclass)env->CallObjectMethod(dcl, midLC, jClsName);
    env->DeleteLocalRef(jClsName);

    if (!cls || env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        logf("[MM] ERROR: loadClass gagal");
        jvm->DetachCurrentThread();
        return nullptr;
    }
    logf("[MM] loadClass OK!");

    aml->ShowToast(false, "[ModMenu] DEX loaded OK!");
    jvm->DetachCurrentThread();
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
