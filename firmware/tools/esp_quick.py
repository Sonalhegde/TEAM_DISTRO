#!/usr/bin/env python3
"""
ESP32 Automation Helper for VS Code & ESP-IDF
- Automatically discovers the ESP32 COM port (even if it changes from COM4 to another port)
- Automatically kills any stale monitor process holding the port (eliminates PermissionError / Access Denied)
- Synchronizes .vscode/settings.json so VS Code is always pointing to the right port
- Flashes firmware seamlessly
- Optionally starts the serial monitor
"""

import sys
import os
import json
import time
import subprocess
import serial.tools.list_ports
try:
    import psutil
except ImportError:
    psutil = None

WORKSPACE_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SETTINGS_PATH = os.path.join(WORKSPACE_ROOT, ".vscode", "settings.json")
BUILD_DIR = os.path.join(WORKSPACE_ROOT, "build")
FLASHER_ARGS_PATH = os.path.join(BUILD_DIR, "flasher_args.json")

# Known USB Vendor IDs for ESP32 and USB-UART bridges
KNOWN_VIDS = {
    0x10C4: "Silicon Labs (CP210x)",
    0x1A86: "WCH (CH340/CH341)",
    0x0403: "FTDI",
    0x303A: "Espressif USB JTAG / CDC",
    0x067B: "Prolific PL2303",
}

ESP_KEYWORDS = ["cp210", "ch340", "ch341", "ftdi", "uart", "esp32", "espressif", "usb serial"]

def detect_esp_port():
    """Scans for ESP32 COM port dynamically."""
    ports = list(serial.tools.list_ports.comports())
    if not ports:
        print("[ESP-Helper] Error: No COM ports found! Is your ESP32 plugged in?")
        return None

    # 1. Match by known Vendor ID
    for p in ports:
        if p.vid in KNOWN_VIDS:
            print(f"[ESP-Helper] Auto-detected device on {p.device}: {KNOWN_VIDS[p.vid]} ({p.description})")
            return p.device

    # 2. Match by description keywords
    for p in ports:
        desc_lower = (p.description or "").lower()
        if any(kw in desc_lower for kw in ESP_KEYWORDS):
            print(f"[ESP-Helper] Auto-detected device on {p.device}: {p.description}")
            return p.device

    # 3. If only one port is connected, use it
    if len(ports) == 1:
        p = ports[0]
        print(f"[ESP-Helper] Only 1 COM port available, using {p.device}: {p.description}")
        return p.device

    # Multiple unknown ports
    print("[ESP-Helper] Multiple ports found, could not uniquely identify ESP32:")
    for p in ports:
        print(f"  - {p.device}: {p.description} (VID: {hex(p.vid) if p.vid else 'None'})")
    return ports[0].device


def kill_monitor_processes():
    """Terminates any python process running idf_monitor or esp_idf_monitor to free the COM port."""
    if not psutil:
        return

    current_pid = os.getpid()
    killed = 0
    for proc in psutil.process_iter(['pid', 'name', 'cmdline']):
        try:
            if proc.info['pid'] == current_pid:
                continue
            name = (proc.info['name'] or "").lower()
            if "python" in name:
                cmdline = " ".join(proc.info['cmdline'] or []).lower()
                if "esp_idf_monitor" in cmdline or "idf_monitor.py" in cmdline:
                    print(f"[ESP-Helper] Releasing locked port: terminating monitor process (PID {proc.info['pid']})...")
                    proc.kill()
                    killed += 1
            elif "serial-studio" in name:
                print(f"[ESP-Helper] Releasing locked port: terminating Serial Studio (PID {proc.info['pid']})...")
                proc.kill()
                killed += 1
        except (psutil.NoSuchProcess, psutil.AccessDenied):
            pass

    if killed > 0:
        time.sleep(0.5)  # Wait for OS to release serial port handle
        print(f"[ESP-Helper] Successfully terminated {killed} monitor process(es). Port is now free!")
    else:
        print("[ESP-Helper] No conflicting monitor processes found.")


def sync_vscode_settings(port):
    """Updates .vscode/settings.json with the detected COM port."""
    if not port or not os.path.exists(SETTINGS_PATH):
        return

    try:
        with open(SETTINGS_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)

        changed = False
        if data.get("idf.port") != port:
            data["idf.port"] = port
            changed = True
        if data.get("idf.portWin") != port:
            data["idf.portWin"] = port
            changed = True

        if changed:
            with open(SETTINGS_PATH, "w", encoding="utf-8") as f:
                json.dump(data, f, indent=4)
            print(f"[ESP-Helper] Synchronized .vscode/settings.json to use port: {port}")
    except Exception as e:
        print(f"[ESP-Helper] Warning: Could not update settings.json: {e}")


def build_project():
    """Builds the project using ninja or idf.py if available."""
    ninja_bin = None
    idf_tools = os.environ.get("IDF_TOOLS_PATH", r"C:\Espressif\tools")
    if os.path.exists(idf_tools):
        for root, dirs, files in os.walk(os.path.join(idf_tools, "ninja")):
            if "ninja.exe" in files:
                ninja_bin = os.path.join(root, "ninja.exe")
                break

    if ninja_bin and os.path.exists(BUILD_DIR):
        print(f"\n[ESP-Helper] Incremental build via Ninja...")
        res = subprocess.run([ninja_bin, "-C", BUILD_DIR])
        if res.returncode != 0:
            print(f"[ESP-Helper] Build failed with exit code {res.returncode}")
            sys.exit(res.returncode)
        print("[ESP-Helper] Build completed successfully!\n")
    else:
        idf_py = os.path.join(os.environ.get("IDF_PATH", r"C:\esp\v5.2.7\esp-idf"), "tools", "idf.py")
        if os.path.exists(idf_py):
            print("\n[ESP-Helper] Building with idf.py...")
            res = subprocess.run([sys.executable, idf_py, "-C", WORKSPACE_ROOT, "build"])
            if res.returncode != 0:
                print(f"[ESP-Helper] Build failed with exit code {res.returncode}")
                sys.exit(res.returncode)
            print("[ESP-Helper] Build completed successfully!\n")


def flash(port, baud=460800):
    """Flashes firmware using esptool."""
    if not os.path.exists(FLASHER_ARGS_PATH):
        print(f"[ESP-Helper] Error: {FLASHER_ARGS_PATH} not found. Please build the project first.")
        build_project()

    with open(FLASHER_ARGS_PATH, "r", encoding="utf-8") as f:
        flasher_args = json.load(f)

    python_bin = sys.executable
    esptool_path = os.path.join(os.environ.get("IDF_PATH", r"C:\esp\v5.2.7\esp-idf"), "components", "esptool_py", "esptool", "esptool.py")

    cmd = [
        python_bin,
        esptool_path,
        "-p", port,
        "-b", str(baud),
        "--before", "default_reset",
        "--after", "hard_reset",
        "--chip", "esp32",
        "write_flash"
    ]

    # Add flash settings
    flash_settings = flasher_args.get("write_flash_args", ["--flash_mode", "dio", "--flash_size", "2MB", "--flash_freq", "40m"])
    cmd.extend(flash_settings)

    # Add binary files with offsets
    flash_files = flasher_args.get("flash_files", {})
    for offset, rel_path in flash_files.items():
        cmd.extend([offset, os.path.join(BUILD_DIR, rel_path)])

    print(f"\n[ESP-Helper] Flashing ESP32 on {port} at {baud} baud...")
    res = subprocess.run(cmd)
    if res.returncode != 0:
        print(f"[ESP-Helper] Flashing failed with exit code {res.returncode}")
        sys.exit(res.returncode)
    print("[ESP-Helper] Flashing completed successfully!\n")


def start_monitor(port, baud=115200):
    """Starts the ESP-IDF monitor."""
    python_bin = sys.executable
    monitor_path = os.path.join(os.environ.get("IDF_PATH", r"C:\esp\v5.2.7\esp-idf"), "tools", "idf_monitor.py")
    elf_file = os.path.join(BUILD_DIR, "unverified_controller.elf")

    cmd = [
        python_bin,
        monitor_path,
        "-p", port,
        "-b", str(baud),
        "--toolchain-prefix", "xtensa-esp32-elf-",
        elf_file
    ]

    print(f"[ESP-Helper] Launching monitor on {port} at {baud} baud (Press Ctrl+] to exit)...")
    subprocess.run(cmd)


def main():
    action = sys.argv[1] if len(sys.argv) > 1 else "all"

    # 1. Kill any existing monitor holding the port
    kill_monitor_processes()

    # 2. Detect the COM port
    port = detect_esp_port()
    if not port:
        sys.exit(1)

    # 3. Keep .vscode/settings.json in sync with reality
    sync_vscode_settings(port)

    # 4. Perform action
    if action == "prep":
        # Just prepare: kill monitor & update port in settings.json
        print(f"[ESP-Helper] Ready to flash on {port}.")
        return

    elif action == "build":
        build_project()

    elif action == "flash":
        build_project()
        flash(port)

    elif action == "monitor":
        start_monitor(port)

    elif action in ("all", "run"):
        build_project()
        flash(port)
        start_monitor(port)

    else:
        print(f"Unknown action: {action}. Available: prep, build, flash, monitor, run")
        sys.exit(1)


if __name__ == "__main__":
    main()
