package com.example.app;

import android.app.AlertDialog;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.navigation.NavController;
import androidx.navigation.Navigation;
import androidx.navigation.ui.AppBarConfiguration;
import androidx.navigation.ui.NavigationUI;

import com.example.app.databinding.ActivityMainBinding;
import com.google.android.material.snackbar.Snackbar;

import java.lang.reflect.Field;
import java.util.Arrays;
import java.util.Enumeration;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

import dalvik.system.DexFile;
import dalvik.system.PathClassLoader;

public class MainActivity extends AppCompatActivity {
    private AppBarConfiguration appBarConfiguration;
    private ActivityMainBinding binding;

    static {
        System.loadLibrary("app");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        setSupportActionBar(binding.toolbar);

        NavController navController = Navigation.findNavController(this, R.id.nav_host_fragment_content_main);
        appBarConfiguration = new AppBarConfiguration.Builder(navController.getGraph()).build();
        NavigationUI.setupActionBarWithNavController(this, navController, appBarConfiguration);

        binding.fab.setOnClickListener(view -> Snackbar.make(view, "Replace with your own action", Snackbar.LENGTH_LONG)
                .setAction("Action", null).show());

        listAllPackageClasses();
    }

    private void listAllPackageClasses() {
        try {
            PathClassLoader classLoader = (PathClassLoader) getClassLoader();
            Object pathList = getFieldObject(classLoader, "pathList");
            Object[] dexElements = (Object[]) getFieldObject(pathList, "dexElements");
            List<DexFile> dexFiles = Arrays.stream(dexElements)
                    .map(dexElement -> {
                        try {
                            return (DexFile) getFieldObject(dexElement, "dexFile");
                        } catch (NoSuchFieldException | IllegalAccessException e) {
                            e.printStackTrace();
                        }

                        return null;
                    })
                    .collect(Collectors.toList());

            Set<String> classNames = new HashSet<>();
            for (DexFile dexFile : dexFiles) {
                Enumeration<String> entries = dexFile.entries();
                while (entries.hasMoreElements()) {
                    classNames.add(entries.nextElement());
                }
            }

            for (String className : classNames) {
                Log.d("test", className);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private static Object getFieldObject(Object instance, String fieldName)
            throws NoSuchFieldException, IllegalAccessException {
        Class<?> cls = instance.getClass();
        while ((null != cls) && (Object.class != cls)) {
            Object fieldObject = getFieldObject(cls, instance, fieldName);
            if (null != fieldObject) {
                // Found the requested field.
                return fieldObject;
            }

            // Didn't find the requested field. Try searching the
            // class's superclass.
            cls = cls.getSuperclass();
        }

        throw new NoSuchFieldException(
                "Class " + instance.getClass() + " does not contain a field named " + fieldName
        );
    }

    private static Object getFieldObject(Class<?> cls, Object instance, String fieldName)
            throws IllegalAccessException {
        Object fieldObject = null;

        try {
            Field field = cls.getDeclaredField(fieldName);
            boolean fieldAccessible = field.isAccessible();

            try {
                field.setAccessible(true);
                fieldObject = field.get(instance);
            } finally {
                field.setAccessible(fieldAccessible);
            }
        } catch (NoSuchFieldException ignored) {
        }

        return fieldObject;
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();

        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onSupportNavigateUp() {
        NavController navController = Navigation.findNavController(this, R.id.nav_host_fragment_content_main);
        return NavigationUI.navigateUp(navController, appBarConfiguration)
                || super.onSupportNavigateUp();
    }

    public void handleClickMeClick(View view) {
        periodicLog("ClickMe");
        Toast.makeText(this, "Button Clicked!", Toast.LENGTH_SHORT).show();
    }

    public void handleAlsoClickMeClick(View view) {
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle("Also Click Me")
                .setMessage(getDialogMessage())
                .setCancelable(false)
                .setPositiveButton("OK", (dialogInterface, i) -> {
                })
                .create();
        dialog.show();
    }

    public void periodicLog(String name) {
        Log.d("jni_test", "Periodic log for: [" + name + ']');
    }

    private String getDialogMessage() {
        return "CLICK CLICK CLICK";
    }
}