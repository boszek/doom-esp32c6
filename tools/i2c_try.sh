#!/bin/bash
# Buduje, wgrywa i mierzy przez ~12 s dla zadanej czestotliwosci I2C.
# Uzycie: tools/i2c_try.sh 800000
set -e
cd "$(dirname "$0")/.."
HZ=$1
sed -i "s/^#define OLED_I2C_HZ .*/#define OLED_I2C_HZ $HZ/; s/^#ifndef OLED_I2C_HZ/#ifndef OLED_I2C_HZ/" doom_c6/platform/oled_c6.cpp
grep -q "^#define OLED_I2C_HZ" doom_c6/platform/oled_c6.cpp || sed -i "s/^#ifndef OLED_I2C_HZ/#define OLED_I2C_HZ $HZ\n#ifndef OLED_I2C_HZ/" doom_c6/platform/oled_c6.cpp
tools/sync_engine.sh >/dev/null
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6:CDCOnBoot=cdc doom_c6 2>&1 | grep -E "error" || true
arduino-cli upload -p /dev/ttyACM1 --fqbn esp32:esp32:XIAO_ESP32C6:CDCOnBoot=cdc doom_c6 2>&1 | tail -1
sleep 5
python3 /tmp/claude-1000/-home-boszek-dev-ai-playground-iic-oled/ca695fdc-86b9-4cb8-843d-062b1c994fdc/scratchpad/readlog.py 12 | grep "kl/s" | tail -4
