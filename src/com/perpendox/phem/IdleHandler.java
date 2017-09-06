package com.perpendox.phem;

import android.os.Handler;
import android.os.Looper;
import android.os.Message;

public class IdleHandler extends Handler {

	public Boolean do_power_button = false;

	public IdleHandler(Looper looper) {
		super(looper);
	}

	@Override
	public void handleMessage(Message msg) {
		synchronized (PHEMApp.idle_sync) {
			// Guarantee that emulated device sees the power button pressed,
			// then released.
			if (do_power_button) {
				PHEMNativeIF.Button_Event(PHEMNativeIF.POWER_BUTTON_ID, true);
			}

			PHEMNativeIF.Handle_Idle();

			if (do_power_button) {
				PHEMNativeIF.Button_Event(PHEMNativeIF.POWER_BUTTON_ID, false);
				do_power_button = false;
			}
		}
	}
}