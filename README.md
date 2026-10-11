# doom-esp32c6

Shareware **Doom, episode 1 (E1M1–E1M9)** running on a **Seeed XIAO ESP32-C6** with a **128×64 monochrome I²C OLED (SH1106)**, played with an **Xbox Series X|S controller over Bluetooth LE**.

No PSRAM, 512 KB of SRAM shared with the Bluetooth stack, 4 MB of flash: the engine is a heavily slimmed-down [doomgeneric](https://github.com/ozkl/doomgeneric) and the game data is read straight from flash.

- 1-bit video: palette → luma → histogram equalisation (temporally smoothed) → 4×4 Bayer dithering → SH1106 pages
- I²C at 1 MHz, about 50 display updates per second (the game logic itself runs at the original 35 tics/s)
- Boot shows *"Turn on your Xbox controller and hold the Sync button…"*; once the pad connects, E1M1 starts
- God mode is always on, difficulty is "Hey, not too rough" (skill 2), no sound or music, no menu/HUD
- After E1M8 an "episode done" screen is shown and the game restarts from E1M1

> **Note on content:** this repository contains only source code. The game data (`doom1.wad`) is **not** included. See [Preparing the WAD](#preparing-the-wad).

## Hardware

| Part | Notes |
|---|---|
| Seeed XIAO ESP32-C6 | 4 MB flash |
| 128×64 OLED, I²C, address `0x3C` | **SH1106** controller (the common "1.3 inch" modules). A real SSD1306 works with `COL_OFFSET 0` in `doom_c6/platform/oled_c6.cpp` |
| Xbox Wireless Controller | Series X\|S (model 1914) or any Xbox pad with Bluetooth LE firmware. Older pads that only speak Classic Bluetooth cannot work: the ESP32-C6 is BLE-only |

Wiring: OLED `SDA` → **D4 / GPIO22**, `SCL` → **D5 / GPIO23**, plus 3V3 and GND.

## Controls

| Pad | Action |
|---|---|
| Left stick | Move / strafe (analog; full deflection = run) |
| Right stick X | Turn (quadratic curve for fine aiming) |
| RT or B | Fire |
| A or X | Use / open |
| LB / RB | Previous / next weapon |

Pairing: hold the **Sync** button on top of the pad until the Xbox logo flashes quickly. The bond is stored in NVS, so afterwards the plain Xbox button is enough to reconnect. If the pad loses the connection, play continues (you are immortal) and the board reconnects in the background.

## Building and running on the ESP32-C6

Requirements: [`arduino-cli`](https://arduino.github.io/arduino-cli/) with the Espressif core **3.3.7**, Python 3, and `doom1.wad` (see below).

```sh
# one-time setup
arduino-cli core install esp32:esp32@3.3.7

# 1) Build the game data (see "Preparing the WAD")
python3 tools/wadtool.py episode /path/to/doom1.wad wad/ep/doom1.wad

# 2) Build and upload the firmware (copies the engine into the sketch first)
tools/sync_engine.sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6:CDCOnBoot=cdc doom_c6
arduino-cli upload  -p /dev/ttyACM0 --fqbn esp32:esp32:XIAO_ESP32C6:CDCOnBoot=cdc doom_c6

# 3) Flash the WAD into its own partition (offset from doom_c6/partitions.csv)
tools/flash_wad.sh wad/ep/doom1.wad 0x150000 /dev/ttyACM0
```

Adjust the serial port as needed. `tools/flash_wad.sh` uses the `esptool` bundled with the Arduino ESP32 core. Changing `doom_c6/build_opt.h` requires `arduino-cli compile --clean`.

Flash layout (`doom_c6/partitions.csv`): `nvs` 20 KB, `otadata`, `app0` 1.25 MB at `0x10000`, and a raw data partition `wad` (type `data`, subtype `0x40`) of 2.69 MB at `0x150000` that the firmware memory-maps.

Serial monitor (115200) prints free heap, the zone size chosen at boot, display fps and I²C error counts every 2 seconds. `tools/serial_log.py SECONDS` is a tiny reader that survives the USB re-enumeration after an upload.

### Display test

`oled_test/oled_test.ino` is a standalone sketch that draws a bouncing square: a quick way to verify wiring and the controller type (SH1106 vs SSD1306) before running Doom.

### BLE test

`ble_pad/` is a small sketch that pairs with the Xbox pad and prints the HID report map and raw reports. Useful when porting to another gamepad.

## Preparing the WAD

You need the shareware `doom1.wad` (v1.9, 4 196 020 bytes, MD5 `f0cefca49926d00903cf57551d901abe`). id Software released it as freely distributable shareware; get it from an official or archive source such as the [Doom Wiki](https://doomwiki.org/wiki/DOOM1.WAD). It is not part of this repository.

`tools/wadtool.py` repacks it for the device:

```sh
python3 tools/wadtool.py info    doom1.wad                              # what is in the WAD
python3 tools/wadtool.py episode doom1.wad wad/ep/doom1.wad             # game WAD, ~2.7 MB
```

`episode` keeps all maps, textures, flats, sprites, palette and colormap tables, and replaces everything else (sounds, music, menu/HUD/intermission graphics, demos) with a single shared 1×1 stub patch. The directory is kept intact so all name lookups still resolve and every lump stays 4-byte aligned for direct flash reads. The output is about 2.7 MB instead of 4.2 MB.

Older mode, used for the original autoplay-demo version: `wadtool.py pack` strips the WAD down to the lumps touched by a recorded lump-access trace (`-DDOOM_LUMPTRACE`).

## Host (PC) build for testing

`host/` contains a PC backend that dumps frames as PPM files and runs the same engine, which makes debugging much faster than on the board.

```sh
make -C host mono                        # 64-bit build, 128x64 1-bit output
make -C host mono32 ZIG=<venv>/bin/python  # 32-bit build via `python -m ziglang cc` (realistic memory numbers)
make -C host mono32reg ZIG=...           # regression build (keeps health items, demo trace)

DG_DUMP_DIR=frames DG_DUMP_EVERY=35 host/doommono -nogui -iwad wad/ep/doom1.wad -warp 1 1 -skill 2
```

Handy test hooks (host only): `DOOM_FAST=1` (virtual clock), `DOOM_BOT=<seed>` (random-walking bot), `DOOM_EXIT_EVERY=<tics>` (force level exits to cycle through all maps), and, with `EXTRA_CFLAGS="-DDOOM_ZSTATS"`, `DOOM_STATS_TIC=<n>` to print zone usage by allocation site.

`tools/zonesweep.sh` finds the smallest zone size at which the three original demos still reproduce bit-identical state hashes (`host/reference_trace.txt`), which is the regression test for the memory work below.

## How memory was squeezed in

Free internal RAM on the XIAO ESP32-C6 is about 443 KB with an empty Arduino sketch. NimBLE (plus its code in IRAM) takes roughly 85 KB, Doom's own statics and the 16 KB task stack roughly 100 KB, which leaves a zone of about 220 KB for Doom's heap. Stock Doom wants several megabytes. The biggest map, E1M6, initially needed 355 KB of zone and now peaks at about 210 KB. All engine changes are behind `#ifdef DG_MONO`.

- **No WAD in RAM.** The WAD sits in a flash partition that is mapped into the address space (`esp_partition_mmap`). `W_CacheLumpNum` returns pointers into flash; `Z_Free`/`Z_ChangeTag` ignore non-zone pointers. Patches, flats, sprites, the blockmap, the reject table and even the map's `SEGS` and `NODES` lumps are used in place.
- **WAD directory in flash.** `lumpinfo[]` *is* the on-disk directory; the name hash uses 16-bit indices (4 bytes per lump instead of 24+).
- **Segs and BSP nodes straight from flash.** `seg_t` and `node_t` are the 12/28-byte WAD records; derived fields (vertices, sidedef, sectors, angle, offset) are computed by small accessor macros. This alone saves about 80 KB on E1M6.
- **Smaller map structures.** `line_t` drops `dx`, `dy` and `bbox` (recomputed from the vertices), `slopetype` is a byte, `states[]`/`mobjinfo[]` are `const` (flash).
- **Textures composed lazily.** No composite textures or column tables are built; columns are composed on demand into a four-slot cache.
- **Shrunk renderer arrays** (visplanes, openings, drawsegs, vissprites, `viewangletox`, node bounding boxes), each sized from measured peaks over bot runs on all nine maps, with graceful overflow handling instead of `I_Error` (a missing plane or clip entry causes a tiny glitch rather than a crash).
- **Everything not needed is gone:** sound (a stub), music, menu, status bar, HUD fonts, intermission, finale, wipe, savegames, networking buffers (`BACKUPTICS` 16), and the 320×200 and 4 extra screen buffers (the engine renders natively at 128×64).
- **Health and armor pickups are not spawned** (`-DDG_KEEP_HEALTH_ITEMS` restores them). With permanent god mode they have no effect, and each map object costs ~180 bytes: this removes about 135 objects (25 KB) from E1M6.
- **Zone sized at boot.** After BLE is up, the sketch takes the largest free heap block minus a small margin as Doom's zone (`-kb`), so the allocation adapts to whatever the Bluetooth stack left over.

Typical flow for tuning: measure with `DOOM_ZSTATS` on the 32-bit host build, change one thing, and check `tools/zonesweep.sh` still reports matching demo hashes.

## Repository layout

| Path | Contents |
|---|---|
| `doom_c6/` | Arduino sketch, partition table, ESP32 platform layer (`platform/`: time, flash-mapped WAD, input, OLED driver, BLE pad) |
| `third_party/doomgeneric/` | Patched copy of the doomgeneric engine (GPL-2), see `third_party/UPSTREAM.md` |
| `host/` | PC backend and Makefile |
| `tools/` | WAD packer, engine sync, flashing, serial helpers, memory/size measurement scripts |
| `ble_pad/`, `oled_test/` | Stand-alone test sketches (`ble_pad` reuses `doom_c6/platform` files through symlinks) |

## License

The engine is derived from Doom (id Software) via [doomgeneric](https://github.com/ozkl/doomgeneric) and is released under the **GNU GPL v2**; see `LICENSE`. The Doom shareware WAD remains the property of id Software and is not distributed here.
