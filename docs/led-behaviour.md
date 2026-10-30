# LEDs on the iBlueControlMod (main)

## What each LED does

| LED | When | Shows |
|---|---|---|
| Port LEDs A to D | the console powers on | drops falling on water, for the length of the BIOS boot animation (below), then they go dark |
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

### The water

Take a line through a water surface. The four LEDs sit on it (A to D, one spacing apart) and the ends of the line are tank walls. A drop lands at a point of the line and a ring ripple spreads from it: seen in the cut, a wave packet with a sharp leading crest and a few weaker ones behind, getting weaker with distance (as the square root, like a ring on a pond) and with time. The ripples bounce off the walls (mirror images of each drop in both walls) and cross each other. **The height of the water at each LED is its brightness**: a crest is bright, a trough dark, calm water glows a little (20 %).

Three drops: a big, slow swell in the middle (between B and C, at 4 % of the animation; longer wavelength and a wider front so the first sequence comes in gently), a smaller one near A/B (at 33 %) and one near C/D (at 62 %).

**The water level over the animation** (`iblue_anim_calm()`): the light eases in over the first 16 % to the start level (20 % brightness for calm water), the water is raised from about 30 % to 50 % of the way (calm water 42 %, so everything is a little brighter while the second drop lands) and from 45 % on it goes down slowly, the light with it, to nothing at the end. Constants at the top of `main/system/iblue_anim_curve.c` (`REST_LOW`, `REST_HIGH`, `RISE_AT`, `RISE_LEN`, `FADE_AT`, `SETTLE`, ripple speed 1.6 LED spacings per second, wavelength 2.4, damping 4.5 s, drop times and places) change the look. The brightness goes through gamma 2.2 so the steps look even.

`sh main/tests/host/run.sh` checks, for 3, 6 and 9 s: dark at both ends and outside the time, every LED gets a clear crest, no jump of more than a third of the range in 10 ms (the splash builds up over 0.25 s so a drop never makes a step), faded out before the end, that the first drop is mirror-symmetric (B equals C, A equals D) and reaches A and D after B and C, that the first second never steps by more than 45/1023 in 10 ms, and the level over time: the start level, raised after a third, then only going down to nothing at the end. `tests/host/anim_frames.c` prints the frames (and the water surface); the preview video is drawn from its output.

## Not verified

Nothing here has run on a board: the build compiles in CI. The animation timing is a guess (see above), the water maths was only judged by eye in the preview, the blue colour in the previews is only an example (the port LEDs' real colour is whatever the board has), and whether 5 kHz PWM on these LEDs looks even (no flicker at low duty) is for your eyes.
