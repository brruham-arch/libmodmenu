# libmodmenu

Base template mod menu native untuk SA-MP Android.  
WindowManager overlay — draggable, interaktif, semua widget tersedia.

## Arsitektur

```
libmodmenu.so  (C++ native, AML mod)
    │
    └── load modmenu.dex via DexClassLoader
            │
            └── ModMenuHelper.java
                    ├── FAB floating (drag + tap untuk buka/tutup)
                    └── Panel berisi:
                        ├── TextView  (label/info realtime)
                        ├── Button    (x2)
                        ├── Switch    (x2)
                        ├── SeekBar   (x2)
                        ├── CheckBox  (x3)
                        └── EditText  (x2, dengan tombol OK)
```

## Install

### File yang dibutuhkan (2 file dari artifact GitHub Actions):

| File | Taruh di |
|---|---|
| `libmodmenu.so` | `/storage/emulated/0/Android/data/com.sampmobilerp.game/mods/` |
| `modmenu.dex` | path dari `aml->GetConfigPath()` (biasanya `/storage/emulated/0/Android/data/com.sampmobilerp.game/config/`) |

### Permission
Game harus punya permission `SYSTEM_ALERT_WINDOW` (overlay).  
SA-MP Mobile RP biasanya sudah punya.

## Cara pakai sebagai template

### Tambah fitur ke Button:
```cpp
// di main.cpp, fungsi nativeOnButton
extern "C" void Java_com_brruham_modmenu_ModMenuHelper_nativeOnButton(
        JNIEnv*, jobject, jint id) {
    if (id == 0) {
        // aksi button 0
    } else if (id == 1) {
        // aksi button 1
    }
}
```

### Update label dari C++:
```cpp
modmenu_update_label(0, "Status: Active");
```

### Tambah widget baru di Java:
Edit `ModMenuHelper.java`, tambah di bagian `buildPanel()`:
```java
content.addView(makeButton(2, "Aksi Baru"));
```
Lalu handle di `nativeOnButton` dengan `id == 2`.

## Build

Push ke `main` atau `dev` → GitHub Actions otomatis build.  
Download 2 artifact: `libmodmenu-arm32` dan `modmenu-dex`.

## Log

Tidak ada log file — semua feedback via `aml->ShowToast`.

## Author

brruham-arch
