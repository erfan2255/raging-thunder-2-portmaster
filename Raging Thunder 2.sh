#!/bin/bash
#
# Raging Thunder 2 — PortMaster Launcher (Optimized for muOS & ARM Handhelds)
# Target: RG35XX Plus / Allwinner H700 (Mali-G31 MP2) & compatible devices
#

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}
SCRIPT_DIR="$(dirname "$(readlink -f "$0")")"

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
elif [ -d "$SCRIPT_DIR/PortMaster" ]; then
  controlfolder="$SCRIPT_DIR/PortMaster"
elif [ -d "/mnt/mmc/MUOS/PortMaster" ]; then
  controlfolder="/mnt/mmc/MUOS/PortMaster"
elif [ -d "/opt/muos/PortMaster" ]; then
  controlfolder="/opt/muos/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

export PORT_32BIT="Y"

[ -f "$controlfolder/control.txt" ]          && source "$controlfolder/control.txt"
[ -f "$controlfolder/tasksetter" ]           && source "$controlfolder/tasksetter"
[ -f "$controlfolder/device_info.txt" ]      && source "$controlfolder/device_info.txt"
[ -f "$controlfolder/mod_${CFW_NAME}.txt" ]  && source "$controlfolder/mod_${CFW_NAME}.txt"

if [ -f "$controlfolder/control.txt" ]; then
  get_controls
fi

if [ -d "$SCRIPT_DIR/ragingthunder2" ]; then
  GAMEDIR="$SCRIPT_DIR/ragingthunder2"
elif [ -d "$(dirname "$SCRIPT_DIR")/../ports/ragingthunder2" ]; then
  GAMEDIR="$(readlink -f "$(dirname "$SCRIPT_DIR")/../ports/ragingthunder2")"
elif [ -d "/roms/ports/ragingthunder2" ]; then
  GAMEDIR="/roms/ports/ragingthunder2"
elif [ -d "/storage/roms/ports/ragingthunder2" ]; then
  GAMEDIR="/storage/roms/ports/ragingthunder2"
elif [ -d "/sdcard/ports/ragingthunder2" ]; then
  GAMEDIR="/sdcard/ports/ragingthunder2"
elif [ -d "/mnt/mmc/ports/ragingthunder2" ]; then
  GAMEDIR="/mnt/mmc/ports/ragingthunder2"
elif [ -n "$directory" ] && [ -d "/$directory/ports/ragingthunder2" ]; then
  GAMEDIR="/$directory/ports/ragingthunder2"
else
  GAMEDIR="$SCRIPT_DIR/ragingthunder2"
fi

cd "$GAMEDIR" || exit 1

LOGFILE="$GAMEDIR/log.txt"
exec > "$LOGFILE" 2>&1

echo "========================================="
echo "  Raging Thunder 2 - PortMaster Launcher  "
echo "========================================="
echo "Date: $(date)"
echo "Directory: $GAMEDIR"
echo "CFW: $CFW_NAME"

export LD_LIBRARY_PATH="$GAMEDIR/libs.armhf:/usr/lib/arm-linux-gnueabihf:/lib/arm-linux-gnueabihf:/usr/lib32:/lib32:/opt/muos/extra/lib:/usr/lib:/lib:$LD_LIBRARY_PATH"
export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"
export SDL_AUDIODRIVER="${SDL_AUDIODRIVER:-alsa}"

CUR_TTY=/dev/tty0
[ -w "$CUR_TTY" ] || CUR_TTY=/dev/tty1

# Locate APK file
APK_FILE=$(ls "$GAMEDIR"/*.apk 2>/dev/null | head -1)

if [ -z "$APK_FILE" ] || [ ! -f "$APK_FILE" ]; then
  $ESUDO chmod 666 "$CUR_TTY" 2>/dev/null
  printf "\033c" > "$CUR_TTY"
  {
    echo ""
    echo "  Raging Thunder 2 - Missing Game APK"
    echo ""
    echo "  Place your 32-bit Android APK inside:"
    echo "    $GAMEDIR/"
    echo "    (e.g., ragingthunder2.apk)"
    echo ""
  } > "$CUR_TTY"
  sleep 10
  printf "\033c" > "$CUR_TTY"
  [ -f "$controlfolder/control.txt" ] && pm_finish
  exit 1
fi

echo "Using Game APK: $APK_FILE"

# Extract native library if missing
mkdir -p "$GAMEDIR/libs.armhf" "$GAMEDIR/gamedata"
if [ ! -f "$GAMEDIR/libs.armhf/librthunder2play.so" ] && [ ! -f "$GAMEDIR/libs.armhf/libpolarbit.so" ] && command -v unzip >/dev/null 2>&1; then
  echo "Extracting native library from APK..."
  unzip -j "$APK_FILE" "lib/armeabi-v7a/*" -d "$GAMEDIR/libs.armhf/" 2>/dev/null || true
  unzip -j "$APK_FILE" "lib/armeabi/*" -d "$GAMEDIR/libs.armhf/" 2>/dev/null || true
fi

# Set up Mali EGL/GLES drivers shim for Allwinner H700 (Mali-G31 MP2)
MALI_BLOB=""
for candidate in \
    /usr/lib/arm-linux-gnueabihf/libmali-bifrost-g31-rxp0-gbm.so \
    /usr/lib/arm-linux-gnueabihf/libMali.so \
    /usr/lib/arm-linux-gnueabihf/libmali.so.1 \
    /usr/lib32/libMali.so \
    /usr/lib32/libmali.so \
    /usr/lib32/libmali.so.1 \
    /lib32/libMali.so \
    /lib32/libmali.so \
    /opt/muos/extra/lib/libMali.so \
    /usr/lib/libMali.so; do
  if [ -e "$candidate" ]; then
    MALI_BLOB="$candidate"
    echo "Found Mali GPU driver: $MALI_BLOB"
    break
  fi
done

if [ -n "$MALI_BLOB" ]; then
  GL_SHIM="/tmp/rthunder2-gl"
  rm -rf "$GL_SHIM"
  if mkdir -p "$GL_SHIM" && ln -sf "$MALI_BLOB" "$GL_SHIM/libEGL.so" \
                          && ln -sf "$MALI_BLOB" "$GL_SHIM/libEGL.so.1" \
                          && ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv1_CM.so" \
                          && ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv1_CM.so.1" \
                          && ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv2.so" \
                          && ln -sf "$MALI_BLOB" "$GL_SHIM/libGLESv2.so.2"; then
    export LD_LIBRARY_PATH="$GL_SHIM:$LD_LIBRARY_PATH"
  fi
fi

$ESUDO chmod +x "$GAMEDIR/ragingthunder2"

# Start gptokeyb for standard hotkey exit (Select + Start)
if [ -n "$GPTOKEYB" ]; then
  $GPTOKEYB "ragingthunder2" -c "$GAMEDIR/ragingthunder2.gptk" &
fi

if command -v pm_platform_helper >/dev/null 2>&1; then
  pm_platform_helper "$GAMEDIR/ragingthunder2"
fi

echo "Launching Raging Thunder 2..."
if command -v stdbuf >/dev/null 2>&1; then
  stdbuf -o0 -e0 $TASKSET "$GAMEDIR/ragingthunder2" "$APK_FILE"
else
  $TASKSET "$GAMEDIR/ragingthunder2" "$APK_FILE"
fi

if [ -n "$ESUDO" ] && command -v pidof >/dev/null 2>&1; then
  $ESUDO kill -9 "$(pidof gptokeyb)" 2>/dev/null || true
fi
rm -rf /tmp/rthunder2-gl
unset LD_LIBRARY_PATH SDL_GAMECONTROLLERCONFIG

if [ -f "$controlfolder/control.txt" ]; then
  pm_finish
fi
