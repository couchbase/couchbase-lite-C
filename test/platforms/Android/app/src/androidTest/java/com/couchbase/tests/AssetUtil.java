package com.couchbase.tests;

import android.content.res.AssetManager;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class AssetUtil {
    public static void copyAssets(AssetManager manager, String destDirPath, boolean clean) throws IOException {
        if (clean) {
            File dir = new File(destDirPath);
            if (dir.exists() && !dir.isDirectory()) {
                throw new IllegalArgumentException("destDirPath already exists as a file");
            }
            deleteFileOrDir(destDirPath);
            dir = new File(destDirPath);
            dir.mkdirs();
        }
        copyAsset(manager, "", destDirPath);
    }

    private static void copyAsset(AssetManager manager, String assetPath, String destDirPath) throws IOException {
        String[] assets = manager.list(assetPath);
        if (assets.length == 0) {
            copyFile(manager, assetPath, destDirPath);
        } else {
            File dir = new File(destDirPath + "/" + assetPath);
            if (!dir.exists())
                dir.mkdir();
            for (String asset : assets) {
                String path = (assetPath.length() > 0 ? assetPath + "/" : "") + asset;
                copyAsset(manager, path, destDirPath);
            }
        }
    }

    private static void copyFile(AssetManager manager, String filePath, String destDirPath) throws IOException {
        InputStream in;
        try { in = manager.open(filePath); } catch (IOException e) { /* Directory */ return; }
        OutputStream out = new FileOutputStream(destDirPath + "/" + filePath);
        byte[] buffer = new byte[1024];
        int bytes;
        while ((bytes = in.read(buffer)) != -1) {
            out.write(buffer, 0, bytes);
        }
        out.flush();
        out.close();
        in.close();
    }

    private static void deleteFileOrDir(String path) {
        File f = new File(path);
        if (!f.exists())
            return;
        if (f.isDirectory()) {
            for (String name : f.list()) {
                deleteFileOrDir(path + "/" + name);
            }
        }
        f.delete();
    }
}
