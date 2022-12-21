#include "main.h"

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


void patchVCS(u32 base_addr);
void patchLCS(u32 base_addr);
int patch(u32 base_addr);