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
 * Checks ZydisGetVmExit and ZydisFormatVerbose.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

static int g_failures = 0;

static void Fail(const char* label, const char* what)
{
    printf("FAIL %s: %s\n", label, what);
    ++g_failures;
}

static int Decode(const ZyanU8* bytes, ZyanUSize length, ZydisDecodedInstruction* instruction,
    ZydisDecodedOperand* operands)
{
    ZydisDecoder decoder;

    return !ZYAN_FAILED(ZydisDecoderInit(&decoder, ZYDIS_MACHINE_MODE_LONG_64,
            ZYDIS_STACK_WIDTH_64)) &&
        !ZYAN_FAILED(ZydisDecoderDecodeFull(&decoder, bytes, length, instruction, operands)) &&
        (instruction->length == length);
}

static void ExpectExit(const char* label, const ZyanU8* bytes, ZyanUSize length,
    ZydisMnemonic mnemonic, ZyanBool vmx, ZyanU16 vmx_reason, ZydisVmxControl vmx_control,
    ZyanBool svm, ZyanU16 svm_code, ZydisSvmControl svm_control)
{
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisVmExit exit_info;

    if (!Decode(bytes, length, &instruction, operands))
    {
        Fail(label, "decode");
        return;
    }
    if (instruction.mnemonic != mnemonic)
    {
        Fail(label, "mnemonic");
        return;
    }
    if (ZYAN_FAILED(ZydisGetVmExit(&instruction, operands, instruction.operand_count, &exit_info)) ||
        (exit_info.vmx != vmx) || (exit_info.vmx_reason != vmx_reason) ||
        (exit_info.vmx_control != vmx_control) || (exit_info.svm != svm) ||
        (exit_info.svm_code != svm_code) || (exit_info.svm_control != svm_control))
    {
        printf("FAIL %s: vmx %u %u %u svm %u %u %u\n", label,
            (unsigned)exit_info.vmx, (unsigned)exit_info.vmx_reason,
            (unsigned)exit_info.vmx_control, (unsigned)exit_info.svm,
            (unsigned)exit_info.svm_code, (unsigned)exit_info.svm_control);
        ++g_failures;
    }
}

static int Contains(const char* text, const char* needle)
{
    return strstr(text, needle) != ZYAN_NULL;
}

static void ExpectLine(const char* label, const ZyanU8* bytes, ZyanUSize length, const char* needle)
{
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisFormatter formatter;
    char text[256];

    if (!Decode(bytes, length, &instruction, operands) ||
        ZYAN_FAILED(ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL)) ||
        ZYAN_FAILED(ZydisFormatVerbose(&formatter, &instruction, operands, instruction.operand_count,
            0x140000000ull, text, sizeof(text))) ||
        !Contains(text, needle))
    {
        printf("FAIL %s: line %s\n", label, text);
        ++g_failures;
    }
}

int main(void)
{
    static const ZyanU8 cpuid[] = { 0x0F, 0xA2 };
    static const ZyanU8 hlt[] = { 0xF4 };
    static const ZyanU8 in_eax[] = { 0xED };
    static const ZyanU8 rdmsr[] = { 0x0F, 0x32 };
    static const ZyanU8 lgdt[] = { 0x0F, 0x01, 0x10 };
    static const ZyanU8 vmcall[] = { 0x0F, 0x01, 0xC1 };
    static const ZyanU8 mov_from_cr3[] = { 0x0F, 0x20, 0xD8 };
    static const ZyanU8 mov_to_cr3[] = { 0x0F, 0x22, 0xD8 };
    static const ZyanU8 mov_from_dr0[] = { 0x0F, 0x21, 0xC0 };
    static const ZyanU8 mov_rr[] = { 0x48, 0x89, 0xC8 };
    static const ZyanU8 pushfq[] = { 0x9C };
    static const ZyanU8 jz[] = { 0x74, 0x00 };
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisInstructionInfo info;
    ZydisFormatter formatter;
    char tiny[4];
    ZyanUSize i;

    if (!ZYAN_FAILED(ZydisGetVmExit(ZYAN_NULL, ZYAN_NULL, 0, ZYAN_NULL)))
    {
        Fail("null", "arguments");
    }

    ExpectExit("cpuid", cpuid, sizeof(cpuid), ZYDIS_MNEMONIC_CPUID, ZYAN_TRUE, 10,
        ZYDIS_VMX_CONTROL_CPUID, ZYAN_TRUE, 0x072, ZYDIS_SVM_CONTROL_INTERCEPT);
    ExpectExit("hlt", hlt, sizeof(hlt), ZYDIS_MNEMONIC_HLT, ZYAN_TRUE, 12,
        ZYDIS_VMX_CONTROL_HLT, ZYAN_TRUE, 0x078, ZYDIS_SVM_CONTROL_INTERCEPT);
    ExpectExit("in", in_eax, sizeof(in_eax), ZYDIS_MNEMONIC_IN, ZYAN_TRUE, 30,
        ZYDIS_VMX_CONTROL_IO, ZYAN_TRUE, 0x07b, ZYDIS_SVM_CONTROL_INTERCEPT);
    ExpectExit("rdmsr", rdmsr, sizeof(rdmsr), ZYDIS_MNEMONIC_RDMSR, ZYAN_TRUE, 31,
        ZYDIS_VMX_CONTROL_MSR, ZYAN_TRUE, 0x07c, ZYDIS_SVM_CONTROL_INTERCEPT);
    ExpectExit("lgdt", lgdt, sizeof(lgdt), ZYDIS_MNEMONIC_LGDT, ZYAN_TRUE, 46,
        ZYDIS_VMX_CONTROL_DESCRIPTOR, ZYAN_TRUE, 0x06b, ZYDIS_SVM_CONTROL_INTERCEPT);
    ExpectExit("vmcall", vmcall, sizeof(vmcall), ZYDIS_MNEMONIC_VMCALL, ZYAN_TRUE, 18,
        ZYDIS_VMX_CONTROL_UNCONDITIONAL, ZYAN_FALSE, 0, ZYDIS_SVM_CONTROL_NONE);
    ExpectExit("mov cr3", mov_from_cr3, sizeof(mov_from_cr3), ZYDIS_MNEMONIC_MOV, ZYAN_TRUE, 28,
        ZYDIS_VMX_CONTROL_CR3_STORE, ZYAN_TRUE, 0x003, ZYDIS_SVM_CONTROL_CR);
    ExpectExit("mov to cr3", mov_to_cr3, sizeof(mov_to_cr3), ZYDIS_MNEMONIC_MOV, ZYAN_TRUE, 28,
        ZYDIS_VMX_CONTROL_CR3_LOAD, ZYAN_TRUE, 0x013, ZYDIS_SVM_CONTROL_CR);
    ExpectExit("mov dr0", mov_from_dr0, sizeof(mov_from_dr0), ZYDIS_MNEMONIC_MOV, ZYAN_TRUE, 29,
        ZYDIS_VMX_CONTROL_MOV_DR, ZYAN_TRUE, 0x020, ZYDIS_SVM_CONTROL_DR);
    ExpectExit("mov rr", mov_rr, sizeof(mov_rr), ZYDIS_MNEMONIC_MOV, ZYAN_FALSE, 0,
        ZYDIS_VMX_CONTROL_NONE, ZYAN_FALSE, 0, ZYDIS_SVM_CONTROL_NONE);
    ExpectExit("pushfq", pushfq, sizeof(pushfq), ZYDIS_MNEMONIC_PUSHFQ, ZYAN_FALSE, 0,
        ZYDIS_VMX_CONTROL_NONE, ZYAN_TRUE, 0x070, ZYDIS_SVM_CONTROL_INTERCEPT);

    if (!Decode(cpuid, sizeof(cpuid), &instruction, operands) ||
        ZYAN_FAILED(ZydisGetInstructionInfo(&instruction, operands, instruction.operand_count,
            &info)) ||
        (info.flow != ZYDIS_INSTRUCTION_FLOW_NEXT) ||
        (info.intercept != ZYDIS_INSTRUCTION_INTERCEPT_NONE))
    {
        Fail("cpuid", "standard info");
    }

    ExpectLine("cpuid line", cpuid, sizeof(cpuid), "flow next");
    ExpectLine("cpuid line", cpuid, sizeof(cpuid), "intercept none");
    ExpectLine("cpuid line", cpuid, sizeof(cpuid), "vmx 10 cpuid");
    ExpectLine("cpuid line", cpuid, sizeof(cpuid), "svm 0x72 intercept");
    ExpectLine("jz line", jz, sizeof(jz), "cc e");
    ExpectLine("mov line", mov_rr, sizeof(mov_rr), "vmx none");
    ExpectLine("mov line", mov_rr, sizeof(mov_rr), "svm none");

    for (i = 0; i < sizeof(tiny); ++i)
    {
        tiny[i] = 'X';
    }
    if (!Decode(cpuid, sizeof(cpuid), &instruction, operands) ||
        ZYAN_FAILED(ZydisFormatterInit(&formatter, ZYDIS_FORMATTER_STYLE_INTEL)) ||
        !ZYAN_FAILED(ZydisFormatVerbose(&formatter, &instruction, operands, instruction.operand_count,
            0, tiny, sizeof(tiny))) ||
        (tiny[0] != 'X'))
    {
        Fail("small", "buffer");
    }

    /* VMX and SVM control enum-string accessors. */
    {
        const char* s = ZydisVmxControlGetString(ZYDIS_VMX_CONTROL_CPUID);
        if (!s || strcmp(s, "cpuid"))
        {
            Fail("vmx string", "cpuid");
        }
        s = ZydisVmxControlGetString(ZYDIS_VMX_CONTROL_XSS);
        if (!s || strcmp(s, "xss"))
        {
            Fail("vmx string", "xss");
        }
        if (ZydisVmxControlGetString((ZydisVmxControl)(ZYDIS_VMX_CONTROL_MAX_VALUE + 1)))
        {
            Fail("vmx string", "out-of-range accepted");
        }
        s = ZydisSvmControlGetString(ZYDIS_SVM_CONTROL_CR);
        if (!s || strcmp(s, "cr"))
        {
            Fail("svm string", "cr");
        }
        if (ZydisSvmControlGetString((ZydisSvmControl)(ZYDIS_SVM_CONTROL_MAX_VALUE + 1)))
        {
            Fail("svm string", "out-of-range accepted");
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphVmExit: ok\n");
    return 0;
}
