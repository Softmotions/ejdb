package org.ejdb.bson.util;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Pattern;

/**
 * @author Tyutyunkov Vyacheslav (tve@softmotions.com)
 * @version $Id$
 */
public final class RegexFlag {
    private static List<RegexFlag> regexFlags;
    private static Map<Character, RegexFlag> characterToRegexFlagMap;

    private int flag;
    private char character;
    private boolean supported;

    private RegexFlag(int flag, char character, boolean supported) {
        this.flag = flag;
        this.character = character;
        this.supported = supported;
    }

    public static String regexFlagsToString(int flags) {
        StringBuilder result = new StringBuilder();
        for (RegexFlag rf : regexFlags) {
            if ((flags & rf.getFlag()) > 0 && rf.isSupported()) {
                result.append(rf.getCharacter());
            }
        }

        return result.toString();
    }

    public static int stringToRegexFlags(String str) {
        int flags = 0;

        for (int i = 0; i < str.length(); ++i) {
            RegexFlag rf = characterToRegexFlagMap.get(str.charAt(i));
            if (rf != null && rf.isSupported()) {
                flags |= rf.getFlag();
            }
        }

        return flags;
    }

    public static void registerRegexFlag(int flag, char character, boolean supported) {
        RegexFlag rf = new RegexFlag(flag, character, supported);
        regexFlags.add(rf);
        characterToRegexFlagMap.put(rf.getCharacter(), rf);
    }

    public int getFlag() {
        return flag;
    }

    public char getCharacter() {
        return character;
    }

    public boolean isSupported() {
        return supported;
    }

    static {
        RegexFlag.regexFlags = new ArrayList<RegexFlag>();
        RegexFlag.characterToRegexFlagMap = new HashMap<Character, RegexFlag>();

        RegexFlag.registerRegexFlag(Pattern.CASE_INSENSITIVE, 'i', true);
        RegexFlag.registerRegexFlag(Pattern.MULTILINE, 'm', true);
        RegexFlag.registerRegexFlag(Pattern.COMMENTS, 'x', true);
        RegexFlag.registerRegexFlag(Pattern.DOTALL, 's', true);
        RegexFlag.registerRegexFlag(Pattern.UNICODE_CASE, 'u', true);
        RegexFlag.registerRegexFlag(Pattern.CANON_EQ, 'c', false);
        RegexFlag.registerRegexFlag(Pattern.LITERAL, 't', false);
        RegexFlag.registerRegexFlag(Pattern.UNIX_LINES, 'd', false);

        Collections.sort(RegexFlag.regexFlags, new Comparator<RegexFlag>() {
            public int compare(RegexFlag o1, RegexFlag o2) {
                return o1.getCharacter() - o2.getCharacter();
            }
        });
    }
}
