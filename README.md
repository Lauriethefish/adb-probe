# ADB Probe

The ADB probe is a simple C program paired with a set of udev rules that will automatically allow the Android Debug Bridge server to access
any USB device which has an ADB interface detected. Currently, most people use a large list of rules and simply hope any devices they connect
will be covered. This is unnecessary since ADB has a unique interface class/subclass/protocol which means the `adb-probe` can identify ADB devices
easily.

## Installation instructions

### Requirements
- GCC
- git

### Steps
1. Clone the repo: `git clone https://github.com/Lauriethefish/adb-probe`

2. Compile: `gcc adb-probe.c`

3. Copy result to udev folder: `cp ./a.out /usr/lib/udev/adb-probe`

4. Copy udev rules: Ideally replace your current Android udev rules with these ones.

(i) `ls /usr/lib/udev/rules.d`

(ii) Look for the ADB rules, probably called `51-android.rules` or with a different number.

(iii) Backup these rules: `cp /usr/lib/udev/old-android-rules.rules ./android-backup.rules`

(iv) Copy the adb-probe rules: `sudo cp android.rules /usr/lib/udev/old-android-rules.rules` (overwriting the existing rules)

5. Reload udev rules: `sudo udevadm control --reload-rules && sudo udevadm trigger`

6. Replug your USB device.

### Debug information

Logs are printed to `syslog`. Run `journalctl -f` for debugging information.
