# Shipwrecked Badge Meshtastic Firmware

![Build](https://github.com/simon0302010/shipwrecked-badge-meshtastic/actions/workflows/build-badge.yml/badge.svg)

A port of the meshtastic firmware to the shipwrecked badge.

## Features

- Sending and receiving message via LoRa
- Snappy navigation using the lower button row
- Partial refresh support (for the eInk display)
- Low power consumption
- Much more

## Installing

### Option 1: Download a release build

Every push and every new tag triggers an automatic build via GitHub Actions. The resulting `.uf2` is attached to releases.

1. Head to the **Releases** section of this repository
2. Download the latest firmware file
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

The output file is `.pio/build/shipwrecked/firmware-shipwrecked-x.x.xx.xxxxxxx.uf2`.

### Entering boot mode

Follow the instructions to enter USB mass-storage boot mode:

- Unplug and power off the badge
- Hold **SW1** on the back of the pcb while plugging in the cable
- Keep holding the button until a drive labeled `RPI-RP2` shows up on your computer

### Flashing

Once the badge appears as an `RPI-RP2` drive on your computer, copy the `.uf2` file to it:

```bash
cp .pio/build/shipwrecked/firmware-shipwrecked-*.uf2 /path/to/RPI-RP2/
```

The badge automatically reboots when the copy finishes.

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
