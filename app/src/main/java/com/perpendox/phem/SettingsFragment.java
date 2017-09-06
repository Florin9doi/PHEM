package com.perpendox.phem;

import java.nio.ByteBuffer;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.View.OnClickListener;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

/**
 * Fragment for updating advanced settings. It'd be nice if Google provided a
 * PreferencesFragment in the compatibility library, but oh well.
 */
public class SettingsFragment extends Fragment implements OnClickListener {
	private String LOG_TAG = SettingsFragment.class.getSimpleName();
	
	// The actual UI elements
	private EditText sync_name, serial_dev;
	private CheckBox enable_sound, enable_net, double_scale, enable_ab,
		use_hwkeys, gps_pass, gps_filter;
	private SeekBar smooth_pixels, smooth_period, update_interval;
	private TextView smooth_pixels_value, smooth_period_value, update_interval_value;
	private Button apply_button;

	// Keeps track of changes.
	int cb_change_count = 0;
	private boolean smooth_pixels_changed = false, smooth_period_changed = false, 
			update_interval_changed = false, sync_name_changed = false,
			serial_dev_changed = false;

	private SuicidalFragmentListener mListener;

	public static SettingsFragment newInstance(String param1, String param2) {
		SettingsFragment fragment = new SettingsFragment();
		return fragment;
	}

	public SettingsFragment() {
		// Required empty public constructor
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
	}

	@SuppressLint("NewApi") // We check for hardware menu key
	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		View the_view = inflater.inflate(R.layout.fragment_settings, container, false);
		
		// Record so we can detect changes.
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Getting HotSync name."); }
		MainActivity.instance.Pause_The_Emulator();
		String temp_sync_name = MainActivity.Convert_Palm_Chars_To_Native(
				ByteBuffer.wrap(PHEMNativeIF.GetHotSyncName()));
		MainActivity.instance.Resume_The_Emulator();
		final String original_sync_name = temp_sync_name;

		final String original_serial_dev = PHEMNativeIF.GetSerialDevice();
		// Get refs to the layout elements, fill in initial vals
		serial_dev = (EditText) the_view.findViewById(R.id.serial_edittext);
		serial_dev.setText(PHEMNativeIF.GetSerialDevice());
		serial_dev.addTextChangedListener(new TextWatcher() {

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count,
                    int after) {
            }

            @Override
            public void afterTextChanged(Editable s) {
            	if (s.toString().contentEquals(original_serial_dev)) {
            		serial_dev_changed = false;
            		if (!Pref_Data_Changed()) {
						apply_button.setVisibility(View.GONE);
            		}
            	} else {
            		serial_dev_changed = true;
            		apply_button.setVisibility(View.VISIBLE);
            	}
            }
        });

		gps_pass = (CheckBox) the_view.findViewById(R.id.chk_gps_passthrough);
		gps_filter = (CheckBox) the_view.findViewById(R.id.chk_no_filter_gps);
		gps_pass.setChecked(MainActivity.Get_Location_Active());
		gps_filter.setChecked(MainActivity.filter_gps);
		if (MainActivity.IsGPSEnabled()) {
			gps_pass.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					// Has it been changed from what we set it?
					MainActivity.gps_checked = ((CheckBox) v).isChecked();
					if (MainActivity.gps_checked) {
						gps_filter.setEnabled(true);
					} else {
						gps_filter.setEnabled(false);
					}
					if (((CheckBox) v).isChecked() != MainActivity
							.Get_Location_Active()) {
						cb_change_count++;
						apply_button.setVisibility(View.VISIBLE);
					} else {
						cb_change_count--;
						if (!Pref_Data_Changed()) {
							apply_button.setVisibility(View.GONE);
						}
					}
				}
			});
			gps_filter.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					// Only allow changes if gps passthrough enabled
					if (MainActivity.gps_checked) {
						// Has it been changed from what we set it?
						if (((CheckBox) v).isChecked() != MainActivity.filter_gps) {
							cb_change_count++;
							apply_button.setVisibility(View.VISIBLE);
						} else {
							cb_change_count--;
							if (!Pref_Data_Changed()) {
								apply_button.setVisibility(View.GONE);
							}
						}
					} else {
						// Disallow changes
						gps_filter.setChecked(MainActivity.filter_gps);
					}
				}
			});
			if (gps_pass.isChecked()) {
				gps_filter.setEnabled(true);
			} else {
				gps_filter.setEnabled(false);
			}
		} else {
			// No GPS, no point in enabling
			gps_pass.setVisibility(View.GONE);
			gps_filter.setVisibility(View.GONE);
		}

		sync_name = (EditText) the_view.findViewById(R.id.hotsync_edittext);
		sync_name.setText(original_sync_name);
		sync_name.addTextChangedListener(new TextWatcher() {

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
            }

            @Override
            public void beforeTextChanged(CharSequence s, int start, int count,
                    int after) {
            }

            @Override
            public void afterTextChanged(Editable s) {
            	if (s.toString().contentEquals(original_sync_name)) {
            		sync_name_changed = false;
            		if (!Pref_Data_Changed()) {
						apply_button.setVisibility(View.GONE);
            		}
            	} else {
            		sync_name_changed = true;
            		apply_button.setVisibility(View.VISIBLE);
            	}
            }
        });

		apply_button = (Button) the_view.findViewById(R.id.apply_button);
		apply_button.setOnClickListener(this);

		double_scale = (CheckBox) the_view.findViewById(R.id.chk_double_scale);
		double_scale.setChecked(PHEMNativeIF.GetScale());
		double_scale.setOnClickListener(new OnClickListener() {
		  @Override
		  public void onClick(View v) {
	        // Has it been changed from what we set it?
			if (((CheckBox) v).isChecked() != PHEMNativeIF.GetScale()) {
				cb_change_count++;
				apply_button.setVisibility(View.VISIBLE);
			} else {
				cb_change_count--;
				if (!Pref_Data_Changed()) {
					apply_button.setVisibility(View.GONE);
				}
			}
		  }
		});

		enable_net = (CheckBox) the_view.findViewById(R.id.chk_enable_net);
		enable_net.setChecked(PHEMNativeIF.GetNetlibRedirect());
		enable_net.setOnClickListener(new OnClickListener() {
		  @Override
		  public void onClick(View v) {
	        // Has it been changed from what we set it?
			if (((CheckBox) v).isChecked() != PHEMNativeIF.GetNetlibRedirect()) {
				cb_change_count++;
				apply_button.setVisibility(View.VISIBLE);
			} else {
				cb_change_count--;
				if (!Pref_Data_Changed()) {
					apply_button.setVisibility(View.GONE);
				}
			}
		  }
		});

		enable_sound = (CheckBox) the_view.findViewById(R.id.chk_enable_sound);
		enable_sound.setChecked(MainActivity.enable_sound);
		enable_sound.setOnClickListener(new OnClickListener() {
		  @Override
		  public void onClick(View v) {
	        // Has it been changed from what we set it?
			if (((CheckBox) v).isChecked() != MainActivity.enable_sound) {
				cb_change_count++;
				apply_button.setVisibility(View.VISIBLE);
			} else {
				cb_change_count--;
				if (!Pref_Data_Changed()) {
					apply_button.setVisibility(View.GONE);
				}
			}
		  }
		});

		enable_ab = (CheckBox) the_view.findViewById(R.id.chk_enable_ab);
		enable_ab.setChecked(MainActivity.enable_ab);
		
		if (android.os.Build.VERSION.SDK_INT <= 10
				|| (android.os.Build.VERSION.SDK_INT >= 14 && ViewConfiguration
						.get(MainActivity.instance).hasPermanentMenuKey())) {
			// Menu key is present
			enable_ab.setOnClickListener(new OnClickListener() {
			  @Override
			  public void onClick(View v) {
		        // Has it been changed from what we set it?
				if (((CheckBox) v).isChecked() != MainActivity.enable_ab) {
					cb_change_count++;
					apply_button.setVisibility(View.VISIBLE);
				} else {
					cb_change_count--;
					if (!Pref_Data_Changed()) {
						apply_button.setVisibility(View.GONE);
					}
				}
			  }
			});
		} else {
			// No menu key
			// Hide this pref and its strut
			enable_ab.setVisibility(View.GONE);
			the_view.findViewById(R.id.ab_strut).setVisibility(View.GONE);
		}

		use_hwkeys = (CheckBox) the_view.findViewById(R.id.chk_use_hwkeys);
		use_hwkeys.setChecked(MainActivity.use_hwkeys);
		use_hwkeys.setOnClickListener(new OnClickListener() {
		  @Override
		  public void onClick(View v) {
	        // Has it been changed from what we set it?
			if (((CheckBox) v).isChecked() != MainActivity.use_hwkeys) {
				cb_change_count++;
				apply_button.setVisibility(View.VISIBLE);
			} else {
				cb_change_count--;
				if (!Pref_Data_Changed()) {
					apply_button.setVisibility(View.GONE);
				}
			}
		  }
		});

		// The values of the seek bars. (Values set later.)
		smooth_pixels_value = (TextView) the_view.findViewById(R.id.smooth_pixel_value);
		smooth_period_value = (TextView) the_view.findViewById(R.id.smooth_period_value);
		update_interval_value = (TextView) the_view.findViewById(R.id.update_interval_value);


		// Now, the seek bars themselves.
		smooth_pixels = (SeekBar) the_view.findViewById(R.id.smooth_pixel_sb);
		smooth_pixels.setProgress(Smooth_Pixels_Value_To_SB(MainActivity.smooth_pixels));
		smooth_pixels.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				// Convert progress to internal values.
				int val = Smooth_Pixels_SB_To_Value(progress);
				smooth_pixels_value.setText(Integer.toString(val));
				if (val != MainActivity.smooth_pixels) {
					smooth_pixels_changed = true;
					apply_button.setVisibility(View.VISIBLE);
				} else {
					smooth_pixels_changed = false;
					if (!Pref_Data_Changed()) {
						apply_button.setVisibility(View.GONE);
					}
				}
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
			}
		});

		smooth_period = (SeekBar) the_view.findViewById(R.id.smooth_period_sb);
		smooth_period.setProgress(Smooth_Period_Value_To_SB(MainActivity.smooth_period));
		smooth_period.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				// Convert progress to internal values.
				int val = Smooth_Period_SB_To_Value(progress);
				smooth_period_value.setText(Integer.toString(val));
				if (val != MainActivity.smooth_period) {
					smooth_period_changed = true;
					apply_button.setVisibility(View.VISIBLE);
				} else {
					smooth_period_changed = false;
					if (!Pref_Data_Changed()) {
						apply_button.setVisibility(View.GONE);
					}
				}
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
			}
		});

		update_interval = (SeekBar) the_view.findViewById(R.id.update_interval_sb);
		update_interval.setProgress(Update_Interval_Value_To_SB(MainActivity.update_interval));
		update_interval.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			public void onProgressChanged(SeekBar seekBar, int progress,
					boolean fromUser) {
				// Convert progress to internal values.
				int val = Update_Interval_SB_To_Value(progress);
				// The display value needs to be handled separately.
				update_interval_value.setText(Integer
						.toString(Update_Interval_SB_To_Display_Value(progress)));
				if (val != MainActivity.update_interval) {
					update_interval_changed = true;
					apply_button.setVisibility(View.VISIBLE);
				} else {
					update_interval_changed = false;
					if (!Pref_Data_Changed()) {
						apply_button.setVisibility(View.GONE);
					}
				}
			}

			public void onStartTrackingTouch(SeekBar seekBar) {
			}

			public void onStopTrackingTouch(SeekBar seekBar) {
			}
		});

		// Now, we set the 'value' TextViews. We need to translate from SeekBar
		// progress values to the values the user sees.
		smooth_pixels_value.setText(Integer.toString(
						Smooth_Pixels_SB_To_Value(smooth_pixels.getProgress())));
		smooth_period_value.setText(Integer
				.toString(Smooth_Period_SB_To_Value(smooth_period.getProgress())));
		update_interval_value.setText(Integer
				.toString(Update_Interval_SB_To_Display_Value(update_interval.getProgress())));

		// Okay, ready for business.
		return the_view;
	}

	// We keep track if any prefs have actually changed, so that we can set
	// the visibility of the 'Apply' button correctly.
	private boolean Pref_Data_Changed() {
		return (cb_change_count > 0 || smooth_pixels_changed || smooth_period_changed
				|| update_interval_changed || sync_name_changed || serial_dev_changed);
	}

	// Maps internal values to positions on seekbar.
	private int Smooth_Pixels_SB_To_Value(int sb_val) {
		return sb_val+1;
	}
	private int Smooth_Pixels_Value_To_SB(int val) {
		return val-1;
	}

	private int Smooth_Period_SB_To_Value(int sb_val) {
		return (sb_val+1)*25;
	}
	private int Smooth_Period_Value_To_SB(int val) {
		return (val/25)-1;
	}

	// The actual value used by the code is in 'milliseconds to wait'.
	// So we map between seek bar values and the internal values.
	private int Update_Interval_SB_To_Value(int sb_val) {
		switch (sb_val) {
		case 0:
			return 200;
		case 1:
			return 100;
		case 2:
			return 67;
		case 3:
			return 50;
		case 4:
			return 40;
		case 5:
			return 33;
		default:
			Log.e(LOG_TAG, "Hit default in UI_SB2V!");
			return 100;
		}
	}
	private int Update_Interval_Value_To_SB(int val) {
		switch (val) {
		case 200:
			return 0;
		case 100:
			return 1;
		case 67:
			return 2;
		case 50:
			return 3;
		case 40:
			return 4;
		case 33:
			return 5;
		default:
			Log.e(LOG_TAG, "Hit default in UI_V2SB!");
			return 1;
		}
	}
	// This one's a little special. The SeekBar holds arbitrary integers, and
	// the actual value we use is in milliseconds. But what we show to the user
	// is 'updates per second'.
	private int Update_Interval_SB_To_Display_Value(int sb_val) {
		switch (sb_val) {
		case 0:
			return 5;
		case 1:
			return 10;
		case 2:
			return 15;
		case 3:
			return 20;
		case 4:
			return 25;
		case 5:
			return 30;
		default:
			Log.e(LOG_TAG, "Hit default in UI_SB2DispV!");
			return 10;
		}
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
		// The "Apply" button's only enabled if there's been a data change.
		// So we know we have to do *something*.
		
		if (sync_name_changed) {
			// Must handle Android/Palm character encoding issues.
			MainActivity.instance.Pause_The_Emulator();
			PHEMNativeIF.SetHotSyncName(MainActivity.Make_Palm_Chars_From_Native(sync_name.getText().toString()));
			MainActivity.instance.Resume_The_Emulator();
		}

		if (cb_change_count > 0) {
			// Nothing special here.
			MainActivity.enable_sound = enable_sound.isChecked();
			MainActivity.use_hwkeys = use_hwkeys.isChecked();

			MainActivity.enable_ab = enable_ab.isChecked();
			MainActivity.Set_Action_Bar_Visible();

			MainActivity.filter_gps = gps_filter.isChecked();

			// Reconcile with emulator.
			if (PHEMNativeIF.GetNetlibRedirect() != enable_net.isChecked()) {
				PHEMNativeIF.SetNetlibRedirect(enable_net.isChecked());
			}

			if (PHEMNativeIF.GetScale() != double_scale.isChecked()) {
				PHEMNativeIF.SetScale(double_scale.isChecked());
			}
		}
		// Can just blindly update these. They'll take effect immediately.
		MainActivity.update_interval =
				Update_Interval_SB_To_Value(update_interval.getProgress());
		MainActivity.smooth_pixels =
				Smooth_Pixels_SB_To_Value(smooth_pixels.getProgress());
		MainActivity.smooth_period =
				Smooth_Period_SB_To_Value(smooth_period.getProgress());

		// Update the serial port state.
		if (serial_dev_changed) {
			PHEMNativeIF.Set_Serial_Device(serial_dev.getText().toString());
		}
		if (gps_pass.isChecked()) {
			PHEMNativeIF.Set_Serial_Device("/@"); // special device name
			MainActivity.instance.Set_Location_Active(gps_pass.isChecked());
		} else {
			if (MainActivity.Get_Location_Active()) {
				// Gotta stop it.
				MainActivity.instance.Set_Location_Active(gps_pass.isChecked());
			}
		}

		// Note that we don't actually save these prefs to the SharedPreferences storage.
		// We let the activity do that when it's paused/shut down.

		// Let our enclosing activity know we're done.
		if (mListener != null) {
			mListener.onFragmentSuicide(getTag());
		}
	}
}
