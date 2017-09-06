package com.perpendox.phem;

import java.io.IOException;
import java.text.MessageFormat;

import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.Toast;
import android.support.v4.app.DialogFragment;


// A progress dialog that can survive the Activity being destroyed and
// recreated on a configuration change.
public class CrashFragment extends DialogFragment {
	private final static String LOG_TAG = CrashFragment.class.getSimpleName();
	private Button report_button, close_button;
	private CheckBox clear_cb;
	private ConditionVariable the_cond;
	
	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		// Retain this instance so it isn't destroyed when the host Activity
		// changes configuration.
		Log.d(LOG_TAG, "onCreate.");
		setRetainInstance(true);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		Log.d(LOG_TAG, "onCreateView.");
		View the_view = inflater.inflate(R.layout.fragment_crash_handler, container);
		report_button = (Button) the_view.findViewById(R.id.report);
		close_button = (Button) the_view.findViewById(R.id.close);
		clear_cb = (CheckBox) the_view.findViewById(R.id.forget);

		report_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				Log.d(LOG_TAG, "Report onClick.");
				final ProgressDialog progress = new ProgressDialog(
						getActivity());
				progress.setMessage(getString(R.string.getting_log));
				progress.setIndeterminate(true);
				progress.setCancelable(false);
				progress.show();
				final AsyncTask<Void, Void, Void> task = new AsyncTask<Void, Void, Void>() {
					String log;
					Process process;

					@Override
					protected Void doInBackground(Void... v) {
						Log.d(LOG_TAG, "doInBackground.");
						try {
							process = Runtime.getRuntime().exec(
									new String[] { "logcat", "-d", "-v", "threadtime" });
							log = PHEMApp.readAllOf(process.getInputStream());
						} catch (IOException e) {
							Log.d(LOG_TAG, "Report exception.");
							e.printStackTrace();
							Toast.makeText(getActivity(), e.toString(),
									Toast.LENGTH_LONG).show();
						}
						return null;
					}

					@Override
					protected void onCancelled() {
						if (null != process) {
							process.destroy();
						}
					}

					@Override
					protected void onPostExecute(Void v) {
						Log.d(LOG_TAG, "Trying to send email.");
						progress.setMessage(getString(R.string.starting_email));
						boolean ok = PHEMApp.tryEmailAuthor(
								getActivity(), true,
								getString(R.string.crash_preamble)
										+ "\n\n\n\nLog:\n" + log);
						if (ok) {
							Log.d(LOG_TAG, "Sent email...");
						}
						if (clear_cb.isChecked()) {
							clearState();
						}
						the_cond.open();
						dismiss();
					}
				}.execute();
				Log.d(LOG_TAG, "Kicked off log gather.");
				report_button.postDelayed(new Runnable() {
					public void run() {
						if (task.getStatus() == AsyncTask.Status.FINISHED)
							return;
						// It's probably one of these devices where some fool
						// broke logcat.
						Log.d(LOG_TAG, "Couldn't get log.");
						progress.dismiss();
						task.cancel(true);
						the_cond.open();
						new AlertDialog.Builder(getActivity())
								.setMessage(
										MessageFormat
												.format(getString(R.string.get_log_failed),
														getString(R.string.author_email)))
								.setCancelable(true)
								.setIcon(android.R.drawable.ic_dialog_alert)
								.show();
					}
				}, 3000);
			}
		});
		
		close_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				Log.d(LOG_TAG, "Close onClick.");
				if (clear_cb.isChecked()) {
					clearState();
				}
				the_cond.open();
				dismiss();
			}
		});

		getDialog().setTitle(R.string.crashtitle);

		getDialog().setCancelable(false);

		return the_view;
	}
	
	// This is to work around what is apparently a bug. If you don't have it
	// here the dialog will be dismissed on rotation, so tell it not to dismiss.
	@Override
	public void onDestroyView()
	{
		Log.d(LOG_TAG, "onDestroyView.");
		if (getDialog() != null && getRetainInstance())
			getDialog().setDismissMessage(null);
		super.onDestroyView();
	}

	@Override
	public void onResume()
	{
		Log.d(LOG_TAG, "onResume.");
		super.onResume();
	}
	
	public void setCond(ConditionVariable condy)
	{
		Log.d(LOG_TAG, "onDestroyView.");
		the_cond = condy;
	}

	void clearState() {
		Log.d(LOG_TAG, "Clearing state.");
		MainActivity.instance.Clear_Prefs();
	}
}
