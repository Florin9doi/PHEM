package com.perpendox.phem;

import java.nio.ByteBuffer;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.BitmapFactory.Options;
import android.graphics.Canvas;
import android.graphics.Point;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;

//The view that shows the emulator screen
public class PHEMView extends View {
	private static final String LOG_TAG = PHEMView.class.getSimpleName();
	private Bitmap mBitmap;
	int bitmap_width, bitmap_height, display_bm_w, display_bm_h;
	int view_width, view_height;
	Rect destiny = new Rect();
	long last_evt_time = 0;
	int last_pix_x = 0, last_pix_y = 0;
	private boolean active_touch = false, out_of_rect = false;
	private boolean session_bitmap = false;

	// Only needed to make Eclipse be quiet... so far as I can see, anyway.
	public PHEMView(Context context, AttributeSet attrs) {
		super(context, attrs);
	}

	public PHEMView(Context context) {
		super(context);
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Creating new PHEMView.");
		}
		Point window_specs = new Point(PHEMNativeIF.INITIAL_BITMAP_WIDTH,
				PHEMNativeIF.INITIAL_BITMAP_HEIGHT);
		PHEMNativeIF.Get_Window_Size(window_specs);
		bitmap_width = display_bm_w = window_specs.x;
		bitmap_height = display_bm_h = window_specs.y;

		if (PHEMNativeIF.Is_Session_Active()) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Active session, getting bitmap.");
			}
			ByteBuffer buffy = PHEMNativeIF.Get_PHEM_Screen_Buffer();
			// Update our bitmap with the data in the buffer.
			buffy.rewind(); // Make sure we start from beginning every time.
			mBitmap = Bitmap.createBitmap(bitmap_width, bitmap_height,
					Bitmap.Config.RGB_565);
			mBitmap.copyPixelsFromBuffer(buffy);
		} else {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "No active session, setting bitmap.");
			}
			Load_Bitmap(context, R.drawable.phem_no_session);
		}
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "creating bitmap, w: " + bitmap_width + " h: "
					+ bitmap_height);
		}
//		mBitmap = Bitmap.createBitmap(bitmap_width, bitmap_height,
//				Bitmap.Config.RGB_565);

//		ByteBuffer temp_buf = PHEMNativeIF.Get_PHEM_Screen_Buffer();
//		temp_buf.rewind(); // Make sure we start from beginning every time.
//		mBitmap.copyPixelsFromBuffer(temp_buf);
		//invalidate();
	}

	public void Load_Bitmap(Context context, int the_drawable) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "Load_Bitmap.");
		}
		/* Set the options */
		Options opts = new Options();
		opts.inDither = true;
		opts.inPreferredConfig = Bitmap.Config.RGB_565;
		opts.inScaled = false; /* Flag for no scaling */

		/* Load the bitmap with the options */
		mBitmap = BitmapFactory.decodeResource(context.getResources(),
		                                           the_drawable, opts);
		bitmap_height = mBitmap.getHeight();
		bitmap_width = mBitmap.getWidth();
		session_bitmap = false;
		invalidate();
	}

	@Override
	protected void onSizeChanged(int xNew, int yNew, int xOld, int yOld) {
		super.onSizeChanged(xNew, yNew, xOld, yOld);
		view_width = xNew;
		view_height = yNew;
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "onSizeChanged, w:" + view_width + " h:"
					+ view_height);
		}
	}

	@Override
	protected void onDraw(Canvas canvas) {
		if (MainActivity.enable_logging) { Log.d(LOG_TAG, "onDraw."); }
		// Draw bitmap, maximizing size while maintaining aspect ratio.
		destiny.left = destiny.top = 0;

		if (view_width * bitmap_height > view_height * bitmap_width) {
//			if (MainActivity.enable_logging) {
//				Log.d(LOG_TAG, "Window is wider than bitmap");
//			}
			// Need to center horizontally
			destiny.top = 0;
			display_bm_h = destiny.bottom = view_height;
			display_bm_w = (bitmap_width * view_height) / bitmap_height;
			int x_off = (view_width - display_bm_w) / 2;
			destiny.left = x_off;
			destiny.right = x_off + display_bm_w;
		} else {
//			if (MainActivity.enable_logging) {
//				Log.d(LOG_TAG, "Bitmap is wider than window");
//			}
			// Need to center vertically
			destiny.left = 0;
			display_bm_w = destiny.right = view_width;
			display_bm_h = (view_width * bitmap_height) / bitmap_width;
			int y_off = (view_height - display_bm_h) / 2;
			destiny.top = y_off;
			destiny.bottom = display_bm_h + y_off;
		}

//		if (MainActivity.enable_logging) {
//			Log.d(LOG_TAG, "w:" + display_bm_w + " h:" + display_bm_h);
//		}

		canvas.drawBitmap(mBitmap, null, destiny, null);
		// if (MainActivity.enable_logging) {Log.d(LOG_TAG, "redrawn at " +
		// System.currentTimeMillis());}
	}

	void set_Bitmap_Size(int W, int H) {
		if (MainActivity.enable_logging) {
			Log.d(LOG_TAG, "set_Bitmap_Size");
			Log.d(LOG_TAG, "w=" + W + " h=" + H);
		}
		session_bitmap = true;
		bitmap_width = W;
		bitmap_height = H;
		mBitmap = Bitmap.createBitmap(bitmap_width, bitmap_height,
				Bitmap.Config.RGB_565);
	}

	void set_Bitmap() {
//		if (MainActivity.enable_logging) {
//			Log.d(LOG_TAG, "set_Bitmap.");
//		}
		if (!session_bitmap) {
			Log.e(LOG_TAG, "set_Bitmap but not session bitmap!");
		}
		ByteBuffer buffy = PHEMNativeIF.Get_PHEM_Screen_Buffer();
		// Update our bitmap with the new data in the buffer.
		buffy.rewind(); // Make sure we start from beginning every time.
		mBitmap.copyPixelsFromBuffer(buffy);
		
		this.postInvalidate();
		/*this.post(new Runnable() {
			@Override
			public void run() {

				// force a redraw
				invalidate();

			}
		});*/
		
		
		/*
		ByteBuffer buffy = PHEMNativeIF.Get_PHEM_Screen_Buffer();
		// Update our bitmap with the new data in the buffer.
		buffy.rewind(); // Make sure we start from beginning every time.
		mBitmap.copyPixelsFromBuffer(buffy);
		// force a redraw
		invalidate();
		*/
	}

	// Utility function, figure out how far the new event is from the
	// previous one.
	int evt_distance(int old_x, int old_y, int new_x, int new_y) {
		int xdist = new_x - old_x;
		int ydist = new_y - old_y;
		return (int) (Math.sqrt((double) (xdist * xdist + ydist * ydist)) + 0.5);
	}

	@Override
	public boolean onTouchEvent(MotionEvent evt) {
		// Need to smooth and filter the touch event stream, so need
		// to measure the time between events.
		long cur_evt_time = System.currentTimeMillis();

		// Make sure that the event's actually on the emulator skin. Wouldn't
		// want to send a negative position on to the emulator...
		if (destiny.contains((int) evt.getX(), (int) evt.getY())) {
			float eventX = evt.getX() - destiny.left;
			float eventY = evt.getY() - destiny.top;
			// Map event coords to emulator coords.
			int pix_x = (int) (((eventX * bitmap_width) / display_bm_w) + 0.5);
			int pix_y = (int) (((eventY * bitmap_height) / display_bm_h) + 0.5);
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "onTouchEvent, x:" + pix_x + " y:" + pix_y);
			}
			// Figure out what type it is, pass it on to the native code.
			switch (evt.getAction()) {
			case MotionEvent.ACTION_DOWN:
				if (MainActivity.enable_logging) {
					Log.d(LOG_TAG, "onTouchEvent, down x:" + pix_x + " y:"
							+ pix_y);
				}
				//MainActivity.Pause_The_Idle_Thread();
				//MainActivity.Send_Pen(IdleHandler.PEN_DOWN, pix_x, pix_y);
				synchronized (PHEMApp.idle_sync) {
					PHEMNativeIF.Pen_Down(pix_x, pix_y);
				}
				//MainActivity.Resume_The_Idle_Thread();
				last_evt_time = cur_evt_time;
				last_pix_x = pix_x;
				last_pix_y = pix_y;
				active_touch = true;
				out_of_rect = false;
				return true;
			case MotionEvent.ACTION_MOVE:
				// In practice, there's a fair amount of 'jitter' in touch
				// events. The emulator was written with mice in mind, not
				// touch screens. So we don't pass on events that come in
				// 'too fast' or are 'too small'.
				int diff_pos = evt_distance(last_pix_x, last_pix_y, pix_x,
						pix_y);
				long diff_time = cur_evt_time - last_evt_time;
				if (diff_time > MainActivity.smooth_period
						|| diff_pos > MainActivity.smooth_pixels) {
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "onTouchEvent, passing move x:" + pix_x
								+ " y:" + pix_y);
					}
					last_evt_time = cur_evt_time;
					last_pix_x = pix_x;
					last_pix_y = pix_y;
					//MainActivity.Pause_The_Idle_Thread();
					if (out_of_rect) {
						// We must have moved into the emulator skin from outside.
						// Pretend this was a pen_down.
						synchronized (PHEMApp.idle_sync) {
						PHEMNativeIF.Pen_Down(pix_x, pix_y);
						}
						//MainActivity.Send_Pen(IdleHandler.PEN_DOWN, pix_x, pix_y);
						out_of_rect = false;
						active_touch = true; // Start a new touch.
					} else {
						// Just an ordinary move.
						synchronized (PHEMApp.idle_sync) {
						PHEMNativeIF.Pen_Move(pix_x, pix_y);
						}
						//MainActivity.Send_Pen(IdleHandler.PEN_MOVE, pix_x, pix_y);
					}
					//MainActivity.Resume_The_Idle_Thread();
				} else {
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "onTouchEvent, suppressing move x:"
								+ pix_x + " y:" + pix_y);
					}
				}
				return true;
			case MotionEvent.ACTION_UP:
				if (MainActivity.enable_logging) {
					Log.d(LOG_TAG, "onTouchEvent, up x:" + pix_x + " y:"
							+ pix_y);
				}
				//MainActivity.Pause_The_Idle_Thread();
				synchronized (PHEMApp.idle_sync) {
				PHEMNativeIF.Pen_Up(pix_x, pix_y);
				}
				//MainActivity.Send_Pen(IdleHandler.PEN_UP, pix_x, pix_y);
				//MainActivity.Resume_The_Idle_Thread();
				last_evt_time = cur_evt_time;
				last_pix_x = pix_x;
				last_pix_y = pix_y;
				active_touch = false;
				out_of_rect = false;
				return true;
			default:
				break;
			}
		} else {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Event not in emulator rect.");
			}
			out_of_rect = true;
			switch (evt.getAction()) {
			case MotionEvent.ACTION_DOWN:
				// This doesn't start a touch.
				break;
			case MotionEvent.ACTION_UP:
				active_touch = false;
				break;
			case MotionEvent.ACTION_MOVE:
				if (active_touch) {
					// We left the emulator skin. Synthesize an ACTION_UP.
					//MainActivity.Pause_The_Idle_Thread();
					synchronized (PHEMApp.idle_sync) {
						PHEMNativeIF.Pen_Up(last_pix_x, last_pix_y);
					}
					//MainActivity.Send_Pen(IdleHandler.PEN_UP, last_pix_x, last_pix_y);
					//MainActivity.Resume_The_Idle_Thread();
					active_touch = false;
				}
			}
		}
		return false;
	}
}
