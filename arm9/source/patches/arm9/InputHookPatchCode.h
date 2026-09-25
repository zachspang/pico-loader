#pragma once
#include "sections.h"
#include "../PatchCode.h"

DEFINE_SECTION_SYMBOLS(patch_inputhook);

extern "C" void patch_inputhook_entry(void);

class InputHookPatchCode : public PatchCode
{
public:
    InputHookPatchCode(PatchHeap& patchHeap)
        : PatchCode(SECTION_START(patch_inputhook), SECTION_SIZE(patch_inputhook), patchHeap)
    {
    }

    const void* GetInputHookFunction() const
    {
        return GetAddressAtTarget((void*)patch_inputhook_entry);
    }
};