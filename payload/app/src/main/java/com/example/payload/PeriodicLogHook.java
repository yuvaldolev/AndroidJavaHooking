package com.example.payload;

import android.util.Log;

public class PeriodicLogHook {
    public static void hook(Object thiz, String name) {
        Log.d("jni_test", "Hooked periodic log for: [" + name + ']');
    }
}
