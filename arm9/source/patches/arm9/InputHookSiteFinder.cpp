#include "common.h"
#include "sharedMemory.h"
#include "thumbInstructions.h"
#include "InputHookSiteFinder.h"

static constexpr u32 REG_KEYINPUT_ADDRESS = 0x04000130;

static constexpr u32 LDRH_DISTANCE = 20; // instructions from the REG_KEYINPUT load to the ldrh
static constexpr u32 ORR_DISTANCE = 8; // instructions from the ldrh to the orr
static constexpr u32 XY_DISTANCE = 24; // instructions around the orr to look for the XY mirror load
static constexpr u32 LR_DISTANCE = 64; // instructions after a thumb orr to look for a call or return

// Reads the literal loaded by a pc relative ldr at address.
bool InputHookSiteFinder::ReadLiteral(u32 address, bool thumb, u32& value, u32& rd) const
{
    u32 literalAddress;
    if (thumb)
    {
        u16 instruction = Read16(address);
        if ((instruction & 0xF800) != 0x4800) // ldr rd, [pc, #imm]
            return false;
        rd = (instruction >> 8) & 7;
        literalAddress = ((address + 4) & ~3) + (instruction & 0xFF) * 4;
    }
    else
    {
        u32 instruction = Read32(address);
        if ((instruction & 0xFF7F0000) != 0xE51F0000) // ldr rd, [pc, #+-imm]
            return false;
        rd = (instruction >> 12) & 0xF;
        u32 imm = instruction & 0xFFF;
        literalAddress = (instruction & (1 << 23)) ? address + 8 + imm : address + 8 - imm;
    }

    if ((literalAddress & 3) || !IsInside(literalAddress, 4))
        return false;

    value = Read32(literalAddress);
    return true;
}

// Finds "ldrh rA, [rK]" after the REG_KEYINPUT load and then the orr that uses rA.
u32 InputHookSiteFinder::FindOrr(u32 loadAddress, u32 rk, bool thumb) const
{
    u32 step = thumb ? 2 : 4;
    u32 ldrh = loadAddress + step;
    for (u32 i = 0; i < LDRH_DISTANCE && IsInside(ldrh, 4); i++, ldrh += step)
    {
        u32 ra;
        if (thumb)
        {
            u16 instruction = Read16(ldrh);
            if ((instruction & 0xFFC0) != 0x8800 || ((instruction >> 3) & 7) != rk) // ldrh ra, [rk]
                continue;
            ra = instruction & 7;
        }
        else
        {
            u32 instruction = Read32(ldrh);
            if ((instruction & 0xFFF00FFF) != 0xE1D000B0 || ((instruction >> 16) & 0xF) != rk) // ldrh ra, [rk]
                continue;
            ra = (instruction >> 12) & 0xF;
        }

        u32 orr = ldrh + step;
        for (u32 j = 0; j < ORR_DISTANCE && IsInside(orr, 4); j++, orr += step)
        {
            if (thumb)
            {
                u16 instruction = Read16(orr);
                if ((instruction & 0xFFC0) == 0x4300 && ((instruction & 7) == ra || ((instruction >> 3) & 7) == ra)) // orrs rd, rm
                    return orr;
            }
            else
            {
                u32 instruction = Read32(orr);
                u32 rd = (instruction >> 12) & 0xF;
                u32 rn = (instruction >> 16) & 0xF;
                u32 rm = instruction & 0xF;
                if ((instruction & 0xFFE00FF0) == 0xE1800000 // orr{s} rd, rn, rm
                    && (rn == ra || rm == ra) && rd != 15 && rn != 15 && rm != 15)
                    return orr;
            }
        }
    }
    return 0;
}

// Finds the XY mirror address loaded around the orr.
u32 InputHookSiteFinder::FindXyMirror(u32 orrAddress, bool thumb) const
{
    u32 step = thumb ? 2 : 4;
    u32 address = orrAddress - XY_DISTANCE * step;
    for (u32 i = 0; i < XY_DISTANCE * 2; i++, address += step)
    {
        u32 value, rd;
        if (IsInside(address, step) && ReadLiteral(address, thumb, value, rd)
            && (value == (u32)&NTR_SHARED_MEMORY->padXYBuffer || value == (u32)&NTR_SHARED_MEMORY_SDK5->padXYBuffer))
            return value;
    }
    return 0;
}

// Checks that the function saved lr, which the bl to the stub overwrites.
// A call or a return with pop {.., pc} comes before any bx lr in that case.
bool InputHookSiteFinder::ThumbSavesLr(u32 orrAddress) const
{
    u32 address = orrAddress + 2;
    for (u32 i = 0; i < LR_DISTANCE && IsInside(address, 2); i++, address += 2)
    {
        u16 instruction = Read16(address);
        if (instruction == THUMB_BX_LR)
            return false;
        if ((instruction & 0xF800) == 0xF000 // bl, blx
            || (instruction & 0xFF00) == 0xBD00 // pop {.., pc}
            || (instruction & 0xFF87) == 0x4700) // bx rX after pop {rX}
            return true;
    }
    return false;
}

// Only instructions that use r0-r7 and are not pc or sp relative can run in the stub.
bool InputHookSiteFinder::CanDisplaceThumb(u32 address) const
{
    if (!IsInside(address, 2))
        return false;

    u16 instruction = Read16(address);
    return instruction < 0x4400 // shifts, add/sub, mov/cmp/add/sub imm8, alu
        || (instruction >= 0x5000 && instruction < 0x9000) // load/store register and immediate offset
        || (instruction >= 0xC000 && instruction < 0xD000); // ldmia, stmia
}

// Checks if a nearby b or bcc targets the address.
bool InputHookSiteFinder::IsThumbBranchTarget(u32 address) const
{
    for (u32 branch = address - 2048; branch < address + 2048; branch += 2)
    {
        if (!IsInside(branch, 2))
            continue;

        u16 instruction = Read16(branch);
        u32 target;
        if ((instruction & 0xF800) == 0xE000) // b
            target = branch + 4 + ((s32)((u32)instruction << 21) >> 20);
        else if ((instruction & 0xF000) == 0xD000 && (instruction & 0x0F00) < 0x0E00) // bcc
            target = branch + 4 + ((s32)(s8)(instruction & 0xFF) << 1);
        else
            continue;

        if (target == address)
            return true;
    }
    return false;
}

// Adds the orr to the sites if it can be hooked.
void InputHookSiteFinder::AddSite(InputHookSite* sites, u32& count, u32 orrAddress, bool thumb) const
{
    for (u32 i = 0; i < count; i++)
    {
        if (sites[i].orrAddress == orrAddress)
            return;
    }

    if (count >= MAX_SITES)
        return;

    InputHookSite site = { orrAddress, thumb, false, FindXyMirror(orrAddress, thumb) };
    if (!site.xyMirrorAddress)
        return;

    if (thumb)
    {
        if (!ThumbSavesLr(orrAddress))
            return;

        // the second halfword of the bl can not be a branch target
        if (CanDisplaceThumb(orrAddress - 2) && !IsThumbBranchTarget(orrAddress))
            site.displaceBefore = true;
        else if (!CanDisplaceThumb(orrAddress + 2) || IsThumbBranchTarget(orrAddress + 2))
            return;
    }

    sites[count++] = site;
}

u32 InputHookSiteFinder::FindSites(InputHookSite* sites) const
{
    u32 count = 0;
    for (u32 pass = 0; pass < 2; pass++)
    {
        bool thumb = pass == 1;
        u32 step = thumb ? 2 : 4;
        for (u32 address = (_start + step - 1) & ~(step - 1); IsInside(address, step); address += step)
        {
            u32 value, rk;
            if (!ReadLiteral(address, thumb, value, rk) || value != REG_KEYINPUT_ADDRESS)
                continue;

            u32 orr = FindOrr(address, rk, thumb);
            if (orr)
                AddSite(sites, count, orr, thumb);
        }
    }
    return count;
}
