package com.perpendox.phem;

import android.app.Activity;
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
import android.widget.CheckBox;
import android.widget.TextView;


public class Card_Mgmt_Fragment extends Fragment implements OnClickListener {

	private String default_dir;
	private TextView hostfs_txt, dir_txt, dir_val;
	private Button install_button, done_button;
	private CheckBox mount_cb;
	private View val_strut, mount_strut;
	//private static final String LOG_TAG = Card_Mgmt_Fragment.class.getSimpleName();
	private static final String DEF_DIR = "default_dir";

	private SuicidalFragmentListener mListener;

	public static Card_Mgmt_Fragment newInstance(String def_dir) {
		Card_Mgmt_Fragment fragment = new Card_Mgmt_Fragment();
		Bundle args = new Bundle();
		args.putString(DEF_DIR, def_dir);
		fragment.setArguments(args);
		return fragment;
	}

	public Card_Mgmt_Fragment() {
		// Required empty public constructor
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (getArguments() != null) {
			default_dir = getArguments().getString(DEF_DIR);
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		View the_view = inflater.inflate(R.layout.fragment_card_mgmt,
				container, false);
		// Get refs to the layout elements.
		install_button = (Button) the_view.findViewById(R.id.install_hostfs_button);
		install_button.setOnClickListener(this);
		
		hostfs_txt = (TextView) the_view.findViewById(R.id.installhostfslabel);
		
		dir_val = (TextView) the_view.findViewById(R.id.dirval);
		
		done_button = (Button) the_view.findViewById(R.id.card_done_button);
		done_button.setOnClickListener(this);
		
		dir_txt = (TextView) the_view.findViewById(R.id.dirlabel);
		
		mount_cb = (CheckBox) the_view.findViewById(R.id.chk_mounted);
		synchronized (PHEMApp.idle_sync) {
			mount_cb.setChecked(PHEMNativeIF.GetCardMountStatus());
		}
		mount_cb.setOnClickListener(this);

		val_strut = the_view.findViewById(R.id.val_strut);
		
		mount_strut = the_view.findViewById(R.id.mount_strut);
		
		// So... what are we doing today?
		if (null != default_dir && default_dir.length() > 0) {
			dir_val.setText(default_dir);
			Log.d("PHEM cardemu", "Default dir found.");
			// Show the dir selection stuff.
			dir_txt.setVisibility(View.VISIBLE);
			dir_val.setVisibility(View.VISIBLE);
			//done_button.setVisibility(View.VISIBLE);
			mount_cb.setVisibility(View.VISIBLE);
			val_strut.setVisibility(View.VISIBLE);
			mount_strut.setVisibility(View.VISIBLE);
		} else {
			Log.d("PHEM cardemu", "No default dir found.");
			// Must need to install the HostFS library.
			// Show the HostFS stuff
			hostfs_txt.setVisibility(View.VISIBLE);
			install_button.setVisibility(View.VISIBLE);
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
	public void onDestroy() {
		MainActivity.sub_fragment_active = false;
		super.onDestroy();
	}

	@Override
	public void onClick(View v) {
		Intent intent;
		switch (v.getId()) {
		case R.id.chk_mounted:
			synchronized (PHEMApp.idle_sync) {
				if (mount_cb.isChecked() != PHEMNativeIF.GetCardMountStatus()) {
					done_button.setVisibility(View.VISIBLE);
				} else {
					done_button.setVisibility(View.GONE);
				}
			}
			break;
		case R.id.install_hostfs_button:
			intent = new Intent(MainActivity.INSTALL_HOSTFS_INTENT);
			LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(
					intent);
			// Let our enclosing activity know we're done.
			if (mListener != null) {
				mListener.onFragmentSuicide(getTag());
			}
			break;
		case R.id.card_done_button:
			intent = new Intent(MainActivity.CARD_OP_INTENT);
			intent.putExtra("mount_stat", mount_cb.isChecked());
			intent.putExtra("card_dir", dir_val.getText());
			LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(
					intent);
			// Let our enclosing activity know we're done.
			if (mListener != null) {
				mListener.onFragmentSuicide(getTag());
			}
			break;
		}
	}
}
