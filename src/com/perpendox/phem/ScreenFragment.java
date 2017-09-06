package com.perpendox.phem;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

/**
 * The fragment that shows the emulator screen (and skin, if any).
 */
public class ScreenFragment extends Fragment {
	private static final String LOG_TAG = ScreenFragment.class.getSimpleName();
	private PHEMView the_view;
	/**
	 * @return A new instance of fragment ScreenFragment.
	 */
	// TODO: Rename and change types and number of parameters
	public static ScreenFragment newInstance() {
		ScreenFragment fragment = new ScreenFragment();
		return fragment;
	}

	public ScreenFragment() {
		// Required empty public constructor
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "constructor.");}
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onCreate.");
		}
		// Retain this instance so it isn't destroyed when the host Activity
		// changes configuration.
		setRetainInstance(true);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onCreateView: " + (null == the_view));
		}
		//Log.d(LOG_TAG, "onCreateView hmmm");
		// All the real work is done in PHEMView.
		the_view = new PHEMView(getActivity());
		return the_view;
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Detaching...");
		}
	}

	@Override
	public void onDetach() {
		super.onDetach();
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Attaching...");
		}
	}
	
	public void Set_Bitmap_Size(int W, int H) {
		the_view.set_Bitmap_Size(W, H);
	}

	// Pass it on to our view.
	public void Set_Bitmap() {
		the_view.set_Bitmap();
	}
	
	public void Load_Bitmap(Context context, int the_drawable) {
		if (!PHEMNativeIF.Is_Session_Active()) {
			the_view.Load_Bitmap(context, the_drawable);
		} else {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Session already active, not setting 'load' bitmap.");
			}
		}
	}
	
	@Override
	public void onResume() {
		MainActivity.screen_paused = false;
		super.onResume();
	}
	
	@Override
	public void onPause() {
		MainActivity.screen_paused = true;
		super.onPause();
	}
}
