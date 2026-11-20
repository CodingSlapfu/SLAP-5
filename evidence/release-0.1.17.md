# SLAP-5 0.1.17 corrective TESTING

Firmware identity `FM-1_75000117`. Native build, package audit and software regressions passed. **Core 1 operation and the 5,804 µs deadline on the physical FM-1 remain unverified.** The prior build failed under real load. See [the owner's nine captures and workload evidence](fm1-0.1.16-physical.md).

## Corrections

- Start Core 1 after the platform rails and clock setup and before LCD or application initialization. This follows the early order used by SLOOP on FM-1. Boot guard, safe mode, mailbox integrity checks, fault detection and retirement remain in force.
- Confirm a real mailbox transaction and verify the work ran on Core 1 before reporting startup success. The boot probe is separate from voice and FX job counters.
- Show startup outcome, current availability, hardware mailbox sends and completions, rejects, stack usage, retirement reason, eligibility skips, completed DSP jobs and timing in GLOBAL. A cold boot followed by those pages distinguishes boot suppression and hardware non-response from dispatch starvation.
- Move the full native voice and FX render kernels to SRAM. Remove the per-sample indirect call to the synth kernel and amortize the XIP-to-SRAM call over independent voice spans. The native link and audit pass with 18,996 bytes of SRAM text, 55,476 bytes data/BSS, and 312,420 bytes pool.
- Hoist drum coefficients and kit table reads into note-on. The kit table, sound recipes, IDs and 16-voice budget are retained. Independent FX jobs skip bypassed tracks. Dispatch gives the worker more opportunity later in the callback and keeps a 240 µs per-wait cap plus the aggregate callback cutoff. A late worker is held before state recovery.

## Evidence and remaining limits

The original photographs establish zero recorded DSP worker activity on v0.1.16. They cannot identify which startup or dispatch condition prevented activity. The revised boot probe and status screen will make that distinction on the owner's unit.

The threaded host completed 4,187 jobs with 10,104 µs aggregate overlap in the 112 BPM dry chord fixture and 5,060 jobs with 5,277 µs overlap in the 137 BPM wet fixture. Both had one host scheduling timeout and passed the single-core PCM comparison after recovery. Neither run measures two-core execution on an FM-1. The synchronous host forced-late, rejected-output and FX rollback tests passed. Studio auditioned all 128 patches and all 56 kit IDs, and the visualizer test passed all eight scenes at four master levels.

An isolated desktop test compared 242 matching 16-voice callbacks and included
only paired threaded samples with completed mailbox work. The single-thread
mean/P95/P99/peak was 49.30/60.69/83.75/95.19 µs, versus
32.14/43.77/78.31/109.86 µs with a real second host thread. The peak increased.
The pre- and post-change drum recording WAVs were byte-identical with the
same single-core fixture. All scripted host cases passed. Chromium CI passed
on the exact candidate source. [CI run](https://github.com/CodingSlapfu/SLAP-5/actions/runs/44701529878)

The interim host revision, before final FX placement, improved wet simulated-worker P99 from 236.56 to 189.51 µs, with mixed dry and single-core results. The desktop peak is noisy. Native SRAM placement reduces flash instruction fetches in the two busiest kernels and awaits direct timing on the unit. There is no measured native speedup or guaranteed headroom yet.

The 611,488-byte package SHA-256 is `090cc3d822095a89e53b18982b023f7c23c3d1a280a4479359bcaac11df33167`. The pinned packaging inputs were recovered from the verified v0.1.16 image and individually matched all three existing SDK hashes. The package audit regenerated and matched every byte using those inputs. The 0.1.14 and 0.1.16 rollback archives are retained.

## Physical acceptance

After backing up banks and installing on a supported FM-1, cold-start the unit and capture GLOBAL **CORE 1 STATUS** and **DISPATCH**. Capture them again after the 112 BPM dry chord recording and the 137 BPM heavy-FX playback. Confirm the mailbox probe reports PASSED, DSP job counters advance and **CORE TIMING** records overlap. Check 16-voice sustained notes, rapid hits across the new kits, FX feedback and all visualizers. Record peak and P99, deadlines and DMA XRUNs after resetting statistics. Acceptance requires clean audio, zero new misses and XRUNs, and a worst callback below 5,804 µs on the hardware stress cases. If startup is barred, fails or sees no reply, record the exact screen without changing hardware safeguards.
