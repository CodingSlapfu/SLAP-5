# Promote SLAP-5 v0.1.55 after owner approval

Promotion requires explicit owner approval of FM-1_75000155 following cold-boot/direct-upgrade testing and clean physical runs of both the saved JUNGLE and VHS SAW / DEEP DUB projects. Record Core 1 activity, zero new deadline misses and DMA XRUNs, peak callback below 5,804 µs with a 4,643–4,933 µs target, and native LCD checks. See [qualification](evidence/qualification-0.1.55.md).

| Item | Verified identifier |
| --- | --- |
| Exact application source | 75d5e5654edc7fff7fcb3857510c14bdb1ce3196 |
| Public deployment commit | 98daab5a51d763b1b166ec6bea5a2fa6985aee9f |
| Expected gh-pages head and exact rollback tree | d597971d8ab68922c6bb9478286964bb5abee5ba |
| Firmware SHA-256 | 8af6b9f9531903a86699bdd847b9d0d54bfa8fc2f94f86b425703c8cb4453a3d |
| Engineering backup SHA-256 | af8bbb8fdb9dfe6b04f66078119f57dbcb87acd2eaa9f6091fe3ae6654c13cc9 |
| Pages archive verification | [Immutable Pages archive tag](https://github.com/CodingSlapfu/SLAP-5/archive/refs/tags/archive-pre-0.1.55-pages.tar.gz), commit `d597971d8ab68922c6bb9478286964bb5abee5ba`. [Verified archive and internal SHA256SUMS](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099851283/artifacts/15245400210), artifact SHA-256 `bd5f01f67762fbfa642c6c9334aec2a1468b017b611f94576cc883a36cdf62fe`. |

- [Native build and verified backup](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099825024/artifacts/15244797680)
- [Matching site and Studio qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099824879)
- [Testing installer live verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099825021)
- [Promotion preparation and archival evidence](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46099851283)
- [Historical archive through v0.1.48](https://github.com/CodingSlapfu/SLAP-5/releases/tag/historical-firmware-through-0.1.48) has SHA-256 `cd3c24eccc7dba13899a56af482af723b6bdc23df429d9ea6e35183181d6a67a`.

## Promote the approved bytes

1. Read the live `gh-pages` head and compare it to the expected SHA above. Check that the prepared deployment has exactly that parent. If Pages has advanced, inspect those changes before any update.
2. Download the installer package from the live testing URL and verify its SHA-256. Compare it to `SLAP-5-0.1.55.fwsc` on the prepared deployment commit. Do not rebuild.
3. With repository Contents write permission, use the GitHub connector’s branch ref update for `gh-pages`, passing the expected old SHA, the exact prepared SHA and `force: false`. If the caller has a GitHub CLI session, the equivalent is `git push origin <prepared-sha>:refs/heads/gh-pages` after verifying the live parent. Pages must build and deploy that exact ref. If it does not trigger, use the repository Pages build API with Pages write permission.
4. Verify that `/SLAP-5/`, `flash.html`, `studio.html`, and `emulator/` serve the prepared files. Fetch `release.json`, verify FM-1_75000155 and SHA-256, and confirm the transferred binary’s digest. Check that the legacy test routes no longer serve pages from this deployment and that the GitHub archive links resolve.
5. To publish a GitHub Release in addition to Pages, use an authorized user with release-write capabilities. Tag `v0.1.55` at the exact application source and attach the same verified package, checksums, release notes and backup. The prior Actions token could not create releases on this repository. Ordinary ChatGPT GitHub access can move the Pages ref when permitted. Release creation and binary asset uploads may require the owner’s authenticated GitHub web UI or CLI.

## Roll back

Revert the exact promotion commit on `gh-pages` as a new commit and deploy it. Verify the restored tree equals the recorded rollback tree, then byte-check the former public installer and Studio. For device rollback use the saved pre-update backup or the verified supported package on compatible hardware and keep untouched project copies. Retain the v0.1.55 source and archives.
