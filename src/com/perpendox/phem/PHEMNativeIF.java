package com.perpendox.phem;

import java.nio.ByteBuffer;

import com.perpendox.phem.MainActivity;

import android.graphics.Point;
import android.os.ConditionVariable;
import android.os.SystemClock;
import android.util.Log;
import android.widget.Toast;

//This is a singleton that persists with the application.
//Activities come and go, but applications persist. This
//way the native thread(s) can have a fixed reference to a
//persistent object.

public class PHEMNativeIF {
	public static final int INITIAL_BITMAP_WIDTH = 332;
	public static final int INITIAL_BITMAP_HEIGHT = 452;
	public static final int POWER_BUTTON_ID = 6;
	public static Boolean session_active = false;
	private static int has_vfs = -1;
	private static PHEMNativeIF instance;
	private static ByteBuffer palm_screen_buffer;
	private static String LOG_TAG = PHEMNativeIF.class.getSimpleName();
	public static String phem_base_dir;
	public static String session_file_name;
	private static int win_w = INITIAL_BITMAP_WIDTH,
			win_h = INITIAL_BITMAP_HEIGHT;
	public static int button_pushed;

	public final static int SOFT_RESET = 0;
	public final static int NOEXT_RESET = 1;
	public final static int HARD_RESET = 2;

	public static void initInstance() {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "initInstance().");
		}
		if (instance == null) {
			// Create the instance
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Creating native instance.");
			}
			instance = new PHEMNativeIF();
		}
	}

	// Might need to synchronize this, eventually.
	public static PHEMNativeIF getInstance() {
		// Return the instance
		if (instance == null) {
			initInstance();
		}
		return instance;
	}

	private PHEMNativeIF() {
		// Constructor hidden because this is a singleton
		// Log.d(LOG_TAG, "Loading libs; in constructor.");
		// System.loadLibrary("gnustl_shared");
		System.loadLibrary("pose");
		session_file_name = "autosave.psf";
		Init_Screen_Buffer();
	}

	// **************************************************
	// *** Methods called by Activities and Fragments ***
	// **************************************************

	private static void Init_Screen_Buffer() {
		win_w = INITIAL_BITMAP_WIDTH;
		win_h = INITIAL_BITMAP_HEIGHT;
		palm_screen_buffer = ByteBuffer.allocateDirect(INITIAL_BITMAP_WIDTH
				* INITIAL_BITMAP_HEIGHT * 2);
		MainActivity.set_Bitmap_Size(win_w, win_h);
		//MainActivity.update_Screen_Bitmap();
	}

	public static Boolean Has_VFS() {
		if (-1 == has_vfs && session_active) {
			// Haven't been able to figure out if there's a VFS or not yet.
			has_vfs = IsVFSManagerPresent();
		}
		switch (has_vfs) {
		case -1:
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Couldn't check VFSManager yet.");}
			return false;
		case 0:
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "No VFSManager found.");}					
			return false;
		default:
			if (MainActivity.enable_logging) { Log.d(LOG_TAG, "VFSManager found.");}					
			return true;
		}
	}

	public static String Get_Session_File_With_Path() {
		// Returns the current session file name w/path.
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Returning " + phem_base_dir + "/"
					+ session_file_name);
		}
		return phem_base_dir + "/" + session_file_name;
	}

	public static void Save_Session(String file_name) {
		if (session_active) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Saving session " + file_name);}
			session_file_name = file_name;
			SaveSession(Get_Session_File_With_Path());
		} else {
			Log.e(LOG_TAG, "Saving but no active session!");
		}
	}

	public static Boolean Is_Session_Active() {
		return session_active;
	}

	public static void Handle_Idle() {
		// long init = System.currentTimeMillis();
		// Log.d(LOG_TAG, "...calling HandleIdle, " + init);
		if (HandleIdle(palm_screen_buffer) != 0) {
			// Log.d(LOG_TAG, "idle call took " + (System.currentTimeMillis() -
			// init));
			//Log.d(LOG_TAG, "screen update");
			MainActivity.update_Screen_Bitmap();
		} else {
			// Log.d(LOG_TAG, "no update took " + (System.currentTimeMillis() -
			// init));
		}
	}

	public static ByteBuffer Get_PHEM_Screen_Buffer() {
		// Log.d(LOG_TAG, "Giving buffer.");
		return palm_screen_buffer;
	}

	public static void Restart_Session(String session_name) {
		// Record the session
		session_file_name = session_name;
		// Call the native code
		String the_file_name = phem_base_dir + "/" + session_name;
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Restarting with: " + the_file_name);}
		RestartSession(the_file_name);
		has_vfs = -1;
	}

	public static void New_Session(String rom_file, String ram_size,
			String dev_name, String skin_name) {
		// Call the native code
		NewSession(rom_file, ram_size, dev_name, skin_name);
		has_vfs = -1;
	}

	public static void Get_Window_Size(Point window_specs) {
		window_specs.x = win_w;
		window_specs.y = win_h;
	}

	public void Set_PHEM_Dir(String phem_dir) {
		// Log.d(LOG_TAG, "Setting PHEM dir.");
		// Create message, send to UI thread.
		phem_base_dir = phem_dir;
		SetPHEMDir(phem_dir);
	}

	public static String Get_PHEM_Dir() {
		return phem_base_dir;
	}

	private static void Warn_User_Reset() {
		Toast toaster = Toast.makeText(MainActivity.instance,
				"Events blocked during Palm OS boot-up.", Toast.LENGTH_SHORT);
		toaster.show();
	}

	public static void Pen_Down(int x, int y) {
		if (session_active) {
			if (PenDown(x, y)) {
				Warn_User_Reset();
			}
		}
	}

	public static void Pen_Move(int x, int y) {
		if (session_active) {
			if (PenMove(x, y)) {
				Warn_User_Reset();			
			}
		}
	}

	public static void Pen_Up(int x, int y) {
		if (session_active) {
			if (PenUp(x, y)) {
				Warn_User_Reset();
			}
		}
	}

	public static void Key_Event(int key) {
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "Key_Event."); }
		if (session_active && key != 0) {
			if (KeyEvent(key)) {
				Warn_User_Reset();
			}
		}
	}

	public static void Button_Event(int button, boolean down) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Button_Event.");}
		if (session_active) {
			if (ButtonEvent(button, down)) {
				Warn_User_Reset();
			}
		}
	}

	public static void Pause_Emulation() {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Pausing emulation.");}
		if (session_active) {
			PauseEmulation(); // tell the native side to pause
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No active session.");}
		}
	}

	public static void Resume_Emulation() {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Resume_Emulation.");}
		if (session_active) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Resuming.");}
			ResumeEmulation(); // tell the native side to pause
		}
	}

	public static synchronized void Shutdown_Emulator(String save_file_name) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Shutting down emulation.");}
		if (session_active) {
			ShutdownEmulator(save_file_name); // tell the native side to pause
			session_active = false;
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No active session!");}
		}
	}

	public static int Install_Palm_File(String palm_file) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Install_Palm_File.");}
		return InstallPalmFile(palm_file);
	}

	public static String[] Get_Session_Info() {
		return GetSessionInfo();
	}

	public static void Set_Serial_Device(String serial_dev) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Setting serial dev: " + serial_dev);}
		SetSerialDevice(serial_dev);
	}

	public static void Pass_NMEA(String nmea_string) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Passing NMEA: " + nmea_string);}
		PassNMEA(nmea_string);
	}

	// ****************************************
	// *** Native methods called from Java. ***
	// ****************************************

	// Tell the native code where to look for skins, ROMs, etc.
	private static native void SetPHEMDir(String phem_dir);

	// Get info about the current session.
	private static native String[] GetSessionInfo();

	// Get supported devices for a given ROM
	public static native String[] GetROMDevices(String rom_file);

	// Get supported memory sizes for a given device
	public static native String[] GetDeviceMemories(String device);

	// Get supported skins for a given device
	public static native String[] GetDeviceSkins(String device);

	// Start a new emulation session with the indicated settings
	private static native void NewSession(String rom_file, String ram_size,
			String dev_name, String skin_name);

	// Use the indicated PSF file to resume an emulation session
	private static native void RestartSession(String psf_file);

	private static native void SaveSession(String psf_file);

	// Get device
	public static native String GetCurrentDevice();

	// Install a file into the emulated Palm.
	private static native int InstallPalmFile(String file_name);

	// Update the bitmap with any new screen changes
	private static native int HandleIdle(ByteBuffer buffer);

	// Touch events
	private static native boolean PenDown(int x, int y);

	private static native boolean PenMove(int x, int y);

	private static native boolean PenUp(int x, int y);

	// Keyboard
	private static native boolean KeyEvent(int key);

	// Button
	private static native boolean ButtonEvent(int button, boolean down);

	// Tell the emulator to execute a reset.
	public static native void ResetEmulator(int reset_type);

	// Used when the activity/fragment is paused
	public static native void PauseEmulation();

	// Used when the activity/fragment is resumed
	public static native void ResumeEmulation();

	// Used when the activity/fragment is closed/destroyed
	private static native void ShutdownEmulator(String save_file_name);

	// *** Card management ***

	// See if current ROM supports VFS Manager
	public static native int IsVFSManagerPresent();

	// See if current session has HostFS installed.
	public static native boolean IsHostFSPresent();

	// See what encoding the current Palm session is using
	public static native int GetPalmEncoding();

	// Manage what dir we use as 'card'
	public static native String GetCurrentCardDir();

	public static native void SetCurrentCardDir(String the_dir);

	// Mount/unmount card
	public static native boolean GetCardMountStatus();

	public static native void SetCardMountStatus(boolean mounted);

	// *** Network control ***
	// Redirect/block net access
	public static native boolean GetNetlibRedirect();

	public static native void SetNetlibRedirect(boolean mounted);

	// Skin resolution
	public static native boolean GetScale();

	public static native void SetScale(boolean mounted);

	// HotSync name
	public static native byte[] GetHotSyncName();
	public static native void SetHotSyncName(ByteBuffer sync_name);

	// Serial port management
	// Also used for fake GPS
	public static native String GetSerialDevice();
	private static native void SetSerialDevice(String serial_dev);

	// Needed for passing NMEA data to Palm via fake serial port.
	private static native void PassNMEA(String nmea_string);
	
	// *********************************************
	// *** Java methods called from native code. ***
	// *********************************************

	public static void Native_Crash_cb() {
		Log.d(LOG_TAG, "Native crashed! Sad face.");
		MainActivity.Pause_Idle();
		MainActivity.app_crashed = true;
		MainActivity.instance.Save_Prefs();
		try {
			System.err.println("Session was:\n" + session_file_name);
		} catch (Exception e) {
			// Can't really do much here.
		}
		Log.d(LOG_TAG, "Runtime exception:");
		new RuntimeException(
				"PHEM crashed here (native trace should follow after the Java trace)")
				.printStackTrace();
		ConditionVariable the_cond = new ConditionVariable(false);
		Log.d(LOG_TAG, "Calling showCrashDialog.");
		MainActivity.instance.showCrashDialog(the_cond);
		the_cond.block(); // TODO: add a timeout?
	}

	public static void Post_Error(String native_err) {
		Log.d(LOG_TAG, "Posting error from native code.");
		// Create message, send to UI thread.
		// MainActivity.setFromNative(native_err);
	}

	// These clipboard functions are called by native code.

	public static void Set_Clip_cb(byte[] clip_arr) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Setting clipboard from native.");}
		MainActivity.set_Clip(MainActivity.Convert_Palm_Chars_To_Native(ByteBuffer.wrap(clip_arr)));
	}

	public static ByteBuffer Get_Clip_cb() {
		return MainActivity.Make_Palm_Chars_From_Native(MainActivity.get_Clip());
	}
	
//	public static String oldGet_Clip_cb() {
//		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Getting clipboard for native.");}
//		
//		final Charset windowsCharset = Charset.forName("windows-1252");
//		final ByteBuffer windowsEncoded = windowsCharset.encode(CharBuffer.wrap(MainActivity.get_Clip()));
//		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "string being sent: "+ windowsEncoded.toString());}
//		return windowsEncoded.toString();
////		String the_clip;
////		try {
////			// Palm OS natively uses the CP1252 ("Windows-1252") encoding (or CP932 for
////			// Japanese). For ASCII it doesn't matter, but many European languages have
////			// chars which don't match. So we try to encode it here.
////			// Note we use a resource to specify the Palm codepage. This is overridden
////			// in the "values-ja" strings file.
////			the_clip = URLEncoder.encode(MainActivity.get_Clip(),
////					MainActivity.Get_Palm_Codepage());
////		} catch (UnsupportedEncodingException e) {
////			// All we can do is pass the original string through.
////			the_clip = MainActivity.get_Clip();
////		}
////		return the_clip;
//	}

	public static void Resize_Window_cb(final int w, final int h) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Resize_Window called.");
		Log.d(LOG_TAG, "w=" + w + " h=" + h);
		}
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Copying size.");}
		win_w = w;
		win_h = h;
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Allocating buffer, size: " + (win_w*win_h*2));}
		palm_screen_buffer = ByteBuffer.allocateDirect(win_w * win_h * 2);
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Buffer allocated. ");}
		session_active = true; // Know it's running now, by gum!
		// Create message, send to UI thread.
		MainActivity.set_Bitmap_Size(win_w, win_h);
	}

	public static void Queue_Sound_cb(final int freq, final int dur, final int amp) {
		// Log.d(LOG_TAG, "Queue_Sound called.");
		MainActivity.Play_Sound(freq, dur, amp);
	}

	// Android just *loathes* modal dialogs. But the emulator expects them. So,
	// we have to
	// block on a condition variable until the user picks something from an
	// uncancelable dialog.
	public static int Common_Dialog_cb(Integer item_ids[], String labels[],
			Boolean properties[]) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Common_Dialog called.");}
		// Note that it's possible that we try to fire off a dialog *just* when
		// the MainActivity is being torn down/rebuilt for a configuration
		// change.
		// In that case, we'll get -1 back from the Activity, and so we try
		// again
		// until we get a valid item_id.
		button_pushed = -1;
		while (-1 == button_pushed) {
			ConditionVariable the_cond = new ConditionVariable(false);
			MainActivity.Do_Alert_Dialog(R.string.emulator_alert_title,
					labels[3], item_ids, labels, properties, the_cond);
			// MainActivity.Do_Common_Dialog(item_ids, labels, properties,
			// the_cond);
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Waiting on common dialog condition.");}
			the_cond.block(); // TODO: add a timeout?
			if (-1 == button_pushed) {
				SystemClock.sleep(100); // sleep a tenth of a second, give the
										// activity a chance to start up.
			}
		}
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Common dialog condition opened, returning "
				+ button_pushed);}
		return button_pushed;
	}

	public static int Reset_Dialog_cb(Integer item_ids[], String labels[],
			Boolean properties[]) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Reset_Dialog called.");}
		// Note that it's possible that we try to fire off a dialog *just* when
		// the MainActivity is being torn down/rebuilt for a configuration
		// change. In that case, we'll get -1 back from the Activity, and so we
		// try again until we get a valid item_id.
		button_pushed = -1;
		while (-1 == button_pushed) {
			ConditionVariable the_cond = new ConditionVariable(false);
			String message = "";
			if (null != MainActivity.instance) {
				message = MainActivity.instance.getResources().getString(
						R.string.choose_reset_type);
			}
			MainActivity.Do_Alert_Dialog(R.string.emulator_reset_title,
					message, item_ids, labels, properties, the_cond);
			// MainActivity.Do_Reset_Dialog(item_ids, labels, properties,
			// the_cond);
			the_cond.block();
			if (-1 == button_pushed) {
				SystemClock.sleep(100); // sleep a tenth of a second, give the
										// activity a chance to start up.
			}
		}
		return button_pushed;
	}

	public static void Start_Vibration_cb() {
		MainActivity.Do_Vibration();
	}

	public static void Stop_Vibration_cb() {
		MainActivity.End_Vibration();
	}

	public static void Set_LED_cb(int red, int green, int blue) {
		int color = 0xff << 24 + red << 16 + green << 8 + blue;
		MainActivity.Do_LED(color);
	}

	public static void Clear_LED_cb() {
		MainActivity.Clear_LED();
	}
}
