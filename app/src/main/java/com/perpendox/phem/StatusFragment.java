package com.perpendox.phem;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

/**
 * Gives a summary of the current emulation session.
 */
public class StatusFragment extends Fragment {
	private static final String LOG_TAG = StatusFragment.class.getSimpleName();
	private OnStatusFragStartedListener mListener;
	private View the_view;
	private TextView session_text, rom_text, dev_text, ram_text;

	public static StatusFragment newInstance() {
		StatusFragment fragment = new StatusFragment();
		return fragment;
	}

	public StatusFragment() {
		// Required empty public constructor
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onCreate");}
		super.onCreate(savedInstanceState);
		// setRetainInstance(true);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onCreateView");}
		// First run. Gotta set up defaults.
		// Inflate the layout for this fragment
		the_view = inflater.inflate(R.layout.fragment_status, container, false);
		session_text = (TextView) the_view
				.findViewById(R.id.status_session_field);
		session_text.setText("[None]");
		// Find the ROM TextView
		rom_text = (TextView) the_view.findViewById(R.id.status_rom_field);
		// Find the Device TextView
		dev_text = (TextView) the_view.findViewById(R.id.status_dev_field);
		// Find the RAM TextView
		ram_text = (TextView) the_view.findViewById(R.id.status_ram_field);

		if (PHEMNativeIF.Is_Session_Active()) {
			update_session_info();
		}
		if (null != mListener) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "frag (re?)started...");}
			mListener.StatusStarted();
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Crud, no listener!");}
		}
		return the_view;
	}

	public void update_session_info() {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "update_session_info");}
		if (PHEMNativeIF.Is_Session_Active()) {
			// Okay, there is in fact a session, I guess.
			// The stuff to extract from the emulator.
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Getting session info.");}
			final String session_info[] = PHEMNativeIF.Get_Session_Info();

			if (null != getActivity() && null != session_info) {
				getActivity().runOnUiThread(new Runnable() {

					@Override
					public void run() {
						if (PHEMNativeIF.session_file_name != null) {
							session_text
									.setText(PHEMNativeIF.session_file_name);
						} else {
							session_text.setText("[Not Set]");
						}

						if (MainActivity.enable_logging) {
							for (int i = 0; i < session_info.length; i++) {
								Log.d(LOG_TAG, "info[" + i + "]"
										+ session_info[i]);
							}
						}
						if (session_info != null && session_info.length == 3) {

							rom_text.setText(session_info[0]);

							dev_text.setText(session_info[1]);

							ram_text.setText(session_info[2]);
						}

					}
				});
			}
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No active session.");}
			if (null != getActivity()) {
				getActivity().runOnUiThread(new Runnable() {

					@Override
					public void run() {
						session_text.setText("[None]");
					}
				});
			}
		}
	}

	@Override
	public void onAttach(Activity activity) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onAttach.");}
		super.onAttach(activity);
		try {
			mListener = (OnStatusFragStartedListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement OnStatusFragStartedListener");
		}
	}

	@Override
	public void onDetach() {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onDetach.");}
		super.onDetach();
		mListener = null;
	}

	public interface OnStatusFragStartedListener {
		public void StatusStarted();
	}
}
