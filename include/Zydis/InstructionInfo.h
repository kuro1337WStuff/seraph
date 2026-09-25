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
 * writes all eight. `fdecstp` and `fincstp` rotate all eight.
 */
typedef struct ZydisInstructionInfo_
{
    ZydisInstructionFlow flow;
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
} ZydisInstructionInfo;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Builds an instruction-info summary from a decoded instruction and its operands.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Operand array from the decoder. May be null when `operand_count` is 0.
 * @param   operand_count   Number of valid entries in `operands`.
 * @param   info            Receives the summary.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetInstructionInfo(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisInstructionInfo* info);

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_INSTR_INFO_H */
