package com.perpendox.phem;

import java.io.EOFException;
import java.io.IOException;
import java.net.HttpURLConnection;
import java.net.MalformedURLException;
import java.net.ProtocolException;
import java.net.URL;

import android.util.Log;

// Special check for review copies.
public class CheckReviewAuth extends PHEMOperation {
	private static final String LOG_TAG = CheckReviewAuth.class.getSimpleName();
	OperationFragment mFragment;
    private String the_target ="";

    public CheckReviewAuth(String target) {
    	super();
    	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Initializing with target:" + target);}
    	the_target = target;
    }

    @Override
    protected void onPreExecute() {
    	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Starting.");}
    }

	@Override
    void Set_Fragment(OperationFragment fragment) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Fragment set.");}
		mFragment = fragment;
	}

    @Override
    protected void onPostExecute(final Boolean success) {
    	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "All done.");}
		if (mFragment == null)
			return;
		mFragment.taskFinished();
    }

	@Override
	protected Boolean doInBackground(Void... params) {
		if (the_target.length() <= 0) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Didn't get a target!");}
			return false;
		}
		URL u;
		//InputStream in;
		try {
			u = new URL(the_target);
			HttpURLConnection huc;
			huc = (HttpURLConnection) u.openConnection();
			huc.setRequestMethod("GET");
			huc.connect();
			
			//in = huc.getInputStream(); // Have to do this or get exception on getResponseCode()?
			int code = huc.getResponseCode();
			if (code == HttpURLConnection.HTTP_OK) {
				if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Found auth.");}
				return true;
			} else {
				// Uh oh.
				Log.e(LOG_TAG, "Uh oh. Couldn't establish beta bona fides.");
				// TODO: tell MainActivity to shut down.
				MainActivity.Review_Check_Failed("Authorization not found.");
				return false;
			}
		} catch (EOFException e) {
			e.printStackTrace();
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "EOFException!");}
			MainActivity.Review_Check_Failed("Authorization not found.");
			return false;
		} catch (ProtocolException e) {
			e.printStackTrace();
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Protocol error!");}
			MainActivity.Review_Check_Failed("Protocol error");
			return false;
		} catch (MalformedURLException e) {
			e.printStackTrace();
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "URL problem!");}
			MainActivity.Review_Check_Failed("URL problem");
			return false;
		} catch (IOException e) {
			e.printStackTrace();
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Network error!");}
			MainActivity.Review_Check_Failed("Network error");
			return false;
		}
	}

	// These methods are used to set up the ProgressDialog that's shown to
	// the user.
	@Override
	String Get_Title() {
		return "Checking Review Authorization";
	}

	@Override
	String Get_Message() {
		return "(Requires network connection...)";
	}
}
