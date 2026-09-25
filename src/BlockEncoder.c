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
#include <Zydis/BlockEncoder.h>

/* ============================================================================================== */
/* Layout                                                                                         */
/* ============================================================================================== */

static ZyanBool ZydisBlockFindLabel(const ZydisBlockSlot* slots, ZyanU8 slot_count,
    const ZyanUSize* offsets, ZyanU32 label_id, ZyanUSize* offset)
{
    ZyanU8 i;

    for (i = 0; i < slot_count; ++i)
    {
        if (slots[i].label_id == label_id)
        {
            *offset = offsets[i];
            return ZYAN_TRUE;
        }
    }
    return ZYAN_FALSE;
}

static ZyanStatus ZydisBlockEncodeSlot(ZydisMachineMode mode, ZyanU64 base_ip,
    const ZydisBlockSlot* slots, ZyanU8 slot_count, const ZyanUSize* offsets, ZyanU8 index,
    ZyanU8 width, ZyanU8* tmp, ZyanUSize* length)
{
    const ZydisBlockSlot* slot = &slots[index];
    ZydisEncoderRequest request;
    ZyanUSize label_offset;
    ZyanU64 absolute;
    ZydisEncoderOperand* operand;

    if (slot->kind == ZYDIS_BLOCK_SLOT_BYTES)
    {
        if (!slot->bytes && slot->byte_count)
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }
        *length = slot->byte_count;
        (void)tmp;
        return ZYAN_STATUS_SUCCESS;
    }

    if (slot->kind != ZYDIS_BLOCK_SLOT_INSTR)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    request = slot->request;
    request.machine_mode = mode;

    if (slot->branch_label)
    {
        if ((slot->branch_operand >= request.operand_count) ||
            !ZydisBlockFindLabel(slots, slot_count, offsets, slot->branch_label, &label_offset))
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }

        absolute = base_ip + (ZyanU64)label_offset;
        operand = &request.operands[slot->branch_operand];
        if (operand->type == ZYDIS_OPERAND_TYPE_IMMEDIATE)
        {
            operand->imm.u = absolute;
            if (width == 8)
            {
                request.branch_type = ZYDIS_BRANCH_TYPE_SHORT;
                request.branch_width = ZYDIS_BRANCH_WIDTH_8;
            }
            else
            {
                request.branch_type = ZYDIS_BRANCH_TYPE_NEAR;
                request.branch_width = ZYDIS_BRANCH_WIDTH_32;
            }
        }
        else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY)
        {
            operand->mem.displacement = (ZyanI64)absolute;
        }
        else
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }

        *length = ZYDIS_MAX_INSTRUCTION_LENGTH;
        return ZydisEncoderEncodeInstructionAbsolute(&request, tmp, length,
            base_ip + (ZyanU64)offsets[index]);
    }

    *length = ZYDIS_MAX_INSTRUCTION_LENGTH;
    return ZydisEncoderEncodeInstruction(&request, tmp, length);
}

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

ZyanStatus ZydisBlockEncode(ZydisMachineMode mode, ZyanU64 base_ip, const ZydisBlockSlot* slots,
    ZyanU8 slot_count, ZyanU8* out, ZyanUSize out_cap, ZyanUSize* written, ZyanU32 options)
{
    ZyanU8 widths[ZYDIS_BLOCK_MAX_SLOTS];
    ZyanUSize offsets[ZYDIS_BLOCK_MAX_SLOTS];
    ZyanUSize lengths[ZYDIS_BLOCK_MAX_SLOTS];
    ZyanU8 encoded[ZYDIS_BLOCK_MAX_SLOTS][ZYDIS_MAX_INSTRUCTION_LENGTH];
    ZyanU8 pass;
    ZyanU8 i;
    ZyanUSize total;
    const ZyanU8 pass_limit = (ZyanU8)(slot_count * 2 + 2);

    if (!slots || !out || !written || !slot_count || (slot_count > ZYDIS_BLOCK_MAX_SLOTS))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_MEMSET(widths, 8, sizeof(widths));
    ZYAN_MEMSET(lengths, 0, sizeof(lengths));
    ZYAN_MEMSET(offsets, 0, sizeof(offsets));

    for (pass = 0; pass < pass_limit; ++pass)
    {
        ZyanBool changed = ZYAN_FALSE;
        ZyanUSize ip = 0;

        for (i = 0; i < slot_count; ++i)
        {
            offsets[i] = ip;
            ip += lengths[i];
        }

        for (i = 0; i < slot_count; ++i)
        {
            ZyanUSize length = 0;
            ZyanStatus status;
            const ZyanBool branch = (slots[i].kind == ZYDIS_BLOCK_SLOT_INSTR) &&
                slots[i].branch_label &&
                (slots[i].branch_operand < slots[i].request.operand_count) &&
                (slots[i].request.operands[slots[i].branch_operand].type ==
                    ZYDIS_OPERAND_TYPE_IMMEDIATE);

            status = ZydisBlockEncodeSlot(mode, base_ip, slots, slot_count, offsets, i,
                branch ? widths[i] : 0, encoded[i], &length);
            if (ZYAN_FAILED(status) && branch && (widths[i] == 8))
            {
                if (options & ZYDIS_BLOCK_ENCODER_DONT_FIX_BRANCHES)
                {
                    return ZYAN_STATUS_FAILED;
                }
                widths[i] = 32;
                changed = ZYAN_TRUE;
                status = ZydisBlockEncodeSlot(mode, base_ip, slots, slot_count, offsets, i, 32,
                    encoded[i], &length);
            }
            if (ZYAN_FAILED(status))
            {
                return status;
            }
            if (length != lengths[i])
            {
                lengths[i] = length;
                changed = ZYAN_TRUE;
            }
        }

        if (!changed && (pass > 0))
        {
            break;
        }
    }

    total = 0;
    for (i = 0; i < slot_count; ++i)
    {
        offsets[i] = total;
        total += lengths[i];
    }
    if (total > out_cap)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }

    for (i = 0; i < slot_count; ++i)
    {
        if (!lengths[i])
        {
            continue;
        }
        if (slots[i].kind == ZYDIS_BLOCK_SLOT_BYTES)
        {
            ZYAN_MEMCPY(out + offsets[i], slots[i].bytes, lengths[i]);
        }
        else
        {
            ZYAN_MEMCPY(out + offsets[i], encoded[i], lengths[i]);
        }
    }
    *written = total;
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
