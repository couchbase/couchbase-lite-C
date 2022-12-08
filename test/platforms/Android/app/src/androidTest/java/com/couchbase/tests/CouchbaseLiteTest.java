package com.couchbase.tests;

import android.content.Context;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import static org.junit.Assert.*;

import java.io.File;

@RunWith(AndroidJUnit4.class)
public class CouchbaseLiteTest {
    static {
        System.loadLibrary("cbl_tests");
    }

    private static final String TEMP_DIR = "CBL_C_Tests_Temp";
    private static final String ASSETS_DIR = "CBL_C_Tests_Assets";

    @Test
    public void testCatchTests() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // Database Dir:
        String filesDir = context.getFilesDir().getPath();

        // Temp Dir:
        File tmpFileDir = new File(context.getFilesDir(), TEMP_DIR);
        tmpFileDir.mkdirs();
        String tmpDir = tmpFileDir.getPath();

        // Assets Dir:
        String assetsDir = filesDir + "/" + ASSETS_DIR;
        // Copy android's assets to the assets directory
        AssetUtil.copyAssets(context.getAssets(), assetsDir, true);

        // Run catch tests:
        int failed = runTests(filesDir, tmpDir, assetsDir, new String[] { });
        assertEquals(0, failed);
    }

    public native int runTests(String filesDir, String tmpDir, String assetsDir, String[] tests);
}
