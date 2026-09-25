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

#include <Zydis/Internal/SharedData.h>

/* ============================================================================================== */
/* Data tables                                                                                    */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Instruction definitions                                                                        */
/* ---------------------------------------------------------------------------------------------- */

#ifdef ZYDIS_MINIMAL_MODE
#   define ZYDIS_NOTMIN(x)
#else
#   define ZYDIS_NOTMIN(x) , x
#endif

/* Drop `static` on the generated tables so the inlined lookups in SharedData.h
   can see them. The word appears only on those array definitions. */
#define static
#include <Generated/InstructionDefinitions.inc>
#undef static

#undef ZYDIS_NOTMIN

/* ---------------------------------------------------------------------------------------------- */
/* Operand definitions                                                                            */
/* ---------------------------------------------------------------------------------------------- */

#define ZYDIS_OPERAND_DEFINITION(type, encoding, access) \
    { type, encoding, access }

#define static
#include <Generated/OperandDefinitions.inc>
#undef static

#undef ZYDIS_OPERAND_DEFINITION

/* ---------------------------------------------------------------------------------------------- */
/* Accessed CPU flags                                                                             */
/* ---------------------------------------------------------------------------------------------- */

#define static
#include <Generated/AccessedFlags.inc>
#undef static

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
/* Functions                                                                                      */
/* ============================================================================================== */

/* ---------------------------------------------------------------------------------------------- */
/* Instruction definition                                                                         */
/* ---------------------------------------------------------------------------------------------- */

/* Lookups are inlined from SharedData.h. */

/* ---------------------------------------------------------------------------------------------- */

/* ============================================================================================== */
