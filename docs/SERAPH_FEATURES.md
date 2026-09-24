# Seraph features

Zydis is the project. New instructions and fixes from zyantific land by merging `upstream/master` into `master`. The rows below are optional Seraph APIs, added only when we choose to. Each one stays in its own files and inside caller-provided buffers, so an upstream merge does not have to carry it.

| Id | Status | What | Done when |
| --- | --- | --- | --- |
| constant-offsets | done | `ZydisGetConstantOffsets` | Patching the immediate of `mov rax, imm` and the displacement of a RIP-relative `mov` changes the decoded value. `enter` reports two immediates. `shl al, 1` reports none. |
| instruction-info | done | `ZydisInstructionInfo` | `push`, `cpuid`, `imul`, `call`, `ret`, `fld`, and `xsave` report implicit registers, flags, flow, and stack or FPU delta. |
| nasm-formatter | optional | `ZYDIS_FORMATTER_STYLE_NASM` and MASM acceptance fixes | A fixed corpus matches iced NASM text. MASM covers broadcast, far `ret`, `st(n)`, and `int 3`. |
| assembler | optional | `ZydisAsm` | `mov`, a scaled memory operand, `lock`, and a forward label assemble and decode back to the same mnemonic and operands. |
| block-encoder | optional | `ZydisBlockEncoder` | A short branch stays short. An out-of-range target widens. RIP-relative memory is relocated. `db` data can be the target of a label. Disabling widening fails cleanly. |
| opcode-cpuid | optional | Opcode and CPUID convenience | A VEX instruction and an APX instruction return the opcode map, mandatory prefix, and feature name from a table with a cited source. |
| fast-formatter | optional | Straight-line formatter | Shipped only when an in-repo bench shows a decode-plus-format win. |

`/seraph-feature` takes one of these ids as `args.feature`.
