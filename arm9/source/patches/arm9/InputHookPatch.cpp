#include "common.h"
#include "ArmHelper.h"
#include "gameCode.h"
#include "sharedMemory.h"
#include "../PatchContext.h"
#include "InputHookPatchCode.h"
#include "InputHookPatch.h"

// orr{s} rd, rn, rm
static u32 makeArmOrr(bool setFlags, u32 rd, u32 rn, u32 rm)
{
    return 0xE1800000 | (setFlags << 20) | (rn << 16) | (rd << 12) | rm;
}

// and{s} rd, rn, rm
static u32 makeArmAnd(bool setFlags, u32 rd, u32 rn, u32 rm)
{
    return 0xE0000000 | (setFlags << 20) | (rn << 16) | (rd << 12) | rm;
}

bool InputHookPatch::FindPatchTarget(PatchContext& patchContext)
{
    _orrAddress = 0;

    switch (patchContext.GetGameCode())
    {
        case GAMECODE("CPUE"): // Pokemon Platinum (USA) Rev 1
        {
            // ldrh r0, [r2]
            // orrs r1, r0
            _orrAddress = 0x02017BC8;
            _thumb = true;
            _orrInstruction = 0x4301;
            _rd = 1;
            _rm = 0;
            _setFlags = true;
            _displacedBefore = true;
            _displacedInstruction = 0x8810;
            break;
        }
        case GAMECODE("AMCE"): // Mario Kart DS (USA)
        {
            // orr r2, r3, r2
            _orrAddress = 0x02043A70;
            _thumb = false;
            _orrInstruction = 0xE1832002;
            _rd = 2;
            _setFlags = false;
            break;
        }
    }

    // make sure the code matches, other revisions have it at a different address
    if (_orrAddress)
    {
        bool match;
        if (_thumb)
        {
            u32 displacedAddress = _displacedBefore ? _orrAddress - 2 : _orrAddress + 2;
            match = *(u16*)_orrAddress == _orrInstruction
                && *(u16*)displacedAddress == _displacedInstruction;
        }
        else
        {
            match = *(u32*)_orrAddress == _orrInstruction;
        }

        if (!match)
        {
            _orrAddress = 0;
        }
    }

    LOG_DEBUG("InputHookPatch: %s\n", _orrAddress ? "hook site matched" : "no hook site matched");
    return true;
}

void InputHookPatch::ApplyPatch(PatchContext& patchContext)
{
    if (!_orrAddress)
        return;

    auto hookCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<InputHookPatchCode>(
        patchContext.GetPatchHeap());

    // nothing pressed until the arm7 publishes the first poll
    NTR_SHARED_MEMORY->inputOverride = 0xFFFFFFFF;

    u32 rd = _rd;
    u32 rs = rd == 0 ? 1 : 0; // scratch register

    u32* block = patch_inputhook_site_block;
    block[0] = _thumb
        ? makeArmOrr(true, rd, rd, _rm) // thumb orrs rd, rm as arm
        : _orrInstruction;
    block[1] = 0xE52D0004 | (rs << 12); // str rs, [sp, #-4]!
    block[2] |= rs << 12; // ldr rs, =&inputOverride (keep the assembled literal offset)
    block[3] = 0xE5900000 | (rs << 16) | (rs << 12); // ldr rs, [rs]
    block[4] = makeArmAnd(_setFlags, rd, rd, rs);
    block[5] = 0xE49D0004 | (rs << 12); // ldr rs, [sp], #4

    if (_thumb)
    {
        ApplyThumbHook(hookCode->GetThumbEntry());
    }
    else
    {
        ApplyArmHook(hookCode->GetArmEntry());
    }
}

void InputHookPatch::ApplyArmHook(u32 hookEntry)
{
    patch_inputhook_return_addr = _orrAddress + 4;
    *(u32*)_orrAddress = ArmHelper::MakeArmBranch(_orrAddress, hookEntry);
}

void InputHookPatch::ApplyThumbHook(u32 hookEntry)
{
    // the hook runs the displaced instruction
    auto displacedSlot = _displacedBefore ? patch_inputhook_thumb_before : patch_inputhook_thumb_after;
    *(u16*)((u32)displacedSlot & ~1) = _displacedInstruction;

    // the 4 byte blx covers the OR and the displaced instruction
    u32 callAddress = _displacedBefore ? _orrAddress - 2 : _orrAddress;
    u32 call = ArmHelper::MakeThumbCall(callAddress, hookEntry); // hookEntry is arm, so blx
    ((u16*)callAddress)[0] = call;
    ((u16*)callAddress)[1] = call >> 16;
}
