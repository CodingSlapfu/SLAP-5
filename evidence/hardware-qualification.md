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
