package com.perpendox.phem;

// When a UI fragment has done its job - the type of reset has been selected,
// the file to install has been chosen, etc. - it uses this interface to notify
// the Activity that it's served its purpose in life and is ready to meet the
// Grim Reaper.
public interface SuicidalFragmentListener {
    void onFragmentSuicide(String tag);
}
