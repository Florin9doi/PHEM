package com.perpendox.phem;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.os.Environment;

import java.io.File;
import java.sql.Date;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.text.DateFormat;

import android.support.v4.app.ListFragment;
import android.support.v4.content.LocalBroadcastManager;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

public class File_Chooser_Fragment extends ListFragment {
	private final static String LOG_TAG = File_Chooser_Fragment.class.getSimpleName();
	private File currentDir;
	private int load_type;
	private FileArrayAdapter adapter;
	private String[] file_types;
	private static final String LOAD_TYPE = "load_type";
	private static final String START_PATH = "start_path";
	private static final String FILE_TYPES = "file_types";

	private SuicidalFragmentListener mCallback;

	public static File_Chooser_Fragment newInstance(int load_type,
			String start_path, String[] file_types) {
		File_Chooser_Fragment fragment = new File_Chooser_Fragment();
		Bundle args = new Bundle();
		args.putInt(LOAD_TYPE, load_type);
		args.putString(START_PATH, start_path);
		args.putStringArray(FILE_TYPES, file_types);
		fragment.setArguments(args);
		return fragment;
	}

	@Override
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Launching.");}
		// Start from specified directory, remember dir when activity
		// reconstituted
		String cur_dir;

		if (null == savedInstanceState) {
			load_type = getArguments().getInt(LOAD_TYPE);
			cur_dir = getArguments().getString(START_PATH);
			file_types = getArguments().getStringArray(FILE_TYPES);
		} else {
			load_type = savedInstanceState.getInt(LOAD_TYPE);
			cur_dir = savedInstanceState.getString(START_PATH);
			file_types = savedInstanceState.getStringArray(FILE_TYPES);
		}
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "cur_dir: " + cur_dir);}
		// There sure as heck ought to be a path here, but just in case...
		if (null == cur_dir) {
			currentDir = Environment.getExternalStorageDirectory();
		} else {
			currentDir = new File(cur_dir);
		}
		if (null == file_types) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "No filtering.");}
		}
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Okay, moving on...
		fill(currentDir, file_types);

		/*
		 * // This way we can pick directories...
		 * this.getListView().setOnLongClickListener(new OnLongClickListener() {
		 * 
		 * @Override public boolean onLongClick(View v) { Item o =
		 * adapter.getItem(getListView() .getSelectedItemPosition()); if
		 * (o.getImage().equalsIgnoreCase("dir_icon")) { Log.d(LOG_TAG,
		 * "Long click on dir:" + o.getPath().toString()); currentDir = new
		 * File(o.getPath()); onFileClick(o); // This *might* work... return
		 * true; } else { return false; } } });
		 */

		return super.onCreateView(inflater, container, savedInstanceState);
	}

	@Override
	public void onSaveInstanceState(Bundle savedInstanceState) {
		super.onSaveInstanceState(savedInstanceState);
		// Record the directory we're looking at
		savedInstanceState.putInt(LOAD_TYPE, load_type);
		savedInstanceState.putString(START_PATH, currentDir.getAbsolutePath());
		savedInstanceState.putStringArray(FILE_TYPES, file_types);
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
		// This makes sure that the container activity has implemented
		// the callback interface. If not, it throws an exception
		try {
			mCallback = (SuicidalFragmentListener) activity;
		} catch (ClassCastException e) {
			throw new ClassCastException(activity.toString()
					+ " must implement SuicidalFragmentListener");
		}
	}

	@SuppressLint("DefaultLocale")
	// Can't handle locale yet...
	private void fill(File f, String[] file_types) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "running fill()");}
		File[] dirs = f.listFiles();
		// this.setTitle("Current Dir: "+f.getName());
		List<FileChooserItem> dir = new ArrayList<FileChooserItem>();
		List<FileChooserItem> fls = new ArrayList<FileChooserItem>();
		try {
			for (File ff : dirs) {
				Date lastModDate = new Date(ff.lastModified());
				DateFormat formatter = DateFormat.getDateTimeInstance();
				String date_modify = formatter.format(lastModDate);
				if (ff.isDirectory()) {

					File[] fbuf = ff.listFiles();
					int buf = 0;
					if (fbuf != null) {
						buf = fbuf.length;
					} else
						buf = 0;
					String num_item = String.valueOf(buf);
					if (buf == 0)
						num_item = num_item + " item";
					else
						num_item = num_item + " items";

					// String formatted = lastModDate.toString();
					dir.add(new FileChooserItem(ff.getName(), num_item,
							date_modify, ff.getAbsolutePath(), "dir_icon",
							FileChooserItem.TYPE_DIR));
				} else {
					// Filter by requested file type(s). No point in showing
					// .prc files if we're loading a ROM, and vice versa.
					boolean matched_filter = false;
					if (null == file_types) {
						matched_filter = true;
					} else {
						int i;
						for (i = 0; i < file_types.length; i++) {
							// Note: case-insensitive.
							if (ff.getName().toLowerCase()
									.endsWith(file_types[i])) {
								matched_filter = true;
								break;
							}
						}
					}
					if (matched_filter) {
						fls.add(new FileChooserItem(ff.getName(), ff.length()
								+ " Byte", date_modify, ff.getAbsolutePath(),
								"file_icon", FileChooserItem.TYPE_FILE));
					}
				}
			}
		} catch (Exception e) {
			// Not sure what to do here...
			Log.e(LOG_TAG, "Got exception:" + e.toString());
		}
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "fill sorting");}
		Collections.sort(dir);
		Collections.sort(fls);
		dir.addAll(fls);
		dir.add(0,
				new FileChooserItem(getString(R.string.current_dir_label), f.getPath(), "", f
						.getParent(), "dir_icon", FileChooserItem.TYPE_CURRENT));
		if (f.getParent() != null) {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "Adding parent: "
							+ f.getParent());}
			dir.add(0,
					new FileChooserItem(getString(R.string.parent_dir_label), "..", "", f
							.getParent(), "dir_up", FileChooserItem.TYPE_PARENT));
		} else {
			if (MainActivity.enable_logging) {Log.d(LOG_TAG, "At top of filesystem?");}
		}
		adapter = new FileArrayAdapter(this.getActivity(), R.layout.file_view,
				dir);
		this.setListAdapter(adapter);
	}

	@Override
	public void onListItemClick(ListView l, View v, int position, long id) {
		super.onListItemClick(l, v, position, id);
		FileChooserItem o = adapter.getItem(position);
		if (o.getImage().equalsIgnoreCase("dir_icon")
				|| o.getImage().equalsIgnoreCase("dir_up")) {
			if (null != o.getPath()) {
				currentDir = new File(o.getPath());
				fill(currentDir, file_types);
			}
		} else {
			onFileClick(o);
		}
	}

	/*
	 * public boolean onItemLongClick(AdapterView<?> l, View v, int pos, long
	 * id) { Item o = adapter.getItem(getListView().getSelectedItemPosition());
	 * if (o.getImage().equalsIgnoreCase("dir_icon")) { Log.d(LOG_TAG,
	 * "Long click on dir:" + o.getPath().toString()); currentDir = new
	 * File(o.getPath()); onFileClick(o); // This *might* work... return true; }
	 * else { return false; } }
	 */

	// Okay, the user picked a file.
	private void onFileClick(FileChooserItem o) {
		if (MainActivity.enable_logging) {Log.d(LOG_TAG, "File clicked in: " + currentDir);}
		// Toast.makeText(this, "Folder Clicked: "+ currentDir,
		// Toast.LENGTH_SHORT).show();
		// mCallback.onFileChosenListener(currentDir.toString(), o.getName());
		Intent intent = new Intent(MainActivity.LOAD_FILE_INTENT);
		intent.putExtra(MainActivity.LOAD_FILE_TYPE, load_type);
		intent.putExtra(MainActivity.LOAD_FILE_PATH, currentDir.toString());
		intent.putExtra(MainActivity.LOAD_FILE_NAME, o.getName());
		LocalBroadcastManager.getInstance(getActivity()).sendBroadcast(intent);
		if (mCallback != null) {
			mCallback.onFragmentSuicide(getTag());
		}
	}

	@Override
	public void onDestroy() {
		MainActivity.sub_fragment_active = false;
		super.onDestroy();
	}

	@Override
	public void onDetach() {
		super.onDetach();
		mCallback = null;
	}
}