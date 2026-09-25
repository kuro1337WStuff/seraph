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

#include <Zycore/LibC.h>
#include <Zydis/ConditionCode.h>
#include <Zydis/InstructionInfo.h>
#include <Zydis/Verbose.h>
#include <Zydis/VmExit.h>

/* ============================================================================================== */
/* Names                                                                                          */
/* ============================================================================================== */

static const char* const ZYDIS_VERBOSE_FLOW[] =
{
    "next",
    "conditional-branch",
    "unconditional-branch",
    "indirect-branch",
    "call",
    "indirect-call",
    "return",
    "interrupt",
    "syscall",
    "xbegin",
    "exception",
    "privileged"
};

static const char* const ZYDIS_VERBOSE_INTERCEPT[] =
{
    "none",
    "io",
    "msr",
    "descriptor",
    "vmx",
    "svm"
};

static const char* const ZYDIS_VERBOSE_CC[] =
{
    "o", "no", "b", "ae", "e", "ne", "be", "a",
    "s", "ns", "p", "np", "l", "ge", "le", "g"
};

static const char* const ZYDIS_VERBOSE_VMX[] =
{
    "none",
    "unconditional",
    "cpuid",
    "hlt",
    "invlpg",
    "rdpmc",
    "rdtsc",
    "cr3-load",
    "cr3-store",
    "cr8-load",
    "cr8-store",
    "cr-mask",
    "mov-dr",
    "io",
    "msr",
    "monitor",
    "mwait",
    "pause",
    "descriptor",
    "wbinvd",
    "rdrand",
    "rdseed",
    "invpcid",
    "xss"
};

static const char* const ZYDIS_VERBOSE_SVM[] =
{
    "none",
    "intercept",
    "cr",
    "dr"
};

ZYAN_STATIC_ASSERT(ZYDIS_INSTRUCTION_FLOW_NEXT == 0);
ZYAN_STATIC_ASSERT(ZYDIS_INSTRUCTION_FLOW_PRIVILEGED == 11);
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_VERBOSE_FLOW) / sizeof(ZYDIS_VERBOSE_FLOW[0])) ==
    (ZYDIS_INSTRUCTION_FLOW_PRIVILEGED + 1));
ZYAN_STATIC_ASSERT(ZYDIS_INSTRUCTION_INTERCEPT_NONE == 0);
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_VERBOSE_INTERCEPT) / sizeof(ZYDIS_VERBOSE_INTERCEPT[0])) ==
    (ZYDIS_INSTRUCTION_INTERCEPT_SVM + 1));
ZYAN_STATIC_ASSERT(ZYDIS_CONDITION_CODE_O == 0);
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_VERBOSE_CC) / sizeof(ZYDIS_VERBOSE_CC[0])) ==
    (ZYDIS_CONDITION_CODE_G + 1));
ZYAN_STATIC_ASSERT(ZYDIS_VMX_CONTROL_NONE == 0);
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_VERBOSE_VMX) / sizeof(ZYDIS_VERBOSE_VMX[0])) ==
    (ZYDIS_VMX_CONTROL_XSS + 1));
ZYAN_STATIC_ASSERT(ZYDIS_SVM_CONTROL_NONE == 0);
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_VERBOSE_SVM) / sizeof(ZYDIS_VERBOSE_SVM[0])) ==
    (ZYDIS_SVM_CONTROL_DR + 1));

/* ============================================================================================== */
/* Buffer                                                                                         */
/* ============================================================================================== */

static ZyanStatus ZydisVerboseAppend(char* buffer, ZyanUSize capacity, ZyanUSize* used,
    const char* text, ZyanUSize length)
{
    if ((capacity <= *used) || ((capacity - *used) <= length))
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }
    ZYAN_MEMCPY(buffer + *used, text, length);
    *used += length;
    buffer[*used] = '\0';
    return ZYAN_STATUS_SUCCESS;
}

static ZyanStatus ZydisVerboseText(char* buffer, ZyanUSize capacity, ZyanUSize* used,
    const char* text)
{
    ZyanUSize length = 0;

    while (text[length] != '\0')
    {
        ++length;
    }
    return ZydisVerboseAppend(buffer, capacity, used, text, length);
}

static ZyanStatus ZydisVerboseDec(char* buffer, ZyanUSize capacity, ZyanUSize* used, ZyanU16 value)
{
    char rev[8];
    char out[8];
    ZyanUSize n = 0;
    ZyanUSize i;

    if (value == 0)
    {
        return ZydisVerboseText(buffer, capacity, used, "0");
    }
    while (value != 0)
    {
        rev[n] = (char)('0' + (value % 10));
        value = (ZyanU16)(value / 10);
        ++n;
    }
    for (i = 0; i < n; ++i)
    {
        out[i] = rev[n - 1 - i];
    }
    return ZydisVerboseAppend(buffer, capacity, used, out, n);
}

static ZyanStatus ZydisVerboseHex(char* buffer, ZyanUSize capacity, ZyanUSize* used, ZyanU16 value)
{
    static const char digits[] = "0123456789abcdef";
    char rev[8];
    char out[8];
    ZyanUSize n = 0;
    ZyanUSize i;
    ZyanStatus status;

    status = ZydisVerboseText(buffer, capacity, used, "0x");
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    if (value == 0)
    {
        return ZydisVerboseText(buffer, capacity, used, "0");
    }
    while (value != 0)
    {
        rev[n] = digits[value & 0x0F];
        value = (ZyanU16)(value >> 4);
        ++n;
    }
    for (i = 0; i < n; ++i)
    {
        out[i] = rev[n - 1 - i];
    }
    return ZydisVerboseAppend(buffer, capacity, used, out, n);
}

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisFormatVerbose(const ZydisFormatter* formatter,
    const ZydisDecodedInstruction* instruction, const ZydisDecodedOperand* operands,
    ZyanU8 operand_count, ZyanU64 runtime_address, char* buffer, ZyanUSize capacity)
{
    char line[512];
    ZyanUSize used = 0;
    ZyanUSize text_length = 0;
    ZydisInstructionInfo info;
    ZydisConditionCodeInfo cc;
    ZydisVmExit exit_info;
    ZyanStatus status;

    if (!formatter || !instruction || !buffer || (operand_count && !operands) || (capacity == 0))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    status = ZydisFormatterFormatInstruction(formatter, instruction, operands,
        instruction->operand_count_visible, line, sizeof(line), runtime_address, ZYAN_NULL);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    while (line[text_length] != '\0')
    {
        ++text_length;
    }
    used = text_length;
    status = ZydisGetInstructionInfo(instruction, operands, operand_count, &info);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    status = ZydisVerboseText(line, sizeof(line), &used, " ; flow ");
    if (ZYAN_FAILED(status) || (info.flow > ZYDIS_INSTRUCTION_FLOW_PRIVILEGED))
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }
    status = ZydisVerboseText(line, sizeof(line), &used, ZYDIS_VERBOSE_FLOW[info.flow]);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    if (ZYAN_SUCCESS(ZydisGetConditionCode(instruction->mnemonic, &cc)) &&
        (cc.code <= ZYDIS_CONDITION_CODE_G))
    {
        status = ZydisVerboseText(line, sizeof(line), &used, " cc ");
        if (ZYAN_FAILED(status))
        {
            return status;
        }
        status = ZydisVerboseText(line, sizeof(line), &used, ZYDIS_VERBOSE_CC[cc.code]);
        if (ZYAN_FAILED(status))
        {
            return status;
        }
    }
    if (info.intercept > ZYDIS_INSTRUCTION_INTERCEPT_SVM)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    status = ZydisVerboseText(line, sizeof(line), &used, " intercept ");
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    status = ZydisVerboseText(line, sizeof(line), &used, ZYDIS_VERBOSE_INTERCEPT[info.intercept]);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    status = ZydisGetVmExit(instruction, operands, operand_count, &exit_info);
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    status = ZydisVerboseText(line, sizeof(line), &used, " vmx ");
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    if (!exit_info.vmx)
    {
        status = ZydisVerboseText(line, sizeof(line), &used, "none");
    }
    else if (exit_info.vmx_control > ZYDIS_VMX_CONTROL_XSS)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = ZydisVerboseDec(line, sizeof(line), &used, exit_info.vmx_reason);
        if (ZYAN_FAILED(status))
        {
            return status;
        }
        status = ZydisVerboseText(line, sizeof(line), &used, " ");
        if (ZYAN_FAILED(status))
        {
            return status;
        }
        status = ZydisVerboseText(line, sizeof(line), &used, ZYDIS_VERBOSE_VMX[exit_info.vmx_control]);
    }
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    status = ZydisVerboseText(line, sizeof(line), &used, " svm ");
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    if (!exit_info.svm)
    {
        status = ZydisVerboseText(line, sizeof(line), &used, "none");
    }
    else if (exit_info.svm_control > ZYDIS_SVM_CONTROL_DR)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    else
    {
        status = ZydisVerboseHex(line, sizeof(line), &used, exit_info.svm_code);
        if (ZYAN_FAILED(status))
        {
            return status;
        }
        status = ZydisVerboseText(line, sizeof(line), &used, " ");
        if (ZYAN_FAILED(status))
        {
            return status;
        }
        status = ZydisVerboseText(line, sizeof(line), &used, ZYDIS_VERBOSE_SVM[exit_info.svm_control]);
    }
    if (ZYAN_FAILED(status))
    {
        return status;
    }
    if (capacity <= used)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }
    ZYAN_MEMCPY(buffer, line, used);
    buffer[used] = '\0';
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
