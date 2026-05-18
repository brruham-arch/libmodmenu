package com.brruham.modmenu;

import android.app.Activity;
import android.app.ActivityThread;
import android.content.Context;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
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

    public native void nativeOnButton(int id);
    public native void nativeOnToggle(int id, boolean state);
    public native void nativeOnSlider(int id, int value);
    public native void nativeOnCheckBox(int id, boolean state);
    public native void nativeOnEditConfirm(int id, String text);

    private Context  mContext;
    private Handler  mHandler;
    private Activity mActivity;
    private View     mFabView;
    private View     mPanelView;
    private boolean  mPanelVisible = false;
    private FrameLayout mRoot;

    // Posisi FAB
    private int mFabX = 20, mFabY = 200;
    // Posisi Panel
    private int mPanelX = 80, mPanelY = 200;

    public ModMenuHelper(Context ctx) {
        mContext = ctx;
        mHandler = new Handler(Looper.getMainLooper());
    }

    public void show() {
        mHandler.post(new Runnable() {
            @Override public void run() {
                // Ambil Activity yang sedang running
                mActivity = ActivityThread.currentActivityThread().getActivity(
                    ActivityThread.currentActivityThread().getActivities().keySet().iterator().next()
                );
                if (mActivity == null) {
                    showToast("Activity null!");
                    return;
                }
                // Ambil root DecorView
                mRoot = (FrameLayout) mActivity.getWindow().getDecorView();
                buildFab();
                buildPanel();
            }
        });
    }

    public void hide() {
        mHandler.post(new Runnable() {
            @Override public void run() {
                if (mFabView != null && mRoot != null) mRoot.removeView(mFabView);
                if (mPanelView != null && mRoot != null) mRoot.removeView(mPanelView);
                mFabView = null; mPanelView = null;
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

    private void buildFab() {
        final TextView fab = new TextView(mContext);
        fab.setText("☰");
        fab.setTextSize(20);
        fab.setTextColor(Color.WHITE);
        fab.setBackgroundColor(Color.argb(220, 30, 30, 30));
        fab.setGravity(Gravity.CENTER);
        fab.setPadding(20, 10, 20, 10);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT,
            ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.leftMargin = mFabX;
        lp.topMargin  = mFabY;

        fab.setOnTouchListener(new SimpleDragListener(fab, lp, mRoot) {
            @Override public void onClick() { togglePanel(); }
        });

        mRoot.addView(fab, lp);
        mFabView = fab;
    }

    private void buildPanel() {
        LinearLayout root = new LinearLayout(mContext);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(Color.argb(230, 20, 20, 20));
        root.setPadding(16, 16, 16, 16);

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
            dpToPx(280), ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.leftMargin = mPanelX;
        lp.topMargin  = mPanelY;

        // Header drag
        TextView header = new TextView(mContext);
        header.setText("Mod Menu  ✕");
        header.setTextColor(Color.WHITE);
        header.setTextSize(16);
        header.setPadding(0, 0, 0, 12);
        header.setOnTouchListener(new SimpleDragListener(root, lp, mRoot) {
            @Override public void onClick() { togglePanel(); }
        });
        root.addView(header);

        // Scroll content
        ScrollView scroll = new ScrollView(mContext);
        LinearLayout content = new LinearLayout(mContext);
        content.setOrientation(LinearLayout.VERTICAL);

        content.addView(makeSectionLabel("── Info ──"));
        content.addView(makeLabel("label_0", "Status: Idle"));

        content.addView(makeSectionLabel("── Button ──"));
        content.addView(makeButton(0, "Aksi 1"));
        content.addView(makeButton(1, "Aksi 2"));

        content.addView(makeSectionLabel("── Toggle ──"));
        content.addView(makeSwitch(0, "Fitur A", false));
        content.addView(makeSwitch(1, "Fitur B", true));

        content.addView(makeSectionLabel("── Slider ──"));
        content.addView(makeSeekBar(0, "Nilai A", 50));
        content.addView(makeSeekBar(1, "Nilai B", 20));

        content.addView(makeSectionLabel("── CheckBox ──"));
        content.addView(makeCheckBox(0, "Opsi 1", false));
        content.addView(makeCheckBox(1, "Opsi 2", true));

        content.addView(makeSectionLabel("── Input ──"));
        content.addView(makeEditText(0, "Masukkan teks..."));

        scroll.addView(content);
        root.addView(scroll, new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, dpToPx(350)));

        mRoot.addView(root, lp);
        mPanelView = root;
        mPanelView.setVisibility(View.GONE);
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
        tv.setTag(tag); tv.setText(text);
        tv.setTextColor(Color.WHITE); tv.setTextSize(13);
        tv.setPadding(0, 4, 0, 4);
        return tv;
    }
    private Button makeButton(final int id, String label) {
        Button btn = new Button(mContext);
        btn.setText(label); btn.setTextColor(Color.WHITE);
        btn.setBackgroundColor(Color.argb(255, 50, 100, 200));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(0, 4, 0, 4); btn.setLayoutParams(lp);
        btn.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                showToast("Button " + id); nativeOnButton(id);
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
            @Override public void onCheckedChanged(CompoundButton b, boolean c) {
                showToast("Toggle " + id + ": " + (c?"ON":"OFF")); nativeOnToggle(id, c);
            }
        });
        row.addView(tv, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(sw);
        return row;
    }
    private LinearLayout makeSeekBar(final int id, String label, int def) {
        LinearLayout col = new LinearLayout(mContext);
        col.setOrientation(LinearLayout.VERTICAL);
        LinearLayout row = makeRow();
        TextView tvL = makeLabel("", label);
        final TextView tvV = makeLabel("", String.valueOf(def));
        tvV.setTextColor(Color.argb(255, 100, 200, 100));
        row.addView(tvL, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(tvV); col.addView(row);
        SeekBar sb = new SeekBar(mContext);
        sb.setMax(100); sb.setProgress(def);
        sb.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar s, int p, boolean f) { tvV.setText(String.valueOf(p)); if(f) nativeOnSlider(id, p); }
            @Override public void onStartTrackingTouch(SeekBar s) {}
            @Override public void onStopTrackingTouch(SeekBar s) { showToast("Slider " + id + ": " + s.getProgress()); }
        });
        col.addView(sb); return col;
    }
    private LinearLayout makeCheckBox(final int id, String label, boolean def) {
        LinearLayout row = makeRow();
        TextView tv = makeLabel("", label);
        CheckBox cb = new CheckBox(mContext);
        cb.setChecked(def);
        cb.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override public void onCheckedChanged(CompoundButton b, boolean c) {
                showToast("CheckBox " + id + ": " + c); nativeOnCheckBox(id, c);
            }
        });
        row.addView(tv, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(cb); return row;
    }
    private LinearLayout makeEditText(final int id, String hint) {
        LinearLayout row = makeRow();
        final EditText et = new EditText(mContext);
        et.setHint(hint); et.setHintTextColor(Color.GRAY);
        et.setTextColor(Color.WHITE); et.setBackgroundColor(Color.argb(255, 40, 40, 40));
        et.setSingleLine(true);
        Button btn = new Button(mContext);
        btn.setText("OK"); btn.setTextColor(Color.WHITE);
        btn.setBackgroundColor(Color.argb(255, 50, 150, 50));
        btn.setOnClickListener(new View.OnClickListener() {
            @Override public void onClick(View v) {
                String t = et.getText().toString();
                showToast("Input " + id + ": " + t); nativeOnEditConfirm(id, t);
            }
        });
        row.addView(et, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        row.addView(btn); return row;
    }
    private LinearLayout makeRow() {
        LinearLayout row = new LinearLayout(mContext);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding(0, 4, 0, 4);
        return row;
    }
    private int dpToPx(int dp) {
        return Math.round(dp * mContext.getResources().getDisplayMetrics().density);
    }
    private void showToast(final String msg) {
        mHandler.post(new Runnable() {
            @Override public void run() {
                Toast.makeText(mContext, msg, Toast.LENGTH_SHORT).show();
            }
        });
    }

    // ── Drag listener sederhana ───────────────────────────────────────────────
    private static abstract class SimpleDragListener implements View.OnTouchListener {
        private final View mTarget;
        private final FrameLayout.LayoutParams mLP;
        private final FrameLayout mRoot;
        private int mIX, mIY, mTX, mTY;
        private boolean mDragged;

        SimpleDragListener(View target, FrameLayout.LayoutParams lp, FrameLayout root) {
            mTarget = target; mLP = lp; mRoot = root;
        }
        public abstract void onClick();

        @Override
        public boolean onTouch(View v, MotionEvent e) {
            switch (e.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    mIX = mLP.leftMargin; mIY = mLP.topMargin;
                    mTX = (int)e.getRawX(); mTY = (int)e.getRawY();
                    mDragged = false; return true;
                case MotionEvent.ACTION_MOVE:
                    int dx = (int)e.getRawX() - mTX;
                    int dy = (int)e.getRawY() - mTY;
                    if (Math.abs(dx) > 5 || Math.abs(dy) > 5) mDragged = true;
                    if (mDragged) {
                        mLP.leftMargin = mIX + dx;
                        mLP.topMargin  = mIY + dy;
                        mRoot.updateViewLayout(mTarget, mLP);
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
