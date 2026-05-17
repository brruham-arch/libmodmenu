#!/bin/bash
# ============================================================
# SETUP & PUSH - libmodmenu
# ============================================================

# ── LANGKAH 1: Copy header AML dari aml_dev ─────────────────
mkdir -p ~/libmodmenu/include/mod
cp ~/aml_dev/AndroidModLoader/mod/amlmod.h   ~/libmodmenu/include/mod/
cp ~/aml_dev/AndroidModLoader/mod/iaml.h     ~/libmodmenu/include/mod/
cp ~/aml_dev/AndroidModLoader/mod/interface.h ~/libmodmenu/include/mod/

# ── LANGKAH 2: Buat repo baru di GitHub ─────────────────────
gh repo create brruham-arch/libmodmenu --private --confirm

# ── LANGKAH 3: Ekstrak zip ──────────────────────────────────
cp /storage/emulated/0/Download/libmodmenu.zip ~/
cd ~ && unzip libmodmenu.zip && cd libmodmenu

# ── LANGKAH 4: Push ─────────────────────────────────────────
git init
git config user.email "email@gmail.com"
git config user.name "brruham-arch"
git remote add origin https://github.com/brruham-arch/libmodmenu.git
git add .
git commit -m "init: libmodmenu v1.0 - base template mod menu"
git branch -M main && git push -u origin main

# ── LANGKAH 5: Pantau build ─────────────────────────────────
gh run watch $(gh run list --limit 1 --json databaseId -q '.[0].databaseId')

# ── LANGKAH 6: Download artifacts ───────────────────────────
gh run download \
  $(gh run list --limit 1 --json databaseId -q '.[0].databaseId') \
  -n libmodmenu-arm32 -D ~/output/

gh run download \
  $(gh run list --limit 1 --json databaseId -q '.[0].databaseId') \
  -n modmenu-dex -D ~/output/

ls -lh ~/output/

# ── LANGKAH 7: Copy ke device ───────────────────────────────
# .so ke folder mods
cp ~/output/libmodmenu.so \
  /storage/emulated/0/Android/data/com.sampmobilerp.game/mods/

# .dex ke folder config AML
# (sesuaikan path config jika berbeda)
cp ~/output/modmenu.dex \
  /storage/emulated/0/Android/data/com.sampmobilerp.game/config/

# ── PERINTAH HARIAN ─────────────────────────────────────────
# cd ~/libmodmenu
# git add .
# git commit -m "fix: deskripsi"
# git push
# gh run watch $(gh run list --limit 1 --json databaseId -q '.[0].databaseId')
