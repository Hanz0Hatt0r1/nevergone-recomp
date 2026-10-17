package org.nevergone.recomp;

import android.content.ContentResolver;
import android.content.Context;
import android.net.Uri;
import android.util.Xml;

import org.xmlpull.v1.XmlPullParser;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;

final class GameSceneControlSettingsLoader {
    private static final String FILE_NAME = "original_control_preferences.xml";
    private static final long MAX_FILE_BYTES = 1024L * 1024L;
    private static volatile String lastStatus = "shipped default layout";

    private GameSceneControlSettingsLoader() {}

    private static native boolean nativeApply(
            boolean enabled,
            float joystickX,
            float joystickY,
            float buttonBX,
            float buttonBY,
            float uniqueSkillX,
            float uniqueSkillY);
    private static native float[] nativePositions();
    private static native void nativeSetEditMode(boolean enabled);

    static boolean copyPreferences(Context context, Uri uri) throws IOException {
        if (context == null || uri == null) throw new IOException("No control preferences selected");
        File destination = new File(context.getFilesDir(), FILE_NAME);
        File temporary = new File(context.getFilesDir(), FILE_NAME + ".importing");
        ContentResolver resolver = context.getContentResolver();
        long copied = 0;
        try (InputStream input = resolver.openInputStream(uri);
             OutputStream output = new FileOutputStream(temporary)) {
            if (input == null) throw new IOException("Could not read control preferences");
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) != -1) {
                copied += count;
                if (copied > MAX_FILE_BYTES) throw new IOException("Control preferences file is too large");
                output.write(buffer, 0, count);
            }
        } catch (IOException error) {
            temporary.delete();
            throw error;
        }
        Settings imported;
        try {
            imported = read(temporary);
        } catch (Exception error) {
            temporary.delete();
            throw new IOException("Invalid control preferences XML", error);
        }
        if (!imported.seenControlSetting || (imported.enabled && !imported.complete())) {
            temporary.delete();
            throw new IOException("XML does not contain a complete saved control layout");
        }
        if (copied == 0 || !temporary.renameTo(destination)) {
            temporary.delete();
            throw new IOException("Could not save control preferences");
        }
        return reloadFromFilesDir(context.getFilesDir().getAbsolutePath());
    }

    static boolean reloadFromFilesDir(String filesDir) {
        File source = filesDir == null || filesDir.isEmpty()
                ? null : new File(new File(filesDir), FILE_NAME);
        if (source == null || !source.isFile()) {
            nativeApply(false, 204.0f, 173.0f, 1040.0f, 90.0f, 949.0f, 154.0f);
            lastStatus = "shipped default layout";
            return false;
        }

        Settings settings;
        try {
            settings = read(source);
        } catch (Exception ignored) {
            nativeApply(false, 204.0f, 173.0f, 1040.0f, 90.0f, 949.0f, 154.0f);
            lastStatus = "invalid imported file; shipped default layout";
            return false;
        }
        if (!settings.enabled) {
            nativeApply(false, 204.0f, 173.0f, 1040.0f, 90.0f, 949.0f, 154.0f);
            lastStatus = "imported file has no custom controls; shipped default layout";
            return false;
        }
        if (!settings.complete() || !nativeApply(
                true,
                settings.joystickX,
                settings.joystickY,
                settings.buttonBX,
                settings.buttonBY,
                settings.uniqueSkillX,
                settings.uniqueSkillY)) {
            nativeApply(false, 204.0f, 173.0f, 1040.0f, 90.0f, 949.0f, 154.0f);
            lastStatus = "incomplete imported layout; shipped default layout";
            return false;
        }
        lastStatus = "imported custom control layout";
        return true;
    }

    static String status() {
        return lastStatus;
    }

    static float[] actionCuePositions() {
        float[] positions = nativePositions();
        return positions != null && positions.length == 10
                ? positions
                : new float[] {204.0f, 173.0f, 898.0f, 90.0f, 1040.0f, 90.0f,
                        1000.0f, 224.0f, 949.0f, 154.0f};
    }

    static void setEditMode(boolean enabled) {
        nativeSetEditMode(enabled);
    }

    static boolean saveCurrentLayout(String filesDir) throws IOException {
        float[] positions = actionCuePositions();
        File directory = new File(filesDir);
        File destination = new File(directory, FILE_NAME);
        File temporary = new File(directory, FILE_NAME + ".saving");
        String xml = "<map>\n" +
                "  <boolean name=\"controls_settings\" value=\"true\" />\n" +
                floatPreference("joyStickX", positions[0]) +
                floatPreference("joyStickY", positions[1]) +
                floatPreference("BX", positions[4]) +
                floatPreference("BY", positions[5]) +
                floatPreference("uniqueSkillX", positions[8]) +
                floatPreference("uniqueSkillY", positions[9]) +
                "</map>\n";
        try (OutputStream output = new FileOutputStream(temporary)) {
            output.write(xml.getBytes(StandardCharsets.UTF_8));
        } catch (IOException error) {
            temporary.delete();
            throw error;
        }
        if (!temporary.renameTo(destination)) {
            temporary.delete();
            throw new IOException("Could not save control layout");
        }
        return reloadFromFilesDir(filesDir);
    }

    private static String floatPreference(String key, float value) {
        return "  <float name=\"" + key + "\" value=\"" + Float.toString(value) + "\" />\n";
    }

    private static Settings read(File source) throws Exception {
        if (source.length() <= 0 || source.length() > MAX_FILE_BYTES) {
            throw new IOException("Invalid control preferences file size");
        }
        Settings settings = new Settings();
        XmlPullParser parser = Xml.newPullParser();
        try (InputStream input = new java.io.FileInputStream(source)) {
            parser.setInput(input, "UTF-8");
            int event;
            while ((event = parser.next()) != XmlPullParser.END_DOCUMENT) {
                if (event != XmlPullParser.START_TAG) continue;
                String key = parser.getAttributeValue(null, "name");
                String value = parser.getAttributeValue(null, "value");
                if (key == null || value == null) continue;
                switch (key) {
                    case "controls_settings":
                        settings.seenControlSetting = true;
                        settings.enabled = Boolean.parseBoolean(value);
                        break;
                    case "joyStickX": settings.joystickX = parseFloat(value); break;
                    case "joyStickY": settings.joystickY = parseFloat(value); break;
                    case "BX": settings.buttonBX = parseFloat(value); break;
                    case "BY": settings.buttonBY = parseFloat(value); break;
                    case "uniqueSkillX": settings.uniqueSkillX = parseFloat(value); break;
                    case "uniqueSkillY": settings.uniqueSkillY = parseFloat(value); break;
                    default: break;
                }
            }
        }
        return settings;
    }

    private static float parseFloat(String value) throws IOException {
        try {
            float parsed = Float.parseFloat(value);
            if (Float.isNaN(parsed) || Float.isInfinite(parsed)) {
                throw new IOException("Non-finite control coordinate");
            }
            return parsed;
        } catch (NumberFormatException error) {
            throw new IOException("Invalid control coordinate", error);
        }
    }

    private static final class Settings {
        boolean seenControlSetting;
        boolean enabled;
        float joystickX = Float.NaN;
        float joystickY = Float.NaN;
        float buttonBX = Float.NaN;
        float buttonBY = Float.NaN;
        float uniqueSkillX = Float.NaN;
        float uniqueSkillY = Float.NaN;

        boolean complete() {
            return finite(joystickX) && finite(joystickY) &&
                    finite(buttonBX) && finite(buttonBY) &&
                    finite(uniqueSkillX) && finite(uniqueSkillY);
        }

        private static boolean finite(float value) {
            return !Float.isNaN(value) && !Float.isInfinite(value);
        }
    }
}
