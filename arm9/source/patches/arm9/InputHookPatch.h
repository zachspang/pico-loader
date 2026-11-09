#pragma once
#include "../Patch.h"
#include "InputHookSiteFinder.h"

/// @brief Arm9 patch that applies the input override from the DSpico to the game's input.
/// @details
/// Every keypad read (the SDK PAD_Read() inline, \see InputHookSiteFinder) combines
/// REG_KEYINPUT with the XY mirror using an orr. Each orr is replaced with a jump to a small
/// stub that does the orr and then ANDs the override word from shared memory into the result
/// (a 0 bit forces the button pressed, 0xFFFFFFFF changes nothing).
///
/// ARM sites: the orr is replaced with a branch to the stub.
///     orr{s} rd, rn, rm              (the game's orr)
///     str    rs, [sp, #-4]!
///     ldr    rs, =&inputOverride
///     ldr    rs, [rs]
///     and{s} rd, rd, rs
///     ldr    rs, [sp], #4
///     b      orr + 4
///
/// Thumb sites: a bl is 4 bytes, so the orr and one neighbour are replaced with a bl to the stub,
/// which runs the neighbour itself. Only used in functions that have saved lr.
///     <neighbour before the orr>
///     orrs   rd, rm                  (the game's orr)
///     push   {rs}
///     ldr    rs, =&inputOverride
///     ldr    rs, [rs]
///     ands   rd, rs
///     pop    {rs}
///     <neighbour after the orr>      (when the one before could not be moved)
///     bx     lr
class InputHookPatch : public Patch
{
public:
    InputHookPatch(const void* arm9Start, u32 arm9Size, bool runInDSiMode)
        : _arm9Start(arm9Start), _arm9Size(arm9Size), _runInDSiMode(runInDSiMode) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    void ApplyArmHook(PatchContext& patchContext, const InputHookSite& site, u32 finalOrrAddress, u32 overrideAddress);
    void ApplyThumbHook(PatchContext& patchContext, const InputHookSite& site, u32 finalOrrAddress, u32 overrideAddress);

    const void* _arm9Start;
    u32 _arm9Size;
    bool _runInDSiMode;
    InputHookSite _sites[InputHookSiteFinder::MAX_SITES];
    u32 _siteCount = 0;
};
