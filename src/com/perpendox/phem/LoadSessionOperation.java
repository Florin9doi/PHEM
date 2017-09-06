package com.perpendox.phem;

import android.util.Log;

// Loading a session can take a while, especially since sometimes we have to
// save an existing session first. So we do it on a separate thread
// using an AsyncTask.
public class LoadSessionOperation extends PHEMOperation {
	private static final String LOG_TAG = LoadSessionOperation.class.getSimpleName();
	OperationFragment mFragment;
    private String the_file ="";

    public LoadSessionOperation(String file) {
    	super();
    	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Initializing with file:" + file);}
    	the_file = file;
    }

    @Override
    protected void onPreExecute() {
    	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Pausing idle.");}
    	MainActivity.Pause_Idle();
    }

	@Override
    void Set_Fragment(OperationFragment fragment) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Fragment set.");}
		mFragment = fragment;
	}

    @Override
    protected void onPostExecute(final Boolean success) {
    	MainActivity.Resume_Idle();
    	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Resuming idle.");}
		if (mFragment == null)
			return;
		mFragment.taskFinished();
    }

	@Override
	protected Boolean doInBackground(Void... params) {
		if (the_file.length() <= 0) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Didn't get a file!");}
			return false;
		}
		if (PHEMNativeIF.Is_Session_Active()) {
			// Technically, Shutdown_Emulator() checks to see if there's an
			// active session, too, but we'll use a belt *and* suspenders.
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Stopping existing session.");}
			PHEMNativeIF.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
		}
		PHEMNativeIF.Restart_Session(the_file);
		return true;
	}

	// These methods are used to set up the ProgressDialog that's shown to
	// the user.
	@Override
	String Get_Title() {
		return "Loading Session";
	}

	@Override
	String Get_Message() {
		return "Session file: " + the_file;
	}
}
