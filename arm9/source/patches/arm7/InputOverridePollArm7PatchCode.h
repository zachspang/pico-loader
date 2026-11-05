#pragma once
#include "sections.h"
#include "patches/PatchCode.h"

DEFINE_SECTION_SYMBOLS(patch_inputoverridepollarm7);

extern "C" void inputoverridepollarm7_entry(void);
extern "C" u32 inputoverridepollarm7_strh;
extern "C" u32 inputoverridepollarm7_pop;
extern "C" u32 inputoverridepollarm7_override_addr;
extern "C" u32 inputoverridepollarm7_lock_addr;
extern "C" u32 inputoverridepollarm7_card_busy_addr;

class InputOverridePollArm7PatchCode : public PatchCode
{
public:
    InputOverridePollArm7PatchCode(PatchHeap& patchHeap)
        : PatchCode(SECTION_START(patch_inputoverridepollarm7), SECTION_SIZE(patch_inputoverridepollarm7), patchHeap)
    {
    }

    const void* GetEntry() const
    {
        return GetAddressAtTarget((void*)inputoverridepollarm7_entry);
    }
};