package com.perpendox.phem;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager.NameNotFoundException;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

/**
 * A simple {@link android.support.v4.app.Fragment} subclass. Activities that
 * contain this fragment must implement the
 * {@link AboutFragment.OnFragmentInteractionListener} interface to handle
 * interaction events. Use the {@link AboutFragment#newInstance} factory method
 * to create an instance of this fragment.
 * 
 */
public class AboutFragment extends Fragment {
	private static final String LOG_TAG = AboutFragment.class.getSimpleName();
	/**
	 * @return A new instance of fragment AboutFragment.
	 */
	public static AboutFragment newInstance() {
		AboutFragment fragment = new AboutFragment();
		return fragment;
	}

	public AboutFragment() {
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
		View the_view = inflater.inflate(R.layout.fragment_about, container,
				false);

		// A cute color effect...
		TextView acronym_label = (TextView) the_view
				.findViewById(R.id.acronym_label);
		String text = "<font color=\"blue\">P</font>alm <font color=\"blue\">H</font>ardware "
				+ "<font color=\"blue\">E</font>mulator <font color=\"blue\">M</font>68k";
		acronym_label.setText(Html.fromHtml(text));

		// Make this a link.
		TextView perp_link = (TextView) the_view.findViewById(R.id.perp_link);
		perp_link.setText(Html.fromHtml(getResources().getString(
				R.string.perpendox_link)));
		perp_link.setMovementMethod(LinkMovementMethod.getInstance());
		
		TextView source_link = (TextView) the_view.findViewById(R.id.source_blurb);
		source_link.setText(Html.fromHtml(getResources().getString(
			R.string.source_blurb)));
		source_link.setMovementMethod(LinkMovementMethod.getInstance());

		PackageInfo pInfo;
		TextView about_title = (TextView) the_view
				.findViewById(R.id.about_title);
		try {
			pInfo = getActivity().getPackageManager().getPackageInfo(
					getActivity().getPackageName(), 0);
			String version = pInfo.versionName;

			about_title.setText("PHEM v" + version);
		} catch (NameNotFoundException e) {
			e.printStackTrace();
			about_title.setText("PHEM (version error!)");
		}

		// Only show the 'Rate' button if (a) the app hasn't already been rated,
		// and (b) the app hasn't crashed for the user.
		// I suppose it's slightly shady, but we don't *prevent* the user from
		// giving us a negative rating. We just don't smooth the way for them.
		if (!MainActivity.app_crashed && !MainActivity.app_rated) {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Showing rate button.");
			}
			View strut = the_view.findViewById(R.id.rate_strut);
			strut.setVisibility(View.VISIBLE);
			Button rate_button = (Button) the_view
					.findViewById(R.id.rate_button);
			rate_button.setVisibility(View.VISIBLE);
			rate_button.setOnClickListener(new OnClickListener() {
				@Override
				public void onClick(View v) {
					if (MainActivity.enable_logging) {
						Log.d(LOG_TAG, "Firing off Google Play.");
					}
					Uri uri = Uri.parse("market://details?id="
							+ getActivity().getPackageName());
					Intent goToMarket = new Intent(Intent.ACTION_VIEW, uri);
					try {
						getActivity().startActivity(goToMarket);
						// Presumably they did rate it...
						MainActivity.app_rated = true;
					} catch (ActivityNotFoundException e) {
						if (MainActivity.enable_logging) {
							Log.d(LOG_TAG, "Couldn't launch Google Play for "
									+ getActivity().getPackageName());
						}
						// TODO: alert on error.
						// UtilityClass.showAlerDialog(context, ERROR,
						// "Couldn't launch the market", null, 0);
					}
				}
			});
		} else {
			if (MainActivity.enable_logging) {
				Log.d(LOG_TAG, "Not showing rate button; crashed: "
						+ MainActivity.app_crashed + " rated: "
						+ MainActivity.app_rated);
			}
		}

		return the_view;
	}

	@Override
	public void onAttach(Activity activity) {
		super.onAttach(activity);
	}

	@Override
	public void onDetach() {
		super.onDetach();
	}
	
	@Override
	public void onDestroy() {
		MainActivity.sub_fragment_active = false;
		super.onDestroy();
	}
}
