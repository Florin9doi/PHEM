package com.perpendox.phem;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.widget.Button;

/**
 * A simple {@link android.support.v4.app.Fragment} subclass. Activities that
 * contain this fragment must implement the
 * {@link SessionManageFragment.OnFragmentInteractionListener} interface to
 * handle interaction events. Use the {@link SessionManageFragment#newInstance}
 * factory method to create an instance of this fragment.
 * 
 */
public class SessionManageFragment extends Fragment implements OnClickListener {

	private static final String LOG_TAG= SessionManageFragment.class.getSimpleName();
	// TODO: Rename and change types and number of parameters
	public static SessionManageFragment newInstance(String param1, String param2) {
		SessionManageFragment fragment = new SessionManageFragment();
		return fragment;
	}

	public SessionManageFragment() {
		// Required empty public constructor
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "onCreateView");}
		// Inflate the layout for this fragment
		View the_view = inflater.inflate(R.layout.fragment_manage_session,
				container, false);
        Button b1 = (Button) the_view.findViewById(R.id.new_session_button);
        b1.setOnClickListener(this);
        Button b2 = (Button) the_view.findViewById(R.id.load_session_button);
        b2.setOnClickListener(this);
        Button b3 = (Button) the_view.findViewById(R.id.save_session_button);
        b3.setOnClickListener(this);
		return the_view;
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
	}

	@Override
	public void onDetach() {
		super.onDetach();
	}

	@Override
	public void onDestroy() {
		MainActivity.sub_fragment_active = false;
		super.onDestroy();
	}

	@Override
	public void onClick(View v) {
		Fragment new_frag;
        switch (v.getId()) {
        case R.id.new_session_button:
    		new_frag = new SessionCreateFragment();
    		getFragmentManager()
    		 .beginTransaction()
    		 .replace(R.id.current_op_fragment, new_frag, MainActivity.TAG_NEW_SESSION)
    		 .addToBackStack(MainActivity.TAG_NEW_SESSION)
    		 .commit();
            break;
        case R.id.load_session_button:
        	if (MainActivity.enable_logging) {Log.d("PHEM mgmt session", "Launching File Chooser.");}
    		String[] file_types = {".psf"};
        	new_frag = File_Chooser_Fragment.newInstance(MainActivity.LOAD_SESSION_CODE,
        			PHEMNativeIF.phem_base_dir, file_types);
        	getFragmentManager().beginTransaction()
					.replace(R.id.current_op_fragment, new_frag,
						MainActivity.TAG_LOAD_SESSION)
						.addToBackStack(MainActivity.TAG_LOAD_SESSION)
						.commit();
            break;
        case R.id.save_session_button:
    		new_frag = new SessionSaveFragment();
    		getFragmentManager()
    		 .beginTransaction()
    		 .replace(R.id.current_op_fragment, new_frag, MainActivity.TAG_SAVE_SESSION)
    		 .addToBackStack(MainActivity.TAG_SAVE_SESSION)
    		 .commit();
            break;
        }
	}
}
