package org.nevergone.recomp;

import android.content.Context;
import android.graphics.Color;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * Temporary project-owned input surface for the fresh-account role path.
 *
 * This intentionally does not imitate the unrecovered SingleSelectHero touch
 * presentation. It exposes the reconstructed semantic state until the native
 * career/confirm routing and CharacterName compositor are connected.
 */
final class FreshRoleCompatOverlay extends LinearLayout {
    private static final int MODE_HIDDEN = 0;
    private static final int MODE_CAREER = 1;
    private static final int MODE_NAME = 2;
    private static final int CAREER_COUNT = 5;
    private static final long POLL_MS = 100L;

    private static native int nativeMode();
    private static native long nativeSelectedCareer();
    private static native boolean nativeSelectCareer(long career);
    private static native boolean nativeConfirmCareer();
    private static native String nativeRoleName();
    private static native boolean nativeRandomizePending();
    private static native boolean nativeSetRoleName(String roleName);
    private static native String nativeDispatchNameAction(int tag);

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final LinearLayout careerPanel;
    private final LinearLayout namePanel;
    private final Button[] careerButtons = new Button[CAREER_COUNT];
    private final EditText roleName;
    private final TextView status;
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

        TextView heading = new TextView(context);
        heading.setText("Compatibility input");
        heading.setTextColor(Color.WHITE);
        heading.setTextSize(13.0f);
        addView(heading, matchWrap());

        // Initialize the final status field before any listener lambda captures
        // it. Keep addView(status, ...) at the original location below so the
        // visible layout order remains unchanged.
        status = new TextView(context);
        status.setTextColor(0xffdddddd);
        status.setTextSize(12.0f);
        status.setGravity(Gravity.START);

        careerPanel = new LinearLayout(context);
        careerPanel.setOrientation(VERTICAL);
        addView(careerPanel, matchWrap());

        LinearLayout careers = new LinearLayout(context);
        careers.setOrientation(HORIZONTAL);
        careerPanel.addView(careers, matchWrap());

        for (int index = 0; index < CAREER_COUNT; ++index) {
            final long career = index + 1L;
            Button button = makeButton(context, Long.toString(career));
            button.setOnClickListener(view -> {
                boolean ok = nativeSelectCareer(career);
                status.setText(ok
                        ? "Career " + career + " selected"
                        : "Career " + career + " selection blocked");
                refreshCareerButtons();
            });
            careerButtons[index] = button;
            careers.addView(button, weightedWrap());
        }

        Button confirmCareer = makeButton(context, "Confirm career");
        confirmCareer.setOnClickListener(view -> {
            boolean ok = nativeConfirmCareer();
            status.setText(ok ? "Opening character name" : "Career cannot be confirmed");
            refreshFromNative();
        });
        careerPanel.addView(confirmCareer, matchWrap());

        namePanel = new LinearLayout(context);
        namePanel.setOrientation(VERTICAL);
        namePanel.setVisibility(GONE);
        addView(namePanel, matchWrap());

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
        namePanel.addView(roleName, matchWrap());

        LinearLayout nameActions = new LinearLayout(context);
        nameActions.setOrientation(HORIZONTAL);
        namePanel.addView(nameActions, matchWrap());

        Button random = makeButton(context, "Random");
        random.setOnClickListener(view -> {
            status.setText(nativeDispatchNameAction(3));
            syncRoleName();
        });
        nameActions.addView(random, weightedWrap());

        Button submit = makeButton(context, "Confirm");
        submit.setOnClickListener(view -> {
            nativeSetRoleName(roleName.getText().toString());
            status.setText(nativeDispatchNameAction(1));
            syncRoleName();
        });
        nameActions.addView(submit, weightedWrap());

        Button cancel = makeButton(context, "Cancel");
        cancel.setOnClickListener(view -> {
            status.setText(nativeDispatchNameAction(2));
            hideKeyboard();
            refreshFromNative();
        });
        nameActions.addView(cancel, weightedWrap());

        addView(status, matchWrap());
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
        if (mode == MODE_HIDDEN) {
            if (previousMode == MODE_NAME) hideKeyboard();
            previousMode = mode;
            setVisibility(GONE);
            return;
        }

        setVisibility(VISIBLE);
        careerPanel.setVisibility(mode == MODE_CAREER ? VISIBLE : GONE);
        namePanel.setVisibility(mode == MODE_NAME ? VISIBLE : GONE);

        if (mode == MODE_CAREER) {
            refreshCareerButtons();
        } else if (mode == MODE_NAME) {
            if (previousMode != MODE_NAME && nativeRandomizePending()) {
                // CharacterNameLayer::CretaUI immediately triggers the shipped
                // random-name callback. Execute it only while the native state
                // still marks that initial request pending, so Activity
                // recreation does not generate a second name after success.
                status.setText(nativeDispatchNameAction(3));
            }
            syncRoleName();
            if (previousMode != MODE_NAME) {
                roleName.requestFocus();
                handler.postDelayed(this::showKeyboard, 80L);
            }
        }
        previousMode = mode;
    }

    private void refreshCareerButtons() {
        long selected = nativeSelectedCareer();
        for (int index = 0; index < CAREER_COUNT; ++index) {
            long career = index + 1L;
            careerButtons[index].setText(
                    selected == career ? career + " ✓" : Long.toString(career));
        }
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
        if (getVisibility() != VISIBLE || roleName.getVisibility() != VISIBLE) return;
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

    private static Button makeButton(Context context, String text) {
        Button button = new Button(context);
        button.setText(text);
        return button;
    }

    private static LayoutParams matchWrap() {
        return new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
    }

    private static LayoutParams weightedWrap() {
        return new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1.0f);
    }
}
