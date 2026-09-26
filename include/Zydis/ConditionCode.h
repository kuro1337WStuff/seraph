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
 * Condition codes for jcc, setcc, and cmovcc.
 *
 * The numeric value is the low nibble of the Jcc opcode. `jz` is E, `jnb` is
 * AE, `jnbe` is A, `jnl` is GE, and `jnle` is G. APX `setzu*` uses the same
 * code as the matching `setcc`. A mnemonic outside that set, including
 * `jcxz`, `loop`, `fcmov*`, `ccmp*`, and `ctest*`, returns
 * `ZYAN_STATUS_NOT_FOUND`.
 */

#ifndef ZYDIS_CONDITION_CODE_H
#define ZYDIS_CONDITION_CODE_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/DecoderTypes.h>
#include <Zydis/Mnemonic.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Enums and types                                                                                */
/* ============================================================================================== */

/**
 * Canonical condition. Values 0 through 15 match the Jcc opcode nibble.
 */
typedef enum ZydisConditionCode_
{
    ZYDIS_CONDITION_CODE_O = 0,   /* OF = 1 */
    ZYDIS_CONDITION_CODE_NO,      /* OF = 0 */
    ZYDIS_CONDITION_CODE_B,       /* CF = 1 */
    ZYDIS_CONDITION_CODE_AE,      /* CF = 0 */
    ZYDIS_CONDITION_CODE_E,       /* ZF = 1 */
    ZYDIS_CONDITION_CODE_NE,      /* ZF = 0 */
    ZYDIS_CONDITION_CODE_BE,      /* CF = 1 or ZF = 1 */
    ZYDIS_CONDITION_CODE_A,       /* CF = 0 and ZF = 0 */
    ZYDIS_CONDITION_CODE_S,       /* SF = 1 */
    ZYDIS_CONDITION_CODE_NS,      /* SF = 0 */
    ZYDIS_CONDITION_CODE_P,       /* PF = 1 */
    ZYDIS_CONDITION_CODE_NP,      /* PF = 0 */
    ZYDIS_CONDITION_CODE_L,       /* SF != OF */
    ZYDIS_CONDITION_CODE_GE,      /* SF = OF */
    ZYDIS_CONDITION_CODE_LE,      /* ZF = 1 or SF != OF */
    ZYDIS_CONDITION_CODE_G,       /* ZF = 0 and SF = OF */

    ZYDIS_CONDITION_CODE_MAX_VALUE = ZYDIS_CONDITION_CODE_G,
    ZYDIS_CONDITION_CODE_REQUIRED_BITS =
        ZYAN_BITS_TO_REPRESENT(ZYDIS_CONDITION_CODE_MAX_VALUE)
} ZydisConditionCode;

/**
 * Condition code for one mnemonic, plus the flags its predicate reads.
 *
 * `tested` uses `ZYDIS_CPUFLAG_*` bits. Those bits are the EFLAGS positions.
 */
typedef struct ZydisConditionCodeInfo_
{
    ZydisConditionCode code;
    ZydisAccessedFlagsMask tested;
} ZydisConditionCodeInfo;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/**
 * Maps a jcc, setcc, cmovcc, or APX setzu mnemonic to its condition code.
 *
 * `info` is left unchanged when the status is not success.
 *
 * @param   mnemonic    Instruction mnemonic.
 * @param   info        Receives the canonical code and the flags it reads.
 *
 * @return  `ZYAN_STATUS_SUCCESS`, `ZYAN_STATUS_INVALID_ARGUMENT` when `info`
 *          is null, or `ZYAN_STATUS_NOT_FOUND` for any other mnemonic.
 */
ZYDIS_EXPORT ZyanStatus ZydisGetConditionCode(ZydisMnemonic mnemonic,
    ZydisConditionCodeInfo* info);

/**
 * Evaluates a condition against an EFLAGS value.
 *
 * `eflags` uses the same bit positions as `ZYDIS_CPUFLAG_*`.
 *
 * @param   code        Condition returned by `ZydisGetConditionCode`.
 * @param   eflags      Flag register value.
 * @param   value       Receives `ZYAN_TRUE` when the predicate holds.
 *
 * @return  A zyan status code. `value` is left unchanged on failure.
 */
ZYDIS_EXPORT ZyanStatus ZydisConditionCodeEvaluate(ZydisConditionCode code, ZyanU32 eflags,
    ZyanBool* value);

/**
 * Returns the human-readable name for a condition code (`e`, `ne`, `a`, ...).
 *
 * @param   code    The `ZydisConditionCode` value.
 *
 * @return  A static, null-terminated string, or `ZYAN_NULL` for an invalid value.
 */
ZYDIS_EXPORT const char* ZydisConditionCodeGetString(ZydisConditionCode code);

/**
 * Inverts the condition of a `jcc`, `setcc`, `cmovcc`, or APX `setzu`
 * instruction directly in its own encoded bytes, in place.
 *
 * The instruction length is preserved: only the low bit of the opcode's tttn
 * nibble is flipped (for example `jz` becomes `jnz`, `setle` becomes `setg`).
 * A mnemonic that carries no condition code (`jcxz`, `loop`, `fcmov*`, an
 * ordinary `mov`, ...) is rejected with `ZYAN_STATUS_NOT_FOUND` and the buffer
 * is left untouched, so the helper cannot corrupt an unrelated instruction.
 *
 * @param   instruction The decoded instruction that `buffer` holds.
 * @param   buffer      The instruction's own bytes. Modified in place on success.
 * @param   length      Number of valid bytes in `buffer`; must be at least
 *                      `instruction->length`.
 *
 * @return  `ZYAN_STATUS_SUCCESS`, `ZYAN_STATUS_INVALID_ARGUMENT` for a null
 *          argument, `ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE` when `length` is
 *          too small, or `ZYAN_STATUS_NOT_FOUND` for a non-conditional
 *          instruction.
 */
ZYDIS_EXPORT ZyanStatus ZydisNegateConditionInPlace(const ZydisDecodedInstruction* instruction,
    ZyanU8* buffer, ZyanUSize length);

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_CONDITION_CODE_H */
