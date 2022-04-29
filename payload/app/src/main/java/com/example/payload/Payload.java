package com.example.payload;

import android.app.Activity;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.util.ArrayMap;
import android.util.Log;
import android.view.View;
import android.view.ViewGroup;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Locale;

public class Payload {
    public static final String LOG_TAG = "jni_test";

    public static void start() {
        Log.i(LOG_TAG, "Payload is RUNNING!!!!!!");

        Handler mainHandler = new Handler(Looper.getMainLooper());
        mainHandler.post(Payload::makeAppPurple);
    }

    private static void makeAppPurple() {
        try {
            // Retrieve the current ActivityThread.
            Class<?> activityThreadClass = Class.forName("android.app.ActivityThread");
            Method currentActivityThreadMethod = activityThreadClass.getMethod("currentActivityThread");
            Object currentActivityThread = currentActivityThreadMethod.invoke(null);

            // Get all active Activities.
            ArrayMap<IBinder, Object> activities = (ArrayMap<IBinder, Object>) getFieldObject(currentActivityThread, "mActivities");
            Log.i(LOG_TAG, String.format(Locale.ENGLISH, "There are (%d) active activities: [%s]", activities.size(), activities));

            // Make each activity purple.
            Method getActivityMethod = activityThreadClass.getMethod("getActivity", IBinder.class);
            for (IBinder key : activities.keySet()) {
                Activity activity = (Activity)getActivityMethod.invoke(currentActivityThread, key);
                Log.i(LOG_TAG, String.format(Locale.ENGLISH, "Purpling activity: [%s]", activity));
                makeActivityPurple(activity);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static void makeActivityPurple(Activity activity) {
        makeViewPurple(activity.getWindow().getDecorView().getRootView(), 0);
    }

    private static void makeViewPurple(View view, int level) {
        // Null view means we reached the last view in the hierarchy.
        if (null == view) {
            return;
        }

        // Log the view.
        StringBuilder logMessageBuilder = new StringBuilder();
        for (int i = 0; i < level; ++i) {
            logMessageBuilder.append("--");
        }
        logMessageBuilder.append(' ');
        logMessageBuilder.append(view.getClass().getName());
        Log.i(LOG_TAG, logMessageBuilder.toString());

        // Make the view purple.
        if (0 == level) {
            view.setBackgroundColor(0xFFFFFFFF);
        } else {
            view.setBackgroundColor(0x20FF00FF);
        }

        // Make all subviews purple if the view is a ViewGroup.
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); ++i) {
                makeViewPurple(viewGroup.getChildAt(i), level + 1);
            }
        }
    }

    private static Object getFieldObject(Object instance, String fieldName) throws NoSuchFieldException, IllegalAccessException {
        Class<?> instanceClass = instance.getClass();

        Field field = instanceClass.getDeclaredField(fieldName);
        boolean fieldAccessible = field.isAccessible();

        Object fieldObject;
        try {
            field.setAccessible(true);
            fieldObject = field.get(instance);
        } finally {
            field.setAccessible(fieldAccessible);
        }

        return fieldObject;
    }
}
