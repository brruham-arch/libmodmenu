package com.brruham.modmenu;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Looper;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;

public class ModMenuHelper {

    // ── Native callbacks ──────────────────────────────────────────────────────
    public native void nativeOnButton(int id);
    public native void nativeOnToggle(int id, boolean state);
    public native void nativeOnSlider(int id, int value);
    public native void nativeOnCheckBox(int id, boolean state);
    public native void nativeOnEditConfirm(int id, String text);

    // ── State ─────────────────────────────────────────────────────────────────
    private Context         mContext;
    private WindowManager   mWM;
    private Handler         mHandler;
    private View            mFabView;      // tombol floating kecil
    private View            mPanelView;    // panel utama
    private boolean         mPanelVisible = false;

    // LayoutParams untuk FAB dan Panel
    private WindowManager.LayoutParams mFabLP;
    private WindowManager.LayoutParams mPanelLP;

    // ── Init ──────────────────────────────────────────────────────────────────
    public ModMenuHelper(Context ctx) {
        mContext = ctx;
        mWM      = (WindowManager) ctx.getSystemService(Context.WINDOW_SERVICE);
        mHandler = new Handler(Looper.getMainLooper());
    }

    // ── Dipanggil dari C++ via JNI ────────────────────────────────────────────
    public void show() {
        mHandler.post(new Runnable() {
            @Override public void run() {
                if (mFabView == null) {
                    buildFab();
                    buildPanel();
                }
            }
        });
    }

    public void hide() {
        mHandler.post(new Runnable() {
            @Override public void run() {
                removeFab();
                removePanel();
            }
        });
    }

    public void updateLabel(final int id, final String text) {
        mHandler.post(new Runnable() {
            @Override public void run() {
                if (mPanelView == null) return;
                View v = mPanelView.findViewWithTag("label_" + id);
                if (v instanceof TextView) ((TextView) v).setText(text);
            }
        });
    }

    // ── FAB (tombol floating kecil) ───────────────────────────────────────────
    private void buildFab() {
        final TextView fab = new TextView(mContext);
        fab.setText("☰");
        fab.setTextSize(20);
        fab.setTextColor(Color.WHITE);
        fab.setBackgroundColor(Color.argb(200, 30, 30, 30));
        fab.setGravity(Gravity.CENTER);
        fab.setPadding(20, 10, 20, 10);

        mFabLP = new WindowManager.LayoutParams(
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_PHONE,
            WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE,
            PixelFormat.TRANSLUCENT
        );
        mFabLP.gravity = Gravity.TOP | Gravity.START;
        mFabLP.x = 20;
        mFabLP.y = 100;

        // Drag FAB
        fab.setOnTouchListener(new DragTouchListener(fab, mFabLP, mWM) {
            @Override public void onClick() {
                togglePanel();
            }
        });

        mWM.addView(fab, mFabLP);
        mFabView = fab;
    }

    private void removeFab() {
        if (mFabView != null) {
            try { mWM.removeView(mFabView); } catch (Exception e) {}
            mFabView = null;
        }
    }

    // ── Panel utama ───────────────────────────────────────────────────────────
    private void buildPanel() {
        // Root container
        LinearLayout root = new LinearLayout(mContext);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.argb(230, 20, 20, 20));
        root.setPadding(16, 16, 16, 16);

        // ── Header + drag ─────────────────────────────────────────────────────
        TextView header = new TextView(mContext);
        header.setText("Mod Menu  ✕");
        header.setTextColor(Color.WHITE);
        header.setTextSize(16);
        header.setPadding(0, 0, 0, 12);

        mPanelLP = new WindowManager.LayoutParams(
            dpToPx(280),
            WindowManager.LayoutParams.WRAP_CONTENT,
            WindowManager.LayoutParams.TYPE_PHONE,
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | WindowManager.LayoutParams.FLAG_WATCH_OUTSIDE_TOUCH,
            PixelFormat.TRANSLUCENT
        );
        mPanelLP.gravity = Gravity.TOP | Gravity.START;
        mPanelLP.x = 80;
        mPanelLP.y = 100;

        // Drag panel via header, tap X untuk tutup
        header.setOnTouchListener(new DragTouchListener(root, mPanelLP, mWM) {
            @Override public void onClick() {
                togglePanel();
            }
        });
        root.addView(header);

        // ── ScrollView berisi semua widget ────────────────────────────────────
        ScrollView scroll = new ScrollView(mContext);
        LinearLayout content = new LinearLayout(mContext);
        content.setOrientation(LinearLayout.VERTICAL);

        // 1. TextView label
        content.addView(makeSectionLabel("── Info ──"));
        TextView infoLabel = makeLabel("label_0", "Status: Idle");
        content.addView(infoLabel);

        // 2. Button
        content.addView(makeSectionLabel("── Button ──"));
        content.addView(makeButton(0, "Aksi 1"));
        content.addView(makeButton(1, "Aksi 2"));

        // 3. Switch / Toggle
        content.addView(makeSectionLabel("── Toggle ──"));
        content.addView(makeSwitch(0, "Fitur A", false));
        content.addView(makeSwitch(1, "Fitur B", true));

        // 4. SeekBar / Slider
        content.addView(makeSectionLabel("── Slider ──"));
        content.addView(makeSeekBar(0, "Nilai A", 50));
        content.addView(makeSeekBar(1, "Nilai B", 20));

        // 5. CheckBox
        content.addView(makeSectionLabel("── CheckBox ──"));
        content.addView(makeCheckBox(0, "Opsi 1", false));
        content.addView(makeCheckBox(1, "Opsi 2", true));
        content.addView(makeCheckBox(2, "Opsi 3", false));

        // 6. EditText + confirm
        content.addView(makeSectionLabel("── Input ──"));
        content.addView(makeEditText(0, "Masukkan teks..."));
        content.addView(makeEditText(1, "Masukkan angka..."));

        scroll.addView(content);
        LinearLayout.LayoutParams scrollLP = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, dpToPx(400));
        root.addView(scroll, scrollLP);

        mWM.addView(root, mPanelLP);
        mPanelView = root;
        mPanelView.setVisibility(View.GONE);
    }

    private void removePanel() {
        if (mPanelView != null) {
            try { mWM.removeView(mPanelView); } catch (Exception e) {}
            mPanelView = null;
            mPanelVisible = false;
        }
    }

    private void togglePanel() {
        if (mPanelView == null) return;
        mPanelVisible = !mPanelVisible;
        mPanelView.setVisibility(mPanelVisible ? View.VISIBLE : View.GONE);
    }

    // ── Widget builders ───────────────────────────────────────────────────────
    private TextView makeSectionLabel(String text) {
        TextView tv = new TextView(mContext);
        tv.setText(text);
        tv.setTextColor(Color.argb(255, 150, 150, 150));
        tv.setTextSize(11);
        tv.setPadding(0, 10, 0, 4);
        return tv;
    }

    private TextView makeLabel(String tag, String text) {
        TextView tv = new TextView(mContext);
        tv.setTag(tag);
        tv.setText(text);
        tv.setTextColor(Color.WHITE);
        tv.setTextSize(13);
        tv.setPadding(0, 4, 0, 4);
        return tv;
    }

    private Button makeButton(final int id, String label) {
        Button btn = new Button(mContext);
        btn.setText(label);
        btn.setTextColor(Color.WHITE);
        btn.setBackgroundColor(Color.argb(255, 50, 100, 200));
        btn.setPadding(12, 8, 12, 8);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, 4, 0, 4);
        btn.setLayoutParams(lp);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                showToast("Button " + id + " clicked");
                nativeOnButton(id);
            }
        });
        return btn;
    }

    private LinearLayout makeSwitch(final int id, String label, boolean def) {
        LinearLayout row = makeRow();
        TextView tv = makeLabel("", label);
        Switch sw = new Switch(mContext);
        sw.setChecked(def);
        sw.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(CompoundButton b, boolean checked) {
                showToast("Toggle " + id + ": " + (checked ? "ON" : "OFF"));
                nativeOnToggle(id, checked);
            }
        });
        row.addView(tv, new LinearLayout.LayoutParams(0,
            ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(sw);
        return row;
    }

    private LinearLayout makeSeekBar(final int id, String label, int def) {
        LinearLayout col = new LinearLayout(mContext);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setPadding(0, 4, 0, 4);

        LinearLayout row = makeRow();
        TextView tvLabel = makeLabel("", label);
        final TextView tvVal = makeLabel("", String.valueOf(def));
        tvVal.setTextColor(Color.argb(255, 100, 200, 100));
        row.addView(tvLabel, new LinearLayout.LayoutParams(0,
            ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(tvVal);
        col.addView(row);

        SeekBar sb = new SeekBar(mContext);
        sb.setMax(100);
        sb.setProgress(def);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean fromUser) {
                tvVal.setText(String.valueOf(p));
                if (fromUser) nativeOnSlider(id, p);
            }
            @Override public void onStartTrackingTouch(SeekBar s) {}
            @Override public void onStopTrackingTouch(SeekBar s) {
                showToast("Slider " + id + ": " + s.getProgress());
            }
        });
        col.addView(sb);
        return col;
    }

    private LinearLayout makeCheckBox(final int id, String label, boolean def) {
        LinearLayout row = makeRow();
        TextView tv = makeLabel("", label);
        CheckBox cb = new CheckBox(mContext);
        cb.setChecked(def);
        cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(CompoundButton b, boolean checked) {
                showToast("CheckBox " + id + ": " + checked);
                nativeOnCheckBox(id, checked);
            }
        });
        row.addView(tv, new LinearLayout.LayoutParams(0,
            ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(cb);
        return row;
    }

    private LinearLayout makeEditText(final int id, String hint) {
        LinearLayout row = makeRow();
        final EditText et = new EditText(mContext);
        et.setHint(hint);
        et.setHintTextColor(Color.GRAY);
        et.setTextColor(Color.WHITE);
        et.setBackgroundColor(Color.argb(255, 40, 40, 40));
        et.setPadding(8, 6, 8, 6);
        et.setSingleLine(true);

        Button btn = new Button(mContext);
        btn.setText("OK");
        btn.setTextColor(Color.WHITE);
        btn.setBackgroundColor(Color.argb(255, 50, 150, 50));
        btn.setPadding(16, 4, 16, 4);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                String txt = et.getText().toString();
                showToast("Input " + id + ": " + txt);
                nativeOnEditConfirm(id, txt);
            }
        });
        row.addView(et, new LinearLayout.LayoutParams(0,
            ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(btn);
        return row;
    }

    // ── Util ──────────────────────────────────────────────────────────────────
    private LinearLayout makeRow() {
        LinearLayout row = new LinearLayout(mContext);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, 4, 0, 4);
        return row;
    }

    private int dpToPx(int dp) {
        float density = mContext.getResources().getDisplayMetrics().density;
        return Math.round(dp * density);
    }

    private void showToast(final String msg) {
        mHandler.post(new Runnable() {
            @Override public void run() {
                Toast.makeText(mContext, msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    // ── DragTouchListener ─────────────────────────────────────────────────────
    private static abstract class DragTouchListener implements View.OnTouchListener {
        private final View                        mTarget;
        private final WindowManager.LayoutParams  mLP;
        private final WindowManager               mWM;
        private int mInitX, mInitY, mTouchX, mTouchY;
        private boolean mDragged = false;

        DragTouchListener(View target, WindowManager.LayoutParams lp, WindowManager wm) {
            mTarget = target; mLP = lp; mWM = wm;
        }

        public abstract void onClick();

        @Override
        public boolean onTouch(View v, MotionEvent e) {
            switch (e.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    mInitX  = mLP.x; mInitY  = mLP.y;
                    mTouchX = (int) e.getRawX(); mTouchY = (int) e.getRawY();
                    mDragged = false;
                    return true;
                case MotionEvent.ACTION_MOVE:
                    int dx = (int) e.getRawX() - mTouchX;
                    int dy = (int) e.getRawY() - mTouchY;
                    if (Math.abs(dx) > 5 || Math.abs(dy) > 5) mDragged = true;
                    if (mDragged) {
                        mLP.x = mInitX + dx;
                        mLP.y = mInitY + dy;
                        mWM.updateViewLayout(mTarget, mLP);
                    }
                    return true;
                case MotionEvent.ACTION_UP:
                    if (!mDragged) onClick();
                    return true;
            }
            return false;
        }
    }
}
