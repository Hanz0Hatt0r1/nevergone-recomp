package org.nevergone.recomp;

import java.util.Locale;

final class SingleSelectHeroPictureLanguage {
    private SingleSelectHeroPictureLanguage() {}

    static String prefix(Locale locale) {
        if (locale == null) return "EN";
        String language = locale.getLanguage();
        // ManagementLayer::GetMultilingualPicturesName() maps the shipped
        // SystemLanguage enum 2 to CN, enum 5 to KR, every other value to EN.
        if ("zh".equalsIgnoreCase(language)) return "CN";
        if ("ko".equalsIgnoreCase(language)) return "KR";
        return "EN";
    }
}
