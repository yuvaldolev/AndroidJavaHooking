package com.example.payload;

public class GetDialogMessageHook {
    public static String hook(Object thiz) {
        return backup(thiz) + " All Day Long!";
    }

    public static String backup(Object thiz) {
        throw new UnsupportedOperationException("Backup method should have been replaced!");
    }
}
