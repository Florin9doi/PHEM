#include <jni.h>
#include <time.h>
#include <android/log.h>

#include <signal.h>
#include <string>
#include "com_perpendox_phem_PHEMNativeIF.h"
#include "EmCommon.h"
#include "EmCPU.h" // gCPU
#include "EmApplicationAndroid.h"
#include "EmSession.h"
#include "EmStreamFile.h"
#include "EmROMReader.h"
#include "EmDocument.h" // gDocument
#include "SystemResources.h" // constants for PalmOS calls
#include "ROMStubs.h" // FtrGet
#include "omnithread.h"
#include "EmTransportSerialAndroid.h"
#include "PHEMNativeIF.h"

#include "algorithm" // remove_if() and such

#define  LOG_TAG    "libpose"
#ifdef PHEM_LOGGING
#define  LOGI(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#else
#define  LOGI(...)
#endif
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

// These variables are needed to call Java functions from native code
static JavaVM *g_vm = NULL;
JNIEnv *g_thread_jnienv = NULL;
JNIEnv *g_cpu_thread_jnienv = NULL;
jclass  g_class; // the PHEMNativeIF class

jclass  g_Integer_class;
jclass  g_Boolean_class;

// Method of last resort
jmethodID g_native_crash;

jmethodID g_Integer_init;
jmethodID g_Boolean_init;

jmethodID g_common_dialog;
jmethodID g_reset_dialog;

jmethodID g_reset_window;
jmethodID g_queue_sound;
jmethodID g_set_clip;
jmethodID g_get_clip;

jmethodID g_start_vibration;
jmethodID g_end_vibration;

jmethodID g_set_led;
jmethodID g_clear_led;

// Thread mgmt vars.
omni_thread* g_app_thread;
omni_mutex  g_loop_mutex;
omni_mutex  g_stopper_mutex;
omni_condition g_loop_cond(&g_loop_mutex);
//omni_condition g_stopper_cond(&g_stopper_mutex);
int g_done=0;
int g_allow_idle=0;
int g_allow_led;

int g_screen_updated=0;
int g_win_w, g_win_h;
int g_first_line, g_last_line;

// Reset handling
int g_in_reset=0;
int g_logo_drawn=0;
time_t g_reset_timestamp;
double g_logo_timestamp;
// "Eight seconds oughta be enough for anybody."
#define RESET_TIMEOUT (8.0)
// "Three hundred milliseconds oughta be enough for anybody."
#define LOGO_TIMEOUT (300)

#if 0
// Extra sound debugging.
FILE *sound_dbg_file;
time_t stampie;
#endif

#if 0 // don't need these; tick measurement folded into
      // EmApplication
unsigned long last_ticks = 0;
unsigned long ticks_per_second = 1000; // reasonable default
struct timespec last_time;
unsigned long max_tps = 0;
unsigned long min_tps = 1000000;
unsigned long tps_count = 0;
unsigned long tps_total = 0;
#endif

#if 0
// Command queue stuff
omni_mutex g_cmd_mutex;
PHEM_Cmd *g_cmd_list;
#endif

// Some info we need to record.
std::string PHEM_base_dir = "";
unsigned char *PHEM_buffer = NULL;
size_t PHEM_buffer_size=0;
// The current 'mouse' (touch) position for the emulator
int PHEM_mouse_x=0;
int PHEM_mouse_y=0;

// The emulator application object.
EmApplicationAndroid the_app;
EmSessionStopper *g_stopper = NULL;
unsigned int g_stop_count=0;
int argc;
char **argv;

// For saving state at close
std::string psf_file_name = "";

static char *app_name = "PHEM";
static char *minus_psf = "-psf";
static char *minus_rom = "-rom";
static char *minus_ram = "-ram_size";
static char *minus_dev = "-device";

void PHEM_Play_Sound(int freq, int dur, int amp);

static struct sigaction old_sa[NSIG]; // Place to record default signal handlers

// Attempt to detect crashes in native code and notify the user.
void android_sigaction(int signal, siginfo_t *info, void *reserved) {
  LOGI("android_sigaction: %d", signal);
  g_thread_jnienv->CallStaticVoidMethod(g_class, g_native_crash);
  old_sa[signal].sa_handler(signal);
}

// Need this so we can find the class we need.
jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  JNIEnv *env;
  g_vm = vm;
  if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    return -1;
  }

  env->GetJavaVM(&g_vm);

  // Try to catch crashes...
  struct sigaction handler;
  memset(&handler, 0, sizeof(struct sigaction));
  handler.sa_sigaction = android_sigaction;
  handler.sa_flags = SA_RESETHAND;
#define CATCHSIG(X) sigaction(X, &handler, &old_sa[X])
  CATCHSIG(SIGILL);
  CATCHSIG(SIGABRT);
  CATCHSIG(SIGBUS);
  CATCHSIG(SIGFPE);
  CATCHSIG(SIGSEGV);
  CATCHSIG(SIGSTKFLT);
  CATCHSIG(SIGPIPE);

  LOGI("JNIOnload completing.");
#if 0
  g_class = reinterpret_cast<jclass>(env->NewGlobalRef(temp_class));
  jclass temp_class = env->FindClass("com/perpendox/phem/PHEMNativeIF");
  if (NULL == temp_class) {
    LOGE("Failed to find native interface class!");
    return -1;
  }

  g_class = reinterpret_cast<jclass>(env->NewGlobalRef(temp_class));
  if (NULL == g_class) {
    LOGE("Failed to get global ref to native interface class!");
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return -1;
  }
#endif

  return JNI_VERSION_1_6;
}

// ************************************************
// *** Utility functions called by others below ***
// ************************************************

// ************************************************
// *** The App thread (creates a CPU subthread) ***
// ************************************************
void *App_Run_Static(void *arg)
{

  // Paranoid, set some vars.

  // First, bind our thread and grab the references we'll need to call
  // methods on the Java side
  int attached = g_vm->AttachCurrentThread(&g_thread_jnienv, NULL);
  if (attached>0) {
    LOGE("Failed to attach thread to JavaVM!");
    return (void *)-1;
  }

  // Get the method ref for Native_Crash
  g_native_crash = g_thread_jnienv->GetStaticMethodID(g_class, "Native_Crash_cb", "()V");
  if (NULL == g_native_crash) {
    LOGE("Failed to get Native_Crash method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Resize_Window
  g_reset_window = g_thread_jnienv->GetStaticMethodID(g_class, "Resize_Window_cb", "(II)V");
  if (NULL == g_reset_window) {
    LOGE("Failed to get Resize_Window method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Common_Dialog 
  g_common_dialog = g_thread_jnienv->GetStaticMethodID(g_class, "Common_Dialog_cb",
                        "([Ljava/lang/Integer;[Ljava/lang/String;[Ljava/lang/Boolean;)I");
  if (NULL == g_common_dialog) {
    LOGE("Failed to get Common_Dialog method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Reset_Dialog 
  g_reset_dialog = g_thread_jnienv->GetStaticMethodID(g_class, "Reset_Dialog_cb",
                        "([Ljava/lang/Integer;[Ljava/lang/String;[Ljava/lang/Boolean;)I");
  if (NULL == g_reset_dialog) {
    LOGE("Failed to get Reset_Dialog method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the classes and init methods for Integer and Boolean.
  g_Integer_class = g_thread_jnienv->FindClass("java/lang/Integer");
  g_Boolean_class = g_thread_jnienv->FindClass("java/lang/Boolean");

  // Get the method ref for Integer_init
  g_Integer_init = g_thread_jnienv->GetMethodID(g_Integer_class, "<init>", "(I)V");
  if (NULL == g_Integer_init) {
    LOGE("Failed to get Init_integer method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Boolean_init
  g_Boolean_init = g_thread_jnienv->GetMethodID(g_Boolean_class, "<init>", "(Z)V");
  if (NULL == g_Boolean_init) {
    LOGE("Failed to get Init_boolean method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Start_Vibration 
  g_start_vibration = g_thread_jnienv->GetStaticMethodID(g_class, "Start_Vibration_cb", "()V");
  if (NULL == g_start_vibration) {
    LOGE("Failed to get Start_Vibration method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Stop_Vibration 
  g_end_vibration = g_thread_jnienv->GetStaticMethodID(g_class, "Stop_Vibration_cb", "()V");
  if (NULL == g_end_vibration) {
    LOGE("Failed to get Stop_Vibration method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Set_LED
  g_set_led = g_thread_jnienv->GetStaticMethodID(g_class, "Set_LED_cb", "(III)V");
  if (NULL == g_set_led) {
    LOGE("Failed to get Set_LED method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  // Get the method ref for Clear_LED
  g_clear_led = g_thread_jnienv->GetStaticMethodID(g_class, "Clear_LED_cb", "()V");
  if (NULL == g_clear_led) {
    LOGE("Failed to get Clear_LED method ref!");
    // Clean up global ref
    g_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return (void *)-1;
  }

  LOGI("App_Run_Static: Starting emulator app.");
  // Actually fire off the emulator process
  try {
    LOGI("App_Run_Static: Startup.");
    the_app.Startup(argc, argv);
    LOGI("App_Run_Static: HandleStartupActions.");
    the_app.HandleStartupActions();
    LOGI("App_Run_Static: Init complete.");
  } catch (...) {
    // !!! TBD
    LOGE("Yikes! Got exception starting up app!");
    return (void *)-1;
  }

  LOGI("App_Run_Static: Allowing idle.");
  g_allow_idle = 1;

  // Wait until we're told to shut down.
  //unsigned long   sec;
  //unsigned long   nanosec;
  while (!g_done) {
#if 0
    {
      //LOGI("A_R_S Checking cmd list");
      // This lock exists for the life of the block.
      omni_mutex_lock lock(g_cmd_mutex);
      while (g_cmd_list) {
         PHEM_Cmd *temp = g_cmd_list;
         PHEM_Play_Sound(g_cmd_list->freq, g_cmd_list->dur, g_cmd_list->amp);
         g_cmd_list = g_cmd_list->next;
         free(temp);
      }
    }
#endif
    //LOGI("App_Run_Static: Sleeping.");
    // calc absolute time to wait until:
    //omni_thread::get_time(&sec, &nanosec, 12*3600, 0); // 3600 secs, one hour
    //g_loop_cond.timedwait(sec, nanosec);

    g_loop_cond.wait();
    LOGI("App_Run_Static: Woke up.");
  }

  LOGI("App_Run_Static: blocking idle.");
  g_allow_idle = 0; 
  g_done = 0;

  LOGI("App_Run_Static: Closing session.");
  //{ 
    //EmSessionStopper stopper (gSession, kStopNow);
    EmFileRef the_psf_file(psf_file_name);
    the_app.HandleSessionClose(the_psf_file);
    LOGI("App_Run_Static: Back from HandleSessionClose.");
  //}

  LOGI("App_Run_Static: Shutting down.");
  the_app.Shutdown ();

#if 0
  // Clean up anything in the command list, don't leak memory.
  {
    omni_mutex_lock lock(g_cmd_mutex);
    while (g_cmd_list) {
      PHEM_Cmd *temp = g_cmd_list;
      g_cmd_list = g_cmd_list->next;
      free(temp);
    }
  }
#endif
  // Don't leak buffer memory.
  if (PHEM_buffer) {
    free(PHEM_buffer);
    PHEM_buffer = NULL;
  }

  // Clean up global ref
  g_thread_jnienv->DeleteGlobalRef(g_class);
  g_class = NULL;
  // Make sure to detach from the JVM.
  g_vm->DetachCurrentThread();
  LOGI("App_Run_Static: All done.");
  return (void *)-1;
}

void Start_App(int argc, char *argv[])
{
  // Clear this from the last time.
  g_screen_updated = 0;

  // We have to create a thread to bind to the JVM, so we can get class
  // and method references to call.
  LOGI("Start_App.");
  g_app_thread = new omni_thread(&App_Run_Static, &the_app);
  g_app_thread->start();
  LOGI("Start_App done.");
}

// *******************************************************
// *** Functions called from elsewhere in the emulator ***
// *******************************************************

// Stupid simple debugging
void PHEM_Logger_Hex(unsigned int code)
{
  LOGI("PHEM code: %x", code);
}

void PHEM_Logger_Place(int code)
{
  LOGI("PHEM code: %d", code);
}

void PHEM_Logger_Msg(const char *msg)
{
  LOGI("PHEM msg: %s", msg);
}

// Return current time in milliseconds 
static double now_ms(void) {
  struct timespec res;
  clock_gettime(CLOCK_REALTIME, &res);
  return 1000.0 * res.tv_sec + (double) res.tv_nsec / 1e6;
}

// Flag so we know when to post changes to host
void PHEM_Mark_Screen_Updated(int first, int last)
{
  g_screen_updated = 1;
  if (first < g_first_line) {
    g_first_line = first;
  }
  if (last > g_last_line) {
    g_last_line = last;
  }

  // Try to detect when Palm OS has finished booting.
  if (g_in_reset) {
    LOGI("Screen update, in reset.");
    // Some kind of logo gets drawn first, then there's a time gap.
    if (g_logo_drawn) {
      LOGI("Screen update, logo drawn.");
      // Assume there's at least a third of a second between the logo and
      // completion of startup.
      if (now_ms() - g_logo_timestamp > LOGO_TIMEOUT) {
        LOGI("Screen update, assuming done.");
        g_in_reset=0;
        g_logo_drawn=0;
      }
    } else {
      LOGI("Screen update, logo drawing.");
      // Must've started drawing the logo.
      g_logo_drawn = 1;
      g_logo_timestamp = now_ms();
    }
  }

#if 0
  LOGI("Update lines: %d, %d", g_first_line, g_last_line);
#endif
}

// Where in the filesystem our roms and save files and stuff are
const char *PHEM_Get_Base_Dir()
{
   return PHEM_base_dir.c_str();
}

// The screen buffer we use for updates
unsigned char *PHEM_Get_Buffer()
{
   return PHEM_buffer;
}

// The CPU thread can't make JNI calls unless it's bound the the JVM.
// And it needs to, to play sounds and exchange clipboard data and so
// forth.
void PHEM_Bind_CPU_Thread()
{
#if 0
  LOGE("Trying to open dbg file in '%s'", PHEM_base_dir.c_str());
  string snd_dbg_file_name = PHEM_base_dir + "/snd_dbg_native.txt";
  sound_dbg_file = fopen(snd_dbg_file_name.c_str(), "a");
  if (NULL == sound_dbg_file) {
    LOGE("Open failed!");
  } else {
    LOGE("Open done.");
  }
#endif

  // First, bind our thread and grab the references we'll need to call
  // methods on the Java side
  int attached = g_vm->AttachCurrentThread(&g_cpu_thread_jnienv, NULL);
  if (attached>0) {
    LOGE("Failed to attach cpu thread to JavaVM!");
    return;
  }

  // Get the method ref for Queue_Sound
  g_queue_sound = g_cpu_thread_jnienv->GetStaticMethodID(g_class, "Queue_Sound_cb", "(III)V");
  if (NULL == g_queue_sound) {
    LOGE("Failed to get Queue_Sound method ref!");
    // Clean up global ref
    g_cpu_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return;
  }

  // Get the method ref for Set_Clip
  g_set_clip = g_cpu_thread_jnienv->GetStaticMethodID(g_class, "Set_Clip_cb", "([B)V");
  if (NULL == g_set_clip) {
    LOGE("Failed to get Set_Clip method ref!");
    // Clean up global ref
    g_cpu_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return;
  }

  // Get the method ref for Get_Clip
  g_get_clip = g_cpu_thread_jnienv->GetStaticMethodID(g_class, "Get_Clip_cb", "()Ljava/nio/ByteBuffer;");
  if (NULL == g_get_clip) {
    LOGE("Failed to get Get_Clip method ref!");
    // Clean up global ref
    g_cpu_thread_jnienv->DeleteGlobalRef(g_class);
    // Make sure to detach.
    g_vm->DetachCurrentThread();
    return;
  }
}

void PHEM_Unbind_CPU_Thread()
{
  // Make sure to detach from the JVM.
  g_vm->DetachCurrentThread();
#if 0
  LOGE("Closing snddbg file.");
  fclose(sound_dbg_file);
#endif
}

// Call into Java to actually play the sound.
void PHEM_Queue_Sound(int freq, int dur, int amp)
{ 
  jthrowable exc;

  LOGI("PHEM_Queue_Sound called: %d %d %d", freq, dur, amp);
#if 0
  time(&stampie);
  fprintf(sound_dbg_file, "%s: PHEM_Queue_Sound called: %d %d %d\n",
    ctime(&stampie), freq, dur, amp);
#endif

  // Call the Java method to tell the Android side to play a sound.
  if (NULL == g_cpu_thread_jnienv) {
    LOGE("env is null!");
    return;
  }
  if (NULL == g_queue_sound) {
    LOGE("method is null!");
    return;
  }
  if (NULL == g_class) {
    LOGE("class is null!");
    return;
  }
  g_cpu_thread_jnienv->CallStaticVoidMethod(g_class, g_queue_sound, (jint)freq, (jint)dur, (jint)amp);
  exc = g_cpu_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling queue_sound!");
#if 0
    time(&stampie);
    fprintf(sound_dbg_file,"%s: Exception calling queue_sound!\n", ctime(&stampie));
#endif
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_cpu_thread_jnienv->ExceptionDescribe();
    g_cpu_thread_jnienv->ExceptionClear();
  }
#if 0
  fprintf(sound_dbg_file,"%s: Sound queued.\n", ctime(&stampie));
#endif
}

// When the emulator loads a new skin, the 'window' size changes. This lets the Java side
// know how big the bitmap is.
void PHEM_Reset_Window(int w, int h)
{ 
  jthrowable exc;

  LOGI("PHEM_Reset_Window called, w: %d, h: %d.", w, h);
  g_win_w = w;
  g_win_h = h;

  // allocate our local buffer.
  if (PHEM_buffer) {
    free(PHEM_buffer);
  }
  PHEM_buffer_size = w*h*2;
  PHEM_buffer = (unsigned char *)calloc(1, PHEM_buffer_size);
  if (!PHEM_buffer) {
    LOGE("Unable to allocate screen buffer!");
    return;
  }
  LOGI("Buffer allocated: %u", PHEM_buffer_size);

  // Call the Java method to let the Android side know the dimensions of the skin we're using.
  // Note: we use the global object ref.
  if (NULL == g_thread_jnienv) {
    LOGE("env is null!");
    return;
  }
  if (NULL == g_reset_window) {
    LOGE("method is null!");
    return;
  }
  if (NULL == g_class) {
    LOGE("class is null!");
    return;
  }
  LOGI("Calling Java...");
  g_thread_jnienv->CallStaticVoidMethod(g_class, g_reset_window, (jint)w, (jint)h);
  exc = g_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling reset_window!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_thread_jnienv->ExceptionDescribe();
    g_thread_jnienv->ExceptionClear();
  }
  LOGI("PHEM_Reset_Window completed.");
}

// Get the contents of the Host (Android) clipboard, if any. Doesn't
// bother with binary data, only text. 
// Note: caller must free returned buffer!
const char *PHEM_Get_Host_Clip() {
  jthrowable exc;
  jlong buf_siz;
  char *clip_buf;

  jobject buf;

  LOGI("Getting clip buffer from host.");
  buf = g_cpu_thread_jnienv->CallStaticObjectMethod(g_class, g_get_clip);
  exc = g_cpu_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling get_clip!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_cpu_thread_jnienv->ExceptionDescribe();
    g_cpu_thread_jnienv->ExceptionClear();
    return NULL;
  }

  LOGI("Getting buffer addr from host.");
  // Gotta call a Java method, get a ByteBuffer address.
  clip_buf = (char *)g_cpu_thread_jnienv->GetDirectBufferAddress(buf);
  LOGI("Call returned.");
  exc = g_cpu_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling GetDirectBufferAddress!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_cpu_thread_jnienv->ExceptionDescribe();
    g_cpu_thread_jnienv->ExceptionClear();
    return NULL;
  }

  LOGI("Getting buffer size from host.");
  // Gotta call a Java method, get a ByteBuffer size.
  buf_siz = (jlong)g_cpu_thread_jnienv->GetDirectBufferCapacity(buf);
  LOGI("Call returned.");
  exc = g_cpu_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling GetDirectBufferCapacity!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_cpu_thread_jnienv->ExceptionDescribe();
    g_cpu_thread_jnienv->ExceptionClear();
    return NULL;
  }

#if 0
  LOGI("Buffer addr: %x size: %ld", clip_buf, buf_siz);

  for (int i=0; i< buf_siz; i++) {
    LOGI("%d %d", i, Java_buffer[i]); 
  }
#endif
  if (NULL != clip_buf) {
    // Now, get the chars, make a string.
    // Allocate buffer for clip...
    char *new_clip = (char *)calloc(sizeof(char), buf_siz+1);
    memcpy(new_clip, clip_buf, buf_siz);
    LOGI("Returning clip, '%s'", new_clip);
    return new_clip;
  } else {
    // Got bupkus from the host.
    LOGI("Got null clip string.");
    return NULL;
  }
}

// Pass text from the Palm to the Host (Android) clipboard.
void PHEM_Set_Host_Clip(const char *clip) {
  jthrowable exc;

  LOGI("Entering PHEM_Set_Host_Clip, %s", clip);

  // Turn these into a jbyte array, pass it on to Android, let it
  // handle any charset weirdness.
  int clip_len = strlen(clip) + 1; //include trailing null
  jbyteArray clippy = g_cpu_thread_jnienv->NewByteArray(clip_len);
  g_cpu_thread_jnienv->SetByteArrayRegion(clippy, 0, clip_len, (jbyte *)clip);
  
  LOGI("Sending string to host.");
  // Send it along, with love, to the host.
  g_cpu_thread_jnienv->CallStaticVoidMethod(g_class, g_set_clip, clippy);
  exc = g_cpu_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling set_clip!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_cpu_thread_jnienv->ExceptionDescribe();
    g_cpu_thread_jnienv->ExceptionClear();
  }
}

// Disabled for now. Can't quite get this working yet.
void PHEM_Enable_LED(Bool draw) {
#if 0
  g_allow_led = draw;
  if (!draw) {
    jthrowable exc;
    LOGI("Calling host Clear_LED...");
    g_thread_jnienv->CallStaticVoidMethod(g_class, g_clear_led);
    exc = g_thread_jnienv->ExceptionOccurred();
    if (exc) {
      LOGE("Exception calling Clear_LED!");
      /* We don't do much with the exception, except that
       we print a debug message for it, clear it, and 
       throw a new exception. */
      g_thread_jnienv->ExceptionDescribe();
      g_thread_jnienv->ExceptionClear();
    }
  }
#endif
}

void PHEM_Set_LED(RGBType color) {
#if 0
  if (g_allow_led) {
    jthrowable exc;
    LOGI("Calling host Set_LED...");
    g_thread_jnienv->CallStaticVoidMethod(g_class, g_set_led,
                  color.fRed, color.fGreen, color.fBlue);
    exc = g_thread_jnienv->ExceptionOccurred();
    if (exc) {
      LOGE("Exception calling Set_LED!");
      /* We don't do much with the exception, except that
       we print a debug message for it, clear it, and 
       throw a new exception. */
      g_thread_jnienv->ExceptionDescribe();
      g_thread_jnienv->ExceptionClear();
    }
  }
#endif
}

// ******
// Dialogs
// ******

#define NUM_WIDGET_PROPS 4

#define NUM_COMMON_WIDGETS 4
#define TOTAL_COMMON_WIDGET_PROPS (NUM_COMMON_WIDGETS*NUM_WIDGET_PROPS)

EmDlgItemID PHEM_Do_Common_Dialog(PHEM_Dialog *dlg) {
  // Suck the data we need out of the dlg object.
  jthrowable exc;
  jobjectArray widget_props;
  jobjectArray widget_labels;
  jobjectArray widget_ids;
  jstring str;
  jint result;

  int i, j;

  //LOGI("Allocating arrays for Do_Common_Dialog.");
  // Create the arrays we'll be passing along.
  widget_ids = (jobjectArray)g_thread_jnienv->NewObjectArray(NUM_COMMON_WIDGETS,
                                    g_thread_jnienv->FindClass("java/lang/Integer"),
                                    NULL);
  widget_labels = (jobjectArray)g_thread_jnienv->NewObjectArray(NUM_COMMON_WIDGETS,
                                    g_thread_jnienv->FindClass("java/lang/String"),
                                    g_thread_jnienv->NewStringUTF(""));
  widget_props = (jobjectArray)g_thread_jnienv->NewObjectArray(TOTAL_COMMON_WIDGET_PROPS,
                                    g_thread_jnienv->FindClass("java/lang/Boolean"),
                                    NULL);
  // Now we load up the arrays.
  for (i=0, j=0; i<TOTAL_COMMON_WIDGET_PROPS; i+=NUM_WIDGET_PROPS, j++) {
    // The item ID the emulator cares about.
    //LOGI("Setting item_id of '%d' to '%d'.", j, (int)dlg->widgets[j].item_id);
    jobject new_int = g_thread_jnienv->NewObject(g_Integer_class, g_Integer_init, (int)dlg->widgets[j].item_id);
    g_thread_jnienv->SetObjectArrayElement(widget_ids, j, new_int);

    // The text label
    //LOGI("Setting label '%d' to '%s'.", j, dlg->widgets[j].label);
    str = g_thread_jnienv->NewStringUTF(dlg->widgets[j].label);
    g_thread_jnienv->SetObjectArrayElement(widget_labels, j, str);

    // The widget properties.
    /*LOGI("Props for '%d' are: %d %d %d %d.", j, dlg->widgets[j].visible,
                         dlg->widgets[j].enabled, dlg->widgets[j].is_default,
                         dlg->widgets[j].is_cancel); */
    jobject new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].visible?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i, new_bool);
    new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].enabled?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i+1, new_bool);
    new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].is_default?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i+2, new_bool);
    new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].is_cancel?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i+3, new_bool);
  }

  //LOGI("Calling host Do_Common_Dialog.");
  // Call the Android side to display a dialog.
  result = g_thread_jnienv->CallStaticIntMethod(g_class, g_common_dialog,
                                      widget_ids, widget_labels, widget_props);
  exc = g_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling common dialog!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_thread_jnienv->ExceptionDescribe();
    g_thread_jnienv->ExceptionClear();
    result = kDlgItemNone;
  }

  //LOGI("Do_Common_Dialog returning %d", result);
  // Send the result along.
  return (EmDlgItemID)result;
}

#define NUM_RESET_WIDGETS 3
#define TOTAL_RESET_WIDGET_PROPS (NUM_RESET_WIDGETS*NUM_WIDGET_PROPS)

EmDlgItemID PHEM_Do_Reset_Dialog(PHEM_Dialog *dlg) {
  // Suck the data we need out of the dlg object.
  jthrowable exc;
  jobjectArray widget_props;
  jobjectArray widget_labels;
  jobjectArray widget_ids;
  jstring str;
  jint result;

  int i, j;

  //LOGI("Allocating arrays for Do_Reset_Dialog.");
  // Create the arrays we'll be passing along.
  widget_ids = (jobjectArray)g_thread_jnienv->NewObjectArray(NUM_RESET_WIDGETS,
                                    g_thread_jnienv->FindClass("java/lang/Integer"),
                                    NULL);
  widget_labels = (jobjectArray)g_thread_jnienv->NewObjectArray(NUM_RESET_WIDGETS,
                                    g_thread_jnienv->FindClass("java/lang/String"),
                                    g_thread_jnienv->NewStringUTF(""));
  widget_props = (jobjectArray)g_thread_jnienv->NewObjectArray(TOTAL_RESET_WIDGET_PROPS,
                                    g_thread_jnienv->FindClass("java/lang/Boolean"),
                                    NULL);
  // Now we load up the arrays.
  //LOGI("Starting to load arrays.");
  for (i=0, j=0; i<TOTAL_RESET_WIDGET_PROPS; i+=NUM_WIDGET_PROPS, j++) {
    // The item ID the emulator cares about.
    LOGI("Setting item_id '%d' to '%d'.", j, (int)dlg->widgets[j].item_id);
    jobject new_int = g_thread_jnienv->NewObject(g_Integer_class, g_Integer_init, (int)dlg->widgets[j].item_id);
    g_thread_jnienv->SetObjectArrayElement(widget_ids, j, new_int);

    // The text label
    LOGI("Setting label '%d' to '%s'.", j, dlg->widgets[j].label);
    str = g_thread_jnienv->NewStringUTF(dlg->widgets[j].label);
    g_thread_jnienv->SetObjectArrayElement(widget_labels, j, str);

    // The widget properties.
    LOGI("Props for '%d' are: %d %d %d %d.", j, dlg->widgets[j].visible,
                         dlg->widgets[j].enabled, dlg->widgets[j].is_default,
                         dlg->widgets[j].is_cancel);
    jobject new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].visible?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i, new_bool);
    new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].enabled?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i+1, new_bool);
    new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].is_default?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i+2, new_bool);
    new_bool = g_thread_jnienv->NewObject(g_Boolean_class, g_Boolean_init,
                                                  dlg->widgets[j].is_cancel?JNI_TRUE:JNI_FALSE);
    g_thread_jnienv->SetObjectArrayElement(widget_props, i+3, new_bool);
  }

  LOGI("Calling host do_reset_dialog...");
  // Call the Android side to display a dialog.
  result = g_thread_jnienv->CallStaticIntMethod(g_class, g_reset_dialog,
                                      widget_ids, widget_labels, widget_props);
  exc = g_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling common dialog!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_thread_jnienv->ExceptionDescribe();
    g_thread_jnienv->ExceptionClear();
    result = kDlgItemNone;
  } else {
    if (0 == result) {
      result = kDlgItemCancel;
    }
  }

  // Send the result along.
  return (EmDlgItemID)result;
}

void PHEM_Begin_Vibration() {
  jthrowable exc;
  LOGI("Calling host Start_Vibration...");
  g_thread_jnienv->CallStaticVoidMethod(g_class, g_start_vibration);
  exc = g_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling Start_Vibration!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_thread_jnienv->ExceptionDescribe();
    g_thread_jnienv->ExceptionClear();
  }
}

void PHEM_End_Vibration() {
  jthrowable exc;
  LOGI("Calling host End_Vibration...");
  g_thread_jnienv->CallStaticVoidMethod(g_class, g_end_vibration);
  exc = g_thread_jnienv->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling End_Vibration!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    g_thread_jnienv->ExceptionDescribe();
    g_thread_jnienv->ExceptionClear();
  }
}

#if 0
timespec spec_diff(timespec start, timespec end)
{
	timespec temp;
	if ((end.tv_nsec-start.tv_nsec)<0) {
		temp.tv_sec = end.tv_sec-start.tv_sec-1;
		temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec-start.tv_sec;
		temp.tv_nsec = end.tv_nsec-start.tv_nsec;
	}
	return temp;
}

void PHEM_Measure_Ticks(unsigned long cur_ticks) {
  struct timespec cur_time;
  clock_gettime(CLOCK_MONOTONIC, &cur_time);

  // Wait until we have two samples to start calculating.
  if (last_time.tv_sec) {
    timespec diffy = spec_diff(last_time, cur_time);

    // Do the math.
    if (diffy.tv_sec == 0) {
      unsigned long tick_diff = cur_ticks - last_ticks;

      double temp_tps = (tick_diff * 1000000000.0) / diffy.tv_nsec;
      ticks_per_second = (unsigned long) (temp_tps + 0.5); // round up

      if (ticks_per_second > max_tps) {
         max_tps = ticks_per_second;
      }
      if (ticks_per_second < min_tps) {
         min_tps = ticks_per_second;
      }
      tps_count++;
      tps_total += ticks_per_second; 
      LOGI("tps: %d min: %d max: %d avg: %d", ticks_per_second, min_tps, max_tps, tps_total/tps_count);
    } else {
      // This ain't right.
      LOGI("Time diff is more than a second! %d %d", diffy.tv_sec, diffy.tv_nsec);
    }
  }

  last_time = cur_time;
  last_ticks = cur_ticks;
}
#endif

void Pause_Emulation() {
  LOGI("Pause_Emulation called.");
  omni_mutex_lock lock (g_stopper_mutex);
  LOGI("Pause_Emulation Mutex locked.");
  if (g_stop_count) {
    // Keep track of how many times we've stopped things.
    g_stop_count++;
    LOGI("Alread stopped, %d.", g_stop_count);
  } else {
    //g_stopper = new EmSessionStopper(gSession, kStopNow);
    LOGI("Making new stopper.");
    g_stopper = new EmSessionStopper(gSession, kStopOnSysCall);
    if (NULL == g_stopper) {
      LOGE("No stopper created!");
    } else {
      if (!g_stopper->Stopped()) {
        LOGE("Got stopper but not stopped!");
      }
    }
    if (!g_stopper->CanCall()) {
        LOGE("Got stopper but can't call!");
    }
    g_stop_count++;
  }
  LOGI("Local Pause_Emulation, g_count=%d", g_stop_count);
}

void Resume_Emulation() {
  omni_mutex_lock lock (g_stopper_mutex);
  LOGI("Resume_Emulation Mutex locked.");
  if (g_stop_count) {
    g_stop_count--;
    if (0 == g_stop_count) {
      if (g_stopper != NULL) {
        delete g_stopper;
        g_stopper = NULL;
        LOGI("Stopper deleted.");
      } else {
        LOGE("...COUNT WAS NONZERO BUT NO STOPPER?!");
      }
    }
  } else {
    LOGE("...BUT NOT LOCKED?!");
  }
  LOGI("Local Resume_Emulation, g_count=%d", g_stop_count);
}

// ***************************************************************
// *** The actual exported functions that are called from Java ***
// ***************************************************************

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetPHEMDir
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetPHEMDir
  (JNIEnv *env, jclass clazz, jstring phem_dir) {

  // Record the directory we should look for skins, roms, sessions, etc.
  const char *temp_phem_dir = env->GetStringUTFChars(phem_dir, 0);

  PHEM_base_dir.assign(temp_phem_dir);  

  env->ReleaseStringUTFChars(phem_dir, temp_phem_dir);
}

struct PrvSupportsROM : unary_function<EmDevice&, bool>
{
        PrvSupportsROM(EmROMReader& inROM) : ROM(inROM) {}
        bool operator()(EmDevice& item)
        {
                return !item.SupportsROM(ROM);
        }

private:
        const EmROMReader& ROM;
};

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetSessionInfo
 * Signature: ()[Ljava/lang/String;
 */
JNIEXPORT jobject JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetSessionInfo
  (JNIEnv *env, jclass clazz) {
  // Get basic information about the current session.
  jobjectArray ret = NULL;
  jstring str;

  if (gSession) {
    LOGI("Get Session Info, gSession set.");
    //Pause_Emulation();
    LOGI("Getting Session config.");
    Configuration cur_cfg = gSession->GetConfiguration();
    ret = (jobjectArray)env->NewObjectArray(3,
                               env->FindClass("java/lang/String"),
                               env->NewStringUTF(""));
    if (NULL != ret) {
      // ROM file
      LOGI("Getting rom file.");
      str = env->NewStringUTF(cur_cfg.fROMFile.GetName().c_str());
      env->SetObjectArrayElement(ret, 0, str);
      LOGI(cur_cfg.fROMFile.GetName().c_str());

      // Device
      LOGI("Getting device.");
      str = env->NewStringUTF(cur_cfg.fDevice.GetIDString().c_str());
      env->SetObjectArrayElement(ret, 1, str);

      // RAM
      LOGI("Getting RAM.");
      MemoryTextList                 sizes;
      ::GetMemoryTextList (sizes);
      LOGI("Got size list?");
      MemoryTextList::iterator       iter = sizes.begin();

      LOGI("Starting loop.");
      while (iter != sizes.end()) {
        LOGI("Cur size is: %ld", iter->first);
        if (cur_cfg.fRAMSize == iter->first) {
         str = env->NewStringUTF(iter->second.c_str());
         env->SetObjectArrayElement(ret, 2, str);
         LOGI("RAM is: %s", iter->second.c_str());
         break;
        }
        iter++;
      }
    } else {
      LOGE("Couldn't allocate session info array!");
    }
    LOGI("All done.");
    //Resume_Emulation();
  }
  LOGI("GetSessionInfo Returning.");
  return ret;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetROMDevices
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobject JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetROMDevices
  (JNIEnv *env, jclass clazz, jstring rom_file) {
  // Given a ROM file, what devices (if any) will it support?
  const char *temp_rom_file = env->GetStringUTFChars(rom_file, 0);

  EmFileRef			the_rom_file(temp_rom_file);
  EmDeviceList			devices = EmDevice::GetDeviceList ();
  EmDeviceList::iterator	iter            = devices.begin();
  EmDeviceList::iterator	devices_end     = devices.end();

  jobjectArray ret = NULL;
  jstring      str;
  char **data = NULL;
  int i, version;

  LOGI("GetRomDevices... '%s'", temp_rom_file);
  if (the_rom_file.IsSpecified()) {
    try {
      EmStreamFile    hROM(the_rom_file, kOpenExistingForRead);
      StMemory        romImage(hROM.GetLength ());

      hROM.GetBytes (romImage.Get(), hROM.GetLength());

      EmROMReader ROM(romImage.Get(), hROM.GetLength());

      if (ROM.AcquireCardHeader()) {
        version = ROM.GetCardVersion();

        if (version < 5) {
          ROM.AcquireROMHeap();
          ROM.AcquireDatabases();
          ROM.AcquireFeatures();
          ROM.AcquireSplashDB();
        }
      }

      devices_end = remove_if(devices.begin (), devices.end (),
                                PrvSupportsROM (ROM));
    } catch (ErrCode) {
      /* On any of our errors, don't remove any devices */
      LOGI("Got error code!");
    }

    // Okay, now we've got a list of valid devices. Iterate and create
    // an array to give the user.
    iter = devices.begin();
    int num_devs = distance(iter, devices_end);
    if (num_devs > 0) {
      LOGI("%d matching devices.", num_devs);
      data = (char **)malloc(num_devs * sizeof(char *));   
      if (NULL == data) {
        LOGE("Oh snap, couldn't allocate arraylist to return!");
      } else {
        // Loop through devices, data array; make strings for Java
        ret = (jobjectArray)env->NewObjectArray(num_devs,
                                    env->FindClass("java/lang/String"),
                                    env->NewStringUTF(""));
        for (i=0; iter != devices_end && i<num_devs; i++, iter++) {
          LOGI("Setting array '%d' to '%s'.", i, iter->GetIDString().c_str());
          str = env->NewStringUTF(iter->GetIDString().c_str());
          env->SetObjectArrayElement(ret, i, str);
        }
      }
    } else {
      LOGI("No matching devices!");
    }
  } else {
    LOGI("Rom file '%s' is 'not specified'.", temp_rom_file);
  }
  env->ReleaseStringUTFChars(rom_file, temp_rom_file);
  if (data) {
    free (data);
  }

  return ret;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetDeviceMemories
 * Signature: (Ljava/lang/String;)[Ljava/lang/String;
 */
JNIEXPORT jobject JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetDeviceMemories
  (JNIEnv *env, jclass clazz, jstring device_name) {
  // Given a device, return the memory sizes it supports.
  jobjectArray ret = NULL;
  jstring      str;
  
  const char *temp_device_name = env->GetStringUTFChars(device_name, 0);

  LOGI("Dev name is '%s'.", temp_device_name);
  // Get the device from the name.
  EmDeviceList			devices = EmDevice::GetDeviceList ();
  EmDeviceList::iterator	dev_it  = devices.begin();
  char **data = NULL;
  int i, dev_found = 0, num_mems = 0;

  while (dev_it != devices.end()) {
    if (!strcmp(temp_device_name, dev_it->GetIDString().c_str())) {
      dev_found = 1;
      break;
    }
    dev_it++;
  }
  if (!dev_found) {
    LOGE("Couldn't find device info!");
  } else {
    // Okay, now get the memory sizes it believes in.
    RAMSizeType	minSize = dev_it->MinRAMSize ();
    MemoryTextList	sizes;
    ::GetMemoryTextList (sizes);

    MemoryTextList::iterator	iter = sizes.begin();

    while (iter != sizes.end()) {
      if (iter->first < minSize) {
        sizes.erase (iter);
        iter = sizes.begin ();
        continue;
      }
      ++iter;
    }
 
    // Turn the remaining sizes (if any) into an ArrayList<String> and pass it back.
    iter = sizes.begin();
    num_mems = distance(iter, sizes.end());
    LOGI("Num mems: %d", num_mems);
    if (num_mems > 0) {
      data = (char **)malloc(num_mems * sizeof(char *));
      if (NULL == data) {
        LOGE("Oh snap, couldn't allocate arraylist to return!");
      } else {
        // Loop through iterators, data array; make strings for Java
        ret = (jobjectArray)env->NewObjectArray(num_mems,
                                    env->FindClass("java/lang/String"),
                                    env->NewStringUTF(""));
        for (i=0; iter != sizes.end() && i<num_mems; i++, iter++) {
          str = env->NewStringUTF(iter->second.c_str());
          env->SetObjectArrayElement(ret, i, str);
        }
      }
    } else {
      LOGI("No memory sizes!");
    }
  }

  env->ReleaseStringUTFChars(device_name, temp_device_name);
  if (data) {
    free(data);
  }
  return ret;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetDeviceSkins
 * Signature: (Ljava/lang/String;)Ljava/util/ArrayList;
 */
JNIEXPORT jobject JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetDeviceSkins
  (JNIEnv *env, jclass clazz, jstring device) {
  // Given a device, return the supported skins.
  jobjectArray ret = NULL;
  // AndroidTODO: everything
  return ret;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    InstallPalmFile
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL Java_com_perpendox_phem_PHEMNativeIF_InstallPalmFile
  (JNIEnv *env, jclass clazz, jstring file_name) {
  ErrCode result = errNone;
  LOGI("InstallPalmFile");
  Pause_Emulation();
  const char *fileName = env->GetStringUTFChars(file_name, 0);

  LOGI("Attempting to load: %s", fileName);
  try {
    EmFileRef       fileRef (fileName);
    EmFileRefList   fileList;
    fileList.push_back(fileRef);

    LOGI("Loading.");
    vector<LocalID> idList;
    EmFileImport::LoadPalmFileList(fileList, kMethodBest, idList);

    LOGI("Switching to HotSync app.");
    // Switch to HotSync app; weirdness and crashes can happen if you're
    // on the main screen (the 'Launcher') and load an app 'behind its back'.

    DmSearchStateType      searchState;
    UInt16                 cardNo;
    LocalID                dbID;

    Err err = ::DmGetNextDatabaseByTypeCreator(true, &searchState,
                                               sysFileTApplication, sysFileCSync,
                                               true, &cardNo, &dbID);
    if (err) {
      LOGE("Could not find HotSync app! Should always be present!");
    } else {
      err = ::SysUIAppSwitch (cardNo, dbID,
                                sysAppLaunchCmdNormalLaunch, NULL);
      if (err) {
        LOGE("Could not switch to HotSync app!");
      }
    }
  } catch (ErrCode errCode) {
    LOGE("Error loading file: %ld", errCode);
    result = errCode;
  }
  env->ReleaseStringUTFChars(file_name, fileName);

  LOGI("Load result is %ld", result);
  Resume_Emulation();
  return result;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    HandleIdle
 * Signature: (Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL Java_com_perpendox_phem_PHEMNativeIF_HandleIdle
  (JNIEnv *env, jclass clazz, jobject buf) {
  unsigned char *Java_buffer;
  //static int count = 0;

  // This function is called by the Java side to get updates on the emulator
  // state, in particular screen updates.
  Java_buffer = (unsigned char *)env->GetDirectBufferAddress(buf);
  if (Java_buffer == NULL) {
    LOGE("Failed to get direct buffer address!");
  } else {
    //LOGI("Calling App::HandleIdle.");
    if (g_allow_idle) {
      // This pauses the emulated CPU, takes care of pending dialogs, and updates
      // the screen if necessary. When that happens, g_screen_updated gets set.
      the_app.HandleIdle();

//      LOGI("min: %ld max: %ld avg: %ld", gApplication->min_tps, gApplication->max_tps,
//                                      gApplication->avg_tps);
      if (g_screen_updated) {
        // Copying data from native to Android is expensive.
        // So we only copy the changed region.
        //LOGI("Updating buffer, lines: %d, %d", g_first_line, g_last_line);
        if (g_last_line >= g_win_h) {
          // For some reason, this can happen when the whole skin gets painted.
          LOGI("Last line > skin size?");
          g_last_line = g_win_h-1;
        }
        long offset = g_first_line * (g_win_w * 2);
        long updt_size = ((g_last_line - g_first_line) + 1) * (g_win_w * 2);
        if (offset < 0 || offset+updt_size > PHEM_buffer_size) {
           LOGE("Yikes! offset: %ld updt_size: %ld buffer size: %u",
                 offset, updt_size, PHEM_buffer_size);
        }
        //memcpy(Java_buffer+offset, PHEM_buffer+offset, updt_size);
        memcpy(Java_buffer, PHEM_buffer, PHEM_buffer_size);
        g_screen_updated = 0; // clear for next time.
        g_first_line = g_win_h+1;
        g_last_line = 0;
        return 1;
      } else {
        //LOGI("No screen update.");
      }
    } else {
      //LOGI("HandleIdle ignored.");
    }
  }

  //LOGI("App::HandleIdle returning.");
  // Should be zero at this point.
  return g_screen_updated;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    NewSession
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_NewSession
  (JNIEnv *env, jclass clazz, jstring rom_file, jstring ram_size, jstring dev_name, jstring skin_name) {

  LOGI("NewSession called.");

  g_class = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
  if (NULL == g_class) {
    LOGE("Failed to get global ref to native interface class!");
    return;
  }

  g_done = 0;

  // Set up the argc, argv variables.
  argc = 9; //TODO: count args

  // Get the args from the Java side of things.
  const char *temp_rom_file = env->GetStringUTFChars(rom_file, 0);
  const char *temp_ram_size = env->GetStringUTFChars(ram_size, 0);
  const char *temp_dev_name = env->GetStringUTFChars(dev_name, 0);

  argv = (char **)calloc(argc+1, sizeof(char *));
 
  argv[0] = app_name;

  argv[1] = minus_rom;
  argv[2] = (char *)calloc(strlen(temp_rom_file)+1, sizeof(char));
  strcpy(argv[2], temp_rom_file);

  argv[3] = minus_ram;
  argv[4] = (char *)calloc(strlen(temp_ram_size)+1, sizeof(char));
  strcpy(argv[4], temp_ram_size);

  argv[5] = minus_dev;
  argv[6] = (char *)calloc(strlen(temp_dev_name)+1, sizeof(char));
  strcpy(argv[6], temp_dev_name);

  // TODO: handle skin
  argv[7] = "-pref";
  argv[8] = "CloseAction=SaveNever";

  // Make sure to release them
  env->ReleaseStringUTFChars(rom_file, temp_rom_file);
  env->ReleaseStringUTFChars(ram_size, temp_ram_size);
  env->ReleaseStringUTFChars(dev_name, temp_dev_name);

  // Kick things off
  Start_App(argc, argv);
  g_in_reset = 1; // Shouldn't send events during boot.
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    RestartSession
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_RestartSession
  (JNIEnv *env, jclass clazz, jstring psf_file) {

  LOGI("RestartSession called.");
  g_class = reinterpret_cast<jclass>(env->NewGlobalRef(clazz));
  if (NULL == g_class) {
    LOGE("Failed to get global ref to native interface class!");
    return;
  }

  g_done = 0;

  // Set up the argc, argv variables.
  argc = 3;

  // Get the args from the Java side of things.
  const char *temp_psf_file = env->GetStringUTFChars(psf_file, 0);

  argv = (char **)calloc(argc+1, sizeof(char *));
  
  argv[0] = app_name;

  argv[1] = minus_psf;
  argv[2] = (char *)calloc(strlen(temp_psf_file)+1, sizeof(char));
  strcpy(argv[2], temp_psf_file);

  LOGI("Loading: '%s %s %s'", argv[0], argv[1], argv[2]);

  env->ReleaseStringUTFChars(psf_file, temp_psf_file);

  // Kick things off
  LOGI("RestartSession starting app.");
  Start_App(argc, argv);
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SaveSession
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SaveSession
  (JNIEnv *env, jclass clazz, jstring psf_file) {
  LOGI("SaveSession called.");
  EmSessionStopper stopper (gSession, kStopNow); // stops session until func returns
  // Get the args from the Java side of things.
  const char *temp_file_name = env->GetStringUTFChars(psf_file, 0);
  psf_file_name.assign(temp_file_name);
  env->ReleaseStringUTFChars(psf_file, temp_file_name);

  EmFileRef psf_file_ref(psf_file_name);
  gSession->Save(psf_file_ref, false);
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetCurrentDevice
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetCurrentDevice
  (JNIEnv *env, jclass clazz) {
  // Get the current device being emulated.
  jstring the_dev;

  the_dev = env->NewStringUTF(gSession->GetDevice().GetIDString().c_str());

  return the_dev;
}

// Helper function; determine if it's safe to pass user events to emulator.
// Returns 0 if safe, 1 if events should be ignored.
int Check_Reset_Condition() {
  if (g_in_reset) {
    if (difftime(time(NULL), g_reset_timestamp) > RESET_TIMEOUT) {
      g_in_reset = 0;
      g_logo_drawn = 0;
      return 0;
    } else {
      // Assume NOT safe yet.
      return 1;
    }
  } else {
    return 0;
  }
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    PenDown
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_PenDown
  (JNIEnv *env, jclass clazz, jint x_pos, jint y_pos) {
  //LOGI("PenDown called.");
  if (Check_Reset_Condition()) {
    LOGI("PenDown: In reset.");
    return JNI_TRUE;
  }
  if (g_allow_idle) {
    PHEM_Event evt;
    evt.type = PHEM_PEN_DOWN;
    PHEM_mouse_x = evt.x_pos = x_pos;
    PHEM_mouse_y = evt.y_pos = y_pos;

    // Pass it on to the emulator...
    the_app.HandleEvent(&evt);
  }
  return JNI_FALSE;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    PenMove
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_PenMove
  (JNIEnv *env, jclass clazz, jint x_pos, jint y_pos) {
  //LOGI("PenMove called.");
  if (Check_Reset_Condition()) {
    LOGI("PenMove: In reset.");
    return JNI_TRUE;
  }
  if (g_allow_idle) {
    PHEM_Event evt;
    evt.type = PHEM_PEN_MOVE;
    PHEM_mouse_x = evt.x_pos = x_pos;
    PHEM_mouse_y = evt.y_pos = y_pos;

    // Pass it on to the emulator...
    the_app.HandleEvent(&evt);
  }
  return JNI_FALSE;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    PenUp
 * Signature: (II)Z
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_PenUp
  (JNIEnv *env, jclass clazz, jint x_pos, jint y_pos) {
  //LOGI("PenUp called.");
  if (Check_Reset_Condition()) {
    LOGI("PenUp: In reset.");
    return JNI_TRUE;
  }
  if (g_allow_idle) {
    PHEM_Event evt;
    evt.type = PHEM_PEN_UP;
    PHEM_mouse_x = evt.x_pos = x_pos;
    PHEM_mouse_y = evt.y_pos = y_pos;

    // Pass it on to the emulator...
    the_app.HandleEvent(&evt);
  }
  return JNI_FALSE;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    KeyEvent
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_KeyEvent
  (JNIEnv *env, jclass clazz, jint key) {
  if (Check_Reset_Condition()) {
    LOGI("KeyEvent: In reset.");
    return JNI_TRUE;
  }
  LOGI("KeyEvent: '%d' '%x'", key, key);

  if (NULL != gDocument && g_allow_idle) {
    LOGI("EmKeyEvent");
    EmKeyEvent event(key);
    LOGI("HandleKey");
    gDocument->HandleKey(event);
  }
  LOGI("KeyEvent done.");
  return JNI_FALSE;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    ButtonEvent
 * Signature: (IZ)Z
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_ButtonEvent
  (JNIEnv *env, jclass clazz, jint button, jboolean down) {
  LOGI("ButtonEvent: '%d' down: %d", button, down);

  if (Check_Reset_Condition()) {
    LOGI("ButtonEvent: In reset.");
    return JNI_TRUE;
  }

  if (NULL != gDocument && g_allow_idle) {
    switch (button) {
     case 0:
       gDocument->HandleButton(kElement_App1Button, down);
       break;
     case 1:
       gDocument->HandleButton(kElement_App2Button, down);
       break;
     case 2:
       gDocument->HandleButton(kElement_UpButton, down);
       break;
     case 3:
       gDocument->HandleButton(kElement_DownButton, down);
       break;
     case 4:
       gDocument->HandleButton(kElement_App3Button, down);
       break;
     case 5:
       gDocument->HandleButton(kElement_App4Button, down);
       break;
     case 6:
       gDocument->HandleButton(kElement_PowerButton, down);
       break;
     default:
       LOGE("Unknown button type '%d'!", button);
       break;
    }
  }
  return JNI_FALSE;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    ResetEmulator
 * Signature: (I)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_ResetEmulator
  (JNIEnv *env, jclass clazz, jint reset_type) {
  LOGI("ResetEmulator called.");
  EmSessionStopper stopper (gSession, kStopNow); // stops session until
                                                 // function returns
  switch (reset_type) {
  case 0:
    LOGI("Executing soft reset.");
    gSession->Reset(kResetSoft);
    break;
  case 1:
    LOGI("Executing no-ext soft reset.");
    gSession->Reset(kResetSoftNoExt);
    break;
  case 2:
    LOGI("Executing hard reset.");
    gSession->Reset(kResetHard);
    break;
  default:
    LOGE("Unknown reset type '%d'!", reset_type);
    return;
  }
  // Record that we're resetting the Palm.
  g_in_reset = 1;
  time(&g_reset_timestamp);
  LOGI("Reset finished.");
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    PauseEmulation
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_PauseEmulation
  (JNIEnv *env, jclass clazz) {
  LOGI("native PauseEmulation called.");
  Pause_Emulation();
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    ResumeEmulation
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_ResumeEmulation
  (JNIEnv *env, jclass clazz) {
  LOGI("native ResumeEmulation called.");
  Resume_Emulation();
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    ShutdownEmulator
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_ShutdownEmulator
  (JNIEnv *env, jclass clazz, jstring save_name)
{
  LOGI("ShutdownEmulator called.");

  // Get the string.
  const char *temp_file_name = env->GetStringUTFChars(save_name, 0);

  LOGI("Saving to '%s'.", temp_file_name);
  // Set up the file name.
  psf_file_name.assign(temp_file_name);

  env->ReleaseStringUTFChars(save_name, temp_file_name);

  // Wake up the app thread, tell it to go away.
  { 
    // Enclosed in a block so the lock goes away.
    omni_mutex_lock lock(g_loop_mutex);
    g_done = 1;
    g_loop_cond.signal();
  }

  // Wait for the app thread to die.
  if (g_app_thread) {
    g_app_thread->join(NULL);
    g_app_thread = NULL;
  }
  LOGI("App stopped, clearing stop count.");
  g_stop_count = 0;
  LOGI("ShutdownEmulator done.");
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    IsVFSManagerPresent
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_perpendox_phem_PHEMNativeIF_IsVFSManagerPresent
  (JNIEnv *env, jclass clazz)
{
    jint retval;
    UInt32 vfsMgrVersion;
    Err err;

#define sysFileCVFSMgr          'vfsm'  // Creator code for VFSMgr...
#define vfsFtrIDVersion         0       // ID of feature containing version of VFSMgr.

    LOGI("Checking VFS.");
    if (Check_Reset_Condition()) {
      LOGI("Whoops, in reset.");
      retval = -1;
      return retval;
    }

    if (gSession->GetSessionState() != kRunning) {
      LOGI("Session not running.");
      // May be stopped somewhere besides a syscall; can't risk it.
      retval = -1;
      return retval;
    } else { 
      LOGI("I think the session is running.");
    }

    // Bad things happen if you don't pause the emulator on a syscall...
    Pause_Emulation();

    LOGI("Making FtrGet call.");
    err = ::FtrGet(sysFileCVFSMgr, vfsFtrIDVersion, &vfsMgrVersion);
    if (err) {
      LOGI("VFS Manager not present.");
      retval = 0;
    } else {
      LOGI("VFS Manager present, version '%ld'", vfsMgrVersion);
      retval = vfsMgrVersion;
    }
    Resume_Emulation();
    return retval;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetPalmEncoding
 * Signature: ()I
 */
JNIEXPORT jint JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetPalmEncoding
  (JNIEnv *env, jclass clazz)
{
    jint retval;
    UInt32 encoding;

    LOGI("Getting Palm encoding.");
    if (gSession->GetSessionState() != kRunning) {
      LOGI("Session not running.");
      // May be stopped somewhere besides a syscall; can't risk it.
      retval = 0;
      return retval;
    } else { 
      LOGI("I think the session is running.");
    }

    if (Check_Reset_Condition()) {
      LOGI("Whoops: In reset.");
      retval = 0;
      return retval;
    }

    if (FtrGet(sysFtrCreator, sysFtrNumEncoding, &encoding) != 0) {
      // Was a problem, have to assume default encoding
      LOGE("Error getting Palm encoding, setting default.");
      encoding = charEncodingPalmLatin;
    }
    switch (encoding) {
    case charEncodingPalmSJIS:
      LOGI("Shift-JIS.");
      // encoding for Palm Shift-JIS
      retval = 1;
      break;
    case charEncodingGB2312:
      LOGI("GB2312.");
      // one Chinese encoding
      retval = 2;
      break;
    case charEncodingBig5:
      LOGI("Big5.");
      // another Chinese encoding
      retval = 3;
      break;
    case charEncodingKsc5601:
      LOGI("KSC5601.");
      // a Korean encoding
      retval = 4;
      break;
    default:
      LOGI("Default/CP2313.");
      // Anything else, assume default.
      retval = 0;
    }

    return retval;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    IsHostFSPresent
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_IsHostFSPresent
  (JNIEnv *env, jclass clazz)
{
    DmSearchStateType      searchState;
    UInt16                 cardNo;
    LocalID                dbID;

    if (Check_Reset_Condition()) {
      LOGI("IsHostFSPresent: In reset.");
      return JNI_FALSE;
    }

    Pause_Emulation();

    LOGI("Checking for HostFS.");
    Err err = ::DmGetNextDatabaseByTypeCreator(true, &searchState,
                                               'libf', 'hstf',
                                               true, &cardNo, &dbID);
    Resume_Emulation();
    if (err) {
      LOGI("HostFS not present.");
      return JNI_FALSE;
    } else {
      LOGI("HostFS installed.");
      return JNI_TRUE;
    }
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetCurrentCardDir
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetCurrentCardDir
  (JNIEnv *env, jclass clazz)
{
  // Gotta make sure the Palm is paused when we do this.
  Pause_Emulation();

  Preference<SlotInfoList>    pref (kPrefKeySlotList);
  LOGI("Pref found.");
  SlotInfoList                slotList = *pref;
  SlotInfoType&   info = slotList[0];
  jstring the_dir;

  if (0 == slotList.size()) {
    LOGI("Size is nil!");
    the_dir = env->NewStringUTF("");
  } else {
    LOGI("Getting path.");
    string fullPath = info.fSlotRoot.GetFullPath();
    if (fullPath.empty()) {
      LOGI("Path empty.");
      the_dir = env->NewStringUTF("");
    } else {
      LOGI("Got path, '%s'", fullPath.c_str());
      the_dir = env->NewStringUTF(fullPath.c_str());
    }
  }

  Resume_Emulation();
  return the_dir;
}

const int kMaxVolumes = 2;

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetCurrentCardDir
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetCurrentCardDir
  (JNIEnv *env, jclass clazz, jstring the_dir)
{
  // Gotta make sure the Palm is paused when we do this.
  Pause_Emulation();

  Preference<SlotInfoList>    pref (kPrefKeySlotList);
  SlotInfoList                slotList = *pref;

  const char *temp_dir_str = env->GetStringUTFChars(the_dir, 0);
  LOGI("Creating dir ref: '%s'", temp_dir_str);
  EmDirRef   dir_ref(temp_dir_str);

  SlotInfoType&   infor = slotList[0];
  infor.fSlotRoot = dir_ref; //AndroidTODO: leak?

  env->ReleaseStringUTFChars(the_dir, temp_dir_str);

  LOGI("Saving changes.");
  pref = slotList;
  Resume_Emulation();
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetCardMountStatus
 * Signature: ()Z;
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetCardMountStatus
  (JNIEnv *env, jclass clazz)
{
  LOGI("GetCardMountStatus");
  jboolean ret_val = JNI_FALSE;
#if 1
  // Gotta make sure the Palm is paused when we do this.
  Pause_Emulation();

  Preference<SlotInfoList>    pref (kPrefKeySlotList);
  SlotInfoList                slotList = *pref;

  // Make sure there are at least kMaxVolume entries.  If not, then
  // fill in the missing ones.
  long    curSize = slotList.size ();
  if (curSize < kMaxVolumes) {
    LOGI("Creating card slot prefs.");
    while (curSize < kMaxVolumes) {
      SlotInfoType    info;
      info.fSlotNumber = curSize + 1;
      LOGI("Creating slot '%ld'.", info.fSlotNumber);
      info.fSlotOccupied = false;
      slotList.push_back (info);

      curSize++;
    }
    LOGI("Saving new prefs.");
    pref = slotList;
  }

  // Now we can check the mount status.
  SlotInfoType&   info = slotList[0];

  if (info.fSlotOccupied) {
    LOGI("Mounted.");
    ret_val = JNI_TRUE;
  }

  LOGI("Resuming emulator.");
  Resume_Emulation();
  LOGI("Done.");
#endif
  return ret_val;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetCardMountStatus
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetCardMountStatus
  (JNIEnv *env, jclass clazz, jboolean mount_stat)
{
  // Gotta make sure the Palm is paused when we do this.
  Pause_Emulation();

  Preference<SlotInfoList>    pref (kPrefKeySlotList);
  SlotInfoList                slotList = *pref;
  SlotInfoType&   info = slotList[0];

  int mounted;
  if (JNI_TRUE == mount_stat) {
    mounted = 1;
  } else {
    // Must be JNI_FALSE, right? Right?
    mounted = 0;
  }
  if (info.fSlotOccupied == mounted) {
    LOGI("Mount status unchanged, skipping.");
    Resume_Emulation();
    return;
  }

  // Oh, well, looks like we've got work to do.
  info.fSlotOccupied = mounted;
  pref = slotList;

  UInt16  refNum;
  Err err = ::SysLibFind("HostFS Library", &refNum);

  if (err != errNone || refNum == sysInvalidRefNum) {
    LOGE("Couldn't find the HostFS refnum!");
    Resume_Emulation();
    return;
  }

  const UInt32    kCreator        = 'pose';
  UInt16  selector = mounted;

  void*   cardNum = (void*) info.fSlotNumber;
  LOGI("Registering slot num '%ld', stat: %d", info.fSlotNumber, selector);

  // Note, in order to make this call, the CPU should be stopped
  // in the UI task.  That's because mounting and unmounting can
  // send out notification.  If the notification is sent out while
  // the current Palm OS task is not the UI task, then the
  // notification manager calls SysTaskWait.  This will switch
  // to another task if it can, and prime a timer to re-awake
  // the background task if not.  However, this timer is based
  // on an interrupt going off, and while we're calling into the
  // ROM, interrupts are turned off, leading to a hang.
  // Therefore, is is imperative that we call this function
  // while the UI task is the current task.

  ::FSCustomControl(refNum, kCreator, selector, cardNum, NULL);
  Resume_Emulation();
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetNetlibRedirect
 * Signature: ()Z;
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetNetlibRedirect
  (JNIEnv *env, jclass clazz)
{
   Preference<bool>        pref(kPrefKeyRedirectNetLib);
                
   if (*pref) {
     return JNI_TRUE;
   } else {
     return JNI_FALSE;
   }
}


/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetNetlibRedirect
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetNetlibRedirect
  (JNIEnv *env, jclass clazz, jboolean redirect)
{
   Preference<bool>        pref(kPrefKeyRedirectNetLib);
   
  if (JNI_TRUE == redirect) {
    pref = true;
  } else {
    pref = false;
  }
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetScale
 * Signature: ()Z;
 */
JNIEXPORT jboolean JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetScale
  (JNIEnv *env, jclass clazz)
{
   Preference<ScaleType>   pref (kPrefKeyScale);
                
   if (*pref == 2) {
     return JNI_TRUE;
   } else {
     return JNI_FALSE;
   }
}


/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetScale
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetScale
  (JNIEnv *env, jclass clazz, jboolean scale)
{
   Preference<ScaleType>   pref (kPrefKeyScale);
   
  if (JNI_TRUE == scale) {
    pref = 2;
  } else {
    pref = 1;
  }
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetHotSyncName
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetHotSyncName
  (JNIEnv *env, jclass clazz) {
  // Get the current HotSync name.
  Preference<string> prefUserName (kPrefKeyUserName);

  string tempie = *prefUserName;

  // Turn string into a jbyte array, pass it on to Android, let it
  // handle any charset weirdness.
  int name_len = tempie.size();
  jbyteArray the_name = env->NewByteArray(name_len);
  env->SetByteArrayRegion(the_name, 0, name_len,
                       (jbyte *)tempie.c_str());
  return the_name;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetHotSyncName
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetHotSyncName
  (JNIEnv *env, jclass clazz, jobject buf)
{
  char *name_buf;
  jthrowable exc;
  jlong buf_siz;

  // Gotta make sure the Palm is paused when we do this.
  Pause_Emulation();

  LOGI("Setting HotSync name.");
  Preference<string> prefUserName (kPrefKeyUserName);

  LOGI("Getting buffer addr from host.");
  // Gotta call a Java method, get a ByteBuffer address.
  name_buf = (char *)env->GetDirectBufferAddress(buf);
  exc = env->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling GetDirectBufferAddress!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }

  LOGI("Getting buffer size from host.");
  // Gotta call a Java method, get a ByteBuffer size.
  buf_siz = (jlong)env->GetDirectBufferCapacity(buf);
  LOGI("Call returned.");
  exc = env->ExceptionOccurred();
  if (exc) {
    LOGE("Exception calling GetDirectBufferCapacity!");
    /* We don't do much with the exception, except that
     we print a debug message for it, clear it, and 
     throw a new exception. */
    env->ExceptionDescribe();
    env->ExceptionClear();
    return;
  }

  if (NULL != name_buf) {
    // Now, get the chars, make a string.
    // Allocate buffer for clip...
    char *new_name = (char *)calloc(sizeof(char), buf_siz+1);
    memcpy(new_name, name_buf, buf_siz);
    LOGI("Setting hotsync: '%s'", new_name);
    string temp_str = new_name;
    ::SetHotSyncUserName (new_name);
    free(new_name);

    LOGI("Saving changes.");
    prefUserName = temp_str;
  } else {
    // Got bupkus from the host.
    LOGI("Got null name buf.");
  }

  Resume_Emulation();
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    GetSerialDevice
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_perpendox_phem_PHEMNativeIF_GetSerialDevice
  (JNIEnv *env, jclass clazz) {
  // Get the current serial dev, if any.
  Preference<EmTransportDescriptor>       prefPortSerial          (kPrefKeyPortSerial);

  string tempie = prefPortSerial->GetMenuName();

  jstring the_name;

  LOGI("Getting serial dev: '%s'", tempie.c_str());

  the_name = env->NewStringUTF(tempie.c_str());

  return the_name;
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    SetSerialDevice
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_SetSerialDevice
  (JNIEnv *env, jclass clazz, jstring device_string)
{
  const char *temp_dev_string = env->GetStringUTFChars(device_string, 0);
  LOGI("Setting serial dev: '%s'", temp_dev_string);

  // Set the preference
  Preference<EmTransportDescriptor>       prefPortSerial          (kPrefKeyPortSerial);
  EmTransportDescriptor           serialPortDesc;
  string dev_string(temp_dev_string);

  serialPortDesc = dev_string;
  prefPortSerial = serialPortDesc;

  // Update the emulator with the new value
  gEmuPrefs->SetTransports();

  // Let go of the jstring data.
  env->ReleaseStringUTFChars(device_string, temp_dev_string);
}

/*
 * Class:     com_perpendox_phem_PHEMNativeIF
 * Method:    PassNMEA
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL Java_com_perpendox_phem_PHEMNativeIF_PassNMEA
  (JNIEnv *env, jclass clazz, jstring nmea_string)
{
  long int sentence_len;
  const char *temp_nmea_string = env->GetStringUTFChars(nmea_string, 0);
  LOGI("NMEA string: '%s'", temp_nmea_string);

  // NMEA sentences are 82 chars max (including trailing CRLF)
  sentence_len = strnlen(temp_nmea_string, 83);

  // Quick set up the serial config.
  EmTransportSerial::ConfigSerial cfg_ser;
  string port_name("/@");
  cfg_ser.fPort = port_name;

  // Pass it on to the read buffer of the emulated serial port.
  //EmTransportSerial *trans = EmTransportSerial::GetTransport(cfg_ser);
  EmTransportSerial *trans = (EmTransportSerial *)cfg_ser.GetTransport();
  if (trans) {
    trans->fHost->PutIncomingData(temp_nmea_string, sentence_len);
  } else {
    // AndroidTODO: log error
    LOGI("Serial port not open/found, ignoring.");
  }

  // Let go of the data.
  env->ReleaseStringUTFChars(nmea_string, temp_nmea_string);
}
