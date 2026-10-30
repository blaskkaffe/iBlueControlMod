# iBlueControlMod and Dreamcast extras: how the code is organised

Everything this fork adds to BlueRetro is grouped so that the upstream files keep a handful of one-line hooks and each feature can be switched off (or dropped) on its own.

## Layout

| Path | What |
|---|---|
| `main/iblue/` | all fork-specific firmware; nothing else in the tree depends on it except the hooks below |
| `main/iblue/Kconfig.iblue` | the options, one menu "iBlueControlMod and Dreamcast extras" (sourced from `Kconfig.projbuild`) |
| `dc/brctl/` | the KallistiOS client library (copied into openMenu); `brctl_proto.h` is the same file as `main/iblue/brctl_proto.h` |
| `configs/hw1/`, `configs/hw2/` | `dreamcast_openmenu` (plain boards), `dreamcast_iblue`, `dreamcast_iblue_vendor` |
| `main/tests/host/` | host tests of the portable code (`sh main/tests/host/run.sh`, needs only a C compiler) |

Files in `main/iblue/`:

| File | Feature | Option |
|---|---|---|
| `iblue_led.c` | RGB status LED, colour presets, custom colour | `BLUERETRO_IBLUE_RGB_LED` (iBlue pins only) |
| `button_map.c` | what each button gesture does (the detection stays in `system/manager.c`) | with the RGB LED |
| `brctl.c`, `brctl_proto.h` | the control channel over the Maple bus | `BLUERETRO_BRCTL` |
| `input.c` | looks at every Bluetooth report: live buttons for press-to-learn, the profile hotkey | with `BRCTL` |
| `hotkey.c` | Home + face button filter (portable, host tested) | with `PROFILES` |
| `profile.c` | the four profile slots and the per-game configurations | `BLUERETRO_IBLUE_PROFILES` |
| `vmu_lcd.c` | keeps the VMU screen picture the Dreamcast writes | always on the iBlue |
| the pairing window | in `system/manager.c` (`IBLUE_PAIR_WINDOW`) | `BLUERETRO_IBLUE_PAIR_WINDOW_S` |

Hooks in upstream files (everything else is in `main/iblue/`): `adapter/adapter.c` one call (`iblue_input`), `adapter/gameid.c` one function (`gid_set`), `system/fs.c` two names kept by a factory reset, `wired/maple.c` the brctl frames and the VMU screen, `bluetooth/host.c` one `case` that runs brctl operations on the Bluetooth task, `system/manager.c` the button/pairing state machine and `brctl_init()`, plus `sys_mgr_port_plugged()`.

## One channel: brctl

openMenu (any KallistiOS program) talks to the adapter with Maple frames to the controller, using a private function code ([`dc/brctl/README.md`](../dc/brctl/README.md)). Items: `INFO`, `STATUS`, `CONFIG` (the whole BlueRetro config, writes apply at once), `PROFILES`, `GAME`. Operations (`SET_CONDITION`) run later on the Bluetooth task; `status.op_seq` counts the finished ones and `status.op_result` says how the last went (`brctl_op_wait()` in the library does the waiting).

There is no second protocol and nothing needs the VMU: remapping, controller type, rumble, VMU on/off and the pairing mode are plain edits of the config struct on the Dreamcast side. The adapter only checks the config when it is saved (an impossible one is thrown away and the saved one reloaded) and clamps the values that could crash it in between.

**Reachability.** With hotplug on (the iBlue default) BlueRetro only enables the Maple pins of a port that has a Bluetooth controller ready, so the channel needs at least one connected controller. Plain HW1/HW2 boards have hotplug off: their ports always answer, so the channel works with no controller.

## Profiles and per-game settings

*Profiles*: four slots (`/fs/prof_0..3`), each the type, rumble/VMU and button table of one port. `PROFILE_SAVE` / `PROFILE_LOAD` / `PROFILE_DELETE` (slot, port). On the controller, **Home + a face button** loads slot 1 (face down), 2 (right), 3 (left), 4 (up) for the controller it is pressed on, stores it and buzzes once; the face buttons are hidden from the game while Home is held. Home is the generic `PAD_MT` button; not verified for every controller.

*Per game*: openMenu writes the game's product number to the `GAME` item and runs `GAME_SET`: the adapter makes it the BlueRetro game id (`DC_<id>`, 12 characters of letters, digits, - and _) and reloads (`config_init(GAMEID_CFG)`): the game's own `/fs/DC_<id>` (the whole configuration, about 12 KB; roughly 70 fit in flash) if it has one, otherwise the normal configuration. `GAME_SAVE` stores the live configuration as the game's, `GAME_DELETE` removes it. The id stays set after the game ends, so openMenu sends an empty id whenever it opens the normal settings.

Profiles and game files **survive a factory reset** (`fs_reset()` skips `prof_*` and `DC_*`). A better factory reset is a separate job; the 10 s hold is still too easy to trigger.

## Branches

- `main`: the build that was tested on hardware.
- `development`: the integration branch.
- `claude/restructure` (this work): `development` with the layout above. The earlier `claude/openmenu-remap` branch had a second channel over VMU blocks 200 to 207; it is superseded by brctl and kept only for reference.
- New features go on their own branch from `development`, with their own option in `Kconfig.iblue`, their own file(s) in `main/iblue/`, a doc paragraph here and a host test if there is portable logic. CI builds the configs with the **quick** workflow (`dreamcast_iblue`, `dreamcast_iblue_vendor`, `dreamcast_openmenu` for hw1 and hw2).

## Not verified

- Nothing here has run on a board. The firmware builds in CI for hw1 and hw2; `hotkey.c` and the openMenu side are tested on a PC (the openMenu test runs against a fake adapter that follows `brctl.c` and `profile.c`).
- Whether blueretro.io's other pages cope with the extra files in `/fs`.
- The hotkey writes the config to flash on every switch.
