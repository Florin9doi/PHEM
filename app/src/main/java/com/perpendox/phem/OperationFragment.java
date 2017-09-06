package com.perpendox.phem;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.support.v4.app.DialogFragment;


// A progress dialog that can survive the Activity being destroyed and
// recreated on a configuration change.
public class OperationFragment extends DialogFragment {
	private static final String LOG_TAG = OperationFragment.class.getSimpleName();
	// The task we are running.
	PHEMOperation the_op;
	String message;

	public void setTask(PHEMOperation op)
	{
		the_op = op;
		message = the_op.Get_Message();
		// Tell the AsyncTask to call updateProgress() and taskFinished() on this fragment.
		the_op.Set_Fragment(this);
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Message is:" + message);}
	}

	@Override
	public void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		
		// Retain this instance so it isn't destroyed when the host Activity
		// changes configuration.
		setRetainInstance(true);

		// Start the task.
		if (the_op != null) {
			the_op.execute();
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onCreateView.");}
		View view = inflater.inflate(R.layout.fragment_task, container);
		TextView item_tv = (TextView)view.findViewById(R.id.item_field);
		item_tv.setText(message);

		getDialog().setTitle(the_op.Get_Title());

		// If you're doing a long task, you probably don't want people to cancel
		// it just by tapping the screen!
		//getDialog().setCanceledOnTouchOutside(false);
		getDialog().setCancelable(false);

		return view;
	}
	
	// This is to work around what is apparently a bug. If you don't have it
	// here the dialog will be dismissed on rotation, so tell it not to dismiss.
	@Override
	public void onDestroyView()
	{
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onDestroyView.");}
		if (getDialog() != null && getRetainInstance())
			getDialog().setDismissMessage(null);
		super.onDestroyView();
	}
/*
	// Also when we are dismissed we need to cancel the task.
	@Override
	public void onDismiss(DialogInterface dialog)
	{
		super.onDismiss(dialog);
		// If true, the thread is interrupted immediately, which may do bad things.
		// If false, it guarantees a result is never returned (onPostExecute() isn't called)
		// but you have to repeatedly call isCancelled() in your doInBackground()
		// function to check if it should exit. For some tasks that might not be feasible.
		if (mTask != null)
			mTask.cancel(false);

		// You don't really need this if you don't want.
		if (getTargetFragment() != null)
			getTargetFragment().onActivityResult(TASK_FRAGMENT, Activity.RESULT_CANCELED, null);
	}
*/
	@Override
	public void onResume()
	{
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onResume.");}
		super.onResume();
		// This is a little hacky, but we will see if the task has finished while we weren't
		// in this activity, and then we can dismiss ourselves.
		if (the_op == null) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onResume dismiss().");}
			dismiss();
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Resuming op with message: " + message);}
		}
	}
/*
	// This is called by the AsyncTask.
	public void updateProgress(int percent)
	{
		mProgressBar.setProgress(percent);
	}
*/
	// This is also called by the AsyncTask.
	public void taskFinished()
	{
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "taskFinished.");}
		// Make sure we check if it is resumed because we will crash if trying to dismiss the dialog
		// after the user has switched to another app.
		if (isResumed()) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "taskFinished dismiss().");}
			dismiss();
		}
		
		// If we aren't resumed, setting the task to null will allow us to dismiss ourselves in
		// onResume().
		the_op = null;
		
		// Tell the fragment that we are done.
		if (getTargetFragment() != null)
			getTargetFragment().onActivityResult(0, Activity.RESULT_OK, null);
	}
}
