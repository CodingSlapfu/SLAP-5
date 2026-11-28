# Promote SLAP-5 v0.1.57 after owner approval

Promotion requires explicit owner approval of FM-1_75000157 following cold-boot/direct-upgrade testing and clean physical runs of both the saved JUNGLE and VHS SAW / DEEP DUB projects. Record Core 1 activity, zero new deadline misses and DMA XRUNs, peak callback below 5,804 µs with a 4,643–4,933 µs target, and native LCD checks. See [qualification](evidence/qualification-0.1.57.md).

| Item | Verified identifier |
| --- | --- |
| Exact application source | 981ad4d16fdfdbba578d282256556d270147d2cc |
| Public deployment commit | ce31bb1d5ced4bc6b6a6bf9397eacfe8fc425350 |
| Expected gh-pages head and exact rollback tree | a162351f396a253c7d31bd849bbacc1592933824 |
| Firmware SHA-256 | e1e00b8eb03db0cf30aec034b1f109ed8079ca8ce6416074a563bb20ffeb4558 |
| Engineering backup SHA-256 | 88ac7dd0615c2ba3f90659a9a6e5d9b50451168a1f192da5b7e5c0015d7fda5e |
| Pages archive verification | [Immutable Pages archive tag](https://github.com/CodingSlapfu/SLAP-5/archive/refs/tags/archive-pre-0.1.57-pages.tar.gz), commit `a162351f396a253c7d31bd849bbacc1592933824`. [Verified archive and internal SHA256SUMS](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105869176/artifacts/15247991099), artifact SHA-256 `7735e1d66bc1be640f9ffb3c5431209080ff15b6a8c7e992b783403dceed07dd`. |

- [Native build and verified backup](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105825310/artifacts/15247727215)
- [Matching site and Studio qualification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105825315)
- [Testing installer live verification](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105869252)
- [Promotion preparation and archival evidence](https://github.com/CodingSlapfu/SLAP-5/actions/runs/46105869176)
- [Historical archive through v0.1.48](https://github.com/CodingSlapfu/SLAP-5/releases/tag/historical-firmware-through-0.1.48) has SHA-256 `cd3c24eccc7dba13899a56af482af723b6bdc23df429d9ea6e35183181d6a67a`.

## Promote the approved bytes

1. Read the live `gh-pages` head and compare it to the expected SHA above. Check that the prepared deployment has exactly that parent. If Pages has advanced, inspect those changes before any update.
2. Download the installer package from the live testing URL and verify its SHA-256. Compare it to `SLAP-5-0.1.57.fwsc` on the prepared deployment commit. Do not rebuild.
3. With repository Contents write permission, use the GitHub connector’s branch ref update for `gh-pages`, passing the expected old SHA, the exact prepared SHA and `force: false`. If the caller has a GitHub CLI session, the equivalent is `git push origin <prepared-sha>:refs/heads/gh-pages` after verifying the live parent. Pages must build and deploy that exact ref. If it does not trigger, use the repository Pages build API with Pages write permission.
4. Verify that `/SLAP-5/`, `flash.html`, `studio.html`, and `emulator/` serve the prepared files. Fetch `release.json`, verify FM-1_75000157 and SHA-256, and confirm the transferred binary’s digest. Check that the legacy test routes no longer serve pages from this deployment and that the GitHub archive links resolve.
5. To publish a GitHub Release in addition to Pages, use an authorized user with release-write capabilities. Tag `v0.1.57` at the exact application source and attach the same verified package, checksums, release notes and backup. The prior Actions token could not create releases on this repository. Ordinary ChatGPT GitHub access can move the Pages ref when permitted. Release creation and binary asset uploads may require the owner’s authenticated GitHub web UI or CLI.

## Roll back

Revert the exact promotion commit on `gh-pages` as a new commit and deploy it. Verify the restored tree equals the recorded rollback tree, then byte-check the former public installer and Studio. For device rollback use the saved pre-update backup or the verified supported package on compatible hardware and keep untouched project copies. Retain the v0.1.57 source and archives.
