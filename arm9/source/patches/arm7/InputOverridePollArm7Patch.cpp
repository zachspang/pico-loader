#include "common.h"
#include <stddef.h>
#include "sharedMemory.h"
#include "patches/PatchContext.h"
#include "InputOverridePollArm7PatchCode.h"
#include "InputOverridePollArm7Patch.h"

static constexpr u32 XY_MIRROR_ADDRESS_NTR = 0x027FFFA8;
static constexpr u32 XY_MIRROR_ADDRESS_SDK5 = 0x02FFFFA8;
static constexpr u32 REG_KEYXY_ADDRESS = 0x04000136;

// offsets in shared memory, the addresses are derived from the mirror address the game uses
static constexpr u32 XY_MIRROR_OFFSET = 0x7A8;
static constexpr u32 INPUT_OVERRIDE_OFFSET = 0x424;
static constexpr u32 ARM7_CARD_BUSY_OFFSET = 0x428;
static constexpr u32 SLOT1_LOCK_OFFSET = 0x7E0;

static_assert(offsetof(shared_memory_ntr_t, inputOverride) == INPUT_OVERRIDE_OFFSET);
static_assert(offsetof(shared_memory_ntr_t, arm7CardBusy) == ARM7_CARD_BUSY_OFFSET);

static constexpr u32 SEARCH_DISTANCE = 16; // instructions

// ldr rd, [pc, #imm]
static bool isArmLdrPcRelative(u32 instruction)
{
    return (instruction & 0xFFFF0000) == 0xE59F0000;
}

// strh rd, [rn]
static bool isArmStrh(u32 instruction, u32 rn)
{
    return (instruction & 0xFFF00FFF) == 0xE1C000B0 && ((instruction >> 16) & 0xF) == rn;
}

// pop {.., lr} without pc
static bool isArmPopLrNoPc(u32 instruction)
{
    return (instruction & 0xFFFF0000) == 0xE8BD0000 && (instruction & (1 << 14)) && !(instruction & (1 << 15));
}

static constexpr u32 ARM_BX_LR = 0xE12FFF1E;

// Reads the literal loaded by an ldr rd, [pc, #imm] at address, if it is inside [begin, end).
static bool readArmLiteral(u32 address, u32 begin, u32 end, u32& value)
{
    u32 instruction = *(u32*)address;
    if (!isArmLdrPcRelative(instruction))
        return false;

    u32 literalAddress = address + 8 + (instruction & 0xFFF);
    if (literalAddress < begin || literalAddress + 4 > end)
        return false;

    value = *(u32*)literalAddress;
    return true;
}

bool InputOverridePollArm7Patch::FindPatchTarget(PatchContext& patchContext)
{
    _site = nullptr;
    const u32 begin = (u32)_arm7Start;
    const u32 end = begin + _arm7Size;
    u32 matches = 0;

    for (u32 address = begin; address + 4 <= end; address += 4)
    {
        // ldr rn, =XY mirror address
        u32 mirror;
        if (!readArmLiteral(address, begin, end, mirror)
            || (mirror != XY_MIRROR_ADDRESS_NTR && mirror != XY_MIRROR_ADDRESS_SDK5))
        {
            continue;
        }
        u32 rn = (*(u32*)address >> 12) & 0xF;

        // strh rX, [rn] shortly after
        u32 strh = 0;
        for (u32 i = 1; i <= SEARCH_DISTANCE && address + i * 4 + 12 <= end; i++)
        {
            if (isArmStrh(*(u32*)(address + i * 4), rn))
            {
                strh = address + i * 4;
                break;
            }
        }
        if (!strh)
            continue;

        // ldr rY, =REG_KEYXY shortly before the strh
        bool loadsKeyXy = false;
        u32 searchStart = strh - begin >= SEARCH_DISTANCE * 4 ? strh - SEARCH_DISTANCE * 4 : begin;
        for (u32 i = searchStart; i < strh; i += 4)
        {
            u32 literal;
            if (readArmLiteral(i, begin, end, literal) && literal == REG_KEYXY_ADDRESS)
            {
                loadsKeyXy = true;
                break;
            }
        }
        if (!loadsKeyXy)
            continue;

        // strh
        // pop {.., lr}
        // bx lr
        if (!isArmPopLrNoPc(*(u32*)(strh + 4)) || *(u32*)(strh + 8) != ARM_BX_LR)
            continue;

        matches++;
        _site = (u32*)strh;
        _overrideAddr = mirror - XY_MIRROR_OFFSET + INPUT_OVERRIDE_OFFSET;
        _cardBusyAddr = mirror - XY_MIRROR_OFFSET + ARM7_CARD_BUSY_OFFSET;
        _lockAddr = mirror - XY_MIRROR_OFFSET + SLOT1_LOCK_OFFSET;
        LOG_DEBUG("InputOverridePollArm7: XY mirror store at 0x%p\n", _site);
    }

    if (matches != 1)
    {
        LOG_WARNING("InputOverridePollArm7: %u candidate sites, skipping\n", matches);
        _site = nullptr;
    }
    return true; // an unsupported game just skips the feature
}

void InputOverridePollArm7Patch::ApplyPatch(PatchContext& patchContext)
{
    if (!_site)
        return;

    auto patchCode = patchContext.GetPatchCodeCollection().AddUniquePatchCode<InputOverridePollArm7PatchCode>(
        patchContext.GetPatchHeap());

    // the patch code runs the two replaced instructions itself
    inputoverridepollarm7_strh = _site[0];
    inputoverridepollarm7_pop = _site[1];
    inputoverridepollarm7_override_addr = _overrideAddr;
    inputoverridepollarm7_lock_addr = _lockAddr;
    inputoverridepollarm7_card_busy_addr = _cardBusyAddr;
    *(u32*)_cardBusyAddr = 0; // no save in progress at boot

    _site[0] = 0xE51FF004; // ldr pc,= entryAddress
    _site[1] = (u32)patchCode->GetEntry();
}
