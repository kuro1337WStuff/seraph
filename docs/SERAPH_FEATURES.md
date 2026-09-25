# Seraph features

Zydis is the project. New instructions and fixes from zyantific land by merging `upstream/master` into `master`. The rows below are optional Seraph APIs, added only when we choose to. Each one stays in its own files and inside caller-provided buffers, so an upstream merge does not have to carry it.

| Id | Status | What | Done when |
| --- | --- | --- | --- |
| constant-offsets | done | `ZydisGetConstantOffsets` | Patching the immediate of `mov rax, imm` and the displacement of a RIP-relative `mov` changes the decoded value. `enter` reports two immediates. `shl al, 1` reports none. |
| instruction-info | done | `ZydisInstructionInfo` | `push`, `cpuid`, `imul`, `call`, `ret`, `fld`, and `xsave` report implicit registers, flags, flow, and stack or FPU delta. GPR views are included: an `al` write is a partial `rax` update, and an `eax` write in 64-bit mode writes `rax` without reading the upper half. `ah` does not overlap `al`. |
| nasm-formatter | done | `ZYDIS_FORMATTER_STYLE_NASM` | `nop` and `mov rax, 0x1337` match. Memory prints `dword [rax]` with no `ptr`. Far `ret` is `retf` for NASM and MASM, and `ret far` for Intel. NASM prints `st1`; MASM prints `st(1)`. |
| assembler | done | `ZydisAsm` | `mov`, a scaled memory operand, `lock`, and a forward label assemble and decode back to the same mnemonic and operands. |
| block-encoder | done | `ZydisBlockEncode` | A short branch stays short. An out-of-range target widens. RIP-relative memory is relocated. `db` data can be the target of a label. Disabling widening fails cleanly. |
| opcode-cpuid | done | `ZydisGetEncodingInfo` | Opcode, opcode map, and ModRM are copied from the decoded instruction. `isa_set` and `isa_ext` come from `instruction->meta`. Null arguments fail. No CPUID leaf numbers. |
| fast-formatter | optional | Straight-line formatter | Shipped only when an in-repo bench shows a decode-plus-format win. |

`/seraph-feature` takes one of these ids as `args.feature`.
