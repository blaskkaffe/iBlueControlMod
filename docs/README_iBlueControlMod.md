# BlueRetro on iBlueControlMod (internal Dreamcast mod)

These notes come from reverse engineering the vendor firmware
`SEGA_DC_24V01_iBlueControlMod.bin` (566,224 bytes, ESP32 app image, built
Jan 27 2024 with ESP-IDF v5.1.1-dirty on Windows).

## How the comparison was done

1. I parsed the ESP32 app image (7 segments, entry point `0x4008159c`, 8 MB / 80 MHz / DIO flash header).
2. I matched the strings against upstream history. The base is **BlueRetro v1.9 (`be6b1bd`, 2023-10-15)**.
   - The firmware still has `rtype: %ld dtype: %ld sub: %ld`, which was removed on Oct 24.
   - It has the OUYA quirk from Oct 7 and the `# BT_ATT_OP_*` logs that were removed on Oct 22.
   - It does not have the fw_name or keepalive code from November.
3. I rebuilt upstream v1.9 for `hw2/dreamcast` with IDF v5.1.1, using the pre-5.2 `startup.c`, `FREERTOS_HZ=1000` and 8 MB / 80 MHz flash.
4. I matched every function in the rebuilt ELF against the vendor image, with PC-relative literals and calls masked.
   - **2,229 of 2,554 functions are byte-identical.**
   - The rest are either alignment noise, logging changes, or the hardware changes listed below.
5. I compared the data tables. Only `led_list`, `sense_list`, `sw_thresholds` and the `power_off_pin`/`current_pulse_led` initial values differ.

Unchanged:
- **The Maple bus driver.** `maple_tx`, `maple_rx`, `maple_init` and `maple_port_cfg` are identical.
- **The Maple pin tables.** Ports are 21/22, 3/5, 18/23 and 26/27.
- **The DC adapter code**, the config format, OTA and SPIFFS.

## Hardware differences (all in `main/system/manager.c` and `main/system/led.c`)

| Function | BlueRetro HW2 | iBlueControlMod |
|---|---|---|
| Boot / pair button | GPIO0 | **GPIO16** (input, pull-up) |
| Error LED PWM (LEDC HS ch0) | GPIO17 | **GPIO14** |
| GPIO17 | error LED | **output held low**, never toggled (`power_off_pin`) |
| POWER_OFF pin | GPIO16 | GPIO17 (never pulsed) |
| GPIO13 | POWER_ON (20 ms pulse) | **status LED side A**. "Power on" just drives it high and holds it, never pulsed |
| GPIO14 | RESET (open drain, 500 ms low) | **status LED side B** (still pulsed low for 500 ms by "reset") |
| Port LEDs P1..P4 | 2, 4, 12, 15 | **15, 4, 2, 12** |
| Port sense P1..P4 | 35, 36, 32, 33 | **36, 35**, 32, 33 (P1/P2 swapped) |
| External/internal detect | samples GPIO13+14 | **removed**: always internal, power sense on GPIO39 |
| Button thresholds | 1 s / 3 s / 6 s | **3 s / 6 s / 10 s** |
| Boot-time button check | immediate, only if power off | **1 s delay**, no power check |
| Deep sleep | plain | **GPIO25 and GPIO15 driven low + `gpio_hold_en` + `gpio_deep_sleep_hold_en`** |
| `err_led_set/clear/pulse` | drive the error LED | **no-ops**. The status LED is driven from the system manager instead |
| GPIO25 | unused | driven high by the button LED pattern and latched low in deep sleep (function unknown) |

The vendor added two new LED helpers. Both are reproduced byte-for-byte in the fork:
- `iblue_status_led_pairing()` (vendor `0x40096290`): GPIO14 gets HS ch0 inverted (breathing), and GPIO13 gets LS ch1.
- `iblue_status_led_connected()` (vendor `0x400962b8`): GPIO13 is driven as a GPIO low, and GPIO14 gets LS ch1.

Software-only differences that are not needed for compatibility:
- Many `printf` calls became `ESP_LOGI`, plus some extra debug logs.
- The supervision-timeout `printf` was removed.
- The build sets `CONFIG_FREERTOS_HZ=1000`.

### Why stock BlueRetro misbehaves on this board

- Stock firmware drives **PWM on GPIO17**, which the vendor keeps held low.
- It drives **GPIO16 as a push-pull output**, but GPIO16 is the button on this board.
- It reads the button on GPIO0, which does nothing here.
- It treats GPIO13/14 as console power/reset and may mark the adapter "external".
- It maps sense P1/P2 and port LEDs to the wrong pins.

## The fork

The patch is gated behind a new Kconfig option, `CONFIG_BLUERETRO_HW_IBLUECONTROLMOD`, and a build config `configs/hw2/dreamcast_iblue`. There are two branches:

- `iblue-v1.9`: the exact vendor base. Use IDF v5.1.1. The first commit restores the IDF 5.1 `startup.c`, as the vendor did.
  - Rebuilt with the patch, **2,266 functions** match the vendor image byte-for-byte.
  - `sys_mgr_power_on`, `sys_mgr_power_off`, `sys_mgr_deep_sleep` and both status-LED functions are identical.
  - `sys_mgr_init`/`sys_mgr_task` differ only in vendor logging and inlining.
- `iblue-latest`: the same changes on top of final upstream (`e1a9831`, Dec 2025). Use IDF `030c995` (v5.5-dev), which BlueRetroRoot pins.
  - Upstream added `hw_config`. The fork sets `power_pin_is_hold=1`, `hotplug=1`, `sw_io0_hold_thres_ms={3000,6000,10000}`, and forces `external_adapter=0`.

Build:

```sh
cp configs/hw2/dreamcast_iblue sdkconfig
BR_HW=_hw2 BR_SYS=_dreamcast_iblue idf.py reconfigure build
```

## Flashing without bricking

**Back up first.** If the board exposes UART0 (TX/RX, EN and GPIO0 for download mode), read the whole flash before anything else:

```sh
esptool.py -p PORT -b 921600 read_flash 0 0x800000 iblue_full_backup.bin
```

That backup holds the vendor bootloader, the partition table and the factory app, so it restores everything.

**Prefer OTA through the BlueRetro web config.** OTA only writes the next `ota_x` slot. The vendor app stays in `factory` and the bootloader is never touched.

**Factory reset recovers from a bad OTA app**, but only if that app boots far enough to run the system manager with the button on GPIO16. Both fork branches keep this path. Holding the button erases `otadata`, so the bootloader falls back to the factory (vendor) app:
- `iblue-v1.9`: hold for more than 10 s.
- `iblue-latest`: hold for 30 s. Upstream only wipes `otadata` after 30 s; between 10 and 30 s it only resets the config.

**Keep the image small and the flash header compatible.** The app must fit the OTA slot. Upstream slots are 1 MB; the fork images are about 565 KB (v1.9) and about 607 KB (latest). Keep the 8 MB flash-size header the vendor used, so the partition table is never seen as larger than the flash.

**Do not use `idf.py flash` blindly.** It also writes the bootloader, partition table and `ota_data_initial`. Over UART, write only the app to `ota_0`:

```sh
esptool.py write_flash 0x110000 app.bin   # only if your partition table matches upstream (check the backup first)
```

Then select it with otatool. Or restore your full backup.

**Check on the hardware first.** The pin roles come from code. Confirm them before relying on them, especially that GPIO17 is safe to hold low and what GPIO25 does.
