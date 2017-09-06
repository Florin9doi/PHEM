package com.perpendox.phem;


import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.support.v4.content.LocalBroadcastManager;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.widget.Button;

/**
 * A simple {@link android.support.v4.app.Fragment} subclass. Activities that
 * contain this fragment must implement the
 * {@link ResetFragment.OnResetChosenListener} interface to handle
 * interaction events.
 * 
 */
public class ResetFragment extends Fragment implements OnClickListener {

	private SuicidalFragmentListener mListener;

	/**
	 * Use this factory method to create a new instance of this fragment using
	 * the provided parameters.
	 */
	// TODO: Rename and change types and number of parameters
	public static ResetFragment newInstance() {
		ResetFragment fragment = new ResetFragment();
		return fragment;
	}

	public ResetFragment() {
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
		View the_view = inflater.inflate(R.layout.fragment_reset, container, false);
        Button b1 = (Button) the_view.findViewById(R.id.soft_reset_button);
        b1.setOnClickListener(this);
        Button b2 = (Button) the_view.findViewById(R.id.noext_reset_button);
        b2.setOnClickListener(this);
        Button b3 = (Button) the_view.findViewById(R.id.hard_reset_button);
        b3.setOnClickListener(this);
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
        case R.id.soft_reset_button:
    		intent = new Intent(MainActivity.RESET_INTENT);
    		intent.putExtra("reset_type", PHEMNativeIF.SOFT_RESET);
    		LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(intent);

    		// Let our enclosing activity know we're done.
    		if (mListener != null) {
    			mListener.onFragmentSuicide(getTag());
    		}
            break;
        case R.id.noext_reset_button:
    		intent = new Intent(MainActivity.RESET_INTENT);
    		intent.putExtra("reset_type", PHEMNativeIF.NOEXT_RESET);
    		LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(intent);

    		// Let our enclosing activity know we're done.
    		if (mListener != null) {
    			mListener.onFragmentSuicide(getTag());
    		}
            break;
        case R.id.hard_reset_button:
    		intent = new Intent(MainActivity.RESET_INTENT);
    		intent.putExtra("reset_type", PHEMNativeIF.HARD_RESET);
    		LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(intent);

    		// Let our enclosing activity know we're done.
    		if (mListener != null) {
    			mListener.onFragmentSuicide(getTag());
    		}
            break;
        }
	}
}
