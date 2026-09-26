/***************************************************************************************************

  Zyan Disassembler Library (Zydis)

  Original Author : Florian Bernd

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.

***************************************************************************************************/

/**
 * @file
 * Register, memory, flag, and control-flow summary for one decoded instruction.
 */

#ifndef ZYDIS_INSTR_INFO_H
#define ZYDIS_INSTR_INFO_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/Register.h>
#include <Zydis/SharedTypes.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Constants                                                                                      */
/* ============================================================================================== */

/**
 * Maximum number of register accesses reported for one instruction.
 */
#define ZYDIS_INSTRUCTION_INFO_MAX_REGISTERS 96

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * Where control goes after the instruction.
 *
 * `ZydisBranchType` is the width of a branch (short, near, far). This enum is the
 * control-flow kind.
 */
typedef enum ZydisInstructionFlow_
{
    ZYDIS_INSTRUCTION_FLOW_NEXT,
    ZYDIS_INSTRUCTION_FLOW_CONDITIONAL_BRANCH,
    ZYDIS_INSTRUCTION_FLOW_UNCONDITIONAL_BRANCH,
    ZYDIS_INSTRUCTION_FLOW_INDIRECT_BRANCH,
    ZYDIS_INSTRUCTION_FLOW_CALL,
    ZYDIS_INSTRUCTION_FLOW_INDIRECT_CALL,
    ZYDIS_INSTRUCTION_FLOW_RETURN,
    ZYDIS_INSTRUCTION_FLOW_INTERRUPT,
    ZYDIS_INSTRUCTION_FLOW_SYSCALL,
    ZYDIS_INSTRUCTION_FLOW_XBEGIN,
    ZYDIS_INSTRUCTION_FLOW_EXCEPTION,
    ZYDIS_INSTRUCTION_FLOW_PRIVILEGED,

    ZYDIS_INSTRUCTION_FLOW_MAX_VALUE = ZYDIS_INSTRUCTION_FLOW_PRIVILEGED,
    ZYDIS_INSTRUCTION_FLOW_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_INSTRUCTION_FLOW_MAX_VALUE)
} ZydisInstructionFlow;

/**
 * Why a hypervisor would intercept the instruction.
 *
 * `ZYDIS_INSTRUCTION_FLOW_PRIVILEGED` is the coarse bucket for `SYSTEM`, `IO`, and `VTX`.
 * This enum splits the exits that share that bucket: `IN` is I/O, `RDMSR` is an MSR,
 * `LGDT` is a descriptor table, and `VMREAD` is VMX. SVM instructions such as `VMRUN`
 * are reported on their own. `CPUID`, `HLT`, and `MOV CR` stay `NONE`. `MOV CR` is a
 * data transfer, so its flow is `NEXT` rather than `PRIVILEGED`.
 */
typedef enum ZydisInstructionIntercept_
{
    ZYDIS_INSTRUCTION_INTERCEPT_NONE,
    ZYDIS_INSTRUCTION_INTERCEPT_IO,
    ZYDIS_INSTRUCTION_INTERCEPT_MSR,
    ZYDIS_INSTRUCTION_INTERCEPT_DESCRIPTOR,
    ZYDIS_INSTRUCTION_INTERCEPT_VMX,
    ZYDIS_INSTRUCTION_INTERCEPT_SVM,

    ZYDIS_INSTRUCTION_INTERCEPT_MAX_VALUE = ZYDIS_INSTRUCTION_INTERCEPT_SVM,
    ZYDIS_INSTRUCTION_INTERCEPT_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_INSTRUCTION_INTERCEPT_MAX_VALUE)
} ZydisInstructionIntercept;

/**
 * One register touched by the instruction.
 *
 * GPR, vector, x87/MMX, flag, and IP accesses are also reported on the other views of that
 * register. A write to `al` is a partial update of `rax`. A 32-bit GPR write in 64-bit mode
 * zero-extends into `rax`. A write to `xmm0` zeroes the upper bits of `ymm0` and `zmm0`.
 * `mm0` aliases `st0`. `flags`, `eflags`, and `rflags` are one register, as are `ip`, `eip`,
 * and `rip`. `ah` does not overlap `al`. Segment registers stay exact.
 */
typedef struct ZydisInstructionRegisterUse_
{
    ZydisRegister reg;
    ZydisOperandActions action;
} ZydisInstructionRegisterUse;

/**
 * One memory operand touched by the instruction.
 */
typedef struct ZydisInstructionMemoryUse_
{
    ZydisRegister segment;
    ZydisRegister base;
    ZydisRegister index;
    ZyanU8 scale;
    ZyanI64 disp;
    /**
     * Access size in bytes.
     */
    ZyanU32 size;
    ZydisOperandActions action;
} ZydisInstructionMemoryUse;

/**
 * Summary of one decoded instruction.
 *
 * `stack_delta` is the constant number of bytes added to RSP/ESP/SP. It is valid only when
 * `stack_delta_known` is true. `leave` is not a constant delta, because it copies RBP into
 * RSP before the pop.
 *
 * `fpu_delta` is how many values the x87 stack gains. `fld` is `+1` and `fstp` is
 * `-1`. It is valid only when `fpu_delta_known` is true. A push reads `st0`–`st6`
 * and writes `st0`–`st7` (`st7` is overwritten and not read). A pop reads and
 * writes all eight. `fdecstp` and `fincstp` rotate all eight. `fptan` and
 * `fsincos` push only when C2 stays clear, so the delta is unknown. `st0` is
 * still read, and the stack writes are conditional. `fsin` and `fcos` write
 * `st0` only on that same in-range path.
 *
 * `fpu_top_written` is set when the FPU `TOP` field is written, including a
 * conditional push and a state load such as `fldenv` or `fxrstor`. `emms` clears
 * tags and leaves `TOP` alone. An unknown `fpu_delta` does not by itself mean
 * `TOP` changed.
 *
 * `intercept` splits privileged exits. See `ZydisInstructionIntercept`.
 */
typedef struct ZydisInstructionInfo_
{
    ZydisInstructionFlow flow;
    ZydisInstructionIntercept intercept;
    ZyanU8 register_count;
    ZydisInstructionRegisterUse registers[ZYDIS_INSTRUCTION_INFO_MAX_REGISTERS];
    ZyanU8 memory_count;
    ZydisInstructionMemoryUse memory[ZYDIS_MAX_OPERAND_COUNT];
    ZydisAccessedFlagsMask flags_tested;
    ZydisAccessedFlagsMask flags_modified;
    ZydisAccessedFlagsMask flags_set_0;
    ZydisAccessedFlagsMask flags_set_1;
    ZydisAccessedFlagsMask flags_undefined;
    ZyanBool stack_delta_known;
    ZyanI32 stack_delta;
    ZyanBool fpu_delta_known;
    ZyanI8 fpu_delta;
    /**
     * `ZYAN_TRUE` when the instruction writes the x87 `TOP` field.
     * `fptan` and `fsincos` set this even though the push is conditional.
     */
    ZyanBool fpu_top_written;
} ZydisInstructionInfo;

/**
 * A memory access distilled for device emulation (for example an MMIO trap).
 *
 * Built from the decoded operands of the faulting instruction. `direction`, `size`, `segment`,
 * and `mem` describe the primary memory operand. `gpr` is the one explicit general-purpose
 * register paired with it (the source of a write or the destination of a read), or
 * `ZYDIS_REGISTER_NONE` when the other side is an immediate or is implicit (string ops).
 * `has_immediate`/`immediate` carry an immediate source. `sign_extend`/`zero_extend` mark a
 * widening load (`movsx`/`movsxd`/`movzx`). `is_string_op` is true when the instruction has two
 * memory sides (`movs`), in which case `mem2` is the second side; `rep_prefixed` reports a
 * `rep`/`repe`/`repne` prefix.
 */
typedef struct ZydisMmioAccess_
{
    ZydisOperandActions direction;
    ZyanU32 size;
    ZydisRegister segment;
    ZydisRegister gpr;
    ZyanBool has_immediate;
    ZyanU64 immediate;
    ZyanBool sign_extend;
    ZyanBool zero_extend;
    ZyanBool rep_prefixed;
    ZyanBool is_string_op;
    ZydisInstructionMemoryUse mem;
    ZydisInstructionMemoryUse mem2;
} ZydisMmioAccess;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Builds an instruction-info summary from a decoded instruction and its operands.
 *
 * `operand_count` must equal `instruction->operand_count`, the full count that
 * includes implicit operands. Passing `operand_count_visible` is rejected with
 * `ZYAN_STATUS_INVALID_ARGUMENT` rather than silently dropping the implicit
 * registers (for example the `RAX`/`RDX`/`RFLAGS` written by `mul`). Use
 * `ZydisGetInstructionInfoInsn` to pass the count automatically.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Operand array from the decoder. May be null when `operand_count` is 0.
 * @param   operand_count   Number of valid entries in `operands`; must be `instruction->operand_count`.
 * @param   info            Receives the summary.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetInstructionInfo(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisInstructionInfo* info);

/**
 * Builds an instruction-info summary using the instruction's own operand count.
 *
 * Convenience wrapper over `ZydisGetInstructionInfo` that passes
 * `instruction->operand_count`, so the caller cannot accidentally truncate the
 * implicit operands. `operands` must hold that many entries.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Operand array from the decoder, `instruction->operand_count` entries.
 * @param   info            Receives the summary.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetInstructionInfoInsn(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZydisInstructionInfo* info);

/**
 * Returns the human-readable name for a control-flow kind.
 *
 * @param   flow    The `ZydisInstructionFlow` value.
 *
 * @return  A static, null-terminated string, or `ZYAN_NULL` for an invalid value.
 */
ZYDIS_EXPORT const char* ZydisInstructionFlowGetString(ZydisInstructionFlow flow);

/**
 * Returns the human-readable name for a hypervisor intercept class.
 *
 * @param   intercept   The `ZydisInstructionIntercept` value.
 *
 * @return  A static, null-terminated string, or `ZYAN_NULL` for an invalid value.
 */
ZYDIS_EXPORT const char* ZydisInstructionInterceptGetString(ZydisInstructionIntercept intercept);

/**
 * Returns the hypervisor intercept class of an instruction from the instruction alone.
 *
 * This is the same value as `ZydisInstructionInfo.intercept`, but computed without operands
 * and without building the full info summary, for a hypervisor's fast exit-dispatch path.
 *
 * @param   instruction Decoded instruction.
 *
 * @return  The `ZydisInstructionIntercept` class, or `ZYDIS_INSTRUCTION_INTERCEPT_NONE` for a
 *          null instruction.
 */
ZYDIS_EXPORT ZydisInstructionIntercept ZydisGetInterceptClass(
    const ZydisDecodedInstruction* instruction);

/**
 * Distills a memory access into a `ZydisMmioAccess` for device emulation.
 *
 * Scans the operands for the memory operand(s), the one explicit GPR or immediate paired with
 * the access, and the direction/size/segment, so an MMIO trap handler does not have to reconcile
 * the register-alias list itself. See `ZydisMmioAccess` for the fields.
 *
 * `operand_count` must equal `instruction->operand_count` (a string op's two memory sides are
 * implicit operands). A gather/scatter (VSIB) access spans one address per lane and cannot be
 * described by a single record, so it returns `ZYAN_STATUS_NOT_FOUND`.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Operand array from the decoder.
 * @param   operand_count   Number of valid entries in `operands`; must be `instruction->operand_count`.
 * @param   access          Receives the distilled access.
 *
 * @return  `ZYAN_STATUS_SUCCESS`, `ZYAN_STATUS_INVALID_ARGUMENT` for a bad argument, or
 *          `ZYAN_STATUS_NOT_FOUND` when the instruction has no single memory operand to describe.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetMmioAccess(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisMmioAccess* access);

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_INSTR_INFO_H */
