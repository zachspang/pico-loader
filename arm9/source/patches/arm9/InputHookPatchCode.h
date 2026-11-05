#pragma once
#include "sections.h"
#include "../PatchCode.h"

DEFINE_SECTION_SYMBOLS(patch_inputhook);

extern "C" void patch_inputhook_arm_entry(void);
extern "C" void patch_inputhook_thumb_entry(void);
extern "C" void patch_inputhook_thumb_before(void);
extern "C" void patch_inputhook_thumb_after(void);
extern "C" u32 patch_inputhook_site_block[];
extern "C" u32 patch_inputhook_return_addr;

class InputHookPatchCode : public PatchCode
{
public:
    InputHookPatchCode(PatchHeap& patchHeap)
        : PatchCode(SECTION_START(patch_inputhook), SECTION_SIZE(patch_inputhook), patchHeap)
    {
    }

    u32 GetArmEntry() const { return (u32)GetAddressAtTarget((void*)patch_inputhook_arm_entry) & ~1u; }
    u32 GetThumbEntry() const { return (u32)GetAddressAtTarget((void*)patch_inputhook_thumb_entry) & ~1u; }
};