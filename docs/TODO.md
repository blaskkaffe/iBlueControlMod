# To do

Left out of the test firmware on purpose, to come later:

- **brctl**: the control channel over the Maple bus (status, config read and write, LED and pairing control, remappable gestures; it is on `development`). When it comes back, the game id mailbox in the VMU (`system/iblue_gameid.c`) should move into it: one channel instead of two.
- **Hotkeys and profiles**: Home + a face button to switch between saved controller profiles; the profile slots and per-game settings from openMenu (written on `claude/restructure`).
- **The openMenu Controllers screen** (remapping, type, rumble, VMU, per-button settings): on `claude/iblue-controllers` in openMenu-Virtual-Folder-Bundle; it needs brctl.
- **A better factory reset**: the 10 s hold is too easy to trigger. Per-game configurations are already kept through it.
- **Measure the BIOS boot animation** on a real console and set `BLUERETRO_IBLUE_BOOT_ANIM_MS` (and check the 700 ms of adapter start-up).
- **Code layout**: the iBlue code in one folder with one Kconfig menu (done on `claude/restructure`); bring the LED work onto it.
- **Plain HW1/HW2 builds** with these features (`dreamcast_openmenu` configs exist on `claude/restructure`).
- **Docs**: remove the `iblue-v1.9` branch text from `docs/README_iBlueControlMod.md` (no such branch on GitHub, and no 1.9 firmware is shipped).
- A port D (or any port) that is always there but not a controller, so the adapter can be reached with no Bluetooth controller (discussed, skipped).

## Status after the web config
- Tap actions: single and triple tap default to nothing; every tap can be mapped in the hosted web config (`web/`, iblue.html).
- Whole-config profiles (4 slots) are saved/loaded from the web config over BLE. Still TODO: switching profiles with a controller hotkey or from openMenu.

## Status after the web config
- Tap actions: single and triple tap default to nothing; every tap can be mapped in the hosted web config (`web/`, iblue.html).
- Whole-config profiles (4 slots) are saved/loaded from the web config over BLE. Still TODO: switching profiles with a controller hotkey or from openMenu.

## Samba de Amigo maracas (research, nothing built)
What dreamcast.wiki/Maracas says (read 2026-10-30): the maracas implement the standard controller function (FT0). Per maraca (table read as left / right): position = analog axis A / analog axis B, shake = button B / A, extra button = C / Start, "connection loss" signal = D / Z. After a reset the ultrasonic timing adjustment runs for 4 / 5 / 6 / 7 s on port A / B / C / D (240 / 300 / 360 / 420 Get Condition queries): buttons are disabled, positions stay fixed and the connection loss signal is active.
Flycast (PR 1035) emulates them with the two gamepad thumbsticks on axis slots 2 to 5 (centre 0x80, 0 to 0xFF), without the SetCondition ultrasonic settings (Samba sets an analog update interval of 2); issue 681 talks of four axes, two per maraca, which the wiki does not say.
Not known yet: the function data word and product name the console expects, which analog slots "A" and "B" are, the value range for high / middle / low. Needs: that information (or a capture from real maracas), a motion input path (Wii remote accelerometer, Joy-Con IMU: BlueRetro reads neither today), tilt to axis conversion, shake detection, an output mode, per-game selection through the game id.
