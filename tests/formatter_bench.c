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
 * Times decode, format, and decode-plus-format for the four formatter styles
 * on one fixed long-mode buffer.
 *
 * Usage: SeraphBench [milliseconds]
 */

#include <stdio.h>
#include <stdlib.h>

#include <Zydis/Zydis.h>

#if defined(ZYAN_WINDOWS)
#   ifndef WIN32_LEAN_AND_MEAN
#       define WIN32_LEAN_AND_MEAN
#   endif
#   include <windows.h>
#else
#   include <time.h>
#endif

#define SERAPH_BENCH_MAX 96
#define SERAPH_BENCH_BASE 0x0000000140000000ull

static const ZyanU8 k_code[] =
{
    0x90,
    0x48, 0x89, 0xC8,
    0x48, 0x83, 0xC0, 0x01,
    0x48, 0x8B, 0x04, 0x8B,
    0x48, 0x8D, 0x44, 0x8B, 0x08,
    0x48, 0x3B, 0xC1,
    0x74, 0x00,
    0xE8, 0x00, 0x00, 0x00, 0x00,
    0xE9, 0x00, 0x00, 0x00, 0x00,
    0x0F, 0x84, 0x00, 0x00, 0x00, 0x00,
    0xC3,
    0x0F, 0x05,
    0x0F, 0xA2,
    0xF4,
    0xED,
    0x0F, 0x32,
    0x0F, 0x01, 0x10,
    0x0F, 0x01, 0xC1,
    0x0F, 0x31,
    0x0F, 0x01, 0xF9,
    0xF3, 0x90,
    0xCC,
    0x9C,
    0x9D,
    0xD9, 0xE8,
    0xD9, 0xC0,
    0x0F, 0x94, 0xC0,
    0x0F, 0x44, 0xC1,
    0xF3, 0x0F, 0x10, 0x08,
    0xF2, 0x0F, 0x10, 0x08,
    0x66, 0x0F, 0xEF, 0xC1,
    0x0F, 0x57, 0xC1,
    0x66, 0x0F, 0x38, 0xDC, 0xC1,
    0xC5, 0xF8, 0x28, 0xC1,
    0xC5, 0xFC, 0x28, 0xC1,
    0x62, 0xF1, 0x7C, 0x08, 0x58, 0xC1,
    0x62, 0xF1, 0x7C, 0x48, 0x58, 0xC1,
    0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
    0x48, 0xC7, 0xC0, 0x37, 0x13, 0x00, 0x00,
    0x48, 0x8B, 0x40, 0x08,
    0x48, 0x89, 0x44, 0x24, 0x08,
    0xF0, 0x48, 0x01, 0x08,
    0x48, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00,
    0x48, 0x81, 0xC4, 0x00, 0x01, 0x00, 0x00,
    0xFF, 0x10,
    0xFF, 0x20,
    0x48, 0x63, 0xC1,
    0x0F, 0xB6, 0xC1,
    0x0F, 0x1F, 0x44, 0x00, 0x00,
    0x0F, 0xBA, 0xE1, 0x05,
    0x48, 0xCF
};

static ZydisDecodedInstruction g_insn[SERAPH_BENCH_MAX];
static ZydisDecodedOperand g_ops[SERAPH_BENCH_MAX][ZYDIS_MAX_OPERAND_COUNT];
static ZyanUSize g_off[SERAPH_BENCH_MAX];
static ZyanUSize g_count = 0;
static ZyanU32 g_sink = 0;
static int g_failures = 0;

#if defined(ZYAN_WINDOWS)
static ZyanU64 g_freq = 0;

static int ClockInit(void)
{
    LARGE_INTEGER freq;

    if (!QueryPerformanceFrequency(&freq) || (freq.QuadPart <= 0))
    {
        return 0;
    }
    g_freq = (ZyanU64)freq.QuadPart;
    return 1;
}

static ZyanU64 Ticks(void)
{
    LARGE_INTEGER now;

    QueryPerformanceCounter(&now);
    return (ZyanU64)now.QuadPart;
}

static ZyanU64 Budget(unsigned ms)
{
    return g_freq * (ZyanU64)ms / 1000ull;
}

static double Seconds(ZyanU64 elapsed)
{
    return (double)elapsed / (double)g_freq;
}
#else
static int ClockInit(void)
{
    return 1;
}

static ZyanU64 Ticks(void)
{
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return (ZyanU64)now.tv_sec * 1000000000ull + (ZyanU64)now.tv_nsec;
}

static ZyanU64 Budget(unsigned ms)
{
    return (ZyanU64)ms * 1000000ull;
}

static double Seconds(ZyanU64 elapsed)
{
    return (double)elapsed / 1000000000.0;
}
#endif

static unsigned Milliseconds(int argc, char** argv)
{
    unsigned long value;
    char* end;

    if (argc < 2)
    {
        return 300u;
    }
    value = strtoul(argv[1], &end, 10);
    if ((end == argv[1]) || (*end != '\0') || (value == 0))
    {
        return 300u;
    }
    if (value > 10000ul)
    {
        return 10000u;
    }
    return (unsigned)value;
}

static void Fail(const char* what, ZyanUSize offset, ZyanStatus status)
{
    printf("FAIL %s at %u status %08x\n", what, (unsigned)offset, (unsigned)status);
    ++g_failures;
}

static int Load(ZydisDecoder* decoder)
{
    ZyanUSize offset = 0;

    if (ZYAN_FAILED(ZydisDecoderInit(decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64)))
    {
        Fail("decoder", 0, ZYAN_STATUS_INVALID_ARGUMENT);
        return 0;
    }
    while (offset < sizeof(k_code))
    {
        ZyanStatus status;

        if (g_count >= SERAPH_BENCH_MAX)
        {
            Fail("capacity", offset, ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE);
            return 0;
        }
        status = ZydisDecoderDecodeFull(decoder, k_code + offset, sizeof(k_code) - offset,
            &g_insn[g_count], g_ops[g_count]);
        if (ZYAN_FAILED(status) || (g_insn[g_count].length == 0))
        {
            Fail("decode", offset, status);
            return 0;
        }
        g_off[g_count] = offset;
        offset += g_insn[g_count].length;
        ++g_count;
    }
    return g_count > 0;
}

static int FormatOne(const ZydisFormatter* formatter, ZyanUSize index, char* text, ZyanUSize length)
{
    ZyanStatus status;

    status = ZydisFormatterFormatInstruction(formatter, &g_insn[index], g_ops[index],
        g_insn[index].operand_count_visible, text, length, SERAPH_BENCH_BASE + g_off[index],
        ZYAN_NULL);
    if (ZYAN_FAILED(status))
    {
        Fail("format", g_off[index], status);
        return 0;
    }
    g_sink += (ZyanU8)text[0] + (ZyanU32)length;
    return 1;
}

static int Preflight(void)
{
    static const ZydisFormatterStyle styles[] =
    {
        ZYDIS_FORMATTER_STYLE_INTEL,
        ZYDIS_FORMATTER_STYLE_ATT,
        ZYDIS_FORMATTER_STYLE_INTEL_MASM,
        ZYDIS_FORMATTER_STYLE_NASM
    };
    ZyanUSize style;
    char text[256];

    for (style = 0; style < (sizeof(styles) / sizeof(styles[0])); ++style)
    {
        ZydisFormatter formatter;
        ZyanUSize index;

        if (ZYAN_FAILED(ZydisFormatterInit(&formatter, styles[style])))
        {
            Fail("formatter", 0, ZYAN_STATUS_INVALID_ARGUMENT);
            return 0;
        }
        for (index = 0; index < g_count; ++index)
        {
            if (!FormatOne(&formatter, index, text, sizeof(text)))
            {
                return 0;
            }
        }
    }
    return 1;
}

static ZyanU64 TimeDecode(const ZydisDecoder* decoder, ZyanU64 budget)
{
    ZyanU64 start = Ticks();
    ZyanU64 count = 0;

    do
    {
        ZyanUSize pass;

        for (pass = 0; pass < 16; ++pass)
        {
            ZyanUSize offset = 0;

            while (offset < sizeof(k_code))
            {
                ZydisDecodedInstruction instruction;
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
                ZyanStatus status;

                status = ZydisDecoderDecodeFull(decoder, k_code + offset, sizeof(k_code) - offset,
                    &instruction, operands);
                if (ZYAN_FAILED(status))
                {
                    Fail("timed decode", offset, status);
                    return 0;
                }
                g_sink += instruction.length;
                offset += instruction.length;
                ++count;
            }
        }
    } while ((Ticks() - start) < budget);

    return count;
}

static void Report(const char* name, ZyanU64 marks, ZyanU64 elapsed)
{
    double seconds = Seconds(elapsed);
    double rate = (seconds > 0.0) ? ((double)marks / seconds) : 0.0;

    printf("SeraphBench: %-12s %10.0f\n", name, rate);
}

static ZyanU64 TimeDecodeFormat(const ZydisDecoder* decoder, const ZydisFormatter* formatter,
    ZyanU64 budget)
{
    ZyanU64 start = Ticks();
    ZyanU64 count = 0;
    char text[256];

    do
    {
        ZyanUSize pass;

        for (pass = 0; pass < 8; ++pass)
        {
            ZyanUSize offset = 0;

            while (offset < sizeof(k_code))
            {
                ZydisDecodedInstruction instruction;
                ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
                ZyanStatus status;

                status = ZydisDecoderDecodeFull(decoder, k_code + offset, sizeof(k_code) - offset,
                    &instruction, operands);
                if (ZYAN_FAILED(status))
                {
                    Fail("timed decode", offset, status);
                    return 0;
                }
                status = ZydisFormatterFormatInstruction(formatter, &instruction, operands,
                    instruction.operand_count_visible, text, sizeof(text),
                    SERAPH_BENCH_BASE + offset, ZYAN_NULL);
                if (ZYAN_FAILED(status))
                {
                    Fail("timed format", offset, status);
                    return 0;
                }
                g_sink += (ZyanU8)text[0];
                offset += instruction.length;
                ++count;
            }
        }
    } while ((Ticks() - start) < budget);

    return count;
}

static ZyanU64 TimeFormat(const ZydisFormatter* formatter, ZyanU64 budget)
{
    ZyanU64 start = Ticks();
    ZyanU64 count = 0;
    char text[256];

    do
    {
        ZyanUSize pass;

        for (pass = 0; pass < 8; ++pass)
        {
            ZyanUSize index;

            for (index = 0; index < g_count; ++index)
            {
                if (!FormatOne(formatter, index, text, sizeof(text)))
                {
                    return 0;
                }
                ++count;
            }
        }
    } while ((Ticks() - start) < budget);

    return count;
}

int main(int argc, char** argv)
{
    static const struct
    {
        ZydisFormatterStyle style;
        const char* name;
    } styles[] =
    {
        { ZYDIS_FORMATTER_STYLE_INTEL, "intel" },
        { ZYDIS_FORMATTER_STYLE_ATT, "att" },
        { ZYDIS_FORMATTER_STYLE_INTEL_MASM, "masm" },
        { ZYDIS_FORMATTER_STYLE_NASM, "nasm" }
    };
    ZydisDecoder decoder;
    unsigned ms;
    ZyanU64 budget;
    ZyanU64 start;
    ZyanU64 count;
    ZyanUSize style;

    ms = Milliseconds(argc, argv);
    if (!ClockInit() || !Load(&decoder) || !Preflight())
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }

    budget = Budget(ms);
    printf("SeraphBench: bytes %u insns %u ms %u\n",
        (unsigned)sizeof(k_code), (unsigned)g_count, ms);

    start = Ticks();
    count = TimeDecode(&decoder, budget);
    if (!count && g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    Report("decode", count, Ticks() - start);

    for (style = 0; style < (sizeof(styles) / sizeof(styles[0])); ++style)
    {
        ZydisFormatter formatter;
        ZyanU64 combined;
        ZyanU64 format_only;
        double combined_rate;
        double format_rate;
        ZyanU64 combined_elapsed;
        ZyanU64 format_elapsed;

        if (ZYAN_FAILED(ZydisFormatterInit(&formatter, styles[style].style)))
        {
            Fail(styles[style].name, 0, ZYAN_STATUS_INVALID_ARGUMENT);
            break;
        }
        start = Ticks();
        combined = TimeDecodeFormat(&decoder, &formatter, budget);
        combined_elapsed = Ticks() - start;
        if (!combined && g_failures)
        {
            break;
        }
        start = Ticks();
        format_only = TimeFormat(&formatter, budget);
        format_elapsed = Ticks() - start;
        if (!format_only && g_failures)
        {
            break;
        }
        combined_rate = Seconds(combined_elapsed) > 0.0
            ? ((double)combined / Seconds(combined_elapsed)) : 0.0;
        format_rate = Seconds(format_elapsed) > 0.0
            ? ((double)format_only / Seconds(format_elapsed)) : 0.0;
        printf("SeraphBench: %-12s %10.0f format %10.0f\n",
            styles[style].name, combined_rate, format_rate);
    }

    if (g_failures)
    {
        printf("%d failure(s)\n", g_failures);
        return 1;
    }
    printf("SeraphBench: ok sink %u\n", (unsigned)g_sink);
    return 0;
}
