package com.perpendox.phem;

import java.io.File;
import java.io.FilenameFilter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;

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
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;
import android.widget.AdapterView.OnItemSelectedListener;

/**
 * A simple {@link android.support.v4.app.Fragment} subclass. Activities that
 * contain this fragment must implement the
 * {@link SessionCreateFragment.OnFragmentInteractionListener} interface to
 * handle interaction events. Use the
 * {@link SessionCreateFragment#newInstance} factory method to create an
 * instance of this fragment.
 * 
 */
public class SessionCreateFragment extends Fragment implements OnClickListener {
	private static final String LOG_TAG = SessionCreateFragment.class.getSimpleName();
	String selected_rom = "";
	String selected_dev = "";
	String selected_ram = "";
	// Set up the spinners.
	Spinner rom_spinner, dev_spinner, ram_spinner;
	Button done_button;
	Boolean session_parameters_specified = false;

	private SuicidalFragmentListener mListener;

	/**
	 * Use this factory method to create a new instance of this fragment using
	 * the provided parameters.
	 *
	 * @return A new instance of fragment Create_Session_Fragment.
	 */
	public static SessionCreateFragment newInstance() {
		SessionCreateFragment fragment = new SessionCreateFragment();
		return fragment;
	}

	public SessionCreateFragment() {
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
		View the_view = inflater.inflate(R.layout.fragment_create_session,
				container, false);
		done_button = (Button)the_view.findViewById(R.id.donebutton);
		done_button.setOnClickListener(this);

		// First, get the available ROMs.
		String rom_dir_path = PHEMNativeIF.phem_base_dir + "/roms";
		File rom_dir = new File(rom_dir_path);
		// Obviously, we want to limit the filenames shown to ones that
		// actually might be ROM images. Hence the filter.
		File[] filenames = rom_dir.listFiles(new FilenameFilter() {
			public boolean accept(File dir, String name) {
				return (name.toLowerCase(Locale.getDefault()).endsWith(".rom") || name
						.toLowerCase(Locale.getDefault()).endsWith(".bin"));
			}
		});

		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Got file names...");}
		// What if they ain't no ROMs?
		if (null == filenames || filenames.length < 1) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No roms alert");}
			String no_roms_msg = getResources().getString(R.string.no_roms_found)
					+ rom_dir_path;
			new AlertDialog.Builder(getActivity())
					.setTitle(R.string.no_roms_title)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(no_roms_msg)
					.setNegativeButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int which) {
									// Nothing to be done here...
								}
							}).show();
			// Can't do jack at this point.

			// Gotta start session from scratch.
			Intent intent = new Intent(MainActivity.NEW_SESSION_INTENT);
			intent.putExtra("start_type", 0);
			LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(
					intent);
			if (null != mListener) {
				mListener.onFragmentSuicide(getTag());
			}
		} else {
			// Okay, at least one ROM found. Add result(s) to spinner.
			ArrayList<String> rom_arrayList = new ArrayList<String>();

			for (File tmpf : filenames) {
				rom_arrayList.add(tmpf.getName());
			}
			// Set up the spinners and button.
			rom_spinner = (Spinner) the_view.findViewById(R.id.romspin);
			dev_spinner = (Spinner) the_view.findViewById(R.id.devspin);
			ram_spinner = (Spinner) the_view.findViewById(R.id.ramspin);
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Looked up spinners and button...");}

			// Set the default.
			selected_rom = filenames[0].getName();
			ArrayAdapter<String> rom_adp = new ArrayAdapter<String>(
					getActivity(),
					android.R.layout.simple_spinner_dropdown_item,
					rom_arrayList);
			rom_spinner.setAdapter(rom_adp);
			rom_spinner.setSelection(0); // Note: this ends up calling
											// Set_Device_Spinner().
			rom_spinner.setVisibility(View.VISIBLE);
			// Set listener; Called when the item is selected in spinner
			rom_spinner.setOnItemSelectedListener(new OnItemSelectedListener() {
				public void onItemSelected(AdapterView<?> parent, View view,
						int position, long arg3) {
					selected_rom = parent.getItemAtPosition(position)
							.toString();
					if (MainActivity.enable_logging) {Log.d(LOG_TAG, "selected_rom = " + selected_rom);}
					ram_spinner.setVisibility(View.GONE);
					Set_Device_Spinner();
				}

				public void onNothingSelected(AdapterView<?> parent) {
					// selected_rom = parent.getSelectedItem().toString();
				}
			});
		}

		return the_view;
	}

	private void Set_Device_Spinner() {
		String[] temp_arr = PHEMNativeIF
				.GetROMDevices(PHEMNativeIF.phem_base_dir + "/roms/"
						+ selected_rom);
		// Do we have any devices?
		if (null == temp_arr) {
			// Uh oh. Post alert.
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No valid device alert");}
			new AlertDialog.Builder(getActivity())
					.setTitle(R.string.no_device_title)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(R.string.no_device_text)
					.setNegativeButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int which) {
									// Nothing to be done here...
								}
							}).show();
			dev_spinner.setVisibility(View.GONE);
			ram_spinner.setVisibility(View.GONE);
			done_button.setVisibility(View.GONE);
			return;
		}
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Converting array to arraylist.");}
		ArrayList<String> dev_arrayList = new ArrayList<String>(
				Arrays.asList(temp_arr));
		ArrayAdapter<String> dev_adp;
		// If we got this far, we've got at least one device.
		dev_adp = new ArrayAdapter<String>(getActivity(),
				android.R.layout.simple_spinner_dropdown_item, dev_arrayList);
		dev_spinner.setAdapter(dev_adp);

		dev_spinner.setVisibility(View.VISIBLE);
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Spinner should be visible.");}
		dev_spinner.setOnItemSelectedListener(new OnItemSelectedListener() {
			public void onItemSelected(AdapterView<?> parent, View view,
					int position, long arg3) {
				selected_dev = parent.getItemAtPosition(position).toString();
				if (MainActivity.enable_logging) {Log.d(LOG_TAG, "selected_dev = " + selected_dev);}
				Set_Memory_Spinner();
			}

			public void onNothingSelected(AdapterView<?> parent) {
				// selected_dev = parent.getSelectedItem().toString();
			}
		});
		// If there's only one device, set up the memory stuff too.
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Calling Set_Memory_Spinner");}
		if (dev_arrayList.size() == 1) {
			selected_dev = temp_arr[0];
			Set_Memory_Spinner();
		}
	}

	private void Set_Memory_Spinner() {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "In Set_Memory_Spinner");}
		String[] temp_arr = PHEMNativeIF.GetDeviceMemories(selected_dev);
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Returned from native");}

		// Do we have any memory sizes?
		if (null == temp_arr) {
			// Uh oh. Post alert.
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No valid RAM alert");}
			new AlertDialog.Builder(getActivity())
					.setTitle(R.string.no_memory_title)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(R.string.no_memory_text)
					.setNegativeButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int which) {
									// Nothing to be done here...
								}
							}).show();
			ram_spinner.setVisibility(View.GONE);
			done_button.setVisibility(View.GONE);
			return;
		}
		
		ArrayList<String> ram_arrayList = new ArrayList<String>(
				Arrays.asList(temp_arr));
		
		// 16MB of RAM causes problems for most ROMs, but the m515 needs it.
		if (!selected_dev.equals("PalmM515")) {
			// Not an m515. Got to filter out the 16MB option, if present.
			int place = ram_arrayList.indexOf("16384K");
			if (-1 != place) {
				ram_arrayList.remove(place);
			}
		}

		// In theory, the ListArray could be empty here, but I don't know of any
		// other ROMs that *require* 16MB.
		ArrayAdapter<String> ram_adp;
		// If we got this far, we've got at least one memory config.
		ram_adp = new ArrayAdapter<String>(getActivity(),
				android.R.layout.simple_spinner_dropdown_item, ram_arrayList);
		ram_spinner.setAdapter(ram_adp);
		// By default, choose the largest available memory.
		ram_spinner.setSelection(ram_arrayList.size() - 1);
		selected_ram = ram_arrayList.get(ram_arrayList.size() - 1);
		session_parameters_specified = true;
		ram_spinner.setVisibility(View.VISIBLE);
		done_button.setVisibility(View.VISIBLE);

		ram_spinner.setOnItemSelectedListener(new OnItemSelectedListener() {
			public void onItemSelected(AdapterView<?> parent, View view,
					int position, long arg3) {
				selected_ram = parent.getItemAtPosition(position).toString();
				if (MainActivity.enable_logging) {Log.d(LOG_TAG, "selected_ram = " + selected_ram);}
			}

			public void onNothingSelected(AdapterView<?> parent) {
				// selected_dev = parent.getSelectedItem().toString();
			}
		});
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
	public void onClick(View v) {
		if (R.id.donebutton == v.getId()) {
			Intent intent;
			if (session_parameters_specified) {
				intent = new Intent(MainActivity.NEW_SESSION_INTENT);
				intent.putExtra("start_type", 2);
				intent.putExtra("rom_name", selected_rom);
				intent.putExtra("device", selected_dev);
				intent.putExtra("ram_size", selected_ram);
				// Starting a new session.
			} else {
				if (MainActivity.enable_logging) {Log.d(LOG_TAG, "specifying autosave.");}
				// For now, just check to see if there's an autosave file.
				File psf_file = new File(PHEMNativeIF.phem_base_dir
						+ "/autosave.psf");

				if (psf_file.exists()) {
					// Loading existing session
					intent = new Intent(MainActivity.NEW_SESSION_INTENT);
					intent.putExtra("start_type", 1);
					intent.putExtra("psf_name", psf_file);

				} else {
					// Gotta start session from scratch.
					intent = new Intent(MainActivity.NEW_SESSION_INTENT);
					intent.putExtra("start_type", 0);
				}
			}
			LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(
					intent);
			if (null != mListener) {
				mListener.onFragmentSuicide(getTag());
			}
		}
	}

}
