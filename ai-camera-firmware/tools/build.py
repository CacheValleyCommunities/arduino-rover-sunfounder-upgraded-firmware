# Set python utf-8 encoding

import os
import shutil
import time
import sys
import subprocess
import zipfile
import glob

# Emoji labels
class Emoji:
    SUCCESS = "✅"
    ERROR = "❌"
    INFO = "ℹ️"
    WARNING = "⚠️"
    BUILD = "🔨"
    COPY = "📋"
    MERGE = "🔄"
    DONE = "🎉"
    FILE = "📁"
    FLASH = "⚡"

SCRIPT_NAME = "ai-camera-firmware"
ENVIRONMENT = "esp32cam"
MAIN_CPP = os.path.join("src", f"main.cpp")
BUILD_DIR = os.path.join(".pio", "build", ENVIRONMENT)
FIRMWARE_DIR = "firmware"
VERSION = ""

# Read VERSION from main.cpp
def get_version():
    global VERSION
    try:
        with open(MAIN_CPP, "r", encoding="utf-8") as f:
            for line in f:
                if line.startswith("#define VERSION"):
                    VERSION = line.split(" ")[2].strip().strip('"')
                    break
        if not VERSION:
            raise ValueError("VERSION not found in main.cpp file")
        return VERSION
    except Exception as e:
        print(f"{Emoji.ERROR} {e}")
        sys.exit(1)

def create_firmware_dir():
    if not os.path.exists(FIRMWARE_DIR):
        os.makedirs(FIRMWARE_DIR)

def clean_old_bins():
    print(f"\n{Emoji.INFO} Cleaning old bin files...")
    pattern = os.path.join(FIRMWARE_DIR, "*.bin")
    old_bins = glob.glob(pattern)
    for bin_file in old_bins:
        try:
            os.remove(bin_file)
            print(f"  {Emoji.SUCCESS} Removed: {os.path.basename(bin_file)}")
        except Exception as e:
            print(f"  {Emoji.WARNING} Failed to remove {bin_file}: {e}")

def update_bat_version(version):
    bat_file = os.path.join(FIRMWARE_DIR, "install_ESP32.bat")
    if not os.path.exists(bat_file):
        print(f"{Emoji.WARNING} {bat_file} not found, skipping version update")
        return
    
    try:
        with open(bat_file, "r", encoding="utf-8") as f:
            content = f.read()
        
        new_version = f'ai-camera-firmware.v{version}-factory.bin'
        content = content.replace("ai-camera-firmware.v1.5.3-factory.bin", new_version)
        
        with open(bat_file, "w", encoding="utf-8") as f:
            f.write(content)
        
        print(f"{Emoji.SUCCESS} Updated version in install_ESP32.bat to v{version}")
    except Exception as e:
        print(f"{Emoji.ERROR} Failed to update bat file: {e}")
        sys.exit(1)

def create_zip(version):
    zip_name = f"ai-camera-firmware.v{version}.zip"
    zip_path = os.path.join("firmware", zip_name)
    
    print(f"\n{Emoji.INFO} Creating zip archive...")
    try:
        with zipfile.ZipFile(zip_path, 'w', zipfile.ZIP_DEFLATED) as zipf:
            for root, dirs, files in os.walk(FIRMWARE_DIR):
                files = [f for f in files if not f.endswith('.zip')]
                for file in files:
                    file_path = os.path.join(root, file)
                    arcname = os.path.relpath(file_path, FIRMWARE_DIR)
                    zipf.write(file_path, arcname)
                    print(f"  {Emoji.FILE} Added: {arcname}")
        
        print(f"{Emoji.SUCCESS} Zip archive created: {zip_path}")
    except Exception as e:
        print(f"{Emoji.ERROR} Failed to create zip: {e}")
        sys.exit(1)

def run_command(cmd, description, show_output=False):
    try:
        if show_output:
            result = os.system(cmd)
            if result != 0:
                print(f"{Emoji.ERROR} {description} failed!")
                sys.exit(1)
        else:
            output = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT, universal_newlines=True)
        return True
    except subprocess.CalledProcessError as e:
        print(f"{Emoji.ERROR} {description} failed!")
        print(f"Command: {cmd}")
        print(f"Output: {e.output}")
        sys.exit(1)

def main():
    version = get_version()
    create_firmware_dir()
    clean_old_bins()
    update_bat_version(version)

    OTA_BIN = os.path.join(FIRMWARE_DIR, f"{SCRIPT_NAME}.v{version}-ota.bin")
    FACTORY_BIN = os.path.join(FIRMWARE_DIR, f"{SCRIPT_NAME}.v{version}-factory.bin")
    BOOT_APP = os.path.join("tools", "boot_app0.bin")
    FIRMWARE = os.path.join(BUILD_DIR, f"firmware.bin")
    BOOTLOADER = os.path.join(BUILD_DIR, f"bootloader.bin")
    PARTITIONS = os.path.join(BUILD_DIR, f"partitions.bin")
    ESPTOOL = os.path.join(FIRMWARE_DIR, "esptool.exe")
    
    print(f"{Emoji.BUILD} {SCRIPT_NAME} Build Tool")
    print(f"Version: {version}")

    print("\nBuilding PIO project...")
    if run_command(f"pio run -e {ENVIRONMENT}", "Building PIO project", show_output=False):
        print(f"{Emoji.SUCCESS} Build completed")
    
    print("\nCopying firmware to OTA bin file...")
    try:
        shutil.copy(FIRMWARE, OTA_BIN)
        print(f"{Emoji.SUCCESS} OTA binary created: {OTA_BIN}")
    except Exception as e:
        print(f"{Emoji.ERROR} Failed to copy OTA binary: {e}")
        sys.exit(1)
    
    print("\nMerging firmware to factory bin file...")
    cmd = f"{ESPTOOL} --chip ESP32 merge_bin -o {FACTORY_BIN} --flash_mode dio --flash_size 4MB 0x1000 {BOOTLOADER} 0x8000 {PARTITIONS} 0xe000 {BOOT_APP} 0x10000 {FIRMWARE}"
    if run_command(cmd, "Merging firmware", show_output=False):
        print(f"{Emoji.SUCCESS} Factory binary created: {FACTORY_BIN}")
    
    print(f"{Emoji.DONE} Build completed successfully!")
    print("Files created:")
    print(f"  - OTA Binary:     {OTA_BIN}")
    print(f"  - Factory Binary: {FACTORY_BIN}")

    create_zip(version)

if __name__ == "__main__":
    main()