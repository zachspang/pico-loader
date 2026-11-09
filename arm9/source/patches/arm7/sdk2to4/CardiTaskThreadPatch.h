#pragma once
#include "patches/Patch.h"

/// @brief Arm7 patch for redirecting save reads and writes on SDK 2-4.
class CardiTaskThreadPatch : public Patch
{
public:
    enum class PatchVariant : u16
    {
        None,
        ArmA,
        ArmB,
        ArmC,
        ArmD,
        ArmE,
        ArmF,
        ThumbA,
        ThumbB,
        ThumbC,
        ThumbD,
        ThumbE,
        ThumbF
    };

    CardiTaskThreadPatch(void* saveTmpBuffer, u32* arm7CardBusy)
        : _saveTmpBuffer(saveTmpBuffer), _arm7CardBusy(arm7CardBusy) { }

    bool FindPatchTarget(PatchContext& patchContext) override;
    void ApplyPatch(PatchContext& patchContext) override;

private:
    u32* _cardiTaskThread = nullptr;
    void* _saveTmpBuffer;
    u32* _arm7CardBusy; // non zero while the save patch sends card commands
    PatchVariant _patchVariant = PatchVariant::None;

    void ApplyArmPatch(void* patch1Address) const;
    void ApplyThumbPatch(void* patch1Address) const;
};