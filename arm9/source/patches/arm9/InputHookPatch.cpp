#include "common.h"
#include "../PatchContext.h"
#include "patches/platform/LoaderPlatform.h"
#include "InputHookPatchCode.h"
#include "InputHookPatch.h"

static const u32 sExpectedBytes[] = { 0x88108801u, 0x48334301u };

bool InputHookPatch::FindPatchTarget(PatchContext& patchContext)
{
    _target = (u32*)kPatchTargetAddress;

    u32* actual = (u32*)kPatchTargetAddress;
    if (actual[0] != sExpectedBytes[0] || actual[1] != sExpectedBytes[1])
    {
        LOG_WARNING("InputHookPatch: bytes at target address did not match expected pattern, skipping\n");
        _target = nullptr;
    }

    return true;
}

void InputHookPatch::ApplyPatch(PatchContext& patchContext)
{
    if (!_target)
        return;

    auto patchCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<InputHookPatchCode>(
        patchContext.GetPatchHeap());

    u32 hookAddr = (u32)patchCode->GetInputHookFunction();
    LOG_DEBUG("InputHookPatch allocated at 0x%X\n", hookAddr);

    // Calculate the relative branch distance
    u32 src = (u32)_target;
    s32 diff = (s32)((hookAddr & ~1) - (src + 4));

    // 4-byte Thumb Branch with Link (BL) instruction
    ((u16*)_target)[0] = 0xF000 | ((diff >> 12) & 0x7FF);
    ((u16*)_target)[1] = 0xF800 | ((diff >> 1) & 0x7FF);
}