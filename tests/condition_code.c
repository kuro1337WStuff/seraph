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
 * Checks condition codes for jcc, setcc, cmovcc, and APX setzu.
 */

#include <stdio.h>
#include <string.h>

#include <Zydis/Zydis.h>

static int g_failures = 0;

static const ZydisMnemonic k_jcc[16] =
{
    ZYDIS_MNEMONIC_JO, ZYDIS_MNEMONIC_JNO, ZYDIS_MNEMONIC_JB, ZYDIS_MNEMONIC_JNB,
    ZYDIS_MNEMONIC_JZ, ZYDIS_MNEMONIC_JNZ, ZYDIS_MNEMONIC_JBE, ZYDIS_MNEMONIC_JNBE,
    ZYDIS_MNEMONIC_JS, ZYDIS_MNEMONIC_JNS, ZYDIS_MNEMONIC_JP, ZYDIS_MNEMONIC_JNP,
    ZYDIS_MNEMONIC_JL, ZYDIS_MNEMONIC_JNL, ZYDIS_MNEMONIC_JLE, ZYDIS_MNEMONIC_JNLE
};

static const ZydisMnemonic k_set[16] =
{
    ZYDIS_MNEMONIC_SETO, ZYDIS_MNEMONIC_SETNO, ZYDIS_MNEMONIC_SETB, ZYDIS_MNEMONIC_SETNB,
    ZYDIS_MNEMONIC_SETZ, ZYDIS_MNEMONIC_SETNZ, ZYDIS_MNEMONIC_SETBE, ZYDIS_MNEMONIC_SETNBE,
    ZYDIS_MNEMONIC_SETS, ZYDIS_MNEMONIC_SETNS, ZYDIS_MNEMONIC_SETP, ZYDIS_MNEMONIC_SETNP,
    ZYDIS_MNEMONIC_SETL, ZYDIS_MNEMONIC_SETNL, ZYDIS_MNEMONIC_SETLE, ZYDIS_MNEMONIC_SETNLE
};

static const ZydisMnemonic k_cmov[16] =
{
    ZYDIS_MNEMONIC_CMOVO, ZYDIS_MNEMONIC_CMOVNO, ZYDIS_MNEMONIC_CMOVB, ZYDIS_MNEMONIC_CMOVNB,
    ZYDIS_MNEMONIC_CMOVZ, ZYDIS_MNEMONIC_CMOVNZ, ZYDIS_MNEMONIC_CMOVBE, ZYDIS_MNEMONIC_CMOVNBE,
    ZYDIS_MNEMONIC_CMOVS, ZYDIS_MNEMONIC_CMOVNS, ZYDIS_MNEMONIC_CMOVP, ZYDIS_MNEMONIC_CMOVNP,
    ZYDIS_MNEMONIC_CMOVL, ZYDIS_MNEMONIC_CMOVNL, ZYDIS_MNEMONIC_CMOVLE, ZYDIS_MNEMONIC_CMOVNLE
};

static const ZydisMnemonic k_setzu[16] =
{
    ZYDIS_MNEMONIC_SETZUO, ZYDIS_MNEMONIC_SETZUNO, ZYDIS_MNEMONIC_SETZUB, ZYDIS_MNEMONIC_SETZUNB,
    ZYDIS_MNEMONIC_SETZUZ, ZYDIS_MNEMONIC_SETZUNZ, ZYDIS_MNEMONIC_SETZUBE, ZYDIS_MNEMONIC_SETZUNBE,
    ZYDIS_MNEMONIC_SETZUS, ZYDIS_MNEMONIC_SETZUNS, ZYDIS_MNEMONIC_SETZUP, ZYDIS_MNEMONIC_SETZUNP,
    ZYDIS_MNEMONIC_SETZUL, ZYDIS_MNEMONIC_SETZUNL, ZYDIS_MNEMONIC_SETZULE, ZYDIS_MNEMONIC_SETZUNLE
};

static const ZydisAccessedFlagsMask k_flags[16] =
{
    ZYDIS_CPUFLAG_OF, ZYDIS_CPUFLAG_OF, ZYDIS_CPUFLAG_CF, ZYDIS_CPUFLAG_CF,
    ZYDIS_CPUFLAG_ZF, ZYDIS_CPUFLAG_ZF, ZYDIS_CPUFLAG_CF | ZYDIS_CPUFLAG_ZF,
    ZYDIS_CPUFLAG_CF | ZYDIS_CPUFLAG_ZF, ZYDIS_CPUFLAG_SF, ZYDIS_CPUFLAG_SF,
    ZYDIS_CPUFLAG_PF, ZYDIS_CPUFLAG_PF, ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_ZF | ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_ZF | ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF
};

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

static void ExpectMnemonic(const char* label, ZydisMnemonic mnemonic, ZydisConditionCode code)
{
    ZydisConditionCodeInfo info;

    info.code = (ZydisConditionCode)7;
    info.tested = 0xFFFFFFFFu;
    if (ZYAN_FAILED(ZydisGetConditionCode(mnemonic, &info)) || (info.code != code) ||
        (info.tested != k_flags[code]))
    {
        Fail(label, "mnemonic");
    }
}

static void ExpectDecoded(const char* label, const ZyanU8* bytes, ZyanUSize length,
    ZydisMnemonic mnemonic, ZydisConditionCode code, int check_nibble, int check_info_flags)
{
    ZydisDecodedInstruction instruction;
    ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
    ZydisInstructionInfo flags;

    if (!Decode(bytes, length, &instruction, operands))
    {
        Fail(label, "decode");
        return;
    }
    if (instruction.mnemonic != mnemonic)
    {
        Fail(label, "decoded mnemonic");
        return;
    }
    if (check_nibble && ((ZyanU8)(instruction.opcode & 0x0F) != (ZyanU8)code))
    {
        Fail(label, "opcode nibble");
    }
    ExpectMnemonic(label, instruction.mnemonic, code);
    if (check_info_flags)
    {
        if (ZYAN_FAILED(ZydisGetInstructionInfo(&instruction, operands, instruction.operand_count,
                &flags)) || (flags.flags_tested != k_flags[code]))
        {
            printf("FAIL %s: info flags %08lx\n", label, (unsigned long)flags.flags_tested);
            ++g_failures;
        }
    }
}

static void ExpectAbsent(ZydisMnemonic mnemonic)
{
    ZydisConditionCodeInfo info;

    info.code = (ZydisConditionCode)7;
    info.tested = 0xFFFFFFFFu;
    if (!ZYAN_FAILED(ZydisGetConditionCode(mnemonic, &info)) ||
        (info.code != (ZydisConditionCode)7) ||
        (info.tested != 0xFFFFFFFFu))
    {
        Fail("absent", "status");
    }
}

static void ExpectValue(ZydisConditionCode code, ZyanU32 eflags, int taken)
{
    ZyanBool value;

    value = taken ? ZYAN_FALSE : ZYAN_TRUE;
    if (ZYAN_FAILED(ZydisConditionCodeEvaluate(code, eflags, &value)) ||
        (value != (taken ? ZYAN_TRUE : ZYAN_FALSE)))
    {
        printf("FAIL eval code %u eflags %08lx\n", (unsigned)code, (unsigned long)eflags);
        ++g_failures;
    }
}

int main(void)
{
    int i;
    ZyanBool value;
    static const struct
    {
        ZyanU32 eflags;
        unsigned taken;
    } rows[] =
    {
        { 0, 0xAAAAu },
        { ZYDIS_CPUFLAG_ZF, 0x6A5Au },
        { ZYDIS_CPUFLAG_CF, 0xAA66u },
        { ZYDIS_CPUFLAG_SF, 0x59AAu },
        { ZYDIS_CPUFLAG_OF, 0x5AA9u },
        { ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF, 0xA9A9u },
        { ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF | ZYDIS_CPUFLAG_ZF, 0x6959u },
        { ZYDIS_CPUFLAG_PF, 0xA6AAu }
    };

    if (!ZYAN_FAILED(ZydisGetConditionCode(ZYDIS_MNEMONIC_JZ, ZYAN_NULL)) ||
        !ZYAN_FAILED(ZydisConditionCodeEvaluate(ZYDIS_CONDITION_CODE_E, 0, ZYAN_NULL)))
    {
        Fail("null", "arguments");
    }
    value = ZYAN_TRUE;
    if (!ZYAN_FAILED(ZydisConditionCodeEvaluate((ZydisConditionCode)16, 0, &value)) ||
        (value != ZYAN_TRUE))
    {
        Fail("range", "evaluate");
    }

    for (i = 0; i < 16; ++i)
    {
        ZyanU8 jcc[2];
        ZyanU8 setcc[3];
        ZyanU8 cmov[3];

        jcc[0] = (ZyanU8)(0x70 + i);
        jcc[1] = 0;
        setcc[0] = 0x0F;
        setcc[1] = (ZyanU8)(0x90 + i);
        setcc[2] = 0xC0;
        cmov[0] = 0x0F;
        cmov[1] = (ZyanU8)(0x40 + i);
        cmov[2] = 0xC0;
        ExpectDecoded("jcc", jcc, sizeof(jcc), k_jcc[i], (ZydisConditionCode)i, 1, 1);
        ExpectDecoded("setcc", setcc, sizeof(setcc), k_set[i], (ZydisConditionCode)i, 1, 0);
        ExpectDecoded("cmovcc", cmov, sizeof(cmov), k_cmov[i], (ZydisConditionCode)i, 1, 0);
        ExpectMnemonic("setzu", k_setzu[i], (ZydisConditionCode)i);
    }

    {
        static const ZyanU8 near_jz[] = { 0x0F, 0x84, 0x00, 0x00, 0x00, 0x00 };
        static const ZyanU8 jrcxz[] = { 0xE3, 0x00 };
        static const ZyanU8 loop[] = { 0xE2, 0x00 };
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

        ExpectDecoded("near jz", near_jz, sizeof(near_jz), ZYDIS_MNEMONIC_JZ,
            ZYDIS_CONDITION_CODE_E, 1, 1);
        if (!Decode(jrcxz, sizeof(jrcxz), &instruction, operands) ||
            (instruction.mnemonic != ZYDIS_MNEMONIC_JRCXZ))
        {
            Fail("jrcxz", "decode");
        }
        ExpectAbsent(ZYDIS_MNEMONIC_JRCXZ);
        if (!Decode(loop, sizeof(loop), &instruction, operands) ||
            (instruction.mnemonic != ZYDIS_MNEMONIC_LOOP))
        {
            Fail("loop", "decode");
        }
        ExpectAbsent(ZYDIS_MNEMONIC_LOOP);
        ExpectAbsent(ZYDIS_MNEMONIC_LOOPE);
        ExpectAbsent(ZYDIS_MNEMONIC_JMP);
        ExpectAbsent(ZYDIS_MNEMONIC_CCMPB);
        ExpectAbsent(ZYDIS_MNEMONIC_CTESTT);
        ExpectAbsent(ZYDIS_MNEMONIC_FCMOVB);
        ExpectAbsent(ZYDIS_MNEMONIC_JKZD);
    }

    for (i = 0; i < (int)(sizeof(rows) / sizeof(rows[0])); ++i)
    {
        int code;

        for (code = 0; code < 16; ++code)
        {
            ExpectValue((ZydisConditionCode)code, rows[i].eflags,
                (rows[i].taken & (1u << code)) ? 1 : 0);
        }
    }

    /* Condition-code enum-string accessor. */
    {
        const char* s = ZydisConditionCodeGetString(ZYDIS_CONDITION_CODE_E);
        if (!s || strcmp(s, "e"))
        {
            Fail("cc string", "e");
        }
        s = ZydisConditionCodeGetString(ZYDIS_CONDITION_CODE_G);
        if (!s || strcmp(s, "g"))
        {
            Fail("cc string", "g");
        }
        if (ZydisConditionCodeGetString((ZydisConditionCode)(ZYDIS_CONDITION_CODE_MAX_VALUE + 1)))
        {
            Fail("cc string", "out-of-range accepted");
        }
    }

    /* In-place condition negation: length preserved, family guarded. */
    {
        /* jz rel8 -> jnz rel8 */
        static const ZyanU8 jz[] = { 0x74, 0x00 };
        /* setle al -> setnle al */
        static const ZyanU8 setle[] = { 0x0F, 0x9E, 0xC0 };
        /* cmovz rax, rcx -> cmovnz rax, rcx (REX prefix exercises the opcode offset) */
        static const ZyanU8 cmovz[] = { 0x48, 0x0F, 0x44, 0xC1 };
        /* mov rax, rcx -> must be refused, bytes untouched */
        static const ZyanU8 mov[] = { 0x48, 0x89, 0xC8 };
        ZyanU8 buffer[ZYDIS_MAX_INSTRUCTION_LENGTH];
        ZydisDecodedInstruction instruction;
        ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];

        struct { const char* label; const ZyanU8* bytes; ZyanUSize length;
            ZydisMnemonic want; } neg[3] =
        {
            { "jz->jnz", jz, sizeof(jz), ZYDIS_MNEMONIC_JNZ },
            { "setle->setnle", setle, sizeof(setle), ZYDIS_MNEMONIC_SETNLE },
            { "cmovz->cmovnz", cmovz, sizeof(cmovz), ZYDIS_MNEMONIC_CMOVNZ }
        };
        int n;

        for (n = 0; n < 3; ++n)
        {
            if (!Decode(neg[n].bytes, neg[n].length, &instruction, operands))
            {
                Fail(neg[n].label, "decode");
                continue;
            }
            memcpy(buffer, neg[n].bytes, neg[n].length);
            if (ZYAN_FAILED(ZydisNegateConditionInPlace(&instruction, buffer, neg[n].length)))
            {
                Fail(neg[n].label, "negate status");
                continue;
            }
            if (!Decode(buffer, neg[n].length, &instruction, operands) ||
                (instruction.length != neg[n].length) ||
                (instruction.mnemonic != neg[n].want))
            {
                Fail(neg[n].label, "result");
            }
        }

        /* A non-conditional instruction must be refused and left untouched. */
        if (Decode(mov, sizeof(mov), &instruction, operands))
        {
            memcpy(buffer, mov, sizeof(mov));
            if (ZydisNegateConditionInPlace(&instruction, buffer, sizeof(mov)) !=
                    ZYAN_STATUS_NOT_FOUND ||
                memcmp(buffer, mov, sizeof(mov)) != 0)
            {
                Fail("mov negate", "not refused");
            }
        }
        else
        {
            Fail("mov negate", "decode");
        }
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphConditionCode: ok\n");
    return 0;
}
