package org.nevergone.recomp;

import java.util.Locale;

public final class SingleSelectHeroPictureLanguageSmoke {
    public static void main(String[] args) {
        assertPrefix(null, "EN");
        assertPrefix(Locale.ENGLISH, "EN");
        assertPrefix(Locale.FRENCH, "EN");
        assertPrefix(new Locale("ru", "RU"), "EN");
        assertPrefix(Locale.SIMPLIFIED_CHINESE, "CN");
        assertPrefix(Locale.TRADITIONAL_CHINESE, "CN");
        assertPrefix(Locale.KOREAN, "KR");
        System.out.println("SingleSelectHero picture language smoke: ok");
    }

    private static void assertPrefix(Locale locale, String expected) {
        String actual = SingleSelectHeroPictureLanguage.prefix(locale);
        if (!expected.equals(actual)) {
            throw new AssertionError("expected " + expected + " but got " + actual + " for " + locale);
        }
    }
}
