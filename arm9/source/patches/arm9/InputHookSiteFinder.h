#pragma once

/// @brief A keypad read found by \see InputHookSiteFinder.
struct InputHookSite
{
    u32 orrAddress;      // the orr that combines REG_KEYINPUT with the XY mirror
    bool thumb;
    bool displaceBefore; // thumb only: the instruction moved into the stub is before the orr, otherwise after
    u32 xyMirrorAddress; // XY mirror address used by the game, selects the shared memory view
};

/// @brief Finds the keypad reads of the SDK PAD_Read() inline in a block of arm9 code.
/// @details
/// PAD_Read() is inlined at every use and compiles to something like:
///     ldr    rK, =0x04000130     (REG_KEYINPUT)
///     ldr    rX, =0x027FFFA8     (XY mirror, 0x02FFFFA8 on SDK5)
///     ldrh   rA, [rK]
///     ldrh   rB, [rX]
///     orr    rd, rA, rB
/// in ARM or Thumb, with any registers and instruction order. The orr is the hook site.
class InputHookSiteFinder
{
public:
    static constexpr u32 MAX_SITES = 8;

    /// @param start The address of the code.
    /// @param size The size of the code.
    InputHookSiteFinder(u32 start, u32 size)
        : _start(start), _end(start + size) { }

    /// @brief Finds the hook sites.
    /// @param sites Array of at least \see MAX_SITES entries that receives the sites.
    /// @return The number of sites found.
    u32 FindSites(InputHookSite* sites) const;

private:
    u32 _start;
    u32 _end;

    bool IsInside(u32 address, u32 size) const { return address >= _start && address + size <= _end; }
    u16 Read16(u32 address) const { return *(const u16*)address; }
    u32 Read32(u32 address) const { return *(const u32*)address; }

    bool ReadLiteral(u32 address, bool thumb, u32& value, u32& rd) const;
    u32 FindOrr(u32 loadAddress, u32 rk, bool thumb) const;
    u32 FindXyMirror(u32 orrAddress, bool thumb) const;
    bool ThumbSavesLr(u32 orrAddress) const;
    bool CanDisplaceThumb(u32 address) const;
    bool IsThumbBranchTarget(u32 address) const;
    void AddSite(InputHookSite* sites, u32& count, u32 orrAddress, bool thumb) const;
};
