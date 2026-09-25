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
 * Inlined register-class lookup for the decoder hot path.
 */

#ifndef ZYDIS_INTERNAL_REGISTER_ENCODE_H
#define ZYDIS_INTERNAL_REGISTER_ENCODE_H

#include <Zycore/Defines.h>
#include <Zydis/Register.h>

typedef struct ZydisRegisterClassLookupItem_
{
    ZydisRegister lo;
    ZydisRegister hi;
    ZydisRegisterWidth width;
    ZydisRegisterWidth width64;
} ZydisRegisterClassLookupItem;

#include <Generated/RegisterClassLookup.inc>

ZYAN_INLINE ZydisRegister ZydisRegisterEncodeInline(ZydisRegisterClass register_class, ZyanU8 id)
{
    if ((register_class == ZYDIS_REGCLASS_INVALID) ||
        (register_class == ZYDIS_REGCLASS_FLAGS) ||
        (register_class == ZYDIS_REGCLASS_IP))
    {
        return ZYDIS_REGISTER_NONE;
    }
    if ((ZyanUSize)register_class >= (sizeof(REG_CLASS_LOOKUP) / sizeof(REG_CLASS_LOOKUP[0])))
    {
        return ZYDIS_REGISTER_NONE;
    }

    if (id <= (REG_CLASS_LOOKUP[register_class].hi - REG_CLASS_LOOKUP[register_class].lo))
    {
        return (ZydisRegister)(REG_CLASS_LOOKUP[register_class].lo + id);
    }
    return ZYDIS_REGISTER_NONE;
}

#endif /* ZYDIS_INTERNAL_REGISTER_ENCODE_H */
