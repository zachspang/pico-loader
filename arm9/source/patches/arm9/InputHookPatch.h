#pragma once
#include "../Patch.h"

/// @brief Arm9 patch to redirect input reads to an external override source.
class InputHookPatch : public Patch
{
public:
    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    // TODO: make work for any game
    static constexpr u32 kPatchTargetAddress = 0x02017bc2;

    u32* _target = nullptr;
};