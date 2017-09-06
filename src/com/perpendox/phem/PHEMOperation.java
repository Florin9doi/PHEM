package com.perpendox.phem;

import android.os.AsyncTask;

/* A potentially longer-running operation that shouldn't be allowed to block
 * the UI thread. E.g. InstallFileOperation, LoadSessionOperation,
 * NewSessionOperation. */
public abstract class PHEMOperation extends AsyncTask<Void, Void, Boolean> {
    abstract void Set_Fragment(OperationFragment frag);
    abstract String Get_Title();
    abstract String Get_Message();
}
