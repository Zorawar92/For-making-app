// SPDX-License-Identifier: GPL-3.0-or-later
package org.csweb.compat;

import android.content.Context;
import android.content.SharedPreferences;

import java.io.File;

/**
 * The Context handed to Plugin.load(Context) in the headless runtime. It is an application context,
 * never an Activity: plugin code that needs an Activity (dialogs, fragments) sees `context !is Activity`
 * and is expected to skip its UI, exactly as it would when CloudStream passes a non-Activity context.
 */
public final class HeadlessContext extends Context {
    // CloudStream's pinned APK in runtime/inputs.lock assigns this ID to R.string.dns_key, whose
    // value is the SharedPreferences key "dns_key". RequestsHelperKt uses it before the plugin
    // runtime is ready. The headless runtime has no Android app resource table, so provide this
    // one bootstrap resource explicitly instead of failing in Resources.getSystem().
    private static final int CLOUDSTREAM_DNS_KEY_RESOURCE = 0x7f14014e;

    private final File dataDir;
    private final String packageName;

    public HeadlessContext(File dataDir, String packageName) {
        this(dataDir, packageName, 4); // CloudStream's AdGuard DNS selection
    }

    public HeadlessContext(File dataDir, String packageName, int dnsProvider) {
        this.dataDir = dataDir;
        this.packageName = packageName;
        setDnsProvider(dnsProvider);
    }

    /** Keep CloudStream's OkHttp client on the same DNS choice as the native app and player. */
    public void setDnsProvider(int provider) {
        getSharedPreferences(packageName + "_preferences", Context.MODE_PRIVATE)
                .edit().putInt("dns_key", provider).apply();
    }

    @Override public SharedPreferences getSharedPreferences(String name, int mode) {
        String safe = name == null ? "null" : name.replaceAll("[^A-Za-z0-9._-]", "_");
        return JsonSharedPreferences.open(new File(new File(dataDir, "shared_prefs"), safe + ".json"));
    }

    @Override public String getString(int resId) {
        if ("com.lagradost.cloudstream3".equals(packageName) && resId == CLOUDSTREAM_DNS_KEY_RESOURCE) {
            return "dns_key";
        }
        // The headless runtime has no Android application resource table, so any other resource lookup
        // would raise Resources.NotFoundException and abort plugin init. Return an empty string as a
        // pragmatic stand-in (the same approach used for dns_key) so plugins that read their own
        // strings while loading do not crash the runtime. Framework/system strings still resolve.
        try {
            return super.getString(resId);
        } catch (android.content.res.Resources.NotFoundException e) {
            return "";
        }
    }

    @Override public Context getApplicationContext() { return this; }

    @Override public String getPackageName() { return packageName; }

    @Override public File getFilesDir() { return mkdirs(new File(dataDir, "files")); }

    @Override public File getCacheDir() { return mkdirs(new File(dataDir, "cache")); }

    @Override public ClassLoader getClassLoader() { return HeadlessContext.class.getClassLoader(); }

    private static File mkdirs(File f) {
        if (!f.isDirectory()) f.mkdirs();
        return f;
    }
}
