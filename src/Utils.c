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
#include <Zydis/Utils.h>

/* ============================================================================================== */
/* Exported functions                                                                             */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Address calculation                                                                            */
/* ---------------------------------------------------------------------------------------------- */

// Signed integer overflow is expected behavior in this function, for wrapping around the
// instruction pointer on jumps right at the end of the address space.
ZYAN_NO_SANITIZE("signed-integer-overflow")
ZyanStatus ZydisCalcAbsoluteAddress(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address, ZyanU64* result_address)
{
    if (!instruction || !operand || !result_address)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    switch (operand->type)
    {
    case ZYDIS_OPERAND_TYPE_MEMORY:
        if (!operand->mem.disp.size)
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }
        if (operand->mem.base == ZYDIS_REGISTER_EIP)
        {
            *result_address = ((ZyanU32)runtime_address + instruction->length +
                (ZyanU32)operand->mem.disp.value);
            return ZYAN_STATUS_SUCCESS;
        }
        if (operand->mem.base == ZYDIS_REGISTER_RIP)
        {
            *result_address = (ZyanU64)(runtime_address + instruction->length +
                operand->mem.disp.value);
            return ZYAN_STATUS_SUCCESS;
        }
        if ((operand->mem.base == ZYDIS_REGISTER_NONE) &&
            (operand->mem.index == ZYDIS_REGISTER_NONE))
        {
            switch (instruction->address_width)
            {
            case 16:
                *result_address = (ZyanU64)operand->mem.disp.value & 0x000000000000FFFF;
                return ZYAN_STATUS_SUCCESS;
            case 32:
                *result_address = (ZyanU64)operand->mem.disp.value & 0x00000000FFFFFFFF;
                return ZYAN_STATUS_SUCCESS;
            case 64:
                *result_address = (ZyanU64)operand->mem.disp.value;
                return ZYAN_STATUS_SUCCESS;
            default:
                return ZYAN_STATUS_INVALID_ARGUMENT;
            }
        }
        break;
    case ZYDIS_OPERAND_TYPE_IMMEDIATE:
        if (!operand->imm.is_address)
        {
            return ZYAN_STATUS_INVALID_ARGUMENT;
        }

        if (operand->imm.is_signed && operand->imm.is_relative)
        {
            *result_address = (ZyanU64)((ZyanI64)runtime_address + instruction->length +
                operand->imm.value.s);
            switch (instruction->machine_mode)
            {
            case ZYDIS_MACHINE_MODE_LONG_COMPAT_16:
            case ZYDIS_MACHINE_MODE_LEGACY_16:
            case ZYDIS_MACHINE_MODE_REAL_16:
            case ZYDIS_MACHINE_MODE_LONG_COMPAT_32:
            case ZYDIS_MACHINE_MODE_LEGACY_32:
                // `XBEGIN` is a special case as it doesn't truncate computed address
                // This behavior is documented by Intel (SDM Vol. 2C):
                // Use of the 16-bit operand size does not cause this address to be truncated to
                // 16 bits, unlike a near jump to a relative offset.
                if ((instruction->operand_width == 16) &&
                    (instruction->mnemonic != ZYDIS_MNEMONIC_XBEGIN))
                {
                    *result_address &= 0xFFFF;
                }
                break;
            case ZYDIS_MACHINE_MODE_LONG_64:
                break;
            default:
                return ZYAN_STATUS_INVALID_ARGUMENT;
            }
            return ZYAN_STATUS_SUCCESS;
        }
        else if (!operand->imm.is_signed && !operand->imm.is_relative && 
            (instruction->machine_mode == ZYDIS_MACHINE_MODE_LONG_64))
        {
            *result_address = operand->imm.value.u;
            return ZYAN_STATUS_SUCCESS;
        }
        break;
    default:
        break;
    }

    return ZYAN_STATUS_INVALID_ARGUMENT;
}

ZyanStatus ZydisCalcAbsoluteAddressEx(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address,
    const ZydisRegisterContext* register_context, ZyanU64* result_address)
{
    // TODO: Add support for Gather/Scatter instructions

    if (!instruction || !operand || !register_context || !result_address)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    // `MIB` operands (`BNDLDX`/`BNDSTX`) have `MPX`-specific addressing semantics that
    // don't map to a single effective address, so they are not supported here
    if ((operand->type == ZYDIS_OPERAND_TYPE_MEMORY) &&
        (operand->mem.type == ZYDIS_MEMOP_TYPE_MIB))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    if ((operand->type != ZYDIS_OPERAND_TYPE_REGISTER) &&
        ((operand->type != ZYDIS_OPERAND_TYPE_MEMORY) ||
        ((operand->mem.base == ZYDIS_REGISTER_NONE) &&
         (operand->mem.index == ZYDIS_REGISTER_NONE)) ||
        (operand->mem.base == ZYDIS_REGISTER_EIP) ||
        (operand->mem.base == ZYDIS_REGISTER_RIP)))
    {
        return ZydisCalcAbsoluteAddress(instruction, operand, runtime_address, result_address);
    }

    ZyanU64 value;
    if (operand->type == ZYDIS_OPERAND_TYPE_REGISTER)
    {
        value = register_context->values[operand->reg.value];
    }
    else if (operand->type == ZYDIS_OPERAND_TYPE_MEMORY)
    {
        value = operand->mem.disp.value;
        if (operand->mem.base)
        {
            value += register_context->values[operand->mem.base];
        }
        if (operand->mem.index)
        {
            value += register_context->values[operand->mem.index] * operand->mem.scale;
        }
    }
    else
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    switch (instruction->address_width)
    {
    case 16:
        *result_address = value & 0x000000000000FFFF;
        return ZYAN_STATUS_SUCCESS;
    case 32:
        *result_address = value & 0x00000000FFFFFFFF;
        return ZYAN_STATUS_SUCCESS;
    case 64:
        *result_address = value;
        return ZYAN_STATUS_SUCCESS;
    default:
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
}

ZYAN_NO_SANITIZE("unsigned-integer-overflow")
ZyanStatus ZydisCalcAbsoluteAddressSeg(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operand, ZyanU64 runtime_address,
    const ZydisRegisterContext* register_context, ZyanU64 segment_base, ZyanU64* result_address)
{
    ZyanStatus status;
    ZyanU64 effective;

    if (!result_address)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    /*
     * `ZydisCalcAbsoluteAddressEx` computes the effective address (the
     * `base + index*scale + displacement` inside the segment) and never adds a
     * segment base, so it returns a flat/effective address. A guest linear
     * address for a non-flat segment (`fs:`/`gs:`, or any real-mode or V8086
     * segment) is that effective address plus the segment base the caller
     * looked up for `operand->mem.segment`. Fold it in here so the caller does
     * not silently lose it. A `segment_base` of 0 reproduces the flat result.
     */
    status = ZydisCalcAbsoluteAddressEx(instruction, operand, runtime_address, register_context,
        &effective);
    if (ZYAN_FAILED(status))
    {
        return status;
    }

    *result_address = effective + segment_base;
    return ZYAN_STATUS_SUCCESS;
}

/* ---------------------------------------------------------------------------------------------- */
/* Constant offsets                                                                               */
/* ---------------------------------------------------------------------------------------------- */

static ZyanStatus ZydisSetConstantField(ZyanU8 size_bits, ZyanU8 offset, ZyanU8* size_bytes,
    ZyanU8* out_offset)
{
    if (!size_bits)
    {
        return ZYAN_STATUS_SUCCESS;
    }
    if ((size_bits % 8) != 0)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    *size_bytes = (ZyanU8)(size_bits / 8);
    *out_offset = offset;
    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisGetConstantOffsets(const ZydisDecodedInstruction* instruction,
    ZydisConstantOffsets* offsets)
{
    ZyanStatus status;

    if (!instruction || !offsets)
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    ZYAN_MEMSET(offsets, 0, sizeof(*offsets));

    status = ZydisSetConstantField(instruction->raw.disp.size, instruction->raw.disp.offset,
        &offsets->displacement_size, &offsets->displacement_offset);
    if (ZYAN_FAILED(status))
    {
        return status;
    }

    status = ZydisSetConstantField(instruction->raw.imm[0].size, instruction->raw.imm[0].offset,
        &offsets->immediate_size, &offsets->immediate_offset);
    if (ZYAN_FAILED(status))
    {
        return status;
    }

    return ZydisSetConstantField(instruction->raw.imm[1].size, instruction->raw.imm[1].offset,
        &offsets->immediate_size2, &offsets->immediate_offset2);
}

/* ---------------------------------------------------------------------------------------------- */
/* Signature mask                                                                                 */
/* ---------------------------------------------------------------------------------------------- */

ZyanStatus ZydisGetSignatureMask(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, ZyanU8* mask,
    ZyanUSize mask_capacity)
{
    ZyanU8 i;
    ZyanU8 k;
    ZyanBool disp_is_address = ZYAN_FALSE;

    if (!instruction || !mask || (operand_count && !operands))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }
    if (mask_capacity < instruction->length)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }

    /* Keep every byte by default: prefixes, opcode, ModRM, SIB, and genuine constants. */
    for (i = 0; i < instruction->length; ++i)
    {
        mask[i] = 0;
    }

    /*
     * A displacement is an address to wildcard when it is the whole address: RIP/EIP-relative,
     * or an absolute address with no base register. With no base, the displacement is the base
     * address itself (`[disp]`, or a global array's `[index*scale + disp]`), so an index does not
     * make it a struct offset. A displacement added to a base register is a struct or array
     * offset and stays in the pattern. That shape lives on the memory operand, so the policy
     * needs the operands; without them the displacement is kept.
     */
    for (i = 0; i < operand_count; ++i)
    {
        if (operands[i].type != ZYDIS_OPERAND_TYPE_MEMORY)
        {
            continue;
        }
        if ((operands[i].mem.base == ZYDIS_REGISTER_RIP) ||
            (operands[i].mem.base == ZYDIS_REGISTER_EIP) ||
            (operands[i].mem.base == ZYDIS_REGISTER_NONE))
        {
            disp_is_address = ZYAN_TRUE;
            break;
        }
    }

    if (disp_is_address && instruction->raw.disp.size)
    {
        ZyanU8 off = instruction->raw.disp.offset;
        ZyanU8 n = (ZyanU8)(instruction->raw.disp.size / 8);
        for (k = 0; k < n; ++k)
        {
            if ((ZyanUSize)(off + k) < instruction->length)
            {
                mask[off + k] = 1;
            }
        }
    }

    /* A relative (branch target) or address immediate is wildcarded; a plain constant stays. */
    for (k = 0; k < 2; ++k)
    {
        if (instruction->raw.imm[k].size &&
            (instruction->raw.imm[k].is_relative || instruction->raw.imm[k].is_address))
        {
            ZyanU8 off = instruction->raw.imm[k].offset;
            ZyanU8 n = (ZyanU8)(instruction->raw.imm[k].size / 8);
            ZyanU8 j;
            for (j = 0; j < n; ++j)
            {
                if ((ZyanUSize)(off + j) < instruction->length)
                {
                    mask[off + j] = 1;
                }
            }
        }
    }

    return ZYAN_STATUS_SUCCESS;
}

ZyanStatus ZydisFormatSignature(const ZydisDecodedInstruction* instruction,
    const ZydisDecodedOperand* operands, ZyanU8 operand_count, const ZyanU8* bytes,
    char* buffer, ZyanUSize capacity)
{
    static const char digits[] = "0123456789ABCDEF";
    ZyanU8 mask[ZYDIS_MAX_INSTRUCTION_LENGTH];
    ZyanStatus status;
    ZyanUSize used = 0;
    ZyanU8 i;

    if (!instruction || !bytes || !buffer || (capacity == 0))
    {
        return ZYAN_STATUS_INVALID_ARGUMENT;
    }

    status = ZydisGetSignatureMask(instruction, operands, operand_count, mask, sizeof(mask));
    if (ZYAN_FAILED(status))
    {
        return status;
    }

    /*
     * The output is exactly `length * 3` bytes: two hex digits per byte, one space between
     * bytes, and the terminator (2*length + (length - 1) + 1). Check once so a valid buffer is
     * never rejected and a short one is caught before any byte is written.
     */
    if (capacity < (ZyanUSize)instruction->length * 3)
    {
        return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE;
    }

    for (i = 0; i < instruction->length; ++i)
    {
        if (i)
        {
            buffer[used++] = ' ';
        }
        if (mask[i])
        {
            buffer[used++] = '?';
            buffer[used++] = '?';
        }
        else
        {
            buffer[used++] = digits[(bytes[i] >> 4) & 0x0F];
            buffer[used++] = digits[bytes[i] & 0x0F];
        }
    }
    buffer[used] = '\0';
    return ZYAN_STATUS_SUCCESS;
}

/* ============================================================================================== */
