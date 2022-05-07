package com.example.payload;

import android.util.Log;

public class PeriodicLogHook {
    public static void hook(Object thiz, String name) {
        backup(thiz, name + "-hook");
    }

    public static void backup(Object thiz, String name) {
        Log.e("jni_test", "Backup method should have been replaced!");
    }
}
