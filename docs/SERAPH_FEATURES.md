# Seraph features

Zydis already decodes, encodes, and formats Intel, AT&T, and Intel-MASM. These are the iced-x86 features Seraph adds on that C API. Each item stays inside caller-provided buffers.

| Id | Status | What | Done when |
| --- | --- | --- | --- |
| constant-offsets | open | `ZydisGetConstantOffsets` | Patching the immediate of `mov rax, imm` and the displacement of a RIP-relative `mov` changes the decoded value. `enter` reports two immediates. `shl al, 1` reports none. |
| instruction-info | open | `ZydisInstructionInfo` | `push`, `cpuid`, `imul`, `call`, `ret`, `fld`, and `xsave` report implicit registers, flags, flow, and stack or FPU delta. |
| nasm-formatter | open | `ZYDIS_FORMATTER_STYLE_NASM` and MASM acceptance fixes | A fixed corpus matches iced NASM text. MASM covers broadcast, far `ret`, `st(n)`, and `int 3`. |
| assembler | open | `ZydisAsm` | `mov`, a scaled memory operand, `lock`, and a forward label assemble and decode back to the same mnemonic and operands. |
| block-encoder | open | `ZydisBlockEncoder` | A short branch stays short. An out-of-range target widens. RIP-relative memory is relocated. `db` data can be the target of a label. Disabling widening fails cleanly. |
| opcode-cpuid | open | Opcode and CPUID convenience | A VEX instruction and an APX instruction return the opcode map, mandatory prefix, and feature name from a table with a cited source. |
| fast-formatter | open | Straight-line formatter | Shipped only when an in-repo bench shows a decode-plus-format win. |

`/seraph-feature` takes one of these ids as `args.feature`.
