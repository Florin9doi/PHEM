package com.perpendox.phem;

import java.io.File;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.EditText;


public class SessionSaveFragment extends Fragment implements OnClickListener {
	private static final String LOG_TAG = SessionSaveFragment.class.getSimpleName();
	private EditText filename_field;
	private SuicidalFragmentListener mListener;

	
	public static SessionSaveFragment newInstance(String param1, String param2) {
		SessionSaveFragment fragment = new SessionSaveFragment();
		return fragment;
	}

	public SessionSaveFragment() {
		// Required empty public constructor
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		View the_view = inflater.inflate(R.layout.fragment_save_session,
				container, false);
		// Get references to the layout elements.
		Button b1 = (Button) the_view.findViewById(R.id.save_button);
		b1.setOnClickListener(this);
		
		filename_field = (EditText)the_view.findViewById(R.id.session_edittext);
		
		if (PHEMNativeIF.session_file_name.length() <= 0 ||
				PHEMNativeIF.session_file_name.equals("autosave.psf")) {

			// Give the user a reasonable starting point.
			String new_file_name = PHEMNativeIF.GetCurrentDevice();
			new_file_name += "-" +
			  android.text.format.DateFormat.format("yyyy-MM-dd", new java.util.Date()).toString();
			filename_field.setText(new_file_name);
		} else {
			// Initialize with the current session name, strip off '.psf' extension
			filename_field.setText(PHEMNativeIF.session_file_name.substring(0,
				PHEMNativeIF.session_file_name.lastIndexOf('.')));
		}
		return the_view;
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
		try {
			mListener = (SuicidalFragmentListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement SuicidalFragmentListener");
		}
	}

	@Override
	public void onDetach() {
		super.onDetach();
		mListener = null;
	}

	@Override
	public void onClick(View arg0) {
		// Make sure we have some text in the EditText.
		if (filename_field.getText().length() <= 0) {
			// Alert the user
        	new AlertDialog.Builder(getActivity())
        	.setTitle(R.string.empty_filename_title)
        	.setIcon(android.R.drawable.ic_dialog_alert)
        	.setMessage(R.string.empty_filename)
        	.setNegativeButton(R.string.ok,
        			new DialogInterface.OnClickListener() {
        		public void onClick(DialogInterface dialog,
        				int which) {
        			//finish();
        		}
        	}).show();
        	return;
		}

		// Confirm if overwriting...
		File psf_file = new File(PHEMNativeIF.phem_base_dir + "/"
				+ filename_field.getText() + ".psf");
		
		if (psf_file.exists()) {
			// Make sure the user wants to overwrite.
        	new AlertDialog.Builder(getActivity())
        	.setTitle(R.string.overwrite_file_title)
        	.setIcon(android.R.drawable.ic_dialog_alert)
        	.setMessage(R.string.overwrite_file_text)
        	.setNegativeButton(R.string.cancel,
        			new DialogInterface.OnClickListener() {
        		public void onClick(DialogInterface dialog,
        				int which) {
					dialog.cancel();
					return; // Not saving...
        		}
        	})
        	.setPositiveButton(R.string.ok,
        			new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog,int id) {
					dialog.cancel();
				}
			  }).show();
		}

		Intent intent = new Intent(MainActivity.SAVE_SESSION_INTENT);
		Log.d(LOG_TAG, "Save file name is:" + filename_field.getText().toString() + ".psf");
		intent.putExtra("psf_file", filename_field.getText().toString() + ".psf");
		LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(
				intent);
		// Let our enclosing activity know we're done.
		if (mListener != null) {
			mListener.onFragmentSuicide(getTag());
		}
		// Actually save the session...
		//PHEMNativeIF.Save_Session(filename_field.getText().toString() + ".psf");		
	}

}
