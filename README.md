# CPP20-QOI
A **C++20** implementation of the **qoi image format** (with a small change)

The  [orginal specification](https://qoiformat.org/qoi-specification.pdf) asks for an end tag consisting of

> 7 0x00 bytes followed by a single 0x01 byte

But since this is cumbersome to parse I replaced it with a single **0x6A** byte, representing a DIFF tag without any offsets.
