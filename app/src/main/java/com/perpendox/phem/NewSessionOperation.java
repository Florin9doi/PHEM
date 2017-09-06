package com.perpendox.phem;

import android.util.Log;

// Loading a session can take a while, especially since sometimes we have to
// save an existing session first. So we do it on a separate thread
// using an AsyncTask.
public class NewSessionOperation extends PHEMOperation {
	private static final String LOG_TAG = NewSessionOperation.class.getSimpleName();
	OperationFragment mFragment;
	private String the_rom = "", the_dev = "", the_ram = "";

	public NewSessionOperation(String rom, String dev, String ram) {
		super();
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Initializing with rom:" + rom + " dev: " + dev
					+ " ram: " + ram);
		}
		the_rom = rom;
		the_dev = dev;
		the_ram = ram;
	}

	@Override
	protected void onPreExecute() {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Pausing idle.");
		}
		MainActivity.Pause_Idle();
	}

	@Override
	void Set_Fragment(OperationFragment fragment) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Fragment set.");
		}
		mFragment = fragment;
	}

	@Override
	protected void onPostExecute(final Boolean success) {
		MainActivity.Resume_Idle();
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Resuming idle.");
		}
		if (mFragment == null)
			return;
		mFragment.taskFinished();
	}

	@Override
	protected Boolean doInBackground(Void... params) {
		if (the_rom.length() <= 0 || the_dev.length() <= 0
				|| the_ram.length() <= 0) {
			Log.e(LOG_TAG, "Parameter error!");
			return false;
		}
		if (PHEMNativeIF.Is_Session_Active()) {
			// Technically, Shutdown_Emulator() checks to see if there's an
			// active session, too, but we'll use a belt *and* suspenders.
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Stopping existing session.");
			}
			PHEMNativeIF
					.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
		}
		PHEMNativeIF.New_Session(PHEMNativeIF.phem_base_dir + "/roms/"
				+ the_rom, the_ram, the_dev, "unsupported");
		PHEMNativeIF.session_file_name = "autosave.psf"; // Make sure we don't
															// overwrite
															// any previous
															// sessions we
															// loaded.
															// (Well, except the
															// autosave, but if
															// the user didn't
															// save the last
															// session, I
															// suppose they
															// didn't
															// care about that.)
		return true;
	}

	// These methods are used to set up the ProgressDialog that's shown to
	// the user.
	@Override
	String Get_Title() {
		return "Creating Session";
	}

	@Override
	String Get_Message() {
		return "ROM file: " + the_rom;
	}
}
