# LEDs, boot animation and game id (test firmware)

What this firmware changes compared with `main`. Everything is for the iBlueControlMod RGB build (`configs/hw2/dreamcast_iblue`); `dreamcast_iblue_vendor` keeps the factory behaviour (none of this).

## Power LED (RGB) and colours

| When | The power LED |
|---|---|
| a controller is **connecting** (found, not ready yet, at most 20 s) | blinks in the chosen colour (3 Hz) |
| searching for a controller (pairing) | breathes in the chosen colour |
| button held | blinks faster at each hold stage |
| otherwise | solid in the chosen colour, **fading in quickly** when it settles (fast at first, slow at the end) |

Three colours: **orange** (default), **blue** and **custom**.
- **Double tap**: tap, then tap again quickly: next colour (orange, blue, custom, orange ...). The custom colour is skipped until you have set one.
- **Set the custom colour**: tap, then tap again and **keep holding the second tap** for more than 0.6 s. The LED sweeps through all the colours (a full circle in 10 s, starting at red); **release when the colour you want shows up**: it locks in as the custom colour and is selected. It is kept over power cycles.
- A single tap still opens the 60 s pairing window; the longer holds (3, 6, 10, 30 s) are as before.

## Port LEDs A to D

- They **never blink without a Bluetooth controller connected**: no pairing pulse, no flash while the button is held. The power LED shows pairing and connecting.
- A controller's LED **fades on quickly** when it connects (about a quarter of a second, fast at first and slow at the end, so it looks logarithmic) and fades out when it goes.
- With a controller connected, the pairing pulse on the next free port and the hold-flash work as before.
- Each LED has its own PWM channel (LEDC high speed 4 to 7, timer 3).

**Maximum brightness**: the power LED and the port LEDs never get brighter than `BLUERETRO_IBLUE_LED_MAX_PCT` percent of the PWM duty (default **80**). I applied it to both; say if you wanted only one of them.

## Boot animation: drops falling on water

The LEDs are taken over first thing at start-up (`sys_mgr_iblue_early()`, before the file system and Bluetooth start, which used to take seconds with the pins in their power-up state). When the console powers on the four port LEDs run the animation for the length of the BIOS boot animation, then show the real state. Details of the shape (ripples, the rising and falling water level, drop schedule) are in [led-behaviour.md](led-behaviour.md). Settings: `BLUERETRO_IBLUE_BOOT_ANIM` (on), `BLUERETRO_IBLUE_BOOT_ANIM_MS` (**9000, a guess: time your console and set it**), `BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS` (700, the adapter's own start-up, also a guess).

## Dreamcast game id

BlueRetro can keep one configuration per game: it learns the game's id from the console (N64, GameCube, PlayStation), and blueretro.io shows the id and can **save a configuration for the current game**. The Dreamcast never tells the adapter anything, so **openMenu does**: before it starts a game it writes the product number into one reserved block of the adapter's emulated VMU (block 200, never stored as VMU data). The adapter makes it BlueRetro's game id (`DC_<product>`, the first 10 characters) with the existing code (`gid_update_sys()`, `config_init(GAMEID_CFG)`): the saved configuration of that game is loaded if there is one, otherwise the normal one. openMenu sends an empty id when it opens its own screens, so you are back on the normal configuration there.

How to use it: start a game from openMenu (with the openMenu build that has the game id writer), open blueretro.io, connect, and the game id is shown; change the mapping and save it as a game configuration. The next time you start the game from openMenu it is loaded. Game configurations (`DC_*` files) **survive the factory reset**.

Block layout, the checks and the limits are in `main/system/iblue_gameid.h`. `sh main/tests/host/run.sh` tests the block handling on a PC (valid and invalid requests, half a write, a game formatting the card).

Needs: a Bluetooth controller connected (the board only answers on a port that has one) and the **VMU accessory on** for that port. A new configuration has the VMU on for port A; an existing configuration with the VMU off has to switch it on in blueretro.io once.

## Not verified

Nothing has run on a board: the build compiles in CI. Whether blocks 200 and up are really never used by a game or the BIOS file manager; whether the openMenu side starts the game before the adapter has applied the id (it reads the block back and retries); the colours of the sweep; how the fades look on your LEDs; the BIOS animation length.

## Game id on / off, Racing mode, per-game controller type

- **Game id on / off**: tap action 10 ("Game id on / off"), mappable to the single, double or triple tap on the hosted web config. Off: games still announce themselves but no game configuration is loaded and the normal configuration is back at once. On: the game that runs gets its configuration. Kept over restarts (NVS `iblue/gid_off`). The LED dips once for on and twice for off.
- **Racing** (output mode 4): function word `0x000700FE`, name "Racing Controller": A, B, Start, d-pad and three analog axes (stick X = steering, right trigger = accelerator, left trigger = brake), no X / Y. The condition packet is the pad's. **Not tried on a console.**
- **Per game**: the configuration BlueRetro saves for a game includes the output mode, so a game can run GamePadAlt (C, Z, D, second d-pad and stick) or Racing while others use the plain GamePad. Some games read the controller's capability mask and misbehave with `0x003FFFFF` (reported for Dolphin Blue), so GamePad stays the default. The hosted web config's Dreamcast presets page writes the type and the default mapping (L1 = Z, R1 = C, L2 / R2 = triggers, sticks as sticks, L3 / R3 unmapped) and can save them for the running game.
