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
