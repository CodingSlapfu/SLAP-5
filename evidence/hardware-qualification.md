# SLAP-5 supervised hardware qualification

## Read-only preflight

1. Start from a working FM-1. Record its current firmware identity over USB MIDI, working playback/controls, hardware variant and any prior bootloader replacement. Use a direct data USB connection and stable power. Disconnect other MIDI devices.
2. Confirm a supported recovery path can enumerate this unit without erasing, programming, installing a bootloader or installing SLAP-5. Use the documented manufacturer/recovery-tool procedure for the unit and stop if neither USB MIDI nor the documented recovery interface can identify it.
3. Use the recovery tool's backup/read function only. Read the entire 1 MiB flash twice. Both 1,048,576-byte files must match byte for byte and have the same SHA-256. Keep two independent copies and protect the dump because it includes device data. Record the bootloader revision and chip key from a read-only inspection of this dump, and obtain the flash JEDEC ID from the device.
4. The current engineering target is stock V15, source identity FM-1_015, chip key 980F and flash JEDEC 856014. A matching MIDI identity alone does not establish that target. Stop for V16, another bootloader, another key or flash ID, a mismatched or incomplete backup, any uncertainty about the recovery path, or unstable power/USB. Confirm stock firmware still boots after leaving recovery.

## Qualification after engineering review

No present candidate is authorized for owner flashing. Record the baseline backup and diagnostics before a supervised transfer is scheduled. The reviewed candidate must have the pinned SDK inputs, a matching SHA-256, and the exact device-compatible app/loader layout. Prepare the same-unit recovery image and a known working recovery host before the test. Keep the full chip backup as a last-resort supervised recovery resource.

On a supported sacrificial or supervised unit, verify staging, loader enumeration, request progress, completion and boot into the expected identity. Stop on the first error, identity change or unplanned disconnect, record the device display and stage, and follow the reviewed recovery route for that unit. Confirm rollback to the original working firmware and do a separate controlled interruption and recovery trial only after the normal path and backup verification pass.

Read and compare the device's boot window [0x0000,0x4000), application window [0x4000,0x93000) and device-data and OTA regions after qualification. The boot area must match the baseline. Changed app sectors must match the qualified image. Account explicitly for OTA staging and cleanup and confirm user-data preservation. Test repeated boots, playback and transport, both melodic notes, concurrent drum lanes, saves/reloads, all patches and visualizers. Measure audio callback deadline and underruns under maximum event load and USB traffic before accepting performance. Only then bind the approved candidate to the public release manifest.

References: https://sbegueria.github.io/fm1-dev-wiki/Recovery.html and https://sbegueria.github.io/fm1-dev-wiki/Tools.html

## Windows USB-only walk-through

Use a known data cable and the working FM-1. In Chrome or Edge, open the public SLAP FLASH page, choose **Detect FM-1**, and copy its reported identity. Close any browser tab or music program using USB MIDI before using the backup tool.

Download and extract the upstream Quixotic7/MvaveFM1Unbricker repository. With Python 3 installed, open Windows Terminal as administrator, navigate to the extracted folder, and run `py fm1_unbrick.py setup`. Run `py fm1_unbrick.py softkey` from the normal firmware screen. Confirm only the ROM-mode prompt (`UBOOT`). This changes the current operating mode and does not write persistent flash. If the command cannot identify the FM-1 or the unit does not enumerate as WL82 UBOOT1.00, end this preflight. Cancel any Windows disk-format prompt.

Run these commands only:

```powershell
py fm1_unbrick.py find
py fm1_unbrick.py info
py fm1_unbrick.py backup .\fm1-backup
```

Require exactly one WL82 UBOOT1.00 device. Record the key and JEDEC ID from `info`. Allow `backup` to finish both flash reads and report a successful match. Keep the resulting 1,048,576-byte full dump on the PC and a second local drive. In PowerShell, run `Get-FileHash -Algorithm SHA256` on each saved copy and compare the hashes. Keep the backup files private. The useful handoff data are the firmware identity, key, JEDEC ID, backup size and SHA-256, the successful comparison result, and known bootloader history. If bootloader history is uncertain, an offline boot-area inspection is still required.

After the read-only commands finish, turn the FM-1 off and back on normally and check that its original firmware plays a note. Stop on any mismatch, unstable USB, read failure or missing recovery mode. Leave the flash program and restore functions unused during preflight.

Upstream tool and command documentation: https://github.com/Quixotic7/MvaveFM1Unbricker
