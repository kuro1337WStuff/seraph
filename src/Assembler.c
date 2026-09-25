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
#include <Zydis/Assembler.h>
#include <Zydis/Register.h>

/* ============================================================================================== */
/* Slots                                                                                          */
/* ============================================================================================== */

static ZyanStatus ZydisAsmBegin(ZydisAsm* assembler, ZydisEncoderRequest* request)
{
    if (!assembler || !request)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (assembler->count >= ZYDIS_BLOCK_MAX_SLOTS)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }

    ZYAN_MEMSET(request, 0, sizeof(*request));
    request->machine_mode = assembler->machine_mode;
    if (assembler->lock_next)
    {
        request->prefixes |= ZYDIS_ATTRIB_HAS_LOCK;
        assembler->lock_next = ZYAN_FALSE;
    }
    return ZYAN_STATUS_SUCCESS;
}

static ZyanStatus ZydisAsmCommit(ZydisAsm* assembler, const ZydisEncoderRequest* request,
    ZyanU32 branch_label, ZyanU8 branch_operand)
{
    ZydisBlockSlot* slot = &assembler->slots[assembler->count];

    ZYAN_MEMSET(slot, 0, sizeof(*slot));
    slot->kind = ZYDIS_BLOCK_SLOT_INSTR;
    slot->request = *request;
    slot->branch_label = branch_label;
    slot->branch_operand = branch_operand;
    if (assembler->pending_label)
    {
        slot->label_id = assembler->pending_label;
        assembler->pending_label = 0;
    }
    ++assembler->count;
    return ZYAN_STATUS_SUCCESS;
}

static ZyanU16 ZydisAsmRegBytes(ZydisMachineMode mode, ZydisRegister reg)
{
    ZydisRegisterWidth bits = ZydisRegisterGetWidth(mode, reg);

    if (!bits || (bits % 8))
    {
        return 0;
    }
    return (ZyanU16)(bits / 8);
}

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisAsmInit(ZydisAsm* assembler, ZydisMachineMode machine_mode)
{
    if (!assembler)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    ZYAN_MEMSET(assembler, 0, sizeof(*assembler));
    assembler->machine_mode = machine_mode;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisAsmMovRegReg(ZydisAsm* assembler, ZydisRegister dest, ZydisRegister src)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);

    if (ZYAN_FAILED(status))
    {
        return status;
    }
    request.mnemonic = ZYDIS_MNEMONIC_MOV;
    request.operand_count = 2;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[0].reg.value = dest;
    request.operands[1].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[1].reg.value = src;
    return ZydisAsmCommit(assembler, &request, 0, 0);
}

ZyanStatus ZydisAsmMovRegImm(ZydisAsm* assembler, ZydisRegister dest, ZyanU64 imm)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);

    if (ZYAN_FAILED(status))
    {
        return status;
    }
    request.mnemonic = ZYDIS_MNEMONIC_MOV;
    request.operand_count = 2;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[0].reg.value = dest;
    request.operands[1].type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
    request.operands[1].imm.u = imm;
    return ZydisAsmCommit(assembler, &request, 0, 0);
}

ZyanStatus ZydisAsmMovRegMem(ZydisAsm* assembler, ZydisRegister dest, ZydisRegister base,
    ZydisRegister index, ZyanU8 scale, ZyanI64 disp)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);
    ZyanU16 size;

    if (ZYAN_FAILED(status))
    {
        return status;
    }
    size = ZydisAsmRegBytes(assembler->machine_mode, dest);
    if (!size)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    request.mnemonic = ZYDIS_MNEMONIC_MOV;
    request.operand_count = 2;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[0].reg.value = dest;
    request.operands[1].type = ZYDIS_OPERAND_TYPE_MEMORY;
    request.operands[1].mem.base = base;
    request.operands[1].mem.index = index;
    request.operands[1].mem.scale = index == ZYDIS_REGISTER_NONE ? 0 : scale;
    request.operands[1].mem.displacement = disp;
    request.operands[1].mem.size = size;
    return ZydisAsmCommit(assembler, &request, 0, 0);
}

ZyanStatus ZydisAsmAddRegReg(ZydisAsm* assembler, ZydisRegister dest, ZydisRegister src)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);

    if (ZYAN_FAILED(status))
    {
        return status;
    }
    request.mnemonic = ZYDIS_MNEMONIC_ADD;
    request.operand_count = 2;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[0].reg.value = dest;
    request.operands[1].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[1].reg.value = src;
    return ZydisAsmCommit(assembler, &request, 0, 0);
}

ZyanStatus ZydisAsmAddMemReg(ZydisAsm* assembler, ZydisRegister base, ZydisRegister src)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);
    ZyanU16 size;

    if (ZYAN_FAILED(status))
    {
        return status;
    }
    size = ZydisAsmRegBytes(assembler->machine_mode, src);
    if (!size)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    request.mnemonic = ZYDIS_MNEMONIC_ADD;
    request.operand_count = 2;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_MEMORY;
    request.operands[0].mem.base = base;
    request.operands[0].mem.size = size;
    request.operands[1].type = ZYDIS_OPERAND_TYPE_REGISTER;
    request.operands[1].reg.value = src;
    return ZydisAsmCommit(assembler, &request, 0, 0);
}

ZyanStatus ZydisAsmRet(ZydisAsm* assembler)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);

    if (ZYAN_FAILED(status))
    {
        return status;
    }
    request.mnemonic = ZYDIS_MNEMONIC_RET;
    return ZydisAsmCommit(assembler, &request, 0, 0);
}

ZyanStatus ZydisAsmDb(ZydisAsm* assembler, const ZyanU8* bytes, ZyanU8 size)
{
    ZydisBlockSlot* slot;

    if (!assembler || (!bytes && size))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (assembler->count >= ZYDIS_BLOCK_MAX_SLOTS)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }
    slot = &assembler->slots[assembler->count];
    ZYAN_MEMSET(slot, 0, sizeof(*slot));
    slot->kind = ZYDIS_BLOCK_SLOT_BYTES;
    slot->bytes = bytes;
    slot->byte_count = size;
    if (assembler->pending_label)
    {
        slot->label_id = assembler->pending_label;
        assembler->pending_label = 0;
    }
    ++assembler->count;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisAsmLabel(ZydisAsm* assembler, ZyanU32 label_id)
{
    if (!assembler || !label_id)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    assembler->pending_label = label_id;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisAsmJmpLabel(ZydisAsm* assembler, ZyanU32 label_id)
{
    ZydisEncoderRequest request;
    ZyanStatus status = ZydisAsmBegin(assembler, &request);

    if (ZYAN_FAILED(status) || !label_id)
    {
        return label_id ? status : ZYAN_STATUS_INVALID_ARGUMENT;
    }
    request.mnemonic = ZYDIS_MNEMONIC_JMP;
    request.operand_count = 1;
    request.operands[0].type = ZYDIS_OPERAND_TYPE_IMMEDIATE;
    return ZydisAsmCommit(assembler, &request, label_id, 0);
}

ZyanStatus ZydisAsmLock(ZydisAsm* assembler)
{
    if (!assembler)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    assembler->lock_next = ZYAN_TRUE;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisAsmEncode(ZydisAsm* assembler, ZyanU64 base_ip, ZyanU8* out, ZyanUSize out_cap,
    ZyanUSize* written)
{
    if (!assembler || assembler->pending_label)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    return ZydisBlockEncode(assembler->machine_mode, base_ip, assembler->slots, assembler->count,
        out, out_cap, written, 0);
}

/* ============================================================================================== */
