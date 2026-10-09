package org.nevergone.recomp;

import android.content.Context;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.LinearLayout;

/**
 * Temporary Android IME bridge for the remaining CharacterName text-entry path.
 *
 * Career selection, SingleSelectHero Confirm, CharacterName Random/Confirm/Cancel,
 * and modal touch ownership are now provided by the reconstructed native surface.
 * This view only mirrors the active role name into an Android EditText so the
 * platform IME can still edit the recovered native CharacterName state.
 */
final class FreshRoleCompatOverlay extends LinearLayout {
    private static final int MODE_HIDDEN = 0;
    private static final int MODE_CAREER = 1;
    private static final int MODE_NAME = 2;
    private static final long POLL_MS = 100L;

    private static native int nativeMode();
    private static native String nativeRoleName();
    private static native boolean nativeRandomizePending();
    private static native boolean nativeSetRoleName(String roleName);
    private static native String nativeDispatchNameAction(int tag);

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final EditText roleName;
    private boolean polling;
    private boolean syncingName;
    private int previousMode = MODE_HIDDEN;

    private final Runnable poll = new Runnable() {
        @Override
        public void run() {
            if (!polling) return;
            refreshFromNative();
            if (polling) handler.postDelayed(this, POLL_MS);
        }
    };

    FreshRoleCompatOverlay(Context context) {
        super(context);
        setOrientation(VERTICAL);
        setPadding(20, 16, 20, 16);
        setBackgroundColor(0xcc111111);
        setVisibility(GONE);

        roleName = new EditText(context);
        roleName.setSingleLine(true);
        roleName.setHint("Character name (max 18 UTF-8 bytes)");
        roleName.setTextColor(Color.WHITE);
        roleName.setHintTextColor(0xffaaaaaa);
        roleName.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
        roleName.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable editable) {
                if (!syncingName && nativeMode() == MODE_NAME) {
                    nativeSetRoleName(editable.toString());
                }
            }
        });
        addView(roleName, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        polling = true;
        handler.post(poll);
    }

    @Override
    protected void onDetachedFromWindow() {
        polling = false;
        handler.removeCallbacks(poll);
        hideKeyboard();
        super.onDetachedFromWindow();
    }

    private void refreshFromNative() {
        int mode = nativeMode();
        if (mode == MODE_HIDDEN || mode == MODE_CAREER) {
            if (previousMode == MODE_NAME) hideKeyboard();
            previousMode = mode;
            setVisibility(GONE);
            return;
        }

        setVisibility(mode == MODE_NAME ? VISIBLE : GONE);
        if (mode == MODE_NAME) {
            if (previousMode != MODE_NAME && nativeRandomizePending()) {
                // CharacterNameLayer::CretaUI immediately triggers the shipped
                // random-name callback. Keep that one-time semantic side effect
                // here until the edit-box itself is reconstructed natively.
                nativeDispatchNameAction(3);
            }
            syncRoleName();
            if (previousMode != MODE_NAME) {
                roleName.requestFocus();
                handler.postDelayed(this::showKeyboard, 80L);
            }
        }
        previousMode = mode;
    }

    private void syncRoleName() {
        String nativeName = nativeRoleName();
        if (nativeName == null) nativeName = "";
        String current = roleName.getText().toString();
        if (current.equals(nativeName)) return;
        syncingName = true;
        roleName.setText(nativeName);
        roleName.setSelection(roleName.length());
        syncingName = false;
    }

    private void showKeyboard() {
        if (getVisibility() != VISIBLE) return;
        InputMethodManager manager =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (manager != null) manager.showSoftInput(roleName, InputMethodManager.SHOW_IMPLICIT);
    }

    private void hideKeyboard() {
        InputMethodManager manager =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (manager != null) manager.hideSoftInputFromWindow(getWindowToken(), 0);
        roleName.clearFocus();
    }
}
