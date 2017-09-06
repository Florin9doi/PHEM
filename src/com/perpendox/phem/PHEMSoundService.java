package com.perpendox.phem;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import android.app.Service;
import android.content.Intent;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.IBinder;
import android.util.Log;

// Used to play sounds sent by the emulator. Has the nice side effect that it
// lets us detect when the app is being destroyed, so we can shut down the
// native emulator.
public class PHEMSoundService extends Service {
	// Use a single-threaded Executor; sounds will be played in order and not
	// overlap.
	private static final String LOG_TAG = PHEMSoundService.class.getSimpleName();
	private static ExecutorService pool;
	private static PHEMSoundService instance;

	@Override
	public void onCreate() {
		super.onCreate();
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onCreate.");
		}
		pool = Executors.newSingleThreadExecutor();
		instance = this;
	}

	@Override
	public int onStartCommand(Intent intent, int flags, int startId) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onStartCommand.");
		}
		if (null == pool) {
			pool = Executors.newSingleThreadExecutor();
		}
		return Service.START_STICKY;
	}

	@Override
	public IBinder onBind(Intent intent) {
		// We don't do that 'binding' stuff.
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onBind.");
		}
		return null;
	}

	private void Clear_Pool() {
		if (null == pool) {
			return;
		}
		pool.shutdown();
		try {
			// Wait a while for existing tasks to terminate
			if (!pool.awaitTermination(100, TimeUnit.MILLISECONDS)) {
				pool.shutdownNow(); // Cancel currently executing tasks
				// Wait a while for tasks to respond to being cancelled
				if (!pool.awaitTermination(100, TimeUnit.MILLISECONDS)) {
					Log.e(LOG_TAG, "Pool did not terminate!");
					System.err.println("Pool did not terminate");
				}
			}
		} catch (InterruptedException ie) {
			// (Re-)Cancel if current thread also interrupted
			pool.shutdownNow();
			// Preserve interrupt status
			Thread.currentThread().interrupt();
		}

	}

	public void Go_Away() {
		if (PHEMNativeIF.Is_Session_Active()) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Go_Away(); shutting down emulator.");
			}
			PHEMNativeIF.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
		}
		Clear_Pool();
		pool = null;
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Go_Away(); wrapping up.");
		}
		this.stopSelf();
		
	}

	@Override
	public void onDestroy() {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onDestroy.");
		}
		Clear_Pool();
		pool = null;
		// If this is being killed, the whole app is going away.
		PHEMNativeIF.Shutdown_Emulator(PHEMNativeIF.Get_Session_File_With_Path());
		super.onDestroy();
	}

	public static void DoSndCmd(int freq, int dur, int amp) {
		try {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Executing command: f: " + freq + " d: " + dur
						+ " a: " + amp);
			}
			pool.execute(instance.new PlaySndFreqDurAmp(freq, dur, amp));
		} catch (Exception ex) {
			Log.e(LOG_TAG, "Exception executing command! " + ex);
			pool.shutdown();
		}
	}

	public static void Actually_Play_Sound(int freq, int dur, int amp) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Actually_Play_Sound.");
		}

		double duration = dur / 1000.0; // seconds
		double freqOfTone = freq; // hz
		int sampleRate = 8000; // a number

		double dnumSamples = duration * sampleRate;
		dnumSamples = Math.ceil(dnumSamples);
		int numSamples = (int) dnumSamples;
		double sample[] = new double[numSamples];
		byte generatedSnd[] = new byte[2 * numSamples];
		AudioTrack audioTrack = null; // Get audio track

		// First, get the audiotrack.
		try {
			//int track_state;
			audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
					sampleRate, AudioFormat.CHANNEL_OUT_MONO,
					AudioFormat.ENCODING_PCM_16BIT, (int) numSamples * 2,
					AudioTrack.MODE_STATIC);

//			track_state = audioTrack.getState();
//			switch (track_state) {
//			case AudioTrack.STATE_INITIALIZED:
//				Log.d(LOG_TAG, "Track initialized.");
//				break;
//			case AudioTrack.STATE_NO_STATIC_DATA:
//				Log.d(LOG_TAG, "No Static Data.");
//				break;
//			case AudioTrack.STATE_UNINITIALIZED:
//				Log.d(LOG_TAG, "Track uninitialized!");
//				break;
//			}
		} catch (Exception e) {
			Log.d(LOG_TAG, "Whoops, exception! " + e);
			return;
		}

		int i;
		for (i = 0; i < numSamples; ++i) { // Fill the sample array
			sample[i] = Math.sin(freqOfTone * 2 * Math.PI * i / (sampleRate))
					* (amp / 64.0); // Palm amplitude ranges from 0-64
		}

		// convert to 16 bit pcm sound array
		// assumes the sample buffer is normalized.
		int idx = 0;
		//int ramp = numSamples / 20; // Amplitude ramp as a percent of sample
									// count
		
		for (i = idx; i < numSamples; ++i) { // Full amplitude
			double dVal = sample[i];
			// scale to maximum amplitude
			final short val = (short) ((dVal * 32767));
			// in 16 bit wav PCM, first byte is the low order byte
			generatedSnd[idx++] = (byte) (val & 0x00ff);
			generatedSnd[idx++] = (byte) ((val & 0xff00) >>> 8);
		}

//		for (i = 0; i < ramp; ++i) { // Ramp amplitude up (to avoid clicks)
//			double dVal = sample[i];
//			// Ramp up to maximum
//			final short val = (short) ((dVal * 32767 * i / ramp));
//			// in 16 bit wav PCM, first byte is the low order byte
//			generatedSnd[idx++] = (byte) (val & 0x00ff);
//			generatedSnd[idx++] = (byte) ((val & 0xff00) >>> 8);
//		}
//
//		for (i = idx; i < numSamples - ramp; ++i) { // Full amplitude for most
//													// of the samples
//			double dVal = sample[i];
//			// scale to maximum amplitude
//			final short val = (short) ((dVal * 32767));
//			// in 16 bit wav PCM, first byte is the low order byte
//			generatedSnd[idx++] = (byte) (val & 0x00ff);
//			generatedSnd[idx++] = (byte) ((val & 0xff00) >>> 8);
//		}
//
//		for (i = idx; i < numSamples; ++i) { // Ramp amplitude down
//			double dVal = sample[i];
//			// Ramp down to zero
//			final short val = (short) ((dVal * 32767 * (numSamples - i) / ramp));
//			// in 16 bit wav PCM, first byte is the low order byte
//			generatedSnd[idx++] = (byte) (val & 0x00ff);
//			generatedSnd[idx++] = (byte) ((val & 0xff00) >>> 8);
//		}


		try {
			//Log.d(LOG_TAG, "Sending audiotrack.");
			audioTrack.write(generatedSnd, 0, generatedSnd.length); // Load the
																	// track
//			int track_state = audioTrack.getState();
//			switch (track_state) {
//			case AudioTrack.STATE_INITIALIZED:
//				Log.d(LOG_TAG, "Data load, track initialized.");
//				break;
//			case AudioTrack.STATE_NO_STATIC_DATA:
//				Log.d(LOG_TAG, "Data load, No Static Data.");
//				track_state = audioTrack.getState();
//				break;
//			case AudioTrack.STATE_UNINITIALIZED:
//				Log.d(LOG_TAG, "Data load, track uninitialized.");
//				break;
//			}

			audioTrack.play(); // Play the track
//			track_state = audioTrack.getState();
//			switch (track_state) {
//			case AudioTrack.STATE_INITIALIZED:
//				Log.d(LOG_TAG, "Play, track initialized.");
//				break;
//			case AudioTrack.STATE_NO_STATIC_DATA:
//				Log.d(LOG_TAG, "Play, No Static Data.");
//				break;
//			case AudioTrack.STATE_UNINITIALIZED:
//				Log.d(LOG_TAG, "Play, track uninitialized.");
//				break;
//			}
		} catch (Exception e) {
			// RunTimeError("Error: " + e);
			Log.d(LOG_TAG, "Logging exception.");
			return;
		}

		int x = 0;
		do { // Monitor playback to find when done
			if (audioTrack != null) {
				x = audioTrack.getPlaybackHeadPosition();
			} else {
				x = numSamples;
			}
		} while (x < numSamples);

		if (audioTrack != null)
			audioTrack.release(); // Track play done.
	}

	// Runnable to pass to thread pool.
	class PlaySndFreqDurAmp implements Runnable {
		int freq;
		int dur;
		int amp;

		PlaySndFreqDurAmp(int frequency, int duration, int amplitude) {
			freq = frequency;
			dur = duration;
			amp = amplitude;
		}

		public void run() {
			Actually_Play_Sound(freq, dur, amp);
		}
	}
}
