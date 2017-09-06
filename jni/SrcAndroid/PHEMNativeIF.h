#ifndef PHEMNativeIF_h
#define PHEMNativeIF_h

#include "EmDlgAndroid.h"
#include "EmStructs.h"

typedef enum {
   PHEM_PEN_DOWN=0,
   PHEM_PEN_MOVE,
   PHEM_PEN_UP,
   PHEM_KEY
} PHEM_EVENT_TYPE;
   
typedef struct phem_evt {
  PHEM_EVENT_TYPE type;
  int x_pos;
  int y_pos;
} PHEM_Event;

typedef enum {
   PHEM_SOUND=0,
   PHEM_CLIPBOARD
} PHEM_CMD_TYPE;
 
// Uncomment to enable logging.
//#define PHEM_LOGGING 1

// Only existed for experimental path for sound support
#if 0
typedef struct phem_cmd {
  PHEM_CMD_TYPE type;
  struct phem_cmd *next;
  int freq;
  int dur;
  int amp;
} PHEM_Cmd;
#endif

// Debugging
#if 0
void PHEM_Log_Place(int code);
void PHEM_Log_Hex(unsigned int code);
void PHEM_Log_Msg(const char *msg);
#endif

#ifdef PHEM_LOGGING
void PHEM_Logger_Place(int code);
void PHEM_Logger_Hex(unsigned int code);
void PHEM_Logger_Msg(const char *msg);
#define PHEM_Log_Place(x) PHEM_Logger_Place(x)
#define PHEM_Log_Hex(x)   PHEM_Logger_Hex(x)
#define PHEM_Log_Msg(x)   PHEM_Logger_Msg(x)
#else
#define PHEM_Log_Place(x)
#define PHEM_Log_Hex(x)
#define PHEM_Log_Msg(x)
#endif

const char *PHEM_Get_Base_Dir();

// Screen management
void PHEM_Mark_Screen_Updated(int first, int last);
unsigned char *PHEM_Get_Buffer();
void PHEM_Reset_Window(int w, int h);

// Allow CPU thread to make JNI calls
void PHEM_Bind_CPU_Thread();
void PHEM_Unbind_CPU_Thread();

// Clipboard management (CPU thread)
const char *PHEM_Get_Host_Clip();
void PHEM_Set_Host_Clip(const char *clip);

// Play sound (a JNI call CPU thread makes)
void PHEM_Queue_Sound(int freq, int duration, int amplitude);

// Vibration
void PHEM_Begin_Vibration();
void PHEM_End_Vibration();

// LED
void PHEM_Enable_LED(Bool draw);
void PHEM_Set_LED(RGBType color);

extern int PHEM_mouse_x;
extern int PHEM_mouse_y;

// Dialogs
EmDlgItemID PHEM_Do_Common_Dialog(PHEM_Dialog *dlg);
EmDlgItemID PHEM_Do_Reset_Dialog(PHEM_Dialog *dlg);

// Tick speed calibration
//void PHEM_Measure_Ticks(unsigned long cur_ticks);

#endif /* PHEMNativeIF_h */
