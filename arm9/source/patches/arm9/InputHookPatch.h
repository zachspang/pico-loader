#pragma once
#include "../Patch.h"

/// @brief Arm9 patch that applies the input override from the DSpico to the game's input.
/// @details
/// The game's keypad read ends with an OR that combines the button bits into one register.
/// That OR is replaced with a call to a small block that does the OR and then ANDs the
/// override word from shared memory into the result (a 0 bit forces the button pressed).
///
/// ARM sites: the OR is replaced with a branch.
///     orr    rd, rn, rm
/// becomes
///     b      patch_inputhook_arm_entry
///
/// Thumb sites: a Thumb bl/blx is 4 bytes, so the OR and its neighbour are replaced.
///     ldrh   r0, [r2]
///     orrs   r1, r0
/// becomes
///     blx    patch_inputhook_thumb_entry
/// and the displaced neighbour is run by the hook.
///
/// The site block the hook calls is rewritten per site with the right registers:
///     orr{s} rd, rd, rm              (the game's OR)
///     str    rs, [sp, #-4]!
///     ldr    rs, =&inputOverride
///     ldr    rs, [rs]
///     and{s} rd, rd, rs
///     ldr    rs, [sp], #4
class InputHookPatch : public Patch
{
public:
    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    void ApplyArmHook(u32 hookEntry);
    void ApplyThumbHook(u32 hookEntry);

    u32 _orrAddress = 0;           // address of the OR being replaced, 0 if not found
    bool _thumb = false;
    u32 _orrInstruction = 0;       // expected OR encoding (halfword for Thumb)
    u32 _rd = 0;                   // OR destination register
    u32 _rm = 0;                   // Thumb only: OR source register
    bool _setFlags = false;        // OR is the S form
    bool _displacedBefore = false; // Thumb only: displaced instruction is before the OR
    u16 _displacedInstruction = 0; // Thumb only
};
