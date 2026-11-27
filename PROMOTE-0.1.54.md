# Promote SLAP-5 v0.1.54 after owner approval

Promotion requires explicit owner approval of FM-1_75000154 following cold-boot/direct-upgrade testing and clean physical runs of both the saved JUNGLE and VHS SAW / DEEP DUB projects. Record Core 1 activity, zero new deadline misses and DMA XRUNs, peak callback below 5,804 µs with headroom, and native LCD checks. See [qualification](evidence/qualification-0.1.54.md).

| Item | Verified identifier |
| --- | --- |
| Exact application source | 1239f4a2b0bdf16b7415c5849a09d5e22fd16786 |
| Public deployment commit | ebc7c6d5e909687804831f2a0e4b37b234423ae2 |
| Expected gh-pages head and exact rollback tree | 0f19b6acea335d3a6bdafc0ae84ed7b94370cc30 |
| Firmware SHA-256 | 8cc23648f6efe1252e96951cdf58731af7de28e26a6ce80bc819bf7e69f258a9 |
| Engineering backup SHA-256 | 6b3db5eeb49ad84b6587c019aa6bfeb522528a642c125874f418c72e67005f73 |
| Pages archive verification | [Immutable Pages archive tag](https://github.com/CodingSlapfu/SLAP-5/archive/refs/tags/archive-pre-0.1.54-pages.tar.gz), commit `0f19b6acea335d3a6bdafc0ae84ed7b94370cc30`. [Verified archive and internal SHA256SUMS](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46057091357/artifacts/15227656064), artifact SHA-256 `00ac5275f0670a56fb24fb763a51459e5e13820554073a333388d4d9f82b64c8`. |

- [Native build and verified backup](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46057013485/artifacts/15227121615)
- [Matching site and Studio qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46057013500)
- [Testing installer live verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46057013519)
- [Promotion preparation and archival evidence](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46057091357)
- [Historical archive through v0.1.48](https://github.com/CodingSlapfu/SLAP-5/releases/tag/historical-firmware-through-0.1.48) has SHA-256 `cd3c24eccc7dba13899a56af482af723b6bdc23df429d9ea6e35183181d6a67a`.

## Promote the approved bytes

1. Read the live `gh-pages` head and compare it to the expected SHA above. Check that the prepared deployment has exactly that parent. If Pages has advanced, inspect those changes before any update.
2. Download the installer package from the live testing URL and verify its SHA-256. Compare it to `SLAP-5-0.1.54.fwsc` on the prepared deployment commit. Do not rebuild.
3. With repository Contents write permission, use the GitHub connector’s branch ref update for `gh-pages`, passing the expected old SHA, the exact prepared SHA and `force: false`. If the caller has a GitHub CLI session, the equivalent is `git push origin <prepared-sha>:refs/heads/gh-pages` after verifying the live parent. Pages must build and deploy that exact ref. If it does not trigger, use the repository Pages build API with Pages write permission.
4. Verify that `/SLAP-5/`, `flash.html`, `studio.html`, and `emulator/` serve the prepared files. Fetch `release.json`, verify FM-1_75000154 and SHA-256, and confirm the transferred binary’s digest. Check that the legacy test routes no longer serve pages from this deployment and that the GitHub archive links resolve.
5. To publish a GitHub Release in addition to Pages, use an authorized user with release-write capabilities. Tag `v0.1.54` at the exact application source and attach the same verified package, checksums, release notes and backup. The prior Actions token could not create releases on this repository. Ordinary ChatGPT GitHub access can move the Pages ref when permitted. Release creation and binary asset uploads may require the owner’s authenticated GitHub web UI or CLI.

## Roll back

Revert the exact promotion commit on `gh-pages` as a new commit and deploy it. Verify the restored tree equals the recorded rollback tree, then byte-check the former public installer and Studio. For device rollback use the saved pre-update backup or the verified supported package on compatible hardware and keep untouched project copies. Retain the v0.1.54 source and archives.
