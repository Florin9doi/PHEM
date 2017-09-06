package com.perpendox.phem;

import java.util.List;

import android.content.Context;
import android.graphics.drawable.Drawable;
//import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.TextView;

public class FileArrayAdapter extends ArrayAdapter<FileChooserItem> {

	private Context c;
	private int id;
	private List<FileChooserItem> items;

	public FileArrayAdapter(Context context, int textViewResourceId,
			List<FileChooserItem> objects) {
		super(context, textViewResourceId, objects);
		// Log.d("FArrAdapt", "Starting.");
		c = context;
		id = textViewResourceId;
		items = objects;
	}

	public FileChooserItem getItem(int i) {
		// Log.d("FArrAdapt", "getItem.");
		return items.get(i);
	}

	@Override
	public View getView(int position, View convertView, ViewGroup parent) {
		// Log.d("FArrAdapt", "getView().");
		View v = convertView;
		if (v == null) {
			LayoutInflater vi = (LayoutInflater) c
					.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
			v = vi.inflate(id, null);
		}

		/* create a new view of my layout and inflate it in the row */
		// convertView = ( RelativeLayout ) inflater.inflate( resource, null );

		final FileChooserItem o = items.get(position);
		if (o != null) {
			// Log.d("FArrAdapt", "Getting TextViews.");
			TextView t1 = (TextView) v.findViewById(R.id.TextView01);
			TextView t2 = (TextView) v.findViewById(R.id.TextView02);
			TextView t3 = (TextView) v.findViewById(R.id.TextViewDate);
			// Log.d("FArrAdapt", "Getting ImageView...");
			/* Take the ImageView from layout and set the city's image */
			ImageView imageCity = (ImageView) v.findViewById(R.id.folder_icon);
			String uri = "drawable/" + o.getImage();
			// Log.d("FArrAdapt", "uri is: " + uri);
			int imageResource = c.getResources().getIdentifier(uri, null,
					c.getPackageName());
			Drawable image = c.getResources().getDrawable(imageResource);
			imageCity.setImageDrawable(image);
			switch (o.getType()) {
			case FileChooserItem.TYPE_PARENT:
				imageCity.setBackgroundColor(c.getResources().getColor(R.color.android_green));
				break;
			case FileChooserItem.TYPE_CURRENT:
				imageCity.setBackgroundColor(c.getResources().getColor(R.color.palm_highlight));
				break;
			case FileChooserItem.TYPE_DIR:
				imageCity.setBackgroundColor(c.getResources().getColor(R.color.palm_green));
				break;
			case FileChooserItem.TYPE_FILE:
				imageCity.setBackgroundColor(c.getResources().getColor(R.color.white));
				break;
			}
			// Log.d("FArrAdapt", "city image set.");
			if (t1 != null) {
				t1.setText(o.getName());
			}
			if (t2 != null) {
				t2.setText(o.getData());
			}
			if (t3 != null) {
				t3.setText(o.getDate());
			}
		}
		return v;
	}
}