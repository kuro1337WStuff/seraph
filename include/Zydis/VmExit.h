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
 * VMX basic exit reason and SVM exit code for one instruction.
 *
 * This is separate from `ZydisGetInstructionInfo`. `CPUID` stays flow
 * `NEXT` and intercept `NONE` there. Here `CPUID` is VMX reason 10 when the
 * CPUID-exiting control is enabled, and SVM code `0x72` when that intercept
 * is enabled. `ZYDIS_VMX_CONTROL_UNCONDITIONAL` is a VMX non-root exit with
 * no extra exiting control. A reason is the exit this instruction can raise,
 * not a promise that the current VMCS raises it.
 */

#ifndef ZYDIS_VM_EXIT_H
#define ZYDIS_VM_EXIT_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * VM-execution control that gates a VMX basic exit reason.
 *
 * Values are names, not the control-bit index.
 */
typedef enum ZydisVmxControl_
{
    ZYDIS_VMX_CONTROL_NONE = 0,
    ZYDIS_VMX_CONTROL_UNCONDITIONAL,
    ZYDIS_VMX_CONTROL_CPUID,
    ZYDIS_VMX_CONTROL_HLT,
    ZYDIS_VMX_CONTROL_INVLPG,
    ZYDIS_VMX_CONTROL_RDPMC,
    ZYDIS_VMX_CONTROL_RDTSC,
    ZYDIS_VMX_CONTROL_CR3_LOAD,
    ZYDIS_VMX_CONTROL_CR3_STORE,
    ZYDIS_VMX_CONTROL_CR8_LOAD,
    ZYDIS_VMX_CONTROL_CR8_STORE,
    ZYDIS_VMX_CONTROL_CR_MASK,
    ZYDIS_VMX_CONTROL_MOV_DR,
    ZYDIS_VMX_CONTROL_IO,
    ZYDIS_VMX_CONTROL_MSR,
    ZYDIS_VMX_CONTROL_MONITOR,
    ZYDIS_VMX_CONTROL_MWAIT,
    ZYDIS_VMX_CONTROL_PAUSE,
    ZYDIS_VMX_CONTROL_DESCRIPTOR,
    ZYDIS_VMX_CONTROL_WBINVD,
    ZYDIS_VMX_CONTROL_RDRAND,
    ZYDIS_VMX_CONTROL_RDSEED,
    ZYDIS_VMX_CONTROL_INVPCID,
    ZYDIS_VMX_CONTROL_XSS,

    ZYDIS_VMX_CONTROL_MAX_VALUE = ZYDIS_VMX_CONTROL_XSS,
    ZYDIS_VMX_CONTROL_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_VMX_CONTROL_MAX_VALUE)
} ZydisVmxControl;

/**
 * SVM intercept class for an exit code.
 *
 * `INTERCEPT` is that instruction's intercept bit. `CR` and `DR` are the
 * read/write bitmaps. The exit code carries the register number.
 */
typedef enum ZydisSvmControl_
{
    ZYDIS_SVM_CONTROL_NONE = 0,
    ZYDIS_SVM_CONTROL_INTERCEPT,
    ZYDIS_SVM_CONTROL_CR,
    ZYDIS_SVM_CONTROL_DR,

    ZYDIS_SVM_CONTROL_MAX_VALUE = ZYDIS_SVM_CONTROL_DR,
    ZYDIS_SVM_CONTROL_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_SVM_CONTROL_MAX_VALUE)
} ZydisSvmControl;

/**
 * VMX and SVM exits one instruction can raise.
 *
 * `vmx_reason` is the Intel basic exit reason. `svm_code` is the AMD
 * `EXITCODE`. Either flag is clear when that vendor has no exit for the
 * instruction. `CR2` has an SVM CR intercept and no VMX exiting control.
 */
typedef struct ZydisVmExit_
{
    ZyanBool vmx;
    ZyanU16 vmx_reason;
    ZydisVmxControl vmx_control;
    ZyanBool svm;
    ZyanU16 svm_code;
    ZydisSvmControl svm_control;
} ZydisVmExit;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Reports the VMX and SVM exits for a decoded instruction.
 *
 * `operands` may be null when `operand_count` is 0. `MOV` to or from a
 * control or debug register uses those operands to pick the register and
 * the direction.
 *
 * @param   instruction     Decoded instruction.
 * @param   operands        Decoded operands.
 * @param   operand_count   Operand count from the decoder.
 * @param   result          Receives the exit record. Cleared on success.
 *
 * @return  A zyan status code.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetVmExit(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisVmExit* result);

/**
 * Reports whether `ZydisGetVmExit` needs the operand array for this instruction.
 *
 * All exits are classified from the mnemonic alone except `MOV` to or from a
 * control or debug register, whose class depends on the CR/DR number and the
 * direction. A caller that decodes without operands (for a fast exit-dispatch
 * path) can use this to decode the operands only when it matters; when this
 * returns `ZYAN_TRUE`, calling `ZydisGetVmExit` with `operand_count` 0 fails
 * with `ZYAN_STATUS_INVALID_ARGUMENT` rather than reporting a wrong "no exit".
 *
 * @param   instruction Decoded instruction.
 *
 * @return  `ZYAN_TRUE` if the operands are required, otherwise `ZYAN_FALSE`
 *          (also `ZYAN_FALSE` for a null instruction).
 */
ZYDIS_EXPORT ZyanBool ZydisVmExitNeedsOperands(const ZydisDecodedInstruction* instruction);

/**
 * Returns the human-readable name for a VMX execution control.
 *
 * @param   control The `ZydisVmxControl` value.
 *
 * @return  A static, null-terminated string, or `ZYAN_NULL` for an invalid value.
 */
ZYDIS_EXPORT const char* ZydisVmxControlGetString(ZydisVmxControl control);

/**
 * Returns the human-readable name for an SVM intercept class.
 *
 * @param   control The `ZydisSvmControl` value.
 *
 * @return  A static, null-terminated string, or `ZYAN_NULL` for an invalid value.
 */
ZYDIS_EXPORT const char* ZydisSvmControlGetString(ZydisSvmControl control);

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_VM_EXIT_H */
