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
 * Small C assembler that fills `ZydisBlockSlot` values and encodes them.
 */

#ifndef ZYDIS_ASSEMBLER_H
#define ZYDIS_ASSEMBLER_H

#include <Zycore/Defines.h>
#include <Zycore/Types.h>
#include <Zydis/BlockEncoder.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================================== */
/* Types                                                                                          */
/* ============================================================================================== */

typedef struct ZydisAsm_
{
    ZydisMachineMode machine_mode;
    ZydisBlockSlot slots[ZYDIS_BLOCK_MAX_SLOTS];
    ZyanU8 count;
    ZyanBool lock_next;
    ZyanU32 pending_label;
} ZydisAsm;

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZYDIS_EXPORT ZyanStatus ZydisAsmInit(ZydisAsm* assembler, ZydisMachineMode machine_mode);
ZYDIS_EXPORT ZyanStatus ZydisAsmMovRegReg(ZydisAsm* assembler, ZydisRegister dest,
    ZydisRegister src);
ZYDIS_EXPORT ZyanStatus ZydisAsmMovRegImm(ZydisAsm* assembler, ZydisRegister dest, ZyanU64 imm);
ZYDIS_EXPORT ZyanStatus ZydisAsmMovRegMem(ZydisAsm* assembler, ZydisRegister dest,
    ZydisRegister base, ZydisRegister index, ZyanU8 scale, ZyanI64 disp);
ZYDIS_EXPORT ZyanStatus ZydisAsmAddRegReg(ZydisAsm* assembler, ZydisRegister dest,
    ZydisRegister src);
ZYDIS_EXPORT ZyanStatus ZydisAsmAddMemReg(ZydisAsm* assembler, ZydisRegister base,
    ZydisRegister src);
ZYDIS_EXPORT ZyanStatus ZydisAsmRet(ZydisAsm* assembler);
ZYDIS_EXPORT ZyanStatus ZydisAsmDb(ZydisAsm* assembler, const ZyanU8* bytes, ZyanU8 size);
ZYDIS_EXPORT ZyanStatus ZydisAsmLabel(ZydisAsm* assembler, ZyanU32 label_id);
ZYDIS_EXPORT ZyanStatus ZydisAsmJmpLabel(ZydisAsm* assembler, ZyanU32 label_id);
ZYDIS_EXPORT ZyanStatus ZydisAsmLock(ZydisAsm* assembler);
ZYDIS_EXPORT ZyanStatus ZydisAsmEncode(ZydisAsm* assembler, ZyanU64 base_ip, ZyanU8* out,
    ZyanUSize out_cap, ZyanUSize* written);

/* ============================================================================================== */

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_ASSEMBLER_H */
