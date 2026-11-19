# SH-2 recompilation core

A platform-neutral C11 foundation for static SH-2 recompilation. The first
library target decodes selected register, immediate, and control-flow
instructions into typed operands. The decoder reports unsupported encodings
explicitly. Instruction execution, interrupt handling, and native code
generation are later milestones.

Board consumers own firmware, the memory map, devices, and scheduling. Both
32X and Saturn can link the same library and supply their own board bindings
as the execution interface is developed.

## Build

```sh
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --timeout 0
```

Consumers may add this repository with `add_subdirectory` and link
`sh2-recomp-core::core`. Encoding and operand fixtures follow the
[Renesas SH-1/SH-2/SH-DSP Software Manual](https://www.renesas.com/en/document/mas/sh-1sh-2sh-dsp-software-manual?r=1054686).
See [LICENSE](LICENSE) for the project license.
