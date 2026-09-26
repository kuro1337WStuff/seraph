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
#include <Zydis/Register.h>
#include <Zydis/VmExit.h>

/* ============================================================================================== */
/* Helpers                                                                                        */
/* ============================================================================================== */

static void ZydisVmExitClear(ZydisVmExit* result)
{
    result->vmx = ZYAN_FALSE;
    result->vmx_reason = 0;
    result->vmx_control = ZYDIS_VMX_CONTROL_NONE;
    result->svm = ZYAN_FALSE;
    result->svm_code = 0;
    result->svm_control = ZYDIS_SVM_CONTROL_NONE;
}

static void ZydisVmExitVmx(ZydisVmExit* result, ZyanU16 reason, ZydisVmxControl control)
{
    result->vmx = ZYAN_TRUE;
    result->vmx_reason = reason;
    result->vmx_control = control;
}

static void ZydisVmExitSvm(ZydisVmExit* result, ZyanU16 code, ZydisSvmControl control)
{
    result->svm = ZYAN_TRUE;
    result->svm_code = code;
    result->svm_control = control;
}

static void ZydisVmExitBoth(ZydisVmExit* result, ZyanU16 vmx_reason, ZydisVmxControl vmx_control,
    ZyanU16 svm_code)
{
    ZydisVmExitVmx(result, vmx_reason, vmx_control);
    ZydisVmExitSvm(result, svm_code, ZYDIS_SVM_CONTROL_INTERCEPT);
}

static ZyanBool ZydisVmExitSystemReg(const ZydisDecodedOperand* operands, ZyanU8 operand_count,
    ZydisRegisterClass reg_class, ZydisRegister* reg, ZyanBool* is_write)
{
    ZyanU8 i;

    for (i = 0; i < operand_count; ++i)
    {
        if (operands[i].type != ZYDIS_OPERAND_TYPE_REGISTER)
        {
            continue;
        }
        if (ZydisRegisterGetClass(operands[i].reg.value) != reg_class)
        {
            continue;
        }
        *reg = operands[i].reg.value;
        *is_write = (operands[i].actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) ? ZYAN_TRUE : ZYAN_FALSE;
        return ZYAN_TRUE;
    }
    return ZYAN_FALSE;
}

static void ZydisVmExitMovCr(ZydisVmExit* result, ZydisRegister reg, ZyanBool is_write)
{
    ZyanU8 index;

    if ((reg < ZYDIS_REGISTER_CR0) || (reg > ZYDIS_REGISTER_CR15))
    {
        return;
    }
    index = (ZyanU8)((int)reg - (int)ZYDIS_REGISTER_CR0);
    ZydisVmExitSvm(result, (ZyanU16)(is_write ? (0x010u + index) : index), ZYDIS_SVM_CONTROL_CR);
    if ((index == 0) || (index == 4))
    {
        ZydisVmExitVmx(result, 28, ZYDIS_VMX_CONTROL_CR_MASK);
    }
    else if (index == 3)
    {
        ZydisVmExitVmx(result, 28, is_write ? ZYDIS_VMX_CONTROL_CR3_LOAD : ZYDIS_VMX_CONTROL_CR3_STORE);
    }
    else if (index == 8)
    {
        ZydisVmExitVmx(result, 28, is_write ? ZYDIS_VMX_CONTROL_CR8_LOAD : ZYDIS_VMX_CONTROL_CR8_STORE);
    }
}

static void ZydisVmExitMovDr(ZydisVmExit* result, ZydisRegister reg, ZyanBool is_write)
{
    ZyanU8 index;

    if ((reg < ZYDIS_REGISTER_DR0) || (reg > ZYDIS_REGISTER_DR7))
    {
        return;
    }
    index = (ZyanU8)((int)reg - (int)ZYDIS_REGISTER_DR0);
    ZydisVmExitVmx(result, 29, ZYDIS_VMX_CONTROL_MOV_DR);
    ZydisVmExitSvm(result, (ZyanU16)(is_write ? (0x030u + index) : (0x020u + index)),
        ZYDIS_SVM_CONTROL_DR);
}

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisGetVmExit(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZydisVmExit* result)
{
    ZydisRegister reg;
    ZyanBool is_write;

    if (!instruction || !result || (operand_count && !operands))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    ZydisVmExitClear(result);

    if (instruction->mnemonic == ZYDIS_MNEMONIC_MOV)
    {
        if (ZydisVmExitSystemReg(operands, operand_count, ZYDIS_REGCLASS_CONTROL, &reg, &is_write))
        {
            ZydisVmExitMovCr(result, reg, is_write);
        }
        else if (ZydisVmExitSystemReg(operands, operand_count, ZYDIS_REGCLASS_DEBUG, &reg, &is_write))
        {
            ZydisVmExitMovDr(result, reg, is_write);
        }
        return ZYAN_STATUS_SUCCESS;
    }

    switch (instruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_CPUID:
        ZydisVmExitBoth(result, 10, ZYDIS_VMX_CONTROL_CPUID, 0x072);
        break;
    case ZYDIS_MNEMONIC_HLT:
        ZydisVmExitBoth(result, 12, ZYDIS_VMX_CONTROL_HLT, 0x078);
        break;
    case ZYDIS_MNEMONIC_GETSEC:
        ZydisVmExitVmx(result, 11, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_INVD:
        ZydisVmExitBoth(result, 13, ZYDIS_VMX_CONTROL_UNCONDITIONAL, 0x076);
        break;
    case ZYDIS_MNEMONIC_INVLPG:
        ZydisVmExitBoth(result, 14, ZYDIS_VMX_CONTROL_INVLPG, 0x079);
        break;
    case ZYDIS_MNEMONIC_RDPMC:
        ZydisVmExitBoth(result, 15, ZYDIS_VMX_CONTROL_RDPMC, 0x06f);
        break;
    case ZYDIS_MNEMONIC_RDTSC:
        ZydisVmExitBoth(result, 16, ZYDIS_VMX_CONTROL_RDTSC, 0x06e);
        break;
    case ZYDIS_MNEMONIC_RDTSCP:
        ZydisVmExitBoth(result, 51, ZYDIS_VMX_CONTROL_RDTSC, 0x087);
        break;
    case ZYDIS_MNEMONIC_VMCALL:
        ZydisVmExitVmx(result, 18, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMCLEAR:
        ZydisVmExitVmx(result, 19, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMLAUNCH:
        ZydisVmExitVmx(result, 20, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMPTRLD:
        ZydisVmExitVmx(result, 21, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMPTRST:
        ZydisVmExitVmx(result, 22, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMREAD:
        ZydisVmExitVmx(result, 23, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMRESUME:
        ZydisVmExitVmx(result, 24, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMWRITE:
        ZydisVmExitVmx(result, 25, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMXOFF:
        ZydisVmExitVmx(result, 26, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_VMXON:
        ZydisVmExitVmx(result, 27, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_IN:
    case ZYDIS_MNEMONIC_OUT:
    case ZYDIS_MNEMONIC_INSB:
    case ZYDIS_MNEMONIC_INSW:
    case ZYDIS_MNEMONIC_INSD:
    case ZYDIS_MNEMONIC_OUTSB:
    case ZYDIS_MNEMONIC_OUTSW:
    case ZYDIS_MNEMONIC_OUTSD:
        ZydisVmExitBoth(result, 30, ZYDIS_VMX_CONTROL_IO, 0x07b);
        break;
    case ZYDIS_MNEMONIC_RDMSR:
        ZydisVmExitBoth(result, 31, ZYDIS_VMX_CONTROL_MSR, 0x07c);
        break;
    case ZYDIS_MNEMONIC_WRMSR:
    case ZYDIS_MNEMONIC_WRMSRNS:
        ZydisVmExitBoth(result, 32, ZYDIS_VMX_CONTROL_MSR, 0x07c);
        break;
    case ZYDIS_MNEMONIC_MWAIT:
        ZydisVmExitBoth(result, 36, ZYDIS_VMX_CONTROL_MWAIT, 0x08b);
        break;
    case ZYDIS_MNEMONIC_MONITOR:
        ZydisVmExitBoth(result, 39, ZYDIS_VMX_CONTROL_MONITOR, 0x08a);
        break;
    case ZYDIS_MNEMONIC_PAUSE:
        ZydisVmExitBoth(result, 40, ZYDIS_VMX_CONTROL_PAUSE, 0x077);
        break;
    case ZYDIS_MNEMONIC_INVEPT:
        ZydisVmExitVmx(result, 50, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_INVVPID:
        ZydisVmExitVmx(result, 53, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        break;
    case ZYDIS_MNEMONIC_WBINVD:
        ZydisVmExitBoth(result, 54, ZYDIS_VMX_CONTROL_WBINVD, 0x089);
        break;
    case ZYDIS_MNEMONIC_XSETBV:
        ZydisVmExitVmx(result, 55, ZYDIS_VMX_CONTROL_UNCONDITIONAL);
        ZydisVmExitSvm(result, 0x08d, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_RDRAND:
        ZydisVmExitVmx(result, 57, ZYDIS_VMX_CONTROL_RDRAND);
        break;
    case ZYDIS_MNEMONIC_INVPCID:
        ZydisVmExitVmx(result, 58, ZYDIS_VMX_CONTROL_INVPCID);
        ZydisVmExitSvm(result, 0x0a2, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_RDSEED:
        ZydisVmExitVmx(result, 61, ZYDIS_VMX_CONTROL_RDSEED);
        break;
    case ZYDIS_MNEMONIC_XSAVES:
    case ZYDIS_MNEMONIC_XSAVES64:
        ZydisVmExitVmx(result, 63, ZYDIS_VMX_CONTROL_XSS);
        break;
    case ZYDIS_MNEMONIC_XRSTORS:
    case ZYDIS_MNEMONIC_XRSTORS64:
        ZydisVmExitVmx(result, 64, ZYDIS_VMX_CONTROL_XSS);
        break;
    case ZYDIS_MNEMONIC_CLTS:
    case ZYDIS_MNEMONIC_LMSW:
        ZydisVmExitVmx(result, 28, ZYDIS_VMX_CONTROL_CR_MASK);
        ZydisVmExitSvm(result, 0x010, ZYDIS_SVM_CONTROL_CR);
        break;
    case ZYDIS_MNEMONIC_INVLPGA:
        ZydisVmExitSvm(result, 0x07a, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_VMRUN:
        ZydisVmExitSvm(result, 0x080, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_VMMCALL:
        ZydisVmExitSvm(result, 0x081, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_VMLOAD:
        ZydisVmExitSvm(result, 0x082, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_VMSAVE:
        ZydisVmExitSvm(result, 0x083, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_STGI:
        ZydisVmExitSvm(result, 0x084, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_CLGI:
        ZydisVmExitSvm(result, 0x085, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_SKINIT:
        ZydisVmExitSvm(result, 0x086, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_PUSHF:
    case ZYDIS_MNEMONIC_PUSHFD:
    case ZYDIS_MNEMONIC_PUSHFQ:
        ZydisVmExitSvm(result, 0x070, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_POPF:
    case ZYDIS_MNEMONIC_POPFD:
    case ZYDIS_MNEMONIC_POPFQ:
        ZydisVmExitSvm(result, 0x071, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_IRET:
    case ZYDIS_MNEMONIC_IRETD:
    case ZYDIS_MNEMONIC_IRETQ:
        ZydisVmExitSvm(result, 0x074, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_INT:
        ZydisVmExitSvm(result, 0x075, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_SGDT:
        ZydisVmExitVmx(result, 46, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x067, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_SIDT:
        ZydisVmExitVmx(result, 46, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x066, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_LGDT:
        ZydisVmExitVmx(result, 46, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x06b, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_LIDT:
        ZydisVmExitVmx(result, 46, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x06a, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_SLDT:
        ZydisVmExitVmx(result, 47, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x068, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_STR:
        ZydisVmExitVmx(result, 47, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x069, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_LLDT:
        ZydisVmExitVmx(result, 47, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x06c, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    case ZYDIS_MNEMONIC_LTR:
        ZydisVmExitVmx(result, 47, ZYDIS_VMX_CONTROL_DESCRIPTOR);
        ZydisVmExitSvm(result, 0x06d, ZYDIS_SVM_CONTROL_INTERCEPT);
        break;
    default:
        break;
    }
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
/* Enum strings                                                                                   */
/* ============================================================================================== */

static const char* const ZYDIS_VMX_CONTROL_NAMES[] =
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

static const char* const ZYDIS_SVM_CONTROL_NAMES[] =
{
    "none",
    "intercept",
    "cr",
    "dr"
};

ZYAN_STATIC_ASSERT((sizeof(ZYDIS_VMX_CONTROL_NAMES) / sizeof(ZYDIS_VMX_CONTROL_NAMES[0])) ==
    (ZYDIS_VMX_CONTROL_MAX_VALUE + 1));
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_SVM_CONTROL_NAMES) / sizeof(ZYDIS_SVM_CONTROL_NAMES[0])) ==
    (ZYDIS_SVM_CONTROL_MAX_VALUE + 1));

const char* ZydisVmxControlGetString(ZydisVmxControl control)
{
    if ((ZyanUSize)control > ZYDIS_VMX_CONTROL_MAX_VALUE)
    {
        return ZYAN_NULL;
    }
    return ZYDIS_VMX_CONTROL_NAMES[control];
}

const char* ZydisSvmControlGetString(ZydisSvmControl control)
{
    if ((ZyanUSize)control > ZYDIS_SVM_CONTROL_MAX_VALUE)
    {
        return ZYAN_NULL;
    }
    return ZYDIS_SVM_CONTROL_NAMES[control];
}

/* ============================================================================================== */
