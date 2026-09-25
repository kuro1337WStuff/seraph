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
 * One disassembly line plus the standard info and the VM-exit record.
 *
 * The line is the formatter text, then `flow`, `intercept`, an optional
 * `cc`, then `vmx` and `svm`. The caller buffer is left unchanged when the
 * line does not fit.
 */

#ifndef ZYDIS_VERBOSE_H
#define ZYDIS_VERBOSE_H

#include <Zycore/Types.h>
#include <Zydis/Formatter.h>
#include <Zydis/Status.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Formats one instruction and the Seraph summaries into `buffer`.
 *
 * @param   formatter       Formatter that supplies the instruction text.
 * @param   instruction     Decoded instruction.
 * @param   operands        Decoded operands.
 * @param   operand_count   Operand count from the decoder.
 * @param   runtime_address Runtime address passed to the formatter.
 * @param   buffer          Caller buffer. Receives a NUL-terminated line.
 * @param   capacity        Size of `buffer` in bytes, including the NUL.
 *
 * @return  `ZYAN_STATUS_SUCCESS`, `ZYAN_STATUS_INVALID_ARGUMENT`, or
 *          `ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE`.
 */
ZYDIS_EXPORT ZyanStatus ZydisFormatVerbose(const ZydisFormatter* formatter,
    const ZydisDecodedInstruction* instruction, const ZydisDecodedOperand* operands,
    ZyanU8 operand_count, ZyanU64 runtime_address, char* buffer, ZyanUSize capacity);

#ifdef __cplusplus
}
#endif

#endif /* ZYDIS_VERBOSE_H */
