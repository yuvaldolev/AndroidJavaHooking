package com.example.payload;

import android.content.Context;
import android.view.View;
import android.widget.Toast;

public class HandleClickMeClickHook {
    public static void hook(Object thiz, View view) {
        Toast.makeText((Context) thiz, "Click Me button has been hooked!", Toast.LENGTH_SHORT).show();
    }
}
