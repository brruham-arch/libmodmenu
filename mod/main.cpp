#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <jni.h>
#include "mod/amlmod.h"

MYMOD(brruham.modmenu, ModMenu, 1.0, brruham)

// ── State ─────────────────────────────────────────────────────────────────────
static jobject g_helper   = nullptr; // instance ModMenuHelper
static jclass  g_cls      = nullptr;
static bool    g_ready    = false;

// ── Native callbacks dari Java ────────────────────────────────────────────────
// Dipanggil saat user tap Button di panel
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnButton(
        JNIEnv*, jobject, jint id) {
    // TODO: implementasi per button id
    // Contoh: if (id == 0) teleport(); else if (id == 1) spawnVehicle();
    aml->ShowToast(false, "Button %d ditekan", (int)id);
}

// Dipanggil saat user toggle Switch
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnToggle(
        JNIEnv*, jobject, jint id, jboolean state) {
    // TODO: implementasi per toggle id
    aml->ShowToast(false, "Toggle %d: %s", (int)id, state ? "ON" : "OFF");
}

// Dipanggil saat user geser SeekBar (saat release)
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnSlider(
        JNIEnv*, jobject, jint id, jint value) {
    // TODO: implementasi per slider id
    aml->ShowToast(false, "Slider %d: %d", (int)id, (int)value);
}

// Dipanggil saat user centang/uncekang CheckBox
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnCheckBox(
        JNIEnv*, jobject, jint id, jboolean state) {
    // TODO: implementasi per checkbox id
    aml->ShowToast(false, "CheckBox %d: %s", (int)id, state ? "true" : "false");
}

// Dipanggil saat user tekan OK di EditText
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnEditConfirm(
        JNIEnv* env, jobject, jint id, jstring jtext) {
    const char* text = env->GetStringUTFChars(jtext, nullptr);
    // TODO: implementasi per input id
    aml->ShowToast(false, "Input %d: %s", (int)id, text);
    env->ReleaseStringUTFChars(jtext, text);
}

// ── Helper: update label dari C++ ────────────────────────────────────────────
// Panggil ini dari mana saja untuk update TextView di panel
// id = 0 untuk label utama (Status)
void modmenu_update_label(int id, const char* text) {
    if (!g_ready || !g_helper) return;
    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) return;

    jmethodID mid = env->GetMethodID(g_cls, "updateLabel",
                        "(ILjava/lang/String;)V");
    if (!mid) return;
    jstring jtext = env->NewStringUTF(text);
    env->CallVoidMethod(g_helper, mid, (jint)id, jtext);
    env->DeleteLocalRef(jtext);
}

// ── Load .dex dan inisialisasi ModMenuHelper ──────────────────────────────────
static void* init_thread(void*) {
    usleep(2000000); // tunggu 2 detik agar game fully loaded

    JNIEnv* env = aml->GetJNIEnvironment();
    if (!env) return nullptr;

    jobject ctx = aml->GetAppContextObject();
    if (!ctx) return nullptr;

    // Path .dex dari config path AML
    char dexPath[256];
    char optPath[256];
    snprintf(dexPath, sizeof(dexPath), "%s/modmenu.dex", aml->GetConfigPath());
    snprintf(optPath, sizeof(optPath), "%s/modmenu_opt",  aml->GetConfigPath());

    // Cek file ada
    FILE* f = fopen(dexPath, "r");
    if (!f) {
        aml->ShowToast(true, "[ModMenu] modmenu.dex tidak ditemukan di config!");
        return nullptr;
    }
    fclose(f);

    // DexClassLoader
    jclass    clsDCL   = env->FindClass("dalvik/system/DexClassLoader");
    jmethodID midDCLi  = env->GetMethodID(clsDCL, "<init>",
                             "(Ljava/lang/String;Ljava/lang/String;"
                             "Ljava/lang/String;Ljava/lang/ClassLoader;)V");

    // Ambil parent ClassLoader dari Context
    jclass    clsCtx   = env->GetObjectClass(ctx);
    jmethodID midGCL   = env->GetMethodID(clsCtx, "getClassLoader",
                             "()Ljava/lang/ClassLoader;");
    jobject   parentCL = env->CallObjectMethod(ctx, midGCL);

    jstring jDexPath = env->NewStringUTF(dexPath);
    jstring jOptPath = env->NewStringUTF(optPath);

    jobject dcl = env->NewObject(clsDCL, midDCLi,
                      jDexPath, jOptPath, nullptr, parentCL);
    env->DeleteLocalRef(jDexPath);
    env->DeleteLocalRef(jOptPath);

    if (!dcl || env->ExceptionCheck()) {
        env->ExceptionClear();
        aml->ShowToast(true, "[ModMenu] Gagal load DexClassLoader");
        return nullptr;
    }

    // Load class ModMenuHelper
    jmethodID midLC  = env->GetMethodID(clsDCL, "loadClass",
                           "(Ljava/lang/String;)Ljava/lang/Class;");
    jstring   jCls   = env->NewStringUTF("com.brruham.modmenu.ModMenuHelper");
    jclass    cls    = (jclass)env->CallObjectMethod(dcl, midLC, jCls);
    env->DeleteLocalRef(jCls);

    if (!cls || env->ExceptionCheck()) {
        env->ExceptionClear();
        aml->ShowToast(true, "[ModMenu] Gagal load class ModMenuHelper");
        return nullptr;
    }

    // Register native methods
    JNINativeMethod methods[] = {
        { "nativeOnButton",      "(I)V",                  (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnButton      },
        { "nativeOnToggle",      "(IZ)V",                 (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnToggle      },
        { "nativeOnSlider",      "(II)V",                 (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnSlider      },
        { "nativeOnCheckBox",    "(IZ)V",                 (void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnCheckBox    },
        { "nativeOnEditConfirm", "(ILjava/lang/String;)V",(void*)Java_com_brruham_modmenu_ModMenuHelper_nativeOnEditConfirm },
    };
    env->RegisterNatives(cls, methods, 5);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        aml->ShowToast(true, "[ModMenu] Gagal register native methods");
        return nullptr;
    }

    // Buat instance ModMenuHelper(context)
    jmethodID midInit = env->GetMethodID(cls, "<init>",
                            "(Landroid/content/Context;)V");
    jobject helper = env->NewObject(cls, midInit, ctx);
    if (!helper || env->ExceptionCheck()) {
        env->ExceptionClear();
        aml->ShowToast(true, "[ModMenu] Gagal buat instance ModMenuHelper");
        return nullptr;
    }

    // Simpan global ref
    g_cls    = (jclass)env->NewGlobalRef(cls);
    g_helper = env->NewGlobalRef(helper);
    g_ready  = true;

    // Panggil show() untuk tampilkan FAB
    jmethodID midShow = env->GetMethodID(g_cls, "show", "()V");
    env->CallVoidMethod(g_helper, midShow);

    aml->ShowToast(false, "[ModMenu] Aktif - tap ☰ untuk buka panel");
    return nullptr;
}

// ── Entry points ──────────────────────────────────────────────────────────────
ON_MOD_PRELOAD() {
    g_helper = nullptr;
    g_cls    = nullptr;
    g_ready  = false;
}

ON_MOD_LOAD() {
    pthread_t tid;
    if (pthread_create(&tid, nullptr, init_thread, nullptr) == 0)
        pthread_detach(tid);
}
