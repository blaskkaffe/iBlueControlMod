# BlueRetro for the iBlueControlMod (Dreamcast)

Open-source firmware for the **iBlueControlMod**, the internal Bluetooth controller adapter for the SEGA Dreamcast sold on AliExpress and elsewhere. It's a fork of [BlueRetro](https://github.com/darthcloud/BlueRetro) by Jacques Gagnon (darthcloud), adapted to this board's wiring.

The board runs a modified BlueRetro v1.9 from the factory, but its pins differ from official BlueRetro hardware. Flashing a stock BlueRetro image misdrives the button, LEDs and port detection, and can make the board look bricked. This fork maps every pin the way the vendor firmware does, so it's safe to flash on this hardware. It's based on the final upstream BlueRetro, which brings newer controller support and fixes.

> **You are on `development`.** It has everything in `main` plus:
> - an openMenu control channel over the controller port
> - remappable button gestures
> - an LED colour you can set yourself
>
> These features have only been tested in an emulator so far. For a build tested on real hardware, use **`main`**.

## What's different from the factory firmware

| | Factory (vendor) firmware | This fork (`main`) |
|---|---|---|
| BlueRetro base | v1.9 (Oct 2023) | Final upstream (Dec 2025): newer controllers, fixes and config options |
| LED colour | Fixed | Orange, blue, or **any custom colour**, set with the button or from Dreamcast software |
| Control from the Dreamcast | None | **openMenu or any homebrew** can read status, change every BlueRetro setting, set the LED, start pairing and take over the button (see below) |
| RGB status LED | Blue, or blinking green/red | **Solid orange** (like the original Dreamcast power LED) or **blue**, your choice |
| Pairing blink | Port LEDs blink forever until a controller connects | Pairing runs for **60 s** after the console starts, then stops. A **tap** on the button opens it again |
| PS4/PS5 light bar | BlueRetro default colours | Follows the Dreamcast menu colours: **A orange, B green, C blue, D magenta** |
| Already-paired controllers | Reconnect | Reconnect at any time, even outside the pairing window |
| Web config / OTA on blueretro.io | Works | Works |

Everything else behaves like normal BlueRetro on the Dreamcast: controllers, rumble, VMU emulation and the blueretro.io web config.

## Button (or TTP223 touch sensor)

| Action | What happens |
|---|---|
| **Tap** (under 3 s) | Opens the 60 s pairing window |
| **Tap, then quickly tap again** | Next LED colour: orange → blue → custom (custom appears once you've saved one) |
| **Tap, then tap and hold** | Colour adjust: the LED cycles through all colours (one lap every 10 s). Let go when it looks right; that colour is saved as your custom colour |
| **Hold 3–6 s** | Stops pairing if it's running, otherwise disconnects all controllers |
| **Hold 6–10 s** | Starts pairing |
| **Hold 10–30 s** | Factory reset: erases settings, pairings and VMU saves (the LED colour is kept) |
| **Hold 30 s or more** | The same reset, then **boots back into the factory vendor firmware** |

While you hold the button, the LED blinks faster at each stage (2, 4, then 8 times a second), so you can see what will happen when you let go.

Dreamcast software can change what each gesture does, or set one to do nothing on the adapter so the software handles it itself (see below). The 30 s hold can't be remapped.

The 30 s hold is your safety net: if a firmware update ever misbehaves, it takes you back to the vendor firmware still stored on the board.

## Control from openMenu / Dreamcast software

The adapter answers private commands on the Maple bus, the same wires the controllers use, so no extra wiring is needed. A small KallistiOS library in [`dc/brctl/`](dc/brctl/README.md) lets Dreamcast software:

- **Read status:** firmware version, which port has which Bluetooth controller, pairing state, LED colour, and button presses as they happen.
- **Change settings:** read and write the full BlueRetro config, the same one blueretro.io edits (controller mappings, VMU/rumble per port, and so on).
- **Set the LED:** any RGB colour, a preset, or saving a colour as the custom one.
- **Control Bluetooth:** start or stop pairing, or disconnect all controllers.
- **Remap the button:** choose an action for each gesture, or "report only", so openMenu can use the button for its own functions.
- **Restart** the adapter or **reset** its config.

```c
#include "brctl.h"
int port = brctl_find();                                  /* -1 if no adapter answers */
brctl_led_rgb(port, 0xFF, 0x16, 0x00, 1);                 /* set + save custom colour */
brctl_btn_set(port, BRCTL_GEST_DOUBLE_TAP, BRCTL_ACT_NONE, 1); /* openMenu owns double tap */
```

The channel only appears on a port where a Bluetooth controller is connected (BlueRetro only presents a device there). Wire protocol: [`main/iblue/brctl_proto.h`](main/iblue/brctl_proto.h). How the fork is organised, profiles and per-game settings: [docs/iblue-extras.md](docs/iblue-extras.md). Test program: [`dc/brctl/example`](dc/brctl/example).

## Installing

### Over Bluetooth (recommended, no wiring)

1. Download **[`firmware/BlueRetro_iBlueControlMod_RGB_v1.bin`](firmware/BlueRetro_iBlueControlMod_RGB_v1.bin)**. This is the build tested on hardware; see [firmware/](firmware/README.md). You can also build it yourself (below).
2. Open the BlueRetro web config on [blueretro.io](https://blueretro.io) in Chrome or Edge, and connect to the adapter.
3. Use the **OTA update** page to upload the `.bin`.

OTA only writes a spare app slot; the vendor firmware and bootloader are never touched. To go back, hold the button for 30 s.

> If blueretro.io says **"Hardware and firmware mismatch"**, the image was built with the `hw2` tag. Rebuild with `BR_HW=` empty (see below). The vendor firmware isn't tagged `hw2`, so the OTA page refuses tagged images.

### Over the debug header (UART)

The board has an unpopulated 6-pin header next to the antenna. Pin 6, the square pad, is GND:

| Pin | Signal |
|---|---|
| 1 | 3V3 |
| 2 | GPIO0 (pull to GND for download mode) |
| 3 | EN (reset) |
| 4 | RXD0 (GPIO3) |
| 5 | TXD0 (GPIO1) |
| 6 | GND (square pad) |

Connect a 3.3 V USB-serial adapter: its TX to pin 4 and its RX to pin 5. **Back up the whole flash first**:

```sh
esptool.py -p PORT -b 921600 read_flash 0 0x800000 iblue_full_backup.bin
```

Don't use `idf.py flash`, because it overwrites the bootloader and partition table. See [docs/README_iBlueControlMod.md](docs/README_iBlueControlMod.md#flashing-without-bricking) for the safe way to write only the app.

## Building

Use the same ESP-IDF that upstream BlueRetro pins: `030c995` (v5.5-dev).

```sh
cp configs/hw2/dreamcast_iblue sdkconfig
BR_HW= BR_SYS=_dreamcast_iblue idf.py reconfigure build
# Output: build/BlueRetro.bin  (leave BR_HW empty, see the OTA note above)
```

| Build config | Use |
|---|---|
| `configs/hw2/dreamcast_iblue` | Normal build: RGB LED colours, pairing window, menu light-bar colours |
| `configs/hw2/dreamcast_iblue_vendor` | Same pins, but the LED, pairing and light-bar behaviour of the factory firmware (no control channel) |

| Kconfig option | Default | Meaning |
|---|---|---|
| `CONFIG_BLUERETRO_HW_IBLUECONTROLMOD` | y | The iBlueControlMod pin map |
| `CONFIG_BLUERETRO_IBLUE_RGB_LED` | y | RGB LED colour driver (R = GPIO25, G = GPIO14, B = GPIO13, common anode) |
| `CONFIG_BLUERETRO_IBLUE_LED_ORANGE` / `_BLUE` | orange | Colour on first boot |
| `CONFIG_BLUERETRO_IBLUE_PAIR_WINDOW_S` | 60 | Seconds of pairing after power-on or a tap; 0 means always pairing (upstream behaviour) |
| `CONFIG_BLUERETRO_IBLUE_DC_MENU_COLORS` | y | PS4/PS5 light bars in the Dreamcast menu colours |
| `CONFIG_BLUERETRO_BRCTL` | y | Maple bus control channel for Dreamcast software |

## Hardware notes

| Function | GPIO |
|---|---|
| Button / touch input (K1) | 16 |
| RGB LED red / green / blue | 25 / 14 / 13 |
| Port LEDs A–D | 15, 4, 2, 12 |
| Port sense A–D | 36, 35, 32, 33 |
| Console power sense | 39 |
| Maple bus ports A–D | 21/22, 3/5, 18/23, 26/27 |

The board has no console power or reset control: GPIO13 and GPIO14 drive the RGB LED, not the console. The controller-port shields are tied to ground, so BlueRetro's wired/wireless auto-switching isn't possible on this board.

## How this fork was made

The vendor firmware (`SEGA_DC_24V01_iBlueControlMod.bin`, SHA-256 `7d95dd7d…bf87fa`) was disassembled and compared function by function against a rebuild of BlueRetro v1.9. 2,229 of 2,554 functions are byte-identical. The rest are the pin changes, which were then ported to the latest BlueRetro.

The full write-up, with every difference, the community findings and recovery options, is in **[docs/README_iBlueControlMod.md](docs/README_iBlueControlMod.md)**.

## Credits and license

- **BlueRetro** © Jacques Gagnon ([darthcloud/BlueRetro](https://github.com/darthcloud/BlueRetro)), Apache-2.0. Upstream is now archived; its original README is kept in [docs/README_BlueRetro_upstream.md](docs/README_BlueRetro_upstream.md), and the [BlueRetro wiki](https://github.com/darthcloud/BlueRetro/wiki) still documents the general features.
- The pin mapping was cross-checked against [BlueRetro discussion #1183](https://github.com/darthcloud/BlueRetro/discussions/1183) (darthcloud, zeus86 and others).
- This fork is released under the same Apache-2.0 license (see [LICENSE](LICENSE)).

This project is not affiliated with the iBlueControlMod vendor or with SEGA. Flash at your own risk, and keep a backup.
