package com.perpendox.phem;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.util.Log;
import android.support.v4.app.DialogFragment;

// A progress dialog that can survive the Activity being destroyed and
// recreated on a configuration change.
public class EmAlertFragment extends DialogFragment {
	// The task we are running.
	private static final String LOG_TAG = EmAlertFragment.class.getSimpleName();
	Integer[] items;
	String[] labels;
	Boolean[] props;
	ConditionVariable the_cond;
	String message;
	int title;

	public void setData(final int the_title, final String the_message,
			final Integer item_ids[], final String the_labels[],
			final Boolean properties[], final ConditionVariable cond) {
		items = item_ids;
		labels = the_labels;
		props = properties;
		title = the_title;
		message = the_message;

		the_cond = cond;

		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Data set."); }
	}

	@SuppressLint({ "NewApi", "InlinedApi" }) // We use the setIconAttribute if available
	@Override
	public Dialog onCreateDialog(Bundle savedInstanceState) {
		AlertDialog.Builder ad_builder = new AlertDialog.Builder(
				MainActivity.instance);
		ad_builder.setTitle(title)
				.setMessage(message)
				.setCancelable(false);
		if (android.os.Build.VERSION.SDK_INT >=11 ) {
			// This is only available if API is over 11 (Honeycomb)
			ad_builder.setIconAttribute(android.R.attr.alertDialogIcon);
		} else {
			// Icon is really light on default background, but no other
			// good option.
			ad_builder.setIcon(android.R.drawable.ic_dialog_alert);
		}
		// We can't handle 'debugging' Palm apps, so there's no point in
		// showing that button, if present.
		if (props[0] && props[0 + 1] && 0 != labels[0].compareTo("Debug")) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Creating positive button: " + labels[0]);
			}
			ad_builder.setPositiveButton(labels[0],
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							PHEMNativeIF.button_pushed = items[0];
							the_cond.open();
						}
					});
		}
		if (props[4] && props[4 + 1] && 0 != labels[1].compareTo("Debug")) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Creating neutral button: " + labels[1]);
			}
			ad_builder.setNeutralButton(labels[1],
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							PHEMNativeIF.button_pushed = items[1];
							the_cond.open();
						}
					});
		}
		if (props[8] && props[8 + 1] && 0 != labels[2].compareTo("Debug")) {
			if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Creating negative button: " + labels[2]);
			}
			ad_builder.setNegativeButton(labels[2],
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							PHEMNativeIF.button_pushed = items[2];
							the_cond.open();

						}
					});
		}
		return ad_builder.create();
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);

		// Retain this instance so it isn't destroyed when the host Activity
		// changes configuration.
		setRetainInstance(true);
	}

	// This is to work around what is apparently a bug. If you don't have it
	// here the dialog will be dismissed on rotation, so tell it not to dismiss.
	@Override
	public void onDestroyView() {
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "onDestroyView."); }
		if (getDialog() != null && getRetainInstance())
			getDialog().setDismissMessage(null);
		super.onDestroyView();
	}

	@Override
	public void onResume() {
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "onResume."); }
		super.onResume();
	}

}
