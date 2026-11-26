# v0.1.37 physical playback overload at 155 BPM

Owner observed this on an actual FM-1 running v0.1.37. The five-track recorded loop used the Jungle drum generator and 60% swing. Playback initially sounded good. Lag began during FX editing. The owner reported that SLICER sounded good during normal playback and did not isolate a particular effect as the trigger. Turning off FX and muting all five tracks did not restore smooth playback. The saved project reproduced the behavior after reboot.

| GLOBAL measurement | Playing, all muted | Stopped, all muted |
|---|---:|---:|
| Average DSP | 113% | 36% |
| Peak callback | 6,859 µs | 6,859 µs |
| P95 / P99 | 6,700 / 6,800 µs | 6,700 / 6,800 µs |
| Deadline budget | 5,804 µs | 5,804 µs |
| Core 1 jobs | 2,987 | 2,987 |
| Worker fallbacks | 1 | 1 |
| Active voices | 16 | 0 |

The histogram, peak and miss counters include earlier callbacks. The stopped capture does not establish a continuing stopped overrun. The unchanged Core 1 job count and one fallback make runtime retirement a lead. They do not identify the failing worker or retirement cause.

Source inspection found that mixer mute zeroed the track at the mixing stage while next_step still scheduled its voices. The candidate at `0ea4d42daa17bc1ec25fdcdbcae0bcbaa5fa1081` stops allocating new sequence voices on explicitly muted tracks. It retains ordinary releases and effect tails. This addresses a confirmed source-level cause of unnecessary work after mute. The original overload trigger requires further investigation. The same candidate draws HOME lane backgrounds through the full row height and uses a larger title face where the name fits.

Use a copy of the saved project for a matched test. Reset GLOBAL counters with K3 before each observation. Compare restart and 155 BPM playback, all-muted playback after tails settle, and stopped idle. If overload returns, preserve the Core 1 START/RUNTIME/MAILBOX/RETIRE page before rebooting. Record whether Core 1 is online and whether the miss/XRUN counters still climb after Stop and a stats reset.
