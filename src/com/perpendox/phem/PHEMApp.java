package com.perpendox.phem;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.text.MessageFormat;

import android.app.Application;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.HandlerThread;
import android.os.Looper;
import android.util.Log;
import android.widget.Toast;

import com.perpendox.phem.PHEMNativeIF;

// We need an object that can be called from native. It must
// always exist; so, we create the PHEMNativeIF object at the
// application level as a singleton. It will exist as long as
// the app does, and there will only ever be one of them.

public class PHEMApp extends Application {
	private static final String LOG_TAG = PHEMApp.class.getSimpleName();
	private static HandlerThread idle_thread;
	private static Looper idle_looper;
	public static IdleHandler idle_handler;
	public static Object idle_sync = new Object();

	@Override
	public void onCreate() {
		super.onCreate();

		// Initialize the singletons so their instances
		// are bound to the application process.
		initSingletons();
	}

	protected void initSingletons() {
		// Initialize the instance of PHEMNativeIF
		// Loads native code library, etc.
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "initSingletons().");
		}
		PHEMNativeIF.initInstance();
		// Set up the idle thread.
		idle_thread = new HandlerThread("IdleThread",
	            android.os.Process.THREAD_PRIORITY_DISPLAY
	            + android.os.Process.THREAD_PRIORITY_LESS_FAVORABLE );
		idle_thread.start();
		idle_looper = idle_thread.getLooper();
	    idle_handler = new IdleHandler(idle_looper);
	}

	// Used to fetch the log for crash reports.
	public static String readAllOf(InputStream s) throws IOException {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "readAllOf...");}
		BufferedReader bufferedReader = new BufferedReader(
				new InputStreamReader(s), 8096);
		String line;
		StringBuilder log = new StringBuilder();
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "About to read lines.");}
		while ((line = bufferedReader.readLine()) != null) {
			log.append(line);
			log.append("\n");
		}
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Done fetching log");
		}
		return log.toString();

	}

	static String getVersion(Context c) {
		try {
			return c.getPackageManager().getPackageInfo(c.getPackageName(), 0).versionName;
		} catch (Exception e) {
			return c.getString(R.string.unknown_version);
		}
	}

	static boolean tryEmailAuthor(Context c, boolean isCrash, String body) {
		String addr = c.getString(R.string.author_email);
		Intent i = new Intent(Intent.ACTION_SEND);
		String modVer = "";
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Tying to send email.");
		}
		try {
			Process p = Runtime.getRuntime().exec(
					new String[] { "getprop", "ro.modversion" });
			modVer = readAllOf(p.getInputStream()).trim();
		} catch (Exception e) {
			// Really, what would we do here?
		}
		if (modVer.length() == 0) {
			modVer = "original";
		}
		// second empty address because of
		// http://code.google.com/p/k9mail/issues/detail?id=589
		i.putExtra(Intent.EXTRA_EMAIL, new String[] { addr, "" });
		i.putExtra(Intent.EXTRA_SUBJECT, MessageFormat.format(
				c.getString(R.string.crash_subject), getVersion(c),
				Build.MODEL, modVer, Build.FINGERPRINT));
		i.setType("message/rfc822");
		i.putExtra(Intent.EXTRA_TEXT, body != null ? body : "");
		try {
			c.startActivity(i);
			return true;
		} catch (ActivityNotFoundException e) {
			try {
				// Get the OS to present a nicely formatted, translated error
				c.startActivity(Intent.createChooser(i, null));
			} catch (Exception e2) {
				e2.printStackTrace();
				Toast.makeText(c, e2.toString(), Toast.LENGTH_LONG).show();
			}
			return false;
		}
	}
}