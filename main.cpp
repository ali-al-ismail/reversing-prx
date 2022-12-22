// HUGE CREDITS TO FREAKLER FOR HIS RESEARCH AND HIS ORIGINAL CODE WRITTEN IN C

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <systemctrl.h>
#include <string.h>
#include <stdio.h>

#define EMULATOR_DEVCTL__IS_EMULATOR 0x00000003

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f)&0x0FFFFFFC) >> 2), a);

#define MAKE_DUMMY_FUNCTION(a, r)            \
    {                                        \
        u32 _func_ = a;                      \
        if (r == 0)                          \
        {                                    \
            _sw(0x03E00008, _func_);         \
            _sw(0x00001021, _func_ + 4);     \
        }                                    \
        else                                 \
        {                                    \
            _sw(0x03E00008, _func_);         \
            _sw(0x24020000 | r, _func_ + 4); \
        }                                    \
    }

#define REDIRECT_FUNCTION(a, f)                                   \
    {                                                             \
        u32 _func_ = a;                                           \
        _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), _func_); \
        _sw(0, _func_ + 4);                                       \
    }

// takes a function address, the new function and a pointer to the original function
#define HIJACK_FUNCTION(a, f, ptr)                                \
    {                                                             \
        u32 _func_ = a;                                           \
        static u32 patch_buffer[3];                               \
        _sw(_lw(_func_), (u32)patch_buffer);                      \
        _sw(_lw(_func_ + 4), (u32)patch_buffer + 8);              \
        MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8);             \
        _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), _func_); \
        _sw(0, _func_ + 4);                                       \
        ptr = (typeof(ptr))patch_buffer;                             \
    }

#define DEFINE_GAME_FUNCTION(funcname, returntype, ...) \
    typedef returntype (* funcname##_t)(__VA_ARGS__); \
    funcname##_t funcname;
   
#define GAME_FUNCTION(funcname,addr) \
    funcname = (funcname##_t)(addr);



PSP_MODULE_INFO("reversing_prx", 0, 1, 0);
register int gp asm("gp");
u32 mod_base_addr; // this is usually 08804000 for PPSSPP
u32 mod_text_size;
u32 mod_data_size;
bool isVCS{0}, isLCS{0};
const char *loggingfile = "ms0:/log.txt"; // LOGGING FILE FOR DEBUGGING

int pplayer, pcar;

SceCtrlData pad;
int hold_n = 0;
u32 old_buttons = 0, current_buttons = 0, pressed_buttons = 0, hold_buttons = 0, lx, ly, rx, ry;
float xstick, ystick;


template <typename ... Args>
void logPrintf(const char *format, Args ... args)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), format, args ...);
    sceIoWrite(sceIoOpen(loggingfile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777), buffer, strlen(buffer));
    sceIoWrite(sceIoOpen(loggingfile, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777), "\n", 1);
}

void clearICacheFor(u32 instructionAddr) {
	// clearing instruction cache for our changes to take effect
	asm("cache 8, 0($a0)\n");
}
////////////////////////////////////////////////////
/*
DEFINE YOUR PROTOYPE FUNCTIONS FROM THE GAME HERE
IT SHOULD FOLLOW THE FORMAT OF:

typedef returntype (* funcname)(funcargs);
funcname SomeName;

then define it in the appropriate patch section like:
SomeName = (funcname)(base_addr + addroOfFunc);

*/

typedef int (* ButtonsToAction)(void *a1);
ButtonsToAction buttonsToAction;
int buttonsToActionPatched(void *a1);

DEFINE_GAME_FUNCTION(getPPlayer, int);
DEFINE_GAME_FUNCTION(getPCar, int);
DEFINE_GAME_FUNCTION(setWantedLevel, int, int pplayer, int stars);
// VCS ONLY
typedef void (*KillPlayer)(int pplayer);
typedef void (*RequestModel)(int something, int model);
typedef void (*LoadRequestedModels)(bool s);
typedef bool (*IsModelLoaded)(int model);

//////////////////THIS RUNS BASICALLY EVERY FRAME, USE IT TO YOUR WILL//////////////////////////
SceInt64 sceKernelGetSystemTimeWidePatched(void) { //LCS & VCS

  /// calculations for FPS
  SceInt64 cur_micros = sceKernelGetSystemTimeWide();

	
	/// pplayer
	pplayer = getPPlayer();
	  
	/// pcar 
	pcar = getPCar();
    
    if(pressed_buttons & PSP_CTRL_LTRIGGER){
        setWantedLevel(pplayer, 4);
    }

	
	return cur_micros;
}

// EXAMPLE OF A HIJACKED FUNCTION
int buttonsToActionPatched(void *a1) { //LCS & VCS
	int res = buttonsToAction(a1);
	 
	///////////////////////////////////////////
	sceCtrlPeekBufferPositive(&pad, 1);
	
	xstick = (float)(pad.Lx - 128) / (float)128; 
	ystick = (float)(pad.Ly - 128) / (float)128;
			
	old_buttons = current_buttons;
	current_buttons = pad.Buttons;
	pressed_buttons = current_buttons & ~old_buttons;
	hold_buttons = pressed_buttons;

	if (old_buttons & current_buttons) {
		if (hold_n >= 10) {
			hold_buttons = current_buttons;
			hold_n = 8;
		} hold_n++;
	} else hold_n = 0;
	///////////////////////////////////////////

	return res;
}

void patchVCS(u32 base_addr){
    logPrintf("> patching VCS");

  
	/// CRITICAL /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///
	MAKE_CALL(base_addr + 0x002030D4, sceKernelGetSystemTimeWidePatched); //
	HIJACK_FUNCTION(base_addr + 0x0018A288, buttonsToActionPatched, buttonsToAction);
	
	/// DEFINE FUNCTIONS FROM GAME HERE /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///
    GAME_FUNCTION(getPPlayer,base_addr + 0x15C424); 
    GAME_FUNCTION(getPCar,base_addr + 0x0015C2C8);
    GAME_FUNCTION(setWantedLevel,base_addr + 0x00143470);
	// KillPlayer = (void*)(base_addr + 0x0015D4B0);
	// RequestModel = (void*)(0x08ad4040);
	// LoadRequestedModels = (void*)(0x08ad3610);
	// IsModelLoaded = (void*)(0x08ad3698);

}
void patchLCS(u32 base_addr){
    logPrintf("> patching LCS(0x%08X)", base_addr);

	
	/// CRITICAL /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///
	HIJACK_FUNCTION(base_addr + 0x00294E88, buttonsToActionPatched, buttonsToAction); //for button input
	MAKE_CALL(base_addr + 0x002AF398, sceKernelGetSystemTimeWidePatched);


	/// DEFINE FUNCTIONS FROM GAME HERE /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// /// ///
	
	
}

int patch(u32 base_addr) {
	// CREDITS TO FREAKLER FOR FINDING THESE
	if (strcmp((char *)(base_addr + 0x00307F54), "GTA3") == 0) {
		logPrintf("> found ULUS-10041 @ 0x%08X", base_addr + 0x00307F54);
		isLCS = 1;
		patchLCS(base_addr);
    }
	
	if( strcmp((char *)(base_addr + 0x0036F8D8), "GTA3") == 0 ) {
		logPrintf("> found ULUS-10160 @ 0x%08X", base_addr + 0x0036F8D8);
		isVCS = 1;
		patchVCS(base_addr);
    }
	
	return 0;
}


// SETUP THE MODULE AND START
void checkPPSSPPModules()
{
    SceUID modules[10];
    int count = 0;
    if (sceKernelGetModuleIdList(modules, sizeof(modules), &count) >= 0)
    {
        int i;
        SceKernelModuleInfo info;
        for (i = 0; i < count; ++i)
        {
            info.size = sizeof(SceKernelModuleInfo);
            if (sceKernelQueryModuleInfo(modules[i], &info) < 0)
            {
                continue;
            }
            if (strcmp(info.name, "GTA3") == 0){
                logPrintf("[IMPORTANT] PPSSPP has been found.");
                logPrintf("[IMPORTANT] GTA has been found... finding version.");
                mod_base_addr = info.text_addr; // base address, PPSSPP is usually 08804000
                mod_text_size = info.text_size;
                mod_data_size = info.data_size;

                int ret = patch(mod_base_addr);
		        if( ret != 0 ){return;} // Error in patching

                sceKernelDcacheWritebackAll();
                return;
                
            }
        }
    }
}
extern "C" int module_start(SceSize args, void *argp)
{
    sceIoRemove(loggingfile);
    checkPPSSPPModules();
    return 0;
}