# Raging Thunder 2 Port (muOS / Linux ARM Handhelds)

Custom Android native port package for **Raging Thunder 2** (Polarbit Fuse Engine) designed for **RG35XX Plus** (Allwinner H700 chipset) running **muOS** and other PortMaster-compatible Linux handhelds.

## Folder Structure

```
ROMS/ports/
├── Raging Thunder 2.sh             <-- The PortMaster launcher script
└── ragingthunder2/                 <-- Game folder
    ├── ragingthunder2              <-- Native armhf loader binary
    ├── ragingthunder2.apk          <-- 32-bit Android APK
    ├── ragingthunder2.gptk         <-- Gamepad / D-Pad mapping file
    ├── libs.armhf/                 <-- Extracted ARM 32-bit libraries (librthunder2play.so)
    ├── gamedata/                   <-- Save files, profiles, and highscores
    └── src/                        <-- C source code & Makefile
```

## How to Play

1. Place your 32-bit Android APK (`ragingthunder2.apk` or `Raging-Thunder-2-HD-v1-0-17.apk`) inside `ports/ragingthunder2/`.
2. Copy `Raging Thunder 2.sh` to your device's `/ROMS/ports/` (or `/ports/`).
3. Copy the `ragingthunder2` folder to `/ports/ragingthunder2/` (or `/ROMS/ports/ragingthunder2/`).
4. Launch **Raging Thunder 2** from your Ports menu!

## Building from Source

To compile the native loader binary for 32-bit ARM Linux:

### Option A: Cross-Compiling on Linux / WSL / Docker
```bash
sudo apt-get install -y gcc-arm-linux-gnueabihf libsdl2-dev:armhf
cd src
make CC=arm-linux-gnueabihf-gcc
```

### Option B: Building on device (via SSH on muOS / Linux terminal)
```bash
cd /mnt/mmc/ports/ragingthunder2/src
make CC=gcc
```

## Troubleshooting & Diagnostics

The launcher automatically writes a log file to `ports/ragingthunder2/log.txt` on every run.
If the game exits:
1. Ensure your APK has `lib/armeabi-v7a/librthunder2play.so` (or `lib/armeabi/librthunder2play.so`) and `assets/Data.vfs`.
2. Check `log.txt` for any initialization or missing library messages.
