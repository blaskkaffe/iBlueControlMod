# LEDs on the iBlueControlMod (main)

## What each LED does

| LED | When | Shows |
|---|---|---|
| Port LEDs A to D | the console powers on | a smooth animation for the length of the BIOS boot animation (below), then they go dark |
| Port LED of a controller | a Bluetooth controller is connected | solid on |
| Port LEDs | pairing, or the button held, **with no controller connected** | **nothing**: they never blink without a connected Bluetooth controller |
| Port LED (next free port) | pairing, **with a controller connected** | pulses, as before |
| RGB power LED | a controller is **connecting** (found, not ready yet, at most 20 s) | blinks, 3 Hz |
| RGB power LED | pairing (searching) | breathes, as before |
| RGB power LED | button held | blinks faster at each stage, as before |
| RGB power LED | otherwise | solid in the chosen colour |

Priority on the RGB LED: button held, then connecting, then pairing, then solid.

## Boot animation

The adapter boots together with the console, so it only knows the console powered on when its own firmware starts. It counts `BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS` (700 ms) plus its own uptime as already played and runs the rest of the animation, so it ends when the BIOS animation does. Port LEDs are driven by LEDC high speed channels 4 to 7 on timer 3 (nothing else uses them); `set_port_led()` and the pulse are ignored while the animation runs, and the real state is shown the moment it is done.

Options (menu "BlueRetro"):

| Option | Default | |
|---|---|---|
| `BLUERETRO_IBLUE_BOOT_ANIM` | y on the iBlue; off in `dreamcast_iblue_vendor` | |
| `BLUERETRO_IBLUE_BOOT_ANIM_MS` | 6000 | length of the BIOS boot animation. **A guess**: I found no figure for it, time your console with a stopwatch from power-on to the end of the swirl and set it |
| `BLUERETRO_IBLUE_BOOT_ANIM_BOOT_MS` | 700 | bootloader time before the firmware runs. Also a guess |
| `BLUERETRO_IBLUE_BOOT_ANIM_STYLE` | scan | scan, comet, fill, wave, centre |

The shapes are `main/system/iblue_anim_curve.c`: plain maths, gamma 2.2 so the brightness steps look even, a fade in and out at the ends, and a whole number of A to D to A sweeps (4 sweeps of 1.5 s for 6 s). `sh main/tests/host/run.sh` checks that every style is dark at both ends, gets every LED bright, never jumps by more than a third of the range in 10 ms and has faded out before the end. `tests/host/anim_frames.c` prints the frames; the preview GIFs are drawn from its output.

## Not verified

Nothing here has run on a board: the build compiles in CI. The animation timing is a guess (see above), the blue colour in the previews is only an example (the port LEDs' real colour is whatever the board has), and whether 5 kHz PWM on these LEDs looks even (no flicker at low duty) is for your eyes.
