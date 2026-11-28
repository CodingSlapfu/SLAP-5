# SLAP-5 v0.1.55 hardware testing

Issue #9 Section E records five separate v0.1.52 snapshots at 95 BPM. The 6,232 µs callback exceeds the 5,804 µs deadline, while Core 1 remained online and worker wait was zero in the photographed breakdown. v0.1.54 still lagged according to the owner. Its callback timings were not captured.

Cold boot after direct installation, load working copies of both saved projects, then reset diagnostics. Replay the actual VHS SAW / SLICER / DEEP DUB session at 95 BPM and JUNGLE at 155 BPM / 60% swing. Include recording, overdub, visible animations and transport restart. Capture peak callback, P95/P99, deadline misses, DMA XRUNs and Core 1 jobs and fallbacks. Target 4,643–4,933 µs peak, zero new misses and XRUNs, and clean sound. Obtain explicit approval before public promotion.
