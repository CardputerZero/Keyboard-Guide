#include "input/modifier_state_parser.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace {

void expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

void testDeviceExample()
{
    keyboard_guide::ModifierStateSnapshot snapshot;
    std::string error;
    expect(keyboard_guide::parseModifierStateSnapshot(
               "version=1 sequence=53 changed_mask=0x00 reason=2 sym=0 shift=0 fn=3 ctrl=0 alt=", snapshot, error),
           "the device example must parse, including an empty optional alt field");
    expect(snapshot.version == 1 && snapshot.sequence == 53, "version and sequence must be preserved");
    expect(snapshot.raw_modes[0] == 0 && snapshot.raw_modes[1] == 0 && snapshot.raw_modes[2] == 3,
           "sysfs fields must be reordered to Shift, Sym, Fn");
    expect(snapshot.has_changed_mask && snapshot.changed_mask == 0, "changed_mask must parse as hexadecimal");
    expect(snapshot.has_reason && snapshot.reason == 2, "reason must be available for diagnostics");
}

void testFieldOrderAndExtensions()
{
    keyboard_guide::ModifierStateSnapshot snapshot;
    std::string error;
    expect(keyboard_guide::parseModifierStateSnapshot(
               "future=x fn=4 shift=1 version=1 sym=2 sequence=18446744073709551615 ctrl=1", snapshot, error),
           "field order and unknown extension fields must not affect parsing");
    expect(snapshot.sequence == std::numeric_limits<uint64_t>::max(),
           "the full 64-bit sequence range must be accepted");
    expect(snapshot.raw_modes[0] == 1 && snapshot.raw_modes[1] == 2 && snapshot.raw_modes[2] == 4,
           "all modifier modes must be preserved");

    expect(
        keyboard_guide::parseModifierStateSnapshot("version=1 sequence=4294967296 sym=0 shift=0 fn=0", snapshot, error),
        "sequence values beyond the 32-bit range must be accepted");
    expect(snapshot.sequence == 0x100000000ULL, "sequence must not be truncated to 32 bits");
}

void testInvalidSnapshots()
{
    keyboard_guide::ModifierStateSnapshot snapshot;
    std::string error;
    expect(!keyboard_guide::parseModifierStateSnapshot("version=2 sequence=1 sym=0 shift=0 fn=0", snapshot, error),
           "unsupported ABI versions must be rejected");
    expect(!keyboard_guide::parseModifierStateSnapshot("version=1 sequence=1 sym=0 shift=0", snapshot, error),
           "missing required fields must be rejected");
    expect(!keyboard_guide::parseModifierStateSnapshot("version=1 sequence=1 sym=0 shift=0 fn=5", snapshot, error),
           "out-of-range modes must be rejected");
    expect(!keyboard_guide::parseModifierStateSnapshot("version=1 sequence=x sym=0 shift=0 fn=0", snapshot, error),
           "malformed required values must be rejected");
    expect(
        !keyboard_guide::parseModifierStateSnapshot("version=1 sequence=1 sym=0 shift=0 shift=1 fn=0", snapshot, error),
        "duplicate required fields must be rejected");
}

void testModeMappings()
{
    keyboard_guide::GuideModifierMode mode = keyboard_guide::GuideModifierMode::Inactive;
    expect(keyboard_guide::modifierModeForSysfsRaw(0, mode) && mode == keyboard_guide::GuideModifierMode::Inactive,
           "sysfs OFF must map to Inactive");
    expect(keyboard_guide::modifierModeForSysfsRaw(1, mode) && mode == keyboard_guide::GuideModifierMode::Held,
           "sysfs PRESSED must map to the physical-down mode");
    expect(keyboard_guide::modifierModeForSysfsRaw(2, mode) && mode == keyboard_guide::GuideModifierMode::OneShot,
           "sysfs ONESHOT must map explicitly");
    expect(keyboard_guide::modifierModeForSysfsRaw(3, mode) && mode == keyboard_guide::GuideModifierMode::Locked,
           "sysfs LOCKED must map explicitly");
    expect(keyboard_guide::modifierModeForSysfsRaw(4, mode) && mode == keyboard_guide::GuideModifierMode::Held,
           "sysfs HELD must map to the physical-down mode");
    expect(!keyboard_guide::modifierModeForSysfsRaw(5, mode), "unknown sysfs modes must be rejected");

    expect(keyboard_guide::modifierModeForLegacyRaw(1, mode) && mode == keyboard_guide::GuideModifierMode::OneShot,
           "legacy mode 1 must remain OneShot");
    expect(keyboard_guide::modifierModeForLegacyRaw(2, mode) && mode == keyboard_guide::GuideModifierMode::Locked,
           "legacy mode 2 must remain Locked");
    expect(keyboard_guide::modifierModeForLegacyRaw(3, mode) && mode == keyboard_guide::GuideModifierMode::Held,
           "legacy mode 3 must remain Held");
}

}  // namespace

int main()
{
    testDeviceExample();
    testFieldOrderAndExtensions();
    testInvalidSnapshots();
    testModeMappings();
    return 0;
}
