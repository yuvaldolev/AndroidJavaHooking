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
import java.lang.reflect.Method;
import java.util.Locale;

public class Payload {
    public static final String LOG_TAG = "jni_test";

    public static void start() {
        Log.i(LOG_TAG, "Payload is RUNNING!!!!!!");

        hookMethods();

        Handler mainHandler = new Handler(Looper.getMainLooper());
        mainHandler.post(Payload::makeAppPurple);
//        mainHandler.post(Payload::hookMethods);
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

    private static void hookMethods() {
        try {
            Class<?> mainActivityClass = Class.forName("com.example.app.MainActivity");

//            // Hook MainActivity.handleClickMeClick
//            Log.d(LOG_TAG, "Hooking MainActivity.handleClickMeClick");
//            Method handleClickMeClickMethod = mainActivityClass.getMethod("handleClickMeClick", View.class);
//            Method handleClickMeClickHookMethod = HandleClickMeClickHook.class.getMethod("hook", Object.class, View.class);
//            hook(handleClickMeClickMethod, handleClickMeClickHookMethod);
//
//            // Hook MainActivity.getDialogMessage
//            Log.d(LOG_TAG, "Hooking MainActivity.getDialogMessage");
//            Method getDialogMessageMethod = mainActivityClass.getDeclaredMethod("getDialogMessage");
//            Method getDialogMessageHookMethod = GetDialogMessageHook.class.getMethod("hook", Object.class);
//            Method getDialogMessageBackupMethod = GetDialogMessageHook.class.getMethod("backup", Object.class);
//            hook(getDialogMessageMethod, getDialogMessageHookMethod, getDialogMessageBackupMethod);

            // Hook MainActivity.periodicLog
            Log.d(LOG_TAG, "Hooking MainActivity.periodicLog");
            Method periodicLogMethod = mainActivityClass.getMethod("periodicLog", String.class);
//            Class<?> periodicLogHookClass = Class.forName("com.example.payload.PeriodicLogHook");
            Method periodicLogHookMethod = mainActivityClass.getMethod("hook", Object.class, String.class);
            Method periodicLogBackupMethod = mainActivityClass.getMethod("backup", Object.class, String.class);
            hook(periodicLogMethod, periodicLogHookMethod, periodicLogBackupMethod);
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

    private static void hook(Method target, Method hook) {
        hook(target, hook, null);
    }

    private static void hook(Method target, Method hook, Method backup) {
        hookNative(target, hook, backup);
    }

    private static native void hookNative(Method target, Method hook, Method backup);
}
