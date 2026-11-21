# SLAP-5 0.1.25 performance evidence

Five alternated runs compared the 0.1.24 source with the 0.1.25 feature tree using the extreme arrangement fixture and desktop thread CPU time. The labels-only final patch retains the same audio processing. Values are median run P95, P99 and peak in microseconds.

| Tempo and host mode | 0.1.24 P95 / P99 / peak | 0.1.25 P95 / P99 / peak |
| --- | --- | --- |
| 112 BPM, worker | 150.38 / 195.28 / 411.10 | 145.28 / 185.25 / 559.37 |
| 137 BPM, worker | 151.35 / 207.43 / 697.69 | 152.59 / 214.08 / 1928.29 |
| 160 BPM, worker | 165.13 / 218.53 / 442.13 | 164.39 / 244.25 / 379.62 |
| 112 BPM, single | 147.87 / 190.95 / 433.06 | 143.51 / 193.13 / 352.43 |
| 137 BPM, single | 172.86 / 238.66 / 897.59 | 176.64 / 249.21 / 904.43 |
| 160 BPM, single | 178.51 / 307.96 / 716.12 | 186.57 / 273.50 / 1533.69 |

Worker P99 rose by 3.2% at 137 BPM and 11.8% at 160 BPM. Large peaks varied between modes. Physical worker deadlines, transfer times and XRUN rates remain open.

The pinned native build reports 554,396 of 581,564 application bytes. The linked pool uses 328,648 of 344,064 bytes. The added stereo capture ring is 6,144 bytes. Native linker usage establishes allocation margins. Physical runtime stack and XIP margin remain open.

The host dual-core suite passed default, single-core and injected retirement/fallback paths. Stereo brake and all seven assignable Live DSP effects passed the bounded signal tests. The V15 app-only package audit and simulated install and rollback passed.
