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

#include <Zydis/ConditionCode.h>

/* ============================================================================================== */
/* Tables                                                                                         */
/* ============================================================================================== */

static const ZydisAccessedFlagsMask ZYDIS_CC_FLAGS[] =
{
    ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_CF,
    ZYDIS_CPUFLAG_CF,
    ZYDIS_CPUFLAG_ZF,
    ZYDIS_CPUFLAG_ZF,
    ZYDIS_CPUFLAG_CF | ZYDIS_CPUFLAG_ZF,
    ZYDIS_CPUFLAG_CF | ZYDIS_CPUFLAG_ZF,
    ZYDIS_CPUFLAG_SF,
    ZYDIS_CPUFLAG_SF,
    ZYDIS_CPUFLAG_PF,
    ZYDIS_CPUFLAG_PF,
    ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_ZF | ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF,
    ZYDIS_CPUFLAG_ZF | ZYDIS_CPUFLAG_SF | ZYDIS_CPUFLAG_OF
};

ZYAN_STATIC_ASSERT(ZYDIS_CONDITION_CODE_O == 0);
ZYAN_STATIC_ASSERT(ZYDIS_CONDITION_CODE_G == 15);
ZYAN_STATIC_ASSERT((sizeof(ZYDIS_CC_FLAGS) / sizeof(ZYDIS_CC_FLAGS[0])) ==
    (ZYDIS_CONDITION_CODE_G + 1));

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisGetConditionCode(ZydisMnemonic mnemonic, ZydisConditionCodeInfo* info)
{
    ZydisConditionCode code;

    if (!info)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    switch (mnemonic)
    {
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_SETO:
    case ZYDIS_MNEMONIC_CMOVO:
    case ZYDIS_MNEMONIC_SETZUO:
        code = ZYDIS_CONDITION_CODE_O;
        break;
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_SETNO:
    case ZYDIS_MNEMONIC_CMOVNO:
    case ZYDIS_MNEMONIC_SETZUNO:
        code = ZYDIS_CONDITION_CODE_NO;
        break;
    case ZYDIS_MNEMONIC_JB:
    case ZYDIS_MNEMONIC_SETB:
    case ZYDIS_MNEMONIC_CMOVB:
    case ZYDIS_MNEMONIC_SETZUB:
        code = ZYDIS_CONDITION_CODE_B;
        break;
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_SETNB:
    case ZYDIS_MNEMONIC_CMOVNB:
    case ZYDIS_MNEMONIC_SETZUNB:
        code = ZYDIS_CONDITION_CODE_AE;
        break;
    case ZYDIS_MNEMONIC_JZ:
    case ZYDIS_MNEMONIC_SETZ:
    case ZYDIS_MNEMONIC_CMOVZ:
    case ZYDIS_MNEMONIC_SETZUZ:
        code = ZYDIS_CONDITION_CODE_E;
        break;
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_SETNZ:
    case ZYDIS_MNEMONIC_CMOVNZ:
    case ZYDIS_MNEMONIC_SETZUNZ:
        code = ZYDIS_CONDITION_CODE_NE;
        break;
    case ZYDIS_MNEMONIC_JBE:
    case ZYDIS_MNEMONIC_SETBE:
    case ZYDIS_MNEMONIC_CMOVBE:
    case ZYDIS_MNEMONIC_SETZUBE:
        code = ZYDIS_CONDITION_CODE_BE;
        break;
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_SETNBE:
    case ZYDIS_MNEMONIC_CMOVNBE:
    case ZYDIS_MNEMONIC_SETZUNBE:
        code = ZYDIS_CONDITION_CODE_A;
        break;
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_SETS:
    case ZYDIS_MNEMONIC_CMOVS:
    case ZYDIS_MNEMONIC_SETZUS:
        code = ZYDIS_CONDITION_CODE_S;
        break;
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_SETNS:
    case ZYDIS_MNEMONIC_CMOVNS:
    case ZYDIS_MNEMONIC_SETZUNS:
        code = ZYDIS_CONDITION_CODE_NS;
        break;
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_SETP:
    case ZYDIS_MNEMONIC_CMOVP:
    case ZYDIS_MNEMONIC_SETZUP:
        code = ZYDIS_CONDITION_CODE_P;
        break;
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_SETNP:
    case ZYDIS_MNEMONIC_CMOVNP:
    case ZYDIS_MNEMONIC_SETZUNP:
        code = ZYDIS_CONDITION_CODE_NP;
        break;
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_SETL:
    case ZYDIS_MNEMONIC_CMOVL:
    case ZYDIS_MNEMONIC_SETZUL:
        code = ZYDIS_CONDITION_CODE_L;
        break;
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_SETNL:
    case ZYDIS_MNEMONIC_CMOVNL:
    case ZYDIS_MNEMONIC_SETZUNL:
        code = ZYDIS_CONDITION_CODE_GE;
        break;
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_SETLE:
    case ZYDIS_MNEMONIC_CMOVLE:
    case ZYDIS_MNEMONIC_SETZULE:
        code = ZYDIS_CONDITION_CODE_LE;
        break;
    case ZYDIS_MNEMONIC_JNLE:
    case ZYDIS_MNEMONIC_SETNLE:
    case ZYDIS_MNEMONIC_CMOVNLE:
    case ZYDIS_MNEMONIC_SETZUNLE:
        code = ZYDIS_CONDITION_CODE_G;
        break;
    default:
        return ZYAN_STATUS_NOT_FOUND;
    }

    info->code = code;
    info->tested = ZYDIS_CC_FLAGS[code];
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisConditionCodeEvaluate(ZydisConditionCode code, ZyanU32 eflags, ZyanBool* value)
{
    ZyanU8 cf;
    ZyanU8 pf;
    ZyanU8 zf;
    ZyanU8 sf;
    ZyanU8 of;
    ZyanU8 result;

    if (!value)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    cf = (eflags & ZYDIS_CPUFLAG_CF) ? 1 : 0;
    pf = (eflags & ZYDIS_CPUFLAG_PF) ? 1 : 0;
    zf = (eflags & ZYDIS_CPUFLAG_ZF) ? 1 : 0;
    sf = (eflags & ZYDIS_CPUFLAG_SF) ? 1 : 0;
    of = (eflags & ZYDIS_CPUFLAG_OF) ? 1 : 0;

    switch (code)
    {
    case ZYDIS_CONDITION_CODE_O:
        result = of;
        break;
    case ZYDIS_CONDITION_CODE_NO:
        result = of ? 0 : 1;
        break;
    case ZYDIS_CONDITION_CODE_B:
        result = cf;
        break;
    case ZYDIS_CONDITION_CODE_AE:
        result = cf ? 0 : 1;
        break;
    case ZYDIS_CONDITION_CODE_E:
        result = zf;
        break;
    case ZYDIS_CONDITION_CODE_NE:
        result = zf ? 0 : 1;
        break;
    case ZYDIS_CONDITION_CODE_BE:
        result = (cf || zf) ? 1 : 0;
        break;
    case ZYDIS_CONDITION_CODE_A:
        result = (!cf && !zf) ? 1 : 0;
        break;
    case ZYDIS_CONDITION_CODE_S:
        result = sf;
        break;
    case ZYDIS_CONDITION_CODE_NS:
        result = sf ? 0 : 1;
        break;
    case ZYDIS_CONDITION_CODE_P:
        result = pf;
        break;
    case ZYDIS_CONDITION_CODE_NP:
        result = pf ? 0 : 1;
        break;
    case ZYDIS_CONDITION_CODE_L:
        result = (sf != of) ? 1 : 0;
        break;
    case ZYDIS_CONDITION_CODE_GE:
        result = (sf == of) ? 1 : 0;
        break;
    case ZYDIS_CONDITION_CODE_LE:
        result = (zf || (sf != of)) ? 1 : 0;
        break;
    case ZYDIS_CONDITION_CODE_G:
        result = (!zf && (sf == of)) ? 1 : 0;
        break;
    default:
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    *value = result ? ZYAN_TRUE : ZYAN_FALSE;
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
