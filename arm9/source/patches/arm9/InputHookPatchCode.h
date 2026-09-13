#pragma once
#include "sections.h"
#include "../PatchCode.h"

DEFINE_SECTION_SYMBOLS(patch_inputhook);

extern "C" void patch_inputhook_entry(void);
extern u32 patch_inputhook_readSectors_address;

class InputHookPatchCode : public PatchCode
{
public:
    InputHookPatchCode(PatchHeap& patchHeap, const IReadSectorsPatchCode* readSectorsPatchCode, u32 resumeAddress)
        : PatchCode(SECTION_START(patch_inputhook), SECTION_SIZE(patch_inputhook), patchHeap)
    {
        patch_inputhook_readSectors_address = (u32)readSectorsPatchCode->GetReadSectorsFunction();
    }

    const void* GetInputHookFunction() const
    {
        return GetAddressAtTarget((void*)patch_inputhook_entry);
    }
};