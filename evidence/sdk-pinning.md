# FM-1 SDK packaging verification

The checkpoint used an AQUORA packaging cache. Its uboot.boot and cfg/eq_cfg_hw.bin differed from fm1-base's explicit reference hashes. The three-file set has been replaced with files from JieLi's upstream fw-AC79_AIoT_SDK tag AC79NN_SDK_V1.2.1_2023-12-13. The tag object is b02c8c9354c077238ccbf764e36eff53782e1c16 and the checked-out source commit is d179b4484759423312073f5fbb232501aa491047.

| File under cpu/wl82/tools | Previous cache SHA-256 | Pinned JieLi SHA-256 and packager reference |
| --- | --- | --- |
| uboot.boot | 64ccc63e4df3238046fadb200f1b134311d8fe6ec6e3ccb8c8b81379231b867e | 4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4 |
| cfg_tool.bin | 276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b | 276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b |
| cfg/eq_cfg_hw.bin | 179ac9f4501d2cf8c3bdbc06fcda1e46228b7ace7e339342c767bb6568de57d8 | 41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d |

The mismatch reproduced in independent existing AQUORA caches. Comparison established different inputs, with no evidence of local corruption or a compiler-induced hash change. The precise upstream revision of the prior cache remains unestablished. The compiler bundle and fm1-base packing code were held fixed. The pinned inputs match every reference hash and the development package builds without SDK mismatch warnings. SHA agreement and static packaging checks do not establish physical compatibility or interrupted-update recovery.

[Upstream tag](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK/tree/AC79NN_SDK_V1.2.1_2023-12-13)
