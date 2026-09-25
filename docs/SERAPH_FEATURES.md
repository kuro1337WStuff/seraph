# Seraph features

Zydis is the project. New instructions and fixes from zyantific land by merging `upstream/master` into `master`. The rows below are optional Seraph APIs, added only when we choose to. Each one stays in its own files and inside caller-provided buffers, so an upstream merge does not have to carry it.

| Id | Status | What | Done when |
| --- | --- | --- | --- |
| constant-offsets | done | `ZydisGetConstantOffsets` | Patching the immediate of `mov rax, imm` and the displacement of a RIP-relative `mov` changes the decoded value. `enter` reports two immediates. `shl al, 1` reports none. |
| instruction-info | done | `ZydisInstructionInfo` | `push`, `cpuid`, `imul`, `call`, `ret`, `fld`, and `xsave` report implicit registers, flags, flow, and stack or FPU delta. GPR, vector, MMX/x87 alias, flag, and IP views are included. An x87 push reads `st0`–`st6` and writes `st0`–`st7`. A pop reads and writes all eight. `fcompp` pops twice. `fdecstp` rotates all eight. `fptan` and `fsincos` push only when C2 stays clear, so `st0` stays a read and the stack writes are conditional. `fsin` and `fcos` write `st0` only on that same in-range path. `fpu_top_written` is set when `TOP` itself changes, including `fldenv` and a conditional `fptan`, and stays clear for `emms`. `intercept` separates `IN`, `RDMSR`, `LGDT`, and `VMREAD`. `MOV CR` stays a normal data transfer. |
| nasm-formatter | done | `ZYDIS_FORMATTER_STYLE_NASM` | `nop` and `mov rax, 0x1337` match. Memory prints `dword [rax]` with no `ptr`. Far `ret` is `retf` for NASM and MASM, and `ret far` for Intel. NASM prints `st1`; MASM prints `st(1)`. |
| assembler | done | `ZydisAsm` | `mov`, a scaled memory operand, `lock`, and a forward label assemble and decode back to the same mnemonic and operands. `ZydisAsmInsn` does the same for `cmp`, `lea`, and `nop`. `ZydisAsmBranch` does it for a near `jz` and `call`. |
| block-encoder | done | `ZydisBlockEncode` | A short branch stays short. An out-of-range target widens. RIP-relative memory is relocated. `db` data can be the target of a label. Disabling widening fails cleanly. |
| opcode-cpuid | done | `ZydisGetEncodingInfo`, `ZydisGetCpuidFlags` | Opcode, opcode map, and ModRM are copied from the decoded instruction. `isa_set` and `isa_ext` come from `instruction->meta`. `ZydisGetCpuidFlags` reports that ISA set as feature names. A 128-bit or 256-bit AVX-512 set also reports `AVX512VL`. Null arguments fail. No CPUID leaf numbers. |
| fast-formatter | optional | Straight-line formatter | Shipped only when an in-repo bench shows a decode-plus-format win. |

`/seraph-feature` takes one of these ids as `args.feature`.
