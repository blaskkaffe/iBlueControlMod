# Remap the controllers from openMenu

Status: **written and tested on a PC only.** The firmware has not been built (no ESP-IDF here) and nothing has run on a board or a console. See "Not verified" at the end.

openMenu's **Files** icon opens a Controllers screen: pick a port (A to D), change the type, remap every Dreamcast output with press-to-learn or from a list, then save or discard. It works with no Bluetooth controller connected (any wired Dreamcast pad navigates; the list picker replaces press-to-learn). The openMenu side is in the openMenu-Virtual-Folder-Bundle repo (`NEWMENU.md`, "Controllers screen").

Left for later (they were in the first draft of this document): the power LED colours, a colour per network (DCNow! / DCNET) and the triple-tap network toggle on the button.

## How it works

- The board answers on the Maple bus as a controller and a VMU on every port whether or not a Bluetooth pad is paired. The **settings channel is eight blocks (200 to 207) of that emulated VMU**. `maple.c` hands reads and writes of those blocks to `iblue_cfg.c` instead of the memory card buffer, so they are never saved as VMU data and no game sees them. openMenu uses KallistiOS' ordinary `vmu_block_read` / `vmu_block_write`; no new Maple function is needed.
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
| 0x02 PORT_INFO | port u8 | dev_mode, acc_mode, connected, 0, map_size u16, vid u16, pid u16, name[24] |
| 0x03 GET_MAP | port u8, first u16, count u16 | `count` entries of 8 bytes |
| 0x04 SET_MAP | port u8, first u16, count u16, new_size u16 (0xFFFF = unchanged), entries | |
| 0x05 SET_TYPE | port, dev_mode (0 controller, 1 alt, 2 keyboard, 3 mouse), acc_mode (0 none, 1 VMU, 2 rumble, 3 both) | |
| 0x06 RESET_MAP | port | |
| 0x07 COMMIT | | writes the configuration to flash (`config_update`) |
| 0x08 REVERT | | reloads it from flash (`config_init`) |
| 0x09 LIVE | port | connected u8, 3 pad bytes, 4 words: the source buttons held now (bit = BlueRetro generic id), for press-to-learn |

An entry is `struct map_cfg`: `src_btn`, `dst_btn`, `dst_id`, `perc_max`, `perc_threshold`, `perc_deadzone`, `turbo`, `algo`. Pressing `src_btn` acts as `dst_btn` on output port `dst_id`. A destination that is no Dreamcast output (`IBLUE_DST_NONE`, 0x40) means "does nothing". Entries are validated (`dst_id` < 4, percentages <= 100, sizes in range) before anything is applied.

Changes **apply at once** (so the player sees them) and are stored only by `COMMIT`; `UNSAVED` stays set until then. `REVERT` goes back to what is in flash.

## Code

iBlueControlMod (`main/`):
- `system/iblue_cfg_proto.{h,c}`: wire format and command handler, portable C with no ESP-IDF or BlueRetro includes. The board supplies an `iblue_cfg_ops` table.
- `system/iblue_cfg.{c,h}`: the board side: the window, the task, the ops over `config`, `bt_host` (connected controller, name, vid/pid), and `iblue_cfg_note_input()` which `adapter_bridge()` calls with every converted report (sticks and triggers count as pressed past half travel).
- `wired/maple.c`: the block read/write hook, and the phase 3 trigger. `adapter/adapter.c`: the input note. `adapter/config.c`: `config_reset_map()` (the old table initialiser split out so a single port can be reset).
- Kconfig `BLUERETRO_IBLUE_CONFIG_CHANNEL` (default y on this hardware). `main.c` starts the task after `mc_init()`.
- Host test of the handler: `sh main/tests/host/run.sh` (needs only a C compiler; 102 checks).

openMenu: `iblue_link.c` (client), `iblue_map.c` (names and the swap rule), `ui_iblue.c` (screen), `iblue_vmu.c` (KOS blocks), `test/` (the same handler as a fake board). `sh openMenu/src/openmenu/test/run_iblue.sh`: 82 checks over link, rules and screen. The handler in `openmenu/test/fw` is a copy; if the protocol changes, change both.

## The screen

Rows: Port (Left/Right, A to D, with the controller's name), Type, Accessories, Remap buttons, Reset this controller, Save and exit, Exit without saving. B asks to save when something is unsaved.

Remap: 24 Dreamcast outputs (A, B, X, Y, Start, D, Z, C, L, R, d-pad, left stick, "pad 2" and "stick 2" directions). A starts press-to-learn (10 s; what is held when it opens is ignored until released; then the first source pressed is taken), X opens a list of all 32 gamepad sources, Y clears the output. **Assigning swaps**: the output the pressed button had until now goes to the button(s) that had the chosen output, so every button keeps doing something.

## Not verified

- **Firmware build**: not compiled (no ESP-IDF). The portable handler is compiled and tested on a PC, the glue (`iblue_cfg.c`), the Maple hook and the config change are only reviewed.
- Blocks 200 to 207 may be used by something on a real console or a game (the BIOS file manager, a game that formats the card). If so, the window moves.
- Whether KOS' `vmu_block_write` of block 200 reaches phase 3 as assumed, and the poll timing (up to 60 reads, 10 ms apart).
- Changing the type or `COMMIT` can make `config_update` reset the wired core (`SYS_MGR_CMD_WIRED_RST`), so the port may drop off the bus for a moment.
- The live input is read from the Bluetooth task while the channel task reads it: single words, no lock.
- The openMenu screen has not been looked at on a console or an emulator: text positions are guesses like the Settings page's.
- Keyboard and mouse types can be selected but are not remappable here.
