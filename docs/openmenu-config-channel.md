# Remap the controllers from openMenu

Status: **written and tested on a PC only.** The firmware has not been built (no ESP-IDF here) and nothing has run on a board or a console. See "Not verified" at the end.

openMenu's **Files** icon opens a Controllers screen: pick a port (A to D), change the type, remap every Dreamcast output with press-to-learn or from a list, then save or discard. **It needs at least one Bluetooth controller connected**: with hotplug on (the iBlue default) `wired_port_hdl()` only enables the Maple pins of ports that have a Bluetooth controller ready, so with none connected no port answers at all, VMU or not. A port without a controller can still be edited, through another port that has one; the list picker replaces press-to-learn for it. The openMenu side is in the openMenu-Virtual-Folder-Bundle repo (`NEWMENU.md`, "Controllers screen").

Left for later (they were in the first draft of this document): the power LED colours, a colour per network (DCNow! / DCNET) and the triple-tap network toggle on the button.

## How it works

- The board answers on the Maple bus as a controller and a VMU on every port **that is enabled**; with hotplug on that means only ports with a Bluetooth controller (see above). The **settings channel is eight blocks (200 to 207) of that emulated VMU**. `maple.c` hands reads and writes of those blocks to `iblue_cfg.c` instead of the memory card buffer, so they are never saved as VMU data and no game sees them. openMenu uses KallistiOS' ordinary `vmu_block_read` / `vmu_block_write`; no new Maple function is needed.
- The Maple interrupt only copies bytes. A task (`iblue_cfg_task`, polls every 20 ms) runs the command.
- **Port A has to keep its VMU** (the channel lives in it): `SET_TYPE` refuses an accessory mode without the VMU bit on port A (`IBLUE_RES_LOCKOUT`). The VMU has to be on for port A in the first place (it is the existing "accessories" setting).
- One mapping table per **output port** (A to D), exactly the table `struct in_cfg` already has and the blueretro.io web config edits. Not one per Bluetooth address: the profile follows the port the controller is assigned to.

## Wire format (little endian)

The window is 4096 bytes: 24 bytes of header, then up to 4072 bytes of data.

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `"iBLU"`: reads back as zeros on a real VMU, so openMenu can tell the board is there |
| 4 | 2 | protocol version (1) |
| 6 | 2 | status: bit0 PENDING (the client sets it, the board clears it when the answer is ready), bit1 UNSAVED |
| 8 | 2 | command |
| 10 | 2 | length of the data (request: argument, answer: result) |
| 12 | 4 | sequence number (the client counts up, the board leaves it: that is the echo) |
| 16 | 4 | CRC32 of the data |
| 20 | 2 | result code (0 ok) |
| 22 | 2 | reserved |

A request: the client writes the data blocks 201.. first, then block 200 with `PENDING` set. A block write is four 128 byte phases; **phase 3 of block 200 starts the command** (it does not wait for the "write complete" command, whose block number has not been checked on hardware). The client then reads block 200 until the sequence number matches and `PENDING` is clear, and reads the data blocks of the answer. A bad CRC or a half-sent request gives `IBLUE_RES_BAD_CRC` / `BAD_ARG`, never a change. While a command runs the board ignores further writes to the window.

| Cmd | Argument | Answer |
|---|---|---|
| 0x01 HELLO | | version u16, ports u8, 0, map max u16 (128), data max u16 |
| 0x02 PORT_INFO | port u8 | dev_mode, acc_mode, connected (a Bluetooth controller is assigned), plugged (the board senses a physical controller), map_size u16, vid u16, pid u16, name[24] |
| 0x03 GET_MAP | port u8, first u16, count u16 | `count` entries of 8 bytes |
| 0x04 SET_MAP | port u8, first u16, count u16, new_size u16 (0xFFFF = unchanged), entries | |
| 0x05 SET_TYPE | port, dev_mode (0 controller, 1 alt, 2 keyboard, 3 mouse), acc_mode (0 none, 1 VMU, 2 rumble, 3 both) | |
| 0x06 RESET_MAP | port | |
| 0x07 COMMIT | | writes the configuration to flash (`config_update`) |
| 0x08 REVERT | | reloads it from flash (`config_init`) |
| 0x09 LIVE | port | connected u8, 3 pad bytes, 4 words: the source buttons held now (bit = BlueRetro generic id), for press-to-learn |
| 0x0A GET_GLOBAL | | inquiry_mode u8 (0 auto, 1 manual), 7 reserved |
| 0x0B SET_GLOBAL | inquiry_mode u8 | applies at once, saved with COMMIT |
| 0x0C PROFILE_LIST | | 4 slots x (used, dev_mode, acc_mode, 0) |
| 0x0D PROFILE_SAVE | slot u8, port u8 | writes the type and mapping of that port to `/fs/prof_<slot>` at once |
| 0x0E PROFILE_LOAD | slot u8, port u8 | applies it to that port (live, unsaved); `EMPTY` if the slot is empty |
| 0x0F PROFILE_DELETE | slot u8 | |
| 0x10 GAME_SET | id (0 to 16 characters of A-Z a-z 0-9 - _) | the board loads `/fs/DC_<id>` if the game has one, else the normal configuration; "" detaches. Answer: 1 byte, 1 if the game has its own. Drops unsaved edits |
| 0x11 GAME_INFO | | bit0 game set, bit1 has its own file, bit2 own file in use; 3 pad bytes; id[17] |
| 0x12 GAME_SAVE | | the live configuration becomes the game's (`NO_GAME` if none is set) |
| 0x13 GAME_DELETE | | removes the game's file, back to the normal configuration |

An entry is `struct map_cfg`: `src_btn`, `dst_btn`, `dst_id`, `perc_max`, `perc_threshold`, `perc_deadzone`, `turbo`, `algo`. Pressing `src_btn` acts as `dst_btn` on output port `dst_id`. A destination that is no Dreamcast output (`IBLUE_DST_NONE`, 0x40) means "does nothing". Entries are validated (`dst_id` < 4, scale and press level <= 100, scaling mode 0 to 5, sizes in range) before anything is applied.

Changes **apply at once** (so the player sees them) and are stored only by `COMMIT`; `UNSAVED` stays set until then. `REVERT` goes back to what is in flash.

## Code

iBlueControlMod (`main/`):
- `system/iblue_cfg_proto.{h,c}`: wire format and command handler, portable C with no ESP-IDF or BlueRetro includes. The board supplies an `iblue_cfg_ops` table.
- `system/iblue_cfg.{c,h}`: the board side: the window, the task, the ops over `config`, `bt_host` (connected controller, name, vid/pid), and `iblue_cfg_note_input()` which `adapter_bridge()` calls with every converted report (sticks and triggers count as pressed past half travel).
- `wired/maple.c`: the block read/write hook, and the phase 3 trigger. `adapter/adapter.c`: the input note. `adapter/config.c`: `config_reset_map()` (the old table initialiser split out so a single port can be reset).
- Kconfig `BLUERETRO_IBLUE_CONFIG_CHANNEL` (any Dreamcast build; default y on the iBlue, set in `configs/hw1|hw2/dreamcast_openmenu` for regular HW1/HW2 boards). A fresh configuration enables the VMU on port A. `main.c` starts the task after `mc_init()`.
- Host test of the handler: `sh main/tests/host/run.sh` (needs only a C compiler; 211 checks).

openMenu: `iblue_link.c` (client), `iblue_map.c` (names and the swap rule), `ui_iblue.c` (screen), `iblue_vmu.c` (KOS blocks), `test/` (the same handler as a fake board). `sh openMenu/src/openmenu/test/run_iblue.sh`: 133 checks over link, rules and screen. The handler in `openmenu/test/fw` is a copy; if the protocol changes, change both.

## The screen

Port screen rows: **Port** (Left/Right, A to D; shows the controller's name and "pad plugged in" when the board senses a physical controller), **Type** (Controller, **Dual stick**, Keyboard, Mouse), **Memory card (VMU)** on/off, **Rumble pack** on/off, **Remap buttons**, **Bluetooth pairing** (Auto / Manual), **Reset this controller**, **Save and exit**, **Exit without saving**. The list scrolls. B asks to save when something is unsaved.

Dual stick is BlueRetro's "alternative controller" output: the console is told about the extra buttons and the second stick (`DESC_CTRL_ALT` in `maple.c`). Port A cannot turn its VMU off.

Remap: 24 Dreamcast outputs (A, B, X, Y, Start, D, Z, C, L, R, d-pad, left stick, "pad 2" and "stick 2" directions). A starts press-to-learn (10 s; what is held when it opens is ignored until released; then the first source pressed is taken), X opens a list of all 32 gamepad sources, Y clears the output. **Assigning swaps**: the output the pressed button had until now goes to the button(s) that had the chosen output, so every button keeps doing something.

**Start on an output** opens that button's own settings (the fields of the table entry): **Scale** (0 to 100 %, the maximum a stick or trigger gives), **Press level** (how far a stick or trigger must move to count as a button), **Dead zone** (0 to 2.55 % of the stick travel), **Rapid fire** (Off, Fast, Medium, Slow, Very slow: the turbo byte is `(frame mask << 1) | phase`, masks 1, 3, 7, 15) and **Stick scaling** (Linear stretches the controller's range over the Dreamcast's, Raw keeps the value). Each change is sent at once.

## Profiles and per-game settings

Upstream BlueRetro has no user profiles: one configuration file, plus per-game files that it loads by game id, but only for systems that report an id (N64, GameCube, PlayStation). The Dreamcast sends none, so openMenu sends it.

**Profiles** are four slots, each the type (controller / dual stick ...), rumble and VMU setting and the whole button table of one port, in `/fs/prof_0..3`. The Profiles row on the port screen loads one into the port being edited (A), saves the port into a slot (X) or deletes it (Y). On the controller: **Home plus a face button** loads slot 1 (face down), 2 (right), 3 (left) or 4 (up) *for the controller it is pressed on*. The hotkey is on the controller's own buttons, not the remapped ones: Home is BlueRetro's generic `PAD_MT` (the PS / Xbox / Home button; not verified for every controller), the face buttons are the four of the right pad by position. While Home is held the face buttons are hidden from the game. The switch is stored (in the game's file if a game's own configuration is in use) and the controller buzzes once. An empty slot does nothing. Loading onto port A always keeps its VMU.

**Per game**: `GAME_SET` makes the id the board's game id with the prefix `DC_`, and the existing game-id code (`config_init(GAMEID_CFG)`) loads `/fs/DC_<id>`: the *whole* configuration for that game (all ports, ~12 KB each; about 70 fit in the 960 KB of flash). openMenu does this in two places:
- **Starting a game** sends its product number first, so the game plays with its own settings if it has any, and with the normal ones otherwise.
- **Y on a game in the list** opens the Controllers screen for that game ("Game settings", `*` if it has its own). Everything there edits the game's copy; Save (also on leaving with B) stores it for the game, Remove game settings deletes it, Exit without saving drops the edits. Either way the board is detached from the game afterwards, so the normal settings are shown again.
Opening the plain Controllers screen also detaches the board from the game that was started last (the game id stays set in the board after a game), so edits go to the normal settings.

**Factory reset** (the 10 s hold) keeps the profiles and the per-game files: `fs_reset()` skips names starting with `prof_` and `DC_`. A better factory reset is planned separately.

### BlueRetro features and this screen

| BlueRetro feature | In this screen |
|---|---|
| Output mode (controller, alternative, keyboard, mouse) | Type |
| Accessories (VMU, rumble) | two separate on/off rows |
| Button mapping, scale, press level, dead zone, rapid fire | Remap, and Start on an output |
| Scaling mode | Linear / Raw. The other modes in the upstream enum (aggressive, relaxed, wide, S curve) and the diagonal scaling are not implemented in this firmware, so they are not offered |
| Inquiry mode | Bluetooth pairing |
| Which port a controller gets | follows the port sense: BlueRetro gives a Bluetooth controller the first port with no physical controller, so a pad in port A moves the Bluetooth controller to B |
| Per-game configuration | Dreamcast: see above (a Home + face hotkey and a game list key) |
| Combo buttons (the 10 shortcut entries after the 118 buttons, e.g. pairing) | not editable yet |
| System, multitap, memory card bank | not used on the Dreamcast, not offered |
| PS4/PS5 light bar colours | not yet (planned with the LED colours) |

## Not verified

- **Firmware build**: not compiled (no ESP-IDF). The portable handler is compiled and tested on a PC, the glue (`iblue_cfg.c`), the Maple hook and the config change are only reviewed.
- Blocks 200 to 207 may be used by something on a real console or a game (the BIOS file manager, a game that formats the card). If so, the window moves.
- Whether KOS' `vmu_block_write` of block 200 reaches phase 3 as assumed, and the poll timing (up to 60 reads, 10 ms apart).
- Changing the type or `COMMIT` can make `config_update` reset the wired core (`SYS_MGR_CMD_WIRED_RST`), so the port may drop off the bus for a moment.
- The live input is read from the Bluetooth task while the channel task reads it: single words, no lock.
- The openMenu screen has not been looked at on a console or an emulator: text positions are guesses like the Settings page's.
- Profile files are 1 KB and per-game files 12 KB; a full flash makes saving fail with a notice. SPIFFS wear from the stored hotkey switch was not looked at.
- Keyboard and mouse types can be selected but are not remappable here.
- Whether the port sense pins really report a plugged-in controller on this board (a user says they do): the Port row shows what the board sees. `sys_mgr_port_plugged()` is a thin wrapper over the existing `sense_port_is_empty()`.
- The turbo presets and the dead zone scale (1/100 %) come from reading the firmware, not from testing on a controller.
