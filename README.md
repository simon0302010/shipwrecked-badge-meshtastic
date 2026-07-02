# Shipwrecked Badge

![Build](https://github.com/USER/REPO/actions/workflows/build-badge.yml/badge.svg)

A LoRa chat badge with a 1.54" E-Ink display and phone-style keyboard, powered by a Raspberry Pi Pico and SX1262 radio.

## Firmware

| Path | Description |
|------|-------------|
| `variants/rp2040/shipwrecked/` | Shipwrecked variant config (pinout, display, radio) |
| `src/` | Meshtastic firmware source |
| `shipwrecked-pcb/` | KiCad PCB design, web flasher, and tools |

## Installing

### Option 1: Download a release build

Every push to `main`/`master` and every new tag triggers an automatic build via GitHub Actions. The resulting `.uf2` is attached to releases.

1. Head to the **Releases** section of this repository
2. Download the latest `shipwrecked.uf2` file
3. Continue to [Entering boot mode](#entering-boot-mode)

### Option 2: Build from source

**Prerequisites** — [PlatformIO](https://platformio.org) installed:

```bash
pip install platformio
```

**Build:**

```bash
pio run -e shipwrecked
```

The output file is `.pio/build/shipwrecked/firmware.uf2`.

### Entering boot mode

The badge enters USB mass-storage boot mode (RPI-RP2 drive) in two ways:

- **Hold the BOOTSEL button on the back** of the Pico while connecting USB — or press and hold it, then tap RESET
- **Software bootloader** — hold **DEL** + **SFT** (bottom row buttons) for 3 seconds. The screen will freeze and the badge re-enumerates as RPI-RP2

### Flashing

Once the badge appears as an `RPI-RP2` drive on your computer, copy the `.uf2` file to it:

```bash
cp .pio/build/shipwrecked/firmware.uf2 /path/to/RPI-RP2/
```

The badge automatically reboots when the copy finishes.

Alternatively, with `picotool` installed:

```bash
picotool load .pio/build/shipwrecked/firmware.uf2 -f
```

## Radio configuration

The badge uses an **SX1262 LoRa radio**. **You must set the correct frequency and channel for your region before transmitting.**

The badge comes pre-configured for **United States** frequencies out of the box. To change settings, navigate to the **LoRa Info** tab, then press **SW4** (OK) to open settings. You only need to adjust **two settings** if you're outside the US or coordinating with other badges:

- **Radio Preset** — selects the frequency band (US, EU868, etc.)
- **Frequency Slot** — selects the specific channel within that band

All badges communicating with each other **must use the same Radio Preset and Frequency Slot**.

> **WARNING:** Operating a LoRa radio on the wrong frequency may violate local telecommunications law. Check your country's regulations before transmitting.

## Button reference

These keys work across the entire UI:

| Button | Function |
|--------|----------|
| SW4  | OK / select |
| SW11 | Move right |
| SW5  | Move down |
| SW12 | Move left |
| SW3  | Move up |

## Usage

- **T9 text entry** — tap a key repeatedly to cycle through letters (multi-tap), pause 1.5s to commit
- **SFT** — shift next letter to uppercase
- **SPC** — insert space
- **BSP** — backspace
- **DEL** — clear the entire compose buffer
- **SND** — send the composed message over LoRa
- **DEL + SFT (3s)** — enter USB bootloader
