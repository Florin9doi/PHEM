package com.perpendox.phem;

import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.MotionEvent;

/**
 * The Button Bar. The original emulator only believed in mice and keyboards;
 * this fragment allows us to 'push' more than one 'hardware' button at a time.
 * Hooray for multi-touch!
 */

public class ButtonBarFragment extends Fragment {
    // Container Activity must implement this interface
	private final static String LOG_TAG = ButtonBarFragment.class.getSimpleName();
    public interface OnButtonPressedListener {
        public void onButtonPressedListener(int button_id, boolean down);
    }

	private OnButtonPressedListener mCallback;

	/**
	 * Use this factory method to create a new instance of this fragment using
	 * the provided parameters.
	 * 
	 * @param param1
	 *            Parameter 1.
	 * @param param2
	 *            Parameter 2.
	 * @return A new instance of fragment ButtonBarFragment.
	 */
	// TODO: Rename and change types and number of parameters
	public static ButtonBarFragment newInstance(String param1, String param2) {
		ButtonBarFragment fragment = new ButtonBarFragment();
		Bundle args = new Bundle();
		// Don't really need arguments...
		fragment.setArguments(args);
		return fragment;
	}

	public ButtonBarFragment() {
		// Required empty public constructor
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (getArguments() != null) {
			// Don't really need arguments...
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.fragment_button_bar, container, false);
	}


	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
        // This makes sure that the container activity has implemented
        // the callback interface. If not, it throws an exception
        try {
            mCallback = (OnButtonPressedListener) activity;
        } catch (ClassCastException e) {
            throw new ClassCastException(activity.toString()
                    + " must implement OnButtonPressedListener");
        }
	}
	
	// Set up the callbacks for the buttons. Can't just use the 'onClick' interface,
	// since we need to know when the button is *released*, too.
	@Override
	public void onActivityCreated (Bundle savedInstanceState) {
		super.onActivityCreated(savedInstanceState);
		// Need a touchListener function. Passes the button press (up or down)
		// on up to the enclosing Activity, to be eventually forwarded to the
		// emulator.
		OnTouchListener touch_listener = new OnTouchListener() {
		    @Override
		    public boolean onTouch(View v, MotionEvent event) {
	        	if (MainActivity.enable_logging) {Log.d(LOG_TAG, "View tag is:" + v.getTag());}
		    	int button_id = Integer.parseInt((String)v.getTag()); // identifies which button
		        switch(event.getAction()) {
		        case MotionEvent.ACTION_DOWN:
					mCallback.onButtonPressedListener(button_id, true);
		            break;
		        case MotionEvent.ACTION_UP:
		        	mCallback.onButtonPressedListener(button_id, false);
		            break;
		        }
		        return false;
		    }
		};
		
		// Set up all the callbacks. Wish I could do this in some kind of loop...
		View db_button = getView().findViewById(R.id.action_datebook_button);
		db_button.setOnTouchListener(touch_listener);

		View contact_button = getView().findViewById(R.id.action_contact_button);
		contact_button.setOnTouchListener(touch_listener);

		View up_button = getView().findViewById(R.id.action_up_button);
		up_button.setOnTouchListener(touch_listener);
		
		View down_button = getView().findViewById(R.id.action_down_button);
		down_button.setOnTouchListener(touch_listener);

		View todo_button = getView().findViewById(R.id.action_todo_button);
		todo_button.setOnTouchListener(touch_listener);

		View memo_button = getView().findViewById(R.id.action_memo_button);
		memo_button.setOnTouchListener(touch_listener);
	}

	@Override
	public void onDetach() {
		super.onDetach();
		mCallback = null;
	}

}
