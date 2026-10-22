# Changelog

All notable changes to the MTB Track Timer firmware. All nodes (Start / Finish / Split,
Heltec / LILYGO / AIR) share one version number, see `lib/mtb_common/src/mtb_version.h`.

## [12.0.0] – 2026-10-22

Replaces the individual node versions (Start v21, Start LILYGO v24, AIR v1–v6, Finish v20, Split v21).
The LoRa protocol is unchanged – 12.0.0 nodes work together with older nodes.

### Fixed
- **Times were too short by the radio latency.** Finish/Split start their clock when `STX` arrives,
  so every result missed the time from start trigger to `STX` reception (typically 30–60 ms, more
  with rider names), and the configurable compensation was *subtracted* on top. The Start node now
  measures trigger → end of `STX` transmission and adds it; Finish/Split timestamp `STX` in the radio
  interrupt. The setting is now an *added* "extra correction" (default 0, stored under a new key –
  old values are intentionally not carried over).
- **Staggered start:** after the finish was re-armed for the next rider, that rider's time was
  calculated too short. All finish times are now converted using the send time of the last `STX`.
- **Memory corruption in duel/staggered setup:** re-submitting the rider name form (back button,
  reload) wrote past the rider name arrays. The step counter is now bounded; starting a mode without
  setup is refused.
- Web settings accept only valid GPIO pins for plate and extra button; `/settime` accepts only
  plausible timestamps; control characters are removed from rider names; backslashes in names no
  longer break the live view (staggered start).
- Start LILYGO did not fit the default 1.25 MB app partition – now uses *Minimal SPIFFS* (1.9 MB, OTA slots kept).
- Compiler warnings: empty `if` bodies from disabled debug macros, unused variables, format types.

### Changed
- RTT/2 compensation recommendation removed from the web UI (latency is compensated automatically);
  the RTT measurement remains as a link-quality indicator.
- One firmware version for all nodes, shown on the OLED splash, in the boot log and in the web UI.

### Added
- `platformio.ini` for all 9 variants (LILYGO and AIR included), platform pinned to `espressif32 @ 8.0.0`.
- `tools/build_all.sh` – builds all variants.
- Shared header-only library `lib/mtb_common` (formatting, battery curve, protocol parsing,
  latency compensation) used by all variants.
- Native unit tests (`tests/`, Unity) in 64-bit and 32-bit mode.
- GitHub Actions: unit tests and build of all 9 variants on every push, `firmware.bin` as artifacts.

## [11.0] – 2026-05-03

Last state before this changelog (tag `v11.0`): Start/Finish/Split nodes for Heltec and LILYGO,
AIR variant (BMP280), web UI v11, duel, lap and staggered-start modes, SD logging (LILYGO).
