#include "common.h"
#include <stddef.h>
#include "sharedMemory.h"
#include "ArmHelper.h"
#include "thumbInstructions.h"
#include "../PatchContext.h"
#include "InputHookPatch.h"

static constexpr u32 ARM_STUB_SIZE = 0x20;
static constexpr u32 THUMB_STUB_SIZE = 0x18; // includes 2 bytes to align the literal

bool InputHookPatch::FindPatchTarget(PatchContext& patchContext)
{
    _siteCount = InputHookSiteFinder((u32)_arm9Start, _arm9Size).FindSites(_sites);
    for (u32 i = 0; i < _siteCount; i++)
    {
        LOG_DEBUG("InputHookPatch: %s keypad read at 0x%x\n", _sites[i].thumb ? "thumb" : "arm", _sites[i].orrAddress);
    }

    if (_siteCount == 0)
    {
        LOG_WARNING("InputHookPatch: no keypad reads found\n");
    }

    return true; // a game without sites just has no input override
}

void InputHookPatch::ApplyPatch(PatchContext& patchContext)
{
    auto autoloadAdjuster = patchContext.GetAutoloadAdjuster();
    for (u32 i = 0; i < _siteCount; i++)
    {
        const InputHookSite& site = _sites[i];
        u32 ntrSharedMem = site.xyMirrorAddress - offsetof(shared_memory_ntr_t, padXYBuffer);
        u32 overrideAddress = ntrSharedMem - offsetof(shared_memory_twl_t, ntrSharedMem) + offsetof(shared_memory_twl_t, inputOverride);

        // the address the orr runs at, autoload can move code
        u32 finalOrrAddress = autoloadAdjuster ? autoloadAdjuster->AdjustInitialToFinal(site.orrAddress) : site.orrAddress;

        // nothing pressed until the arm7 publishes the first poll
        // patching runs before the switch to ds mode, which maps the override word to the last 4 MB of main memory
        u32 initialOverrideAddress = _runInDSiMode ? overrideAddress : (0x02000000 | (overrideAddress & 0x3FFFFF));
        *(u32*)initialOverrideAddress = 0xFFFFFFFF;

        if (site.thumb)
        {
            ApplyThumbHook(patchContext, site, finalOrrAddress, overrideAddress);
        }
        else
        {
            ApplyArmHook(patchContext, site, finalOrrAddress, overrideAddress);
        }
    }
}

void InputHookPatch::ApplyArmHook(PatchContext& patchContext, const InputHookSite& site, u32 finalOrrAddress, u32 overrideAddress)
{
    u32* stub = (u32*)patchContext.GetPatchHeap().Alloc(ARM_STUB_SIZE);
    u32* orr = (u32*)site.orrAddress;
    u32 regDest = (*orr >> 12) & 0xF;
    u32 setFlags = *orr & (1 << 20);
    u32 regScratch = regDest == 0 ? 1 : 0;

    stub[0] = *orr;
    stub[1] = 0xE52D0004 | (regScratch << 12); // str regScratch, [sp, #-4]!
    stub[2] = 0xE59F000C | (regScratch << 12); // ldr regScratch, [pc, #0xC] (=stub[7])
    stub[3] = 0xE5900000 | (regScratch << 16) | (regScratch << 12); // ldr regScratch, [regScratch]
    stub[4] = 0xE0000000 | setFlags | (regDest << 16) | (regDest << 12) | regScratch; // and{s} regDest, regDest, regScratch
    stub[5] = 0xE49D0004 | (regScratch << 12); // ldr regScratch, [sp], #4
    stub[6] = ArmHelper::MakeArmBranch((u32)&stub[6], finalOrrAddress + 4);
    stub[7] = overrideAddress;

    *orr = ArmHelper::MakeArmBranch(finalOrrAddress, (u32)stub);
}

void InputHookPatch::ApplyThumbHook(PatchContext& patchContext, const InputHookSite& site, u32 finalOrrAddress, u32 overrideAddress)
{
    u16* stub = (u16*)patchContext.GetPatchHeap().Alloc(THUMB_STUB_SIZE);

    // thumb bl reaches +-4MB
    s32 distance = (s32)((u32)stub - finalOrrAddress);
    if (distance >= 0x400000 - 4 || distance < -0x400000 + 4)
    {
        LOG_WARNING("InputHookPatch: patch heap out of bl range for 0x%x\n", site.orrAddress);
        return;
    }

    u16* orr = (u16*)site.orrAddress;
    u16 displaced = site.displaceBefore ? orr[-1] : orr[1];
    u32 regDest = *orr & 7;
    u32 regScratch = regDest == 0 ? 1 : 0;

    u32 i = 0;
    if (site.displaceBefore)
        stub[i++] = displaced;
    stub[i++] = *orr;
    stub[i++] = 0xB400 | (1 << regScratch); // push {regScratch}
    u32 ldrIndex = i++; // ldr regScratch, =overrideAddress
    stub[i++] = 0x6800 | (regScratch << 3) | regScratch; // ldr regScratch, [regScratch]
    stub[i++] = 0x4000 | (regScratch << 3) | regDest; // ands regDest, regScratch
    stub[i++] = 0xBC00 | (1 << regScratch); // pop {regScratch}
    if (!site.displaceBefore)
        stub[i++] = displaced;
    stub[i++] = THUMB_BX_LR;

    if ((u32)&stub[i] & 2)
        stub[i++] = THUMB_NOP; // aligns the literal
    u32 ldrPc = ((u32)&stub[ldrIndex] + 4) & ~3;
    stub[ldrIndex] = THUMB_LDR_PC_IMM(regScratch, (u32)&stub[i] - ldrPc);
    stub[i++] = overrideAddress & 0xFFFF;
    stub[i++] = overrideAddress >> 16;

    u32 bl = ArmHelper::MakeThumbCall(site.displaceBefore ? finalOrrAddress - 2 : finalOrrAddress, (u32)stub | 1);
    u16* call = site.displaceBefore ? orr - 1 : orr;
    call[0] = bl;
    call[1] = bl >> 16;
}
