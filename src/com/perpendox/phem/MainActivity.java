package com.perpendox.phem;

import java.io.File;
import java.io.FileOutputStream;
import java.io.FilenameFilter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.Thread.UncaughtExceptionHandler;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Locale;












import com.perpendox.phem.PHEMNativeIF;

import android.location.GpsStatus.NmeaListener;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.os.ConditionVariable;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.os.Vibrator;
import android.app.AlertDialog;
import android.app.Notification;
import android.app.NotificationManager;
import android.text.ClipboardManager; // Old clipboard manager to support Froyo 2.2
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.support.v4.app.Fragment;
import android.support.v4.app.FragmentManager;
import android.support.v4.app.FragmentTransaction;
import android.support.v4.content.LocalBroadcastManager;
import android.support.v7.app.ActionBarActivity;

@SuppressWarnings("deprecation") // Using old clipboard to support 2.2
public class MainActivity extends ActionBarActivity implements
		ButtonBarFragment.OnButtonPressedListener, SuicidalFragmentListener,
		StatusFragment.OnStatusFragStartedListener, LocationListener, NmeaListener {
	public static final Boolean enable_logging = false;
	public static MainActivity instance = null;
	private static View screen_view, button_view, cur_op_view;
	private static ScreenFragment screen_frag;
	private static StatusFragment status_frag;
	static PHEMSoundService phem_sound_service;
	private static ClipboardManager clipboard; // Cut and paste text between Palm and host
	private Vibrator vibrate; // For passing on vibration
	private static NotificationManager n_mgr; // For controlling the LED

	private static final int AB_MENU_GROUP = 1;
	public static final int LOAD_SESSION_CODE = 101;
	public static final int INSTALL_FILE_CODE = 102;
	private static final String LOG_TAG = MainActivity.class.getSimpleName();
	
	private static String app_load_dir;
	
	public static LocationManager LM;

	private static Charset palm_charset;
	
	// Intent types
	public static final String LOAD_FILE_INTENT = "load_file";
	public static final String LOAD_FILE_TYPE = "load_type";
	public static final String LOAD_FILE_PATH = "load_path";
	public static final String LOAD_FILE_NAME = "load_name";

	public static final String NEW_SESSION_INTENT = "new_session";
	public static final String SAVE_SESSION_INTENT = "save_session";
	public static final String INSTALL_HOSTFS_INTENT = "install_hostfs";
	public static final String CARD_OP_INTENT = "card_op";
	public static final String RESET_INTENT = "reset_file";

	// Fragment tags
	public static final String TAG_SESSION_MGMT = "session_mgmt_tag";
	public static final String TAG_NEW_SESSION = "new_session_tag";
	public static final String TAG_LOAD_SESSION = "load_session_tag";
	public static final String TAG_SAVE_SESSION = "save_session_tag";
	public static final String TAG_INSTALL_FILE = "install_file_tag";
	public static final String TAG_CARD_MGMT = "card_mgmt_tag";
	public static final String TAG_STATUS = "status_tag";
	public static final String TAG_RESET = "reset_tag";
	public static final String TAG_ABOUT = "about_tag";
	public static final String TAG_SETTINGS = "settings_tag";
	public static final String TAG_TASK_FRAGMENT = "task_frag_tag";
	public static final String TAG_ALERT_FRAGMENT = "alert_frag_tag";

	// Preference keys
	public static final String PREF_ROOT = "com.perpendox.phem";
	public static final String PREF_ENABLE_SOUND = PREF_ROOT + ".enable_sound";
	public static final String PREF_SMOOTH_PIXELS = PREF_ROOT
			+ ".smooth_pixels";
	public static final String PREF_SMOOTH_PERIOD = PREF_ROOT
			+ ".smooth_period";
	public static final String PREF_UPDATE_INTERVAL = PREF_ROOT
			+ ".update_interval";
	public static final String PREF_LAST_SESSION = PREF_ROOT + ".last_session";
	public static final String PREF_APP_CRASHED = PREF_ROOT + ".has_crashed";
	public static final String PREF_APP_RATED = PREF_ROOT + ".rated";
	public static final String PREF_LAST_LOAD_DIR = PREF_ROOT + ".last_load_dir";
	public static final String PREF_ENABLE_ACTION_BAR = PREF_ROOT + ".enable_actionbar";
	public static final String PREF_USE_HWKEYS = PREF_ROOT + ".use_hwkeys";
	public static final String PREF_FILTER_GPS = PREF_ROOT + ".filter_gps";
	private static final int LED_NOTIFICATION_ID = 0;

	// workaround for bug in Android; even if a checkbox is disabled you can still click it
	public static boolean gps_checked = false;

	// Quarter-ass licensing for beta tests.
	private static boolean review_copy = false;
	public static final String PHEM_SITE = "http://perpendox.com/phem/";
	public static final String REVIEW_TARGET = "rev-2.txt";

	// Fragment management - don't allow willy-nilly fragment creation
	public static boolean sub_fragment_active = false;
	
	// Used to suppress key events if the emulator isn't in a position to
	// do anything about them.
	public static boolean screen_paused = true; // set to 'false' by onResume in ScreenFragment

	// Variables set by preferences.
	public static boolean enable_sound;
	public static boolean app_crashed;
	public static boolean app_rated;
	public static boolean enable_ab;
	public static boolean enable_location;
	public static boolean use_hwkeys;
	public static boolean filter_gps;
	public static int smooth_pixels;
	public static int smooth_period;
	public static int update_interval; // milliseconds
	SharedPreferences prefs;

	/*
	 * We can't use the UI thread to tell the emulator to do idle processing.
	 * And not just because of the potential to over-work the UI thread!
	 * If the emulator pops a dialog when we call HandleIdle(), it'll *block*
	 * the UI thread. Which is bad.
	 * 
	 * So, we spawn a separate thread to poll for updates. We do have to
	 * synchronize to make sure we don't send a touch or button press at the
	 * same moment as we're doing the idle polling, though. We use a periodic
	 * timer to wake up the thread to trigger idle processing.
	 * 
	 * The thread is actually owned by the application, not a particular
	 * activity, though. JNI is picky about the 'environment' that native
	 * calls happen in, and those are bound to particular threads. If we
	 * only ever have one thread, we don't have to keep re-binding every
	 * time the orientation changes and such.
	 */
	public static int pause_count = 0;

    public Handler ui_handler = new Handler(Looper.getMainLooper());

	private static boolean run_periodic_idle;
	// Trigger periodic polling of the emulator for screen updates, etc.
	private static Runnable Idle_Trigger = new Runnable() {
		public void run() {
			if (run_periodic_idle) {
				Message msg = PHEMApp.idle_handler.obtainMessage();
				//msg.arg1 = IdleHandler.DO_IDLE;
				PHEMApp.idle_handler.sendMessage(msg);
				// Schedule us to pop again.
				if (null != MainActivity.instance) {
					MainActivity.instance.ui_handler.postDelayed(Idle_Trigger,
							update_interval);
				}
			}
		}
	};

	// We need to know if we're in landscape mode or not, to know what fragments
	// should be visible.
	private boolean not_landscape() {
		return !getResources().getBoolean(R.bool.is_landscape);
	}

	// Our handlers for received Intents.
	private BroadcastReceiver LoadFileReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			// Get extra data included in the Intent
			String the_path, the_file;
			OperationFragment opFragment;
			int load_type = intent.getIntExtra(LOAD_FILE_TYPE,
					INSTALL_FILE_CODE);
			the_path = intent.getStringExtra(LOAD_FILE_PATH);
			the_file = the_path + "/";
			the_file += intent.getStringExtra(LOAD_FILE_NAME);
			if (the_file.length() <= 1) {
				Log.e(LOG_TAG, "Load file msg but no file specified! "
						+ load_type);
				return;
			} else {
				if (MainActivity.enable_logging)  {
				Log.d(LOG_TAG, "Got load file msg: " + the_file);
				}
			}
			switch (load_type) {
			case INSTALL_FILE_CODE:
				if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Installing file."); }
				app_load_dir = the_path; // Track where we loaded from last time.
				opFragment = new OperationFragment();
				opFragment.setTask(new InstallFileOperation(the_file));

				// Show the fragment.
				opFragment.show(getSupportFragmentManager(), TAG_TASK_FRAGMENT);

				break;
			case LOAD_SESSION_CODE:
				if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Loading session."); }
				// The instance can go away if the native side crashes.
				if (null != MainActivity.instance) {
					opFragment = new OperationFragment();

					opFragment.setTask(new LoadSessionOperation(intent
							.getStringExtra(LOAD_FILE_NAME)));

					// Show the fragment.
					opFragment.show(getSupportFragmentManager(),
							TAG_TASK_FRAGMENT);
					// Set the bitmap for the emulator screen.
					if (null != screen_frag) {
						screen_frag.Load_Bitmap(MainActivity.instance,
								R.drawable.phem_loading);
					}
				}
				break;
			default:
				if (MainActivity.enable_logging)  {
				Log.e(LOG_TAG, "Unknown load type! " + load_type);
				}
			}
		}
	};

	private BroadcastReceiver NewSessionReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			OperationFragment opFragment;
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Starting new session.");
			}
			String the_rom = intent.getStringExtra("rom_name");
			String the_dev = intent.getStringExtra("device");
			String the_ram = intent.getStringExtra("ram_size");
			if (MainActivity.enable_logging)  {
			Log.d("PHEM main activity", "Specs are: " + the_rom + " " + the_dev
					+ " " + the_ram);
			}
			// The instance can go away if the native side crashes.
			if (null != MainActivity.instance) {
				opFragment = new OperationFragment();
				opFragment.setTask(new NewSessionOperation(the_rom, the_dev,
						the_ram));

				// Show the fragment.
				opFragment.show(getSupportFragmentManager(), TAG_TASK_FRAGMENT);
				// Set the bitmap for the emulator screen.

				screen_frag.Load_Bitmap(MainActivity.instance,
						R.drawable.phem_loading);
			}
		}
	};

	private BroadcastReceiver SaveSessionReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			String save_file = intent.getStringExtra("psf_file");
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Got save session message: " + save_file);
			}
			PHEMNativeIF.Save_Session(save_file);
		}
	};

	private BroadcastReceiver ResetReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			// What kind of reset are we doing?
			int reset_type = intent.getIntExtra("reset_type",
					PHEMNativeIF.SOFT_RESET);
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Got reset message: " + reset_type);
			}
			PHEMNativeIF.ResetEmulator(reset_type);
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Reset initiated.");
			}
		}
	};

	private BroadcastReceiver CardOpReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			// Get extra data included in the Intent
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "onARes, setting card status.");
			}
			String the_path = intent.getStringExtra("card_dir");
			boolean mount_stat = intent.getBooleanExtra("mount_stat", false);
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Card dir is: '" + the_path + "' mounted: "
					+ mount_stat);
			}
			// Actually set data
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Setting dir.");
			}
			PHEMNativeIF.SetCurrentCardDir(the_path);
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Setting mount status.");}
			PHEMNativeIF.SetCardMountStatus(mount_stat);
		}
	};

	private BroadcastReceiver InstallHostFSReceiver = new BroadcastReceiver() {
		@Override
		public void onReceive(Context context, Intent intent) {
			Install_HostFS();
		}
	};

	// Utility to extract files from the 'asset' section of our apk
	// into the device storage.
	private File Save_Asset_File(String dir, String asset) {
		File the_file;
		try {
			InputStream is = getAssets().open(asset);
			// Write it out to a file.
			the_file = new File(dir, asset);

			OutputStream out_st = new FileOutputStream(the_file);
			byte file_buf[] = new byte[1024];
			int len;
			while ((len = is.read(file_buf)) > 0) {
				out_st.write(file_buf, 0, len);
			}
			out_st.close();
			is.close();
		} catch (IOException e) {
			// Should never happen!
			Log.e(LOG_TAG, "error saving " + asset + "!");
			throw new RuntimeException(e);
		}
		return the_file;
	}

	// The emulator can't see into our APK file, so we extract the HostFS.prc
	// asset to a file, install it, then delete the file and reset the Palm.
	private void Install_HostFS() {
		File lib_file;

		lib_file = Save_Asset_File(PHEMNativeIF.phem_base_dir, "HostFS.prc");
		// Okay, the file is in storage, visible to the emulator.
		// Load it.
		Pause_Idle();
		PHEMNativeIF.Install_Palm_File(PHEMNativeIF.phem_base_dir
				+ "/HostFS.prc");
		Resume_Idle();
		// Trigger soft reset.
		PHEMNativeIF.ResetEmulator(PHEMNativeIF.SOFT_RESET);

		// Delete library file from storage, not needed anymore.
		lib_file.delete();
	}

	// Using old clip API to support Android 2.2
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		if (MainActivity.enable_logging)  {
		Log.d(LOG_TAG, "onCreate.");
		}
		super.onCreate(savedInstanceState);
		instance = this;

		// Get the preferences, first thing.
		prefs = this.getSharedPreferences(PREF_ROOT, Context.MODE_PRIVATE);
		enable_sound = prefs.getBoolean(PREF_ENABLE_SOUND, true);
		app_crashed = prefs.getBoolean(PREF_APP_CRASHED, false);
		app_rated = prefs.getBoolean(PREF_APP_RATED, false);
		enable_ab = prefs.getBoolean(PREF_ENABLE_ACTION_BAR, true);
		use_hwkeys = prefs.getBoolean(PREF_USE_HWKEYS, false);
		filter_gps = prefs.getBoolean(PREF_FILTER_GPS, true);
		smooth_pixels = prefs.getInt(PREF_SMOOTH_PIXELS, 4);
		smooth_period = prefs.getInt(PREF_SMOOTH_PERIOD, 250);
		update_interval = prefs.getInt(PREF_UPDATE_INTERVAL, 100);
		PHEMNativeIF.session_file_name = prefs.getString(PREF_LAST_SESSION, "");

		// Don't really need the app title. Make room for more icons.
		getSupportActionBar().setDisplayShowTitleEnabled(false);

		// Save some screen real estate if the user asks.
		Set_Action_Bar_Visible();

		// For passing GPS to Palm
		LM = (LocationManager) getSystemService(Context.LOCATION_SERVICE);

		vibrate = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);

		// Needed to exchange cut/paste between host and emulated Palm
		clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);

		// Palm OS natively uses the CP1252 ("Windows-1252") encoding (or CP932 for
		// Japanese). For ASCII it doesn't matter, but many European languages have
		// chars which don't match. So we try to encode it here.
		// Note we use a resource to specify the Palm codepage. This is overridden
		// in the "values-ja" strings file.
		if (Charset.isSupported(MainActivity.instance.getResources().getString(R.string.palm_codepage))) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, MainActivity.instance.getResources().getString(R.string.palm_codepage)
						+ " is supported."); }
			palm_charset = Charset.forName(MainActivity.instance.getResources().getString(R.string.palm_codepage));
		} else {
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Falling back to ISO-8859-1."); }
			// Shucks. Can't find the charset we really want. Fall back on one that's close,
			// at least for Western languages
			palm_charset = Charset.forName("ISO-8859-1");
		}

		// For controlling the LED
		n_mgr = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);

		// Start the sound service, so we can hear those fabulous beeps
		if (MainActivity.enable_logging)  {
		Log.d(LOG_TAG, "Creating sound service.");
		}
		phem_sound_service = new PHEMSoundService();
		Intent intent = new Intent(this, PHEMSoundService.class);
		if (MainActivity.enable_logging)  { Log.d(LOG_TAG, "Starting sound service."); }
		startService(intent);

		/*
		idle_thread = new HandlerThread("IdleThread",
	            android.os.Process.THREAD_PRIORITY_DISPLAY
	            + android.os.Process.THREAD_PRIORITY_LESS_FAVORABLE );
//	            +
//	            android.os.Process.THREAD_PRIORITY_MORE_FAVORABLE);
		idle_thread.start();
		idle_looper = idle_thread.getLooper();
	    idle_handler = new IdleHandler(idle_looper);
	    */
		//idler_thread.start();
		
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG,
					"Using vals from: "
							+ getResources().getString(R.string.which_one));
//			DisplayMetrics displayMetrics = getResources().getDisplayMetrics();
//			float screenWidthDp = displayMetrics.widthPixels
//					/ displayMetrics.density;
//			float screenHeightDp = displayMetrics.heightPixels
//					/ displayMetrics.density;
//			Log.d(LOG_TAG, "Screen width is: " + screenWidthDp + " height: "
//					+ screenHeightDp);
		}
		setContentView(R.layout.main);

		// By default, orientation changes are permitted. But we only want
		// to allow landscape if the screen is physically large enough.
		// (Seriously, on a 3.7-inch screen a landscape Palm emulator is
		// pointless.
		// Even on a 5-inch screen it's marginal.)
		if (!getResources().getBoolean(R.bool.big_screen)) {
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Not big screen, locking orientation.");
			}
			// Restrict the orientation to portrait.
			setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
		}

		// Set up the fragments we're using.
		if ((screen_view = findViewById(R.id.emu_fragment)) != null) {
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Found screen fragment.");
			}
			// If we're not being restored from a previous state,
			// we need to set up the screen fragment.
			if (savedInstanceState == null) {
				// Create an instance of the ScreenFragment (actual emulator)
				screen_frag = new ScreenFragment();

				// In case this activity was started with special instructions
				// from an Intent,
				// pass the Intent's extras to the fragment as arguments
				screen_frag.setArguments(getIntent().getExtras());

				// Add the fragment to the 'fragment_container' FrameLayout
				getSupportFragmentManager().beginTransaction()
						.add(R.id.emu_fragment, screen_frag).commit();
			} else {
				// Find existing screen fragment...
				if (enable_logging) {Log.d(LOG_TAG, "Finding previous screen fragment.");}
				screen_frag = (ScreenFragment) getSupportFragmentManager()
						.findFragmentById(R.id.emu_fragment);
				if (null == screen_frag) {
					Log.e(LOG_TAG, "Couldn't find previous screen fragment!");
				}
			}
			screen_view.requestFocus();
		} else {
			Log.e(LOG_TAG, "Whoa, couldn't find screen_vew!");
		}

		if ((cur_op_view = findViewById(R.id.current_op_fragment)) != null) {
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Found current_op fragment.");
			}
			// If we're not being restored from a previous state,
			// we need to set up the current_op fragment.
			if (savedInstanceState == null) {
				// Create an instance of the StatusFragment
				status_frag = new StatusFragment();

				// In case this activity was started with special
				// instructions from an Intent,
				// pass the Intent's extras to the fragment as arguments
				status_frag.setArguments(getIntent().getExtras());

				// Add the fragment to the 'fragment_container' FrameLayout
				getSupportFragmentManager().beginTransaction()
						.add(R.id.current_op_fragment, status_frag, TAG_STATUS)
						//.addToBackStack(TAG_STATUS)
						.commit();
			} else {
				// Find existing status fragment...
				if (enable_logging) {Log.d(LOG_TAG, "Finding previous status fragment.");}
//				status_frag = (StatusFragment) getSupportFragmentManager()
//						.findFragmentById(R.id.current_op_fragment);
				status_frag = (StatusFragment) getSupportFragmentManager()
						.findFragmentByTag(TAG_STATUS);
				if (null == status_frag) {
					Log.e(LOG_TAG, "Couldn't find previous status fragment!");
				}
			}
		} else {
			Log.e(LOG_TAG, "Whoa, couldn't find cur_op_view!");
		}

		// Set up the fragments we're using.
		if ((button_view = findViewById(R.id.button_bar_fragment)) != null) {
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Found button bar fragment."); }
			// If we're not being restored from a previous state,
			// we need to set up the button bar fragment.
			if (savedInstanceState == null) {

				// Create an instance of the ButtonBarFragment (multitouch help)
				ButtonBarFragment secondFragment = new ButtonBarFragment();

				// In case this activity was started with special instructions
				// from an Intent,
				// pass the Intent's extras to the fragment as arguments
				secondFragment.setArguments(getIntent().getExtras());

				// Add the fragment to the 'fragment_container' FrameLayout
				getSupportFragmentManager().beginTransaction()
						.add(R.id.button_bar_fragment, secondFragment).commit();
			}
		} else {
			Log.e(LOG_TAG, "Whoa, couldn't find button_view!");
		}

		// Helps keep track of whether the app has crashed or not.
        UncaughtExceptionHandler currentHandler = Thread.getDefaultUncaughtExceptionHandler();
        // Don't register again if already registered.
        if (!(currentHandler instanceof ExceptionHandler)) {
                // Register default exceptions handler.
                Thread.setDefaultUncaughtExceptionHandler(new ExceptionHandler(currentHandler, this));
        }

		// Now... if we're not in landscape, we can't show all our fragments at
		// once. If that's the case, hide what's necessary.
		if (not_landscape()) {
			if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Not landscape.");
			}
			// We need to hide *something*. Determine if it's the screen &
			// button bar, or if it's the 'current_op'.
			if (MainActivity.enable_logging)  {
				Log.d(LOG_TAG, "onCreate frag stack count is: "
					+ getSupportFragmentManager().getBackStackEntryCount());
			}
			if (getSupportFragmentManager().getBackStackEntryCount() > 0) {
				// We had some kind of pending operation from before a config
				// change.
				// Hide the screen & button bar
				cur_op_view.setVisibility(View.VISIBLE);
				screen_view.setVisibility(View.GONE);
				button_view.setVisibility(View.GONE);
			} else {
				// Hide the current_op view.
				cur_op_view.setVisibility(View.GONE);
				screen_view.setVisibility(View.VISIBLE);
				button_view.setVisibility(View.VISIBLE);
			}
		}

		// What's the storage situation?
		if (!Environment.MEDIA_MOUNTED.equals(Environment
				.getExternalStorageState())
				|| !Environment.getExternalStorageDirectory().canRead()) {
			// Hey, we can't get at the storage!
			new AlertDialog.Builder(this)
					.setTitle(R.string.no_sdcard_title)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(R.string.no_sdcard)
					.setNegativeButton(R.string.quit,
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int which) {
									finish();
								}
							}).show();

			return;
		}

		// Confirm and/or create our directories.
		File ext_dir = Environment.getExternalStorageDirectory();
		if (MainActivity.enable_logging)  {
		Log.d(LOG_TAG, "stor abs path is: " + ext_dir.getAbsolutePath());
		}
		String phem_dir_path = ext_dir.getAbsolutePath() + "/phem";
		File phem_dir = new File(phem_dir_path);
		phem_dir.mkdirs(); // Create dir if necessary.

		if (!phem_dir.isDirectory()) {
			// Maybe storage is not writable?
			new AlertDialog.Builder(this)
					.setTitle(R.string.cannot_make_dirs_title)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(R.string.cannot_make_dirs)
					.setNegativeButton(R.string.quit,
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int which) {
									finish();
								}
							}).show();
			return;
		}

		// Okay, tell the emulator where its home dir is.
		PHEMNativeIF.getInstance().Set_PHEM_Dir(phem_dir_path);
		
		// *Now* we can set the load dir.
		app_load_dir = prefs.getString(PREF_LAST_LOAD_DIR,
				PHEMNativeIF.phem_base_dir);

		// Create the ROM, skin, and card dirs, if needed.
		String phem_rom_dir_path = phem_dir_path + "/roms";
		File phem_rom_dir = new File(phem_rom_dir_path);
		phem_rom_dir.mkdirs();
		String phem_skin_dir_path = phem_dir_path + "/skins";
		File phem_skin_dir = new File(phem_skin_dir_path);
		if (!phem_skin_dir.exists()) {
			phem_skin_dir.mkdirs();
			// Load up the special PHEM-specific Handera skin so we can
			// transparently handle its nonstandard 240x320 screen.
			Save_Asset_File(phem_skin_dir_path, "HandEra330.skin");
			Save_Asset_File(phem_skin_dir_path, "HandEra330_1x.jpg");
			Save_Asset_File(phem_skin_dir_path, "HandEra330_2x.jpg");
		}
		String phem_card_dir_path = phem_dir_path + "/card";
		File phem_card_dir = new File(phem_card_dir_path);
		phem_card_dir.mkdirs();
		
		// Are there any ROMs in the ROM dir?
		File[] filenames = phem_rom_dir.listFiles(new FilenameFilter() {
			public boolean accept(File dir, String name) {
				return (name.toLowerCase(Locale.getDefault()).endsWith(".rom") || name
						.toLowerCase(Locale.getDefault()).endsWith(".bin"));
			}
		});

		// What if they ain't no ROMs?
		if (null == filenames || filenames.length < 1) {
			if (enable_logging) {Log.d(LOG_TAG, "No roms alert");}
			String no_rom_msg = getResources().getString(R.string.no_roms_found)
					+ phem_rom_dir_path;
			new AlertDialog.Builder(this)
					.setTitle(R.string.no_roms_title)
					.setIcon(android.R.drawable.ic_dialog_alert)
					.setMessage(no_rom_msg)
					.setNegativeButton(R.string.ok,
							new DialogInterface.OnClickListener() {
								public void onClick(DialogInterface dialog,
										int which) {
									finish();
								}
							}).show();
		}
	}

	@Override
	protected void onStop() {
		super.onStop();
		if (enable_logging) {
			Log.d(LOG_TAG, "onStop, saving session.");
		}
		//Pause_The_Emulator();
		//PHEMNativeIF.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
		PHEMNativeIF.Save_Session(PHEMNativeIF.session_file_name);
	}
	@Override
	protected void onDestroy() {
		// By now, session should be saved.
		PHEMNativeIF.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
		super.onDestroy();
	}

	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		getMenuInflater().inflate(R.menu.main_menu, menu);
		
		// Got to give the user some way to access these if the action bar
		// is disabled.
		menu.add(AB_MENU_GROUP, R.id.power_butn, Menu.NONE,
				getResources().getText(R.string.power_title));
		menu.add(AB_MENU_GROUP, R.id.session_mgt, Menu.NONE,
				getResources().getText(R.string.session_mgt_title));
		menu.add(AB_MENU_GROUP, R.id.install_file, Menu.NONE,
				getResources().getText(R.string.install_file_title));
		menu.add(AB_MENU_GROUP, R.id.input_methd, Menu.NONE,
				getResources().getText(R.string.input_menu_title));
		// Note: visible (in overflow menu) only if ab *dis*abled.
		menu.setGroupVisible(AB_MENU_GROUP, !enable_ab);

		// By default, this entry is disabled.
		menu.findItem(R.id.card_emu).setEnabled(false);
		menu.findItem(R.id.card_emu).setVisible(false);
		return super.onCreateOptionsMenu(menu);
	}

	@Override
	public boolean onPrepareOptionsMenu(Menu menu) {
		if (MainActivity.enable_logging)  {		Log.d(LOG_TAG, "Checking if ROM has VFSManager...");}
		// Note: visible if ab disabled.
		menu.setGroupVisible(AB_MENU_GROUP, !enable_ab);

		synchronized (PHEMApp.idle_sync) { // needed?
			if (PHEMNativeIF.Has_VFS()) {
				menu.findItem(R.id.card_emu).setEnabled(true);
				menu.findItem(R.id.card_emu).setVisible(true);
			}
		}
		return true;
	}

	private void Hide_Emulator_If_Not_Landscape() {
		if (not_landscape()) {
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Hiding screen, button bar.");}
			// Hide the screen & button bar
			cur_op_view.setVisibility(View.VISIBLE);
			screen_view.setVisibility(View.GONE);
			button_view.setVisibility(View.GONE);
		}
	}

	// We need to do this a couple different places, so we factor out this code.
	private void Start_Session_Manager() {
		Hide_Emulator_If_Not_Landscape();
		Fragment new_frag = new SessionManageFragment();
		FragmentTransaction transaction = getSupportFragmentManager()
				.beginTransaction().replace(R.id.current_op_fragment,
						new_frag, TAG_SESSION_MGMT);
		transaction.addToBackStack(TAG_SESSION_MGMT).commit();
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		Fragment new_frag;
		FragmentTransaction transaction;
		
		// The first two ops can be allowed at any time.
		switch (item.getItemId()) {
		case R.id.power_butn:
			// No fragment needed for this one. Just emulating quick tap on the
			// power button.
			synchronized (PHEMApp.idle_sync) {
				PHEMApp.idle_handler.do_power_button = true;
			}
			return true;
		case R.id.input_methd:
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Input method selected.");}
			screen_view.requestFocus();
			screen_view.requestFocusFromTouch();
			InputMethodManager inputMgr = (InputMethodManager)getSystemService(Context.INPUT_METHOD_SERVICE);
			inputMgr.toggleSoftInput(0, 0);
			return true;
		default:
			// Remaining ops shouldn't be allowed when another sub-fragment is active.
			if (sub_fragment_active) {
				if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Blocking new fragment.");}
			} else {
				switch (item.getItemId()) {
				case R.id.about_phem:
					Hide_Emulator_If_Not_Landscape();
					// Load up the specified fragment, add whatever's currently
					// there to
					// the back stack.
					new_frag = new AboutFragment();
					transaction = getSupportFragmentManager()
							.beginTransaction().replace(
									R.id.current_op_fragment, new_frag);
					transaction.addToBackStack(null).commit();
					sub_fragment_active = true;
					return true;
				case R.id.session_mgt:
					Start_Session_Manager();
					sub_fragment_active = true;
					return true;
				case R.id.reset_menu:
					Hide_Emulator_If_Not_Landscape();
					// Load up the specified fragment, add whatever's currently
					// there to
					// the back stack.
					new_frag = new ResetFragment();
					transaction = getSupportFragmentManager()
							.beginTransaction().replace(
									R.id.current_op_fragment, new_frag,
									TAG_RESET);
					transaction.addToBackStack(TAG_RESET).commit();
					sub_fragment_active = true;
					return true;
				case R.id.install_file:
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "Install file selected.");
					}
					Hide_Emulator_If_Not_Landscape();
					String[] file_types = { ".prc", ".pdb", ".pqa" };
					// What if the place we loaded from last time doesn't exist
					// no more?
					File load_dir = new File(app_load_dir);
					if (!load_dir.exists()) {
						app_load_dir = PHEMNativeIF.phem_base_dir;
					}
					new_frag = File_Chooser_Fragment.newInstance(
							INSTALL_FILE_CODE, app_load_dir, file_types);
					transaction = getSupportFragmentManager()
							.beginTransaction().replace(
									R.id.current_op_fragment, new_frag,
									TAG_INSTALL_FILE);
					transaction.addToBackStack(TAG_INSTALL_FILE).commit();
					sub_fragment_active = true;
					return true;
				case R.id.card_emu: // Note: this menu's only enabled if VFS
									// found.
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "Card emu selected.");
					}
					Hide_Emulator_If_Not_Landscape();
					// Load up the specified fragment, add whatever's currently
					// there to
					// the back stack.
					boolean has_hostfs;
					synchronized (PHEMApp.idle_sync) {
						has_hostfs = PHEMNativeIF.IsHostFSPresent();
					}
					if (has_hostfs) {
						if (MainActivity.enable_logging) {
							Log.d(LOG_TAG, "Got hostfs.");
						}
						String cur_card_dir = PHEMNativeIF.GetCurrentCardDir();
						File card_dir_file = new File(cur_card_dir);
						if (!card_dir_file.exists()
								|| cur_card_dir.length() < 2) {
							if (MainActivity.enable_logging) {
								Log.d(LOG_TAG, "Getting default card dir.");
							}
							cur_card_dir = PHEMNativeIF.phem_base_dir + "/card";
						}
						new_frag = Card_Mgmt_Fragment.newInstance(cur_card_dir);
					} else {
						new_frag = new Card_Mgmt_Fragment();
					}
					transaction = getSupportFragmentManager()
							.beginTransaction().replace(
									R.id.current_op_fragment, new_frag,
									TAG_CARD_MGMT);
					transaction.addToBackStack(TAG_CARD_MGMT).commit();
					sub_fragment_active = true;
					return true;
				case R.id.action_settings:
					Hide_Emulator_If_Not_Landscape();
					// Load up the specified fragment, add whatever's currently
					// there to
					// the back stack.
					new_frag = new SettingsFragment();
					transaction = getSupportFragmentManager()
							.beginTransaction().replace(
									R.id.current_op_fragment, new_frag,
									TAG_SETTINGS);
					transaction.addToBackStack(TAG_SETTINGS).commit();
					sub_fragment_active = true;
					return true;
				default:
					// We'll return below. Nothing to be done here.
				}
			}
		}
		return super.onOptionsItemSelected(item);
	}

	@Override
	public boolean onKeyDown(int keyCode, KeyEvent event) {
		// Handle the back button
		if (keyCode == KeyEvent.KEYCODE_BACK && isTaskRoot()
				&& (0 == getSupportFragmentManager().getBackStackEntryCount())) {
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "App exiting, toasting user.");}
			Pause_Idle();
			//Toast.makeText(getApplicationContext(),
			//		R.string.notify_app_shutdown, Toast.LENGTH_SHORT).show();
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Saving prefs.");}
			Save_Prefs();
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Shutting down emulator.");}
			Pause_The_Emulator();
			PHEMNativeIF.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "About to finish().");}
			finish();
			return true;
		}
		// Make sure keypresses are actually meant for the emulator.
		if (!screen_paused) {
			// Everything else, try to pass it on to the emulator.
			int the_key = event.getUnicodeChar(event.getMetaState());
			// Printable stuff gets handled here...
			if (the_key != 0) {
				if (MainActivity.enable_logging) {
					Log.d(LOG_TAG, "onKeyDown, passing " + the_key);
				}
				synchronized (PHEMApp.idle_sync) {
					PHEMNativeIF.Key_Event(the_key);
				}
			} else {
				// Non-printable. Let's see if we can translate it to something
				// the emulator grasps.
				switch (event.getKeyCode()) {
				case KeyEvent.KEYCODE_ENTER:
				case KeyEvent.KEYCODE_DPAD_CENTER:
					the_key = 0x0a;
					break;
				case KeyEvent.KEYCODE_DPAD_LEFT:
					the_key = 0x1c;
					break;
				case KeyEvent.KEYCODE_DPAD_RIGHT:
					the_key = 0x1d;
					break;
				case KeyEvent.KEYCODE_DPAD_UP:
					the_key = 0x1e;
					break;
				case KeyEvent.KEYCODE_DPAD_DOWN:
					the_key = 0x1f;
					break;
				case KeyEvent.KEYCODE_DEL:
					the_key = 0x08;
					break;
				case KeyEvent.KEYCODE_VOLUME_UP:
					if (use_hwkeys) {
						if (event.getRepeatCount() == 0) {
							synchronized (PHEMApp.idle_sync) {
								PHEMNativeIF.Button_Event(2, true);
								PHEMNativeIF.Button_Event(2, false);
							}
						}
						return true;
					} else {
						the_key = 0;
					}
					break;
				case KeyEvent.KEYCODE_VOLUME_DOWN:
					if (use_hwkeys) {
						if (event.getRepeatCount() == 0) {
							synchronized (PHEMApp.idle_sync) {
								PHEMNativeIF.Button_Event(3, true);
								PHEMNativeIF.Button_Event(3, false);
							}
						}
						return true;
					} else {
						the_key = 0;
					}
					break;
				default:
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG,
								"onKeyDown, unhandled keycode "
										+ event.getKeyCode());
					}
					the_key = 0;
				}
				if (the_key != 0) {
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "onKeyDown, passing in translated "
								+ the_key);
					}
					synchronized (PHEMApp.idle_sync) {
						PHEMNativeIF.Key_Event(the_key);
					}
					screen_view.requestFocusFromTouch();
					return true;
				}
			}
		}
		return super.onKeyDown(keyCode, event);
	}

	public static void Pause_Idle() {
		run_periodic_idle = false;
	}
	
	public static void Resume_Idle() {
		run_periodic_idle = true;
		Idle_Trigger.run();
	}
/*
	public static void Pause_The_Idle_Thread() {
		// if (true == run_periodic_idle) {
		synchronized (idle_sync) {
			pause_count++;
			// run_periodic_idle = false;
			idle_sync.notify();
		}
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Idle paused: " + pause_count);}
		// }
	}

	public static void Resume_The_Idle_Thread() {
		synchronized (idle_sync) {
			// run_periodic_idle = true;
			if (pause_count > 0) {
				pause_count--;
			}
			idle_sync.notify();
		}
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Idle resumed: " + pause_count);}
	}
*/
	public void Pause_The_Emulator() {
		Pause_Idle();
		PHEMNativeIF.Pause_Emulation();
	}

	public void Resume_The_Emulator() {
		PHEMNativeIF.Resume_Emulation();
		Resume_Idle();
	}

	public void Save_Prefs() {
		// Save current prefs.
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Save_Prefs.");}
		prefs.edit().putBoolean(PREF_ENABLE_SOUND, enable_sound)
				.putBoolean(PREF_APP_CRASHED, app_crashed)
				.putBoolean(PREF_APP_RATED, app_rated)
				.putBoolean(PREF_ENABLE_ACTION_BAR, enable_ab)
				.putBoolean(PREF_FILTER_GPS, filter_gps)
				.putInt(PREF_SMOOTH_PIXELS, smooth_pixels)
				.putInt(PREF_SMOOTH_PERIOD, smooth_period)
				.putInt(PREF_UPDATE_INTERVAL, update_interval)
				.putString(PREF_LAST_LOAD_DIR, app_load_dir).commit();
		// Save the current session (if any) to the preferences.
		if (PHEMNativeIF.session_active) {
			if (PHEMNativeIF.session_file_name == null
					|| PHEMNativeIF.session_file_name.length() <= 0) {
				PHEMNativeIF.session_file_name = "autosave.psf";
			}
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Saving session name pref: "
					+ PHEMNativeIF.session_file_name);}
			prefs.edit()
					.putString(PREF_LAST_SESSION,
							PHEMNativeIF.session_file_name).commit();
		}
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Done saving prefs.");}
	}
	
	public void Clear_Prefs() {
		// Clear out the preferences; optional for crash handling.
		prefs.edit().clear().commit();
	}

	@Override
	public void onPause() {
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "onPause.");}
		// Unregister broadcast receivers since the activity is about to be
		// paused.
		Pause_The_Emulator();
		End_Vibration();
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Unregistering receviers.");}
		LocalBroadcastManager.getInstance(this).unregisterReceiver(
				LoadFileReceiver);
		LocalBroadcastManager.getInstance(this).unregisterReceiver(
				NewSessionReceiver);
		LocalBroadcastManager.getInstance(this).unregisterReceiver(
				SaveSessionReceiver);
		LocalBroadcastManager.getInstance(this).unregisterReceiver(
				InstallHostFSReceiver);
		LocalBroadcastManager.getInstance(this).unregisterReceiver(
				CardOpReceiver);
		LocalBroadcastManager.getInstance(this).unregisterReceiver(
				ResetReceiver);

		// Stop location stuff, if enabled.
		if (enable_location) {
			// Stop NMEA listener
			LM.removeUpdates(this);
			LM.removeNmeaListener(this);
		}
		// Save the current session (if any) to the preferences.
		Save_Prefs();
		super.onPause();
	}

	@Override
	public void onResume() {
		super.onResume();
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "onResume.");}

		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Registering receivers.");}
		// Set up the broadcast receivers. This is how UI fragments tell us what
		// to do.
		LocalBroadcastManager.getInstance(this).registerReceiver(
				LoadFileReceiver, new IntentFilter(LOAD_FILE_INTENT));
		LocalBroadcastManager.getInstance(this).registerReceiver(
				NewSessionReceiver, new IntentFilter(NEW_SESSION_INTENT));
		LocalBroadcastManager.getInstance(this).registerReceiver(
				SaveSessionReceiver, new IntentFilter(SAVE_SESSION_INTENT));
		LocalBroadcastManager.getInstance(this).registerReceiver(
				InstallHostFSReceiver, new IntentFilter(INSTALL_HOSTFS_INTENT));
		LocalBroadcastManager.getInstance(this).registerReceiver(
				CardOpReceiver, new IntentFilter(CARD_OP_INTENT));
		LocalBroadcastManager.getInstance(this).registerReceiver(ResetReceiver,
				new IntentFilter(RESET_INTENT));

		// Test for beta?
		if (review_copy) {
			// Kick off a 'license check'.
			OperationFragment opFragment = new OperationFragment();
			opFragment.setTask(new CheckReviewAuth(PHEM_SITE + REVIEW_TARGET));
			// Show the fragment.
			opFragment.show(getSupportFragmentManager(), TAG_TASK_FRAGMENT);
		}
		
		// Do we need to load up a previous session?
		if (!PHEMNativeIF.Is_Session_Active()) {
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "No active session on resume.");}
			pause_count = 0; // Just in case it was left in a funky state.
			if (PHEMNativeIF.session_file_name.length() > 0) {
				if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Trying to load "
						+ PHEMNativeIF.session_file_name);}
				// Make sure it exists.
				File test_file = new File(PHEMNativeIF.Get_PHEM_Dir() + "/"
						+ PHEMNativeIF.session_file_name);
				if (test_file.exists()) {
					if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Old session exists.");}
					if (test_file.length() == 0) {
						Log.e(LOG_TAG, "Old session " + PHEMNativeIF.session_file_name + " has zero length!");
						// Let the user know...
						Corrupt_Session_File(PHEMNativeIF.session_file_name);
						Start_Session_Manager();
					} else {
						// Try to load previous session.
						Intent intent = new Intent(
								MainActivity.LOAD_FILE_INTENT);
						intent.putExtra(MainActivity.LOAD_FILE_TYPE,
								LOAD_SESSION_CODE);
						intent.putExtra(MainActivity.LOAD_FILE_PATH,
								PHEMNativeIF.Get_PHEM_Dir());
						intent.putExtra(MainActivity.LOAD_FILE_NAME,
								PHEMNativeIF.session_file_name);
						LocalBroadcastManager.getInstance(this).sendBroadcast(
								intent);
					}
				} else {
					// User's got to pick/create a new session.
					if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Old session doesn't exist.");}
					Start_Session_Manager();
				}
			} else {
				// Invite the user to create a session. We can't actually launch
				// it until the fragments are settled, so we set a flag that will
				// trigger it when the status fragment fires off the first time.
				if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Pending session manager.");}
				Start_Session_Manager();
			}
		}

		// Start location stuff, if enabled.
		if (enable_location) {
			// Start NMEA listener
			LM.requestLocationUpdates(LocationManager.GPS_PROVIDER, 200l, 0, this);
			LM.addNmeaListener(this);
		}

		// Do we need to check for session_active?
		Resume_The_Emulator();
	}

	// Using old clip API to support Android 2.2
	public static void set_Clip(String clip_text) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "set_Clip, text: " + clip_text);
		}
		// Using the old-style API because we're supporting Gingerbread.
		clipboard.setText(clip_text);
	}

	// Using old clip API to support Android 2.2
	public static String get_Clip() {
		// Using the old-style API because we're supporting Gingerbread.
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "get_Clip."); }
		if (clipboard.hasText()) {
			String temp_str = (String) clipboard.getText().toString();
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "got clip:" + temp_str);
			}
			return temp_str;
		}
		return null;
	}

//	public static String Get_Palm_Codepage() {
//		return MainActivity.instance.getResources().getString(R.string.palm_codepage);
//	}
	
	private static void SetPalmCharset() {
		// Palm OS natively uses the CP1252 ("Windows-1252") encoding for European languages.
		// It also may support CP932 for Japanese, GB2312 or Big5 for Chinese, or even
		// Ksc5601 for Korean. We try to find out what the current Palm session is using and
		// get the correct Charset.
		String charset_name;
		int encoding = PHEMNativeIF.GetPalmEncoding();
		switch (encoding) {
		case 1:
			charset_name = "CP932";
			break;
		case 2:
			charset_name = "GB2312";
			break;
		case 3:
			charset_name = "Big5";
			break;
		case 4:
			charset_name = "KSC5601";
			break;
		default:
			// No other sensible default.
			charset_name = "CP1252";
			break;
		}
		if (Charset.isSupported(charset_name)) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Charset " + charset_name
						+ " is supported."); }
			palm_charset = Charset.forName(charset_name);
		} else {
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Falling back to ISO-8859-1."); }
			// Shucks. Can't find the charset we really want. Fall back on one that's close,
			// at least for Western languages
			palm_charset = Charset.forName("ISO-8859-1");
		}
	}

	public static ByteBuffer Make_Palm_Chars_From_Native(String the_string) {
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Encoding: " + the_string); }
		SetPalmCharset(); // Make sure we're using the right charset
		ByteBuffer tempie = palm_charset.encode(the_string);
		// Yes, all this twiddling seems to be necessary.
		tempie.rewind();
		ByteBuffer the_clip = ByteBuffer.allocateDirect(tempie.capacity());
		tempie.rewind();
		the_clip.put(tempie);
		the_clip.rewind();
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "direct: " + 
					the_clip.capacity() + " original: " + tempie.capacity()); }
		return the_clip;
	}

	public static String Convert_Palm_Chars_To_Native(ByteBuffer the_chars) {
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Decoding palm chars."); }
		SetPalmCharset(); // Make sure we're using the right charset
		return palm_charset.decode(the_chars).toString();
	}

	public static void set_Bitmap_Size(final int W, final int H) {
		/*if (MainActivity.instance != null) {
			MainActivity.instance.runOnUiThread(new Runnable() {
				@Override
				public void run() {*/
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "set size, screen frag.");
					}
					if (null != screen_frag) {
						screen_frag.Set_Bitmap_Size(W, H);
						if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Size set.");}
						status_frag.update_session_info(); // Know there's a working session
															// here...
						if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Session updated.");}

					} else {
						if (MainActivity.enable_logging) {
							Log.d(LOG_TAG, "null screen frag?");
						}
					}
					/*}
			});
		}*/
	}

	public static void update_Screen_Bitmap() {
//		if (MainActivity.enable_logging) {
//			Log.d(LOG_TAG, "update_Screen_Bitmap");
//		}

		if (MainActivity.instance != null) {
//					if (MainActivity.enable_logging) {
//						Log.d(LOG_TAG, "updating screen frag.");
//					}
			if (null != screen_frag) {
				screen_frag.Set_Bitmap();
			} else {
				if (MainActivity.enable_logging) {
					Log.d(LOG_TAG, "null screen frag?");
				}
			}
		}
	}

	// Pass it on to the service.
	public static void Play_Sound(int freq, int dur, int amp) {
		if (enable_sound) {
			PHEMSoundService.DoSndCmd(freq, dur, amp);
		}
	}


	// For posting alerts from the emulator. We don't just use a standard
	// AlertDialog because we need to handle configuration changes (screen
	// rotation, etc.) without canceling the dialog. 
	public static void Do_Alert_Dialog(final int title, final String message,
			final Integer item_ids[],
			final String labels[], final Boolean properties[],
			final ConditionVariable the_cond) {
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Do_Alert_Dialog");}

		if (MainActivity.instance != null) {
			EmAlertFragment alert_frag = new EmAlertFragment();
			alert_frag.setData(title,
					message, item_ids, labels, properties, the_cond);
			alert_frag.show(MainActivity.instance.getSupportFragmentManager(),
					TAG_ALERT_FRAGMENT);
		} else {
			// Must have tried to post a dialog when the activity was being
			// destroyed/recreated. Bad luck, but we'll tell the emulator to
			// try again.
			PHEMNativeIF.button_pushed = -1;
			the_cond.open();
		}
	}

	public static void Do_Vibration() {
		if (MainActivity.instance != null) {
			// Start without a delay
			// Vibrate for 100 milliseconds
			// Sleep for 0 milliseconds
			long[] pattern = { 0, 100, 0 };

			// The '0' here means to repeat indefinitely
			// '-1' would play the vibration once
			MainActivity.instance.vibrate.vibrate(pattern, 0);
		}
	}

	public static void End_Vibration() {
		if (MainActivity.instance != null) {
			MainActivity.instance.vibrate.cancel();
		}
	}

	public static void Do_LED(int color) {
		try {
			Notification notif = new Notification();
			notif.defaults = 0;
			notif.ledARGB = color;
			notif.flags = Notification.FLAG_SHOW_LIGHTS;
			notif.ledOnMS = 100;
			notif.ledOffMS = 100;
			n_mgr.notify(LED_NOTIFICATION_ID, notif);
		} catch (Exception e) {
			Log.e(LOG_TAG, "Error setting LED notification.");
			e.printStackTrace();
		}
	}

	public static void Clear_LED() {
		try {
			n_mgr.cancel(LED_NOTIFICATION_ID);
		} catch (Exception e) {
			Log.e(LOG_TAG, "Error clearing LED notification.");
			e.printStackTrace();
		}
	}

	@Override
	public void onFragmentSuicide(String tag) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Despairing fragment: " + tag);}
		
		if (tag.contentEquals(TAG_NEW_SESSION)
				|| tag.contentEquals(TAG_LOAD_SESSION)
				|| tag.contentEquals(TAG_SAVE_SESSION)) {
			// The only way to get to these fragments is via the
			// "Session Management" fragment, so we pop back to that one.
			getSupportFragmentManager().popBackStack(TAG_SESSION_MGMT,
					FragmentManager.POP_BACK_STACK_INCLUSIVE);
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Popped session frags.");}
		} else {
			// All the other fragments are one deep.
			getSupportFragmentManager().popBackStack(tag,
					FragmentManager.POP_BACK_STACK_INCLUSIVE);
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Fragment: " + tag + " popped.");
			}
		}
		// Allow user to launch other fragments.
		sub_fragment_active = false;
	}

	@Override
	public void onButtonPressedListener(int button_id, boolean down) {
		// Pass the button presses on to the emulator.
		//MainActivity.Pause_The_Idle_Thread();
		synchronized (PHEMApp.idle_sync) {
			PHEMNativeIF.Button_Event(button_id, down);
		}
		//
		//MainActivity.Resume_The_Idle_Thread();
	}

	@Override
	public void StatusStarted() {
		// We only show the 'Status' fragment in landscape mode. It's the
		// 'default' fragment in the current_op_fragment space.
		// Should we restore the view of the screen & button bar?
		if (not_landscape()) {
			if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Status started, frag stack count is: "
					+ getSupportFragmentManager().getBackStackEntryCount());}
			// Hide the current_op view.
			cur_op_view.setVisibility(View.GONE);
			screen_view.setVisibility(View.VISIBLE);
			button_view.setVisibility(View.VISIBLE);
		}
		if (MainActivity.enable_logging)  {Log.d(LOG_TAG, "Status started complete");}
	}

	public void showCrashDialog(ConditionVariable the_cond) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Kicking off crash handler.");
		}
		try {
			// startActivity(new Intent(MainActivity.instance,
			// CrashHandlerActivity.class));
			CrashFragment cf = new CrashFragment();
			cf.setCond(the_cond);

			// Show the fragment.
			cf.show(getSupportFragmentManager(), "CrashHandler");
		} catch (Exception e) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, e.toString());
			}
		}
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "showCrashDialog returned.");
		}
	}

	public static void Review_Check_Failed(final String message) {
		MainActivity.instance.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				new AlertDialog.Builder(MainActivity.instance)
						.setTitle(R.string.review_check_failed)
						.setIcon(android.R.drawable.ic_dialog_alert)
						.setMessage(message)
						.setNegativeButton(R.string.quit,
								new DialogInterface.OnClickListener() {
									public void onClick(DialogInterface dialog,
											int which) {
										MainActivity.instance.finish();
									}
								}).show();
			}
		});
	}
	
	public void Corrupt_Session_File(final String file_name) {
		MainActivity.instance.runOnUiThread(new Runnable() {
			@Override
			public void run() {
				String message = getResources().getString(R.string.corrupt_session_msg1)
						+ file_name + getResources().getString(R.string.corrupt_session_msg2);
				new AlertDialog.Builder(MainActivity.instance)
						.setTitle(R.string.corrupt_session_title)
						.setIcon(android.R.drawable.ic_dialog_alert)
						.setMessage(message)
						.setNegativeButton(R.string.ok,
								new DialogInterface.OnClickListener() {
									public void onClick(DialogInterface dialog,
											int which) {
									}
								}).show();
			}
		});
	}
	public static void Set_Action_Bar_Visible() {
		if (enable_ab) {
			MainActivity.instance.getSupportActionBar().show();
		} else {
			MainActivity.instance.getSupportActionBar().hide();
		}
	}
	
	public void Set_Location_Active(boolean active) {
		if (active) {
			// Start NMEA listener
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "starting NMEA listener."); }
			LM.requestLocationUpdates(LocationManager.GPS_PROVIDER, 200l, 0, this);
			LM.addNmeaListener(this);
		} else {
			// Stop NMEA listener
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "stopping NMEA listener."); }
			LM.removeUpdates(this);
			LM.removeNmeaListener(this);
		}
		enable_location = active;
	}
	
	public static boolean Get_Location_Active() {
		return enable_location;
	}

	public static boolean IsGPSEnabled() {
		return LM != null && LM.isProviderEnabled (LocationManager.GPS_PROVIDER);
	}

	@Override
	public void onNmeaReceived(long timestamp, String nmea) {
		if (MainActivity.enable_logging)  {
			Log.d(LOG_TAG, "Recieved NMEA update: " + nmea);
		}

		// A lot of the strings we get from the NMEA listener have nothing
		// to do with GPS, and the user has the option to filter them so
		// the poor emulated Palm doesn't have to process that stuff.
		if (filter_gps && !nmea.startsWith("$GP")) {
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Ignoring non-GPS NMEA update."); }
			return;
		}
		// Call PHEMNativeIF.Pass_NMEA() to send the data on to the
		// emulator.
		PHEMNativeIF.Pass_NMEA(nmea);
	}

	@Override
	public void onLocationChanged(Location location) {
		// We don't care about this.
		
	}

	@Override
	public void onStatusChanged(String provider, int status, Bundle extras) {
		// We don't care about this.
		
	}

	@Override
	public void onProviderEnabled(String provider) {
		// We don't care about this.
		
	}

	@Override
	public void onProviderDisabled(String provider) {
		// We don't care about this.
		
	}
}
