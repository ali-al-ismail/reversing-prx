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
        ptr = (void *)patch_buffer;                               \
    }


u32 mod_text_addr; // module text start address
u32 mod_text_size;
u32 mod_data_size;

PSP_MODULE_INFO("reversing_prx", 0, 1, 0);
static STMOD_HANDLER previous;
register int gp asm("gp");
SceCtrlData pad;
const char *loggingfile = "ms0:/log.txt"; // LOGGING FILE FOR DEBUGGING

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
                logPrintf("[IMPORTANT] PPSSPP has been found.");
                logPrintf("[IMPORTANT] Module name: %s", info.name);
                logPrintf("[IMPORTANT] Module text address: %08X", info.text_addr);
                logPrintf("[IMPORTANT] Module text size: %08X", info.text_size);
                logPrintf("[IMPORTANT] Module data size: %08X", info.data_size);
                mod_text_addr = info.text_addr;
                mod_text_size = info.text_size;
                mod_data_size = info.data_size;

                sceKernelDcacheWritebackAll();
                return;
        }
    }
}
extern "C" int module_start(SceSize args, void *argp)
{
    checkPPSSPPModules();
    return 0;
}