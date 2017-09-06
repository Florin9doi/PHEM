package com.perpendox.phem;

import android.util.Log;

// Installing a file to the emulator can take a while. So we do it on a separate
// thread using an AsyncTask.
public class InstallFileOperation extends PHEMOperation {
	private static final String LOG_TAG = InstallFileOperation.class.getSimpleName();
	OperationFragment mFragment;
    private String the_file ="";

    public InstallFileOperation(String file) {
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
		if (0 == PHEMNativeIF.Install_Palm_File(the_file)) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Loaded " + the_file);}
			return true;
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Error loading file!");}
			return false;
		}
	}

	// These methods are used to set up the ProgressDialog that's shown to
	// the user.
	@Override
	String Get_Title() {
		return "Installing";
	}

	@Override
	String Get_Message() {
		return "File: " + the_file;
	}
}
