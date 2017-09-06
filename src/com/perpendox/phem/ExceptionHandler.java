package com.perpendox.phem;

import java.lang.Thread.UncaughtExceptionHandler;

import android.content.Context;
import android.content.SharedPreferences;
import android.util.Log;

public class ExceptionHandler implements UncaughtExceptionHandler {
	private static final String LOG_TAG = ExceptionHandler.class
			.getSimpleName();
	private UncaughtExceptionHandler defaultExceptionHandler;
	SharedPreferences preferences;

	// Constructor.
	public ExceptionHandler(UncaughtExceptionHandler uncaughtExceptionHandler,
			Context context) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Setting up exception handler.");
		}
		preferences = context.getSharedPreferences(MainActivity.PREF_ROOT, 0);
		defaultExceptionHandler = uncaughtExceptionHandler;
	}

	public void uncaughtException(Thread thread, Throwable throwable) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Oy. Uncaught exception.");
		}
		preferences.edit().putBoolean(MainActivity.PREF_APP_CRASHED, true)
				.commit();

		// Call the original handler.
		defaultExceptionHandler.uncaughtException(thread, throwable);
	}
}
