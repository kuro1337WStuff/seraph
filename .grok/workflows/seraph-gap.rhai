let meta = #{
    name: "seraph-gap",
    description: "Compare Zydis with iced-x86, add the missing C APIs, and prove them with ctest",
    when_to_use: "When Seraph should grow the iced-x86 gaps that Zydis does not already have",
    phases: [
        #{ title: "Compare", detail: "quote both trees" },
        #{ title: "Implement", detail: "disjoint new files" },
        #{ title: "Register", detail: "wire the build" },
        #{ title: "Test", detail: "Release ctest" },
        #{ title: "Prove", detail: "critics re-run the tests" },
        #{ title: "Ship", detail: "commit and push if green" },
    ],
};

let gap_schema = #{
    "type": "object",
    "required": ["id", "add", "evidence"],
    "properties": #{
        "id": #{ "type": "string" },
        "add": #{ "type": "boolean" },
        "zydis": #{ "type": "string" },
        "iced": #{ "type": "string" },
        "evidence": #{ "type": "string" },
    },
};

let note_schema = #{
    "type": "object",
    "required": ["summary"],
    "properties": #{
        "summary": #{ "type": "string" },
        "files": #{ "type": "string" },
    },
};

let report_schema = #{
    "type": "object",
    "required": ["green", "summary"],
    "properties": #{
        "green": #{ "type": "boolean" },
        "summary": #{ "type": "string" },
    },
};

let verdict_schema = #{
    "type": "object",
    "required": ["real", "summary"],
    "properties": #{
        "real": #{ "type": "boolean" },
        "summary": #{ "type": "string" },
    },
};

let root = "C:\\Users\\Loki\\seraph";
let iced = "C:\\Users\\Loki\\AppData\\Local\\Temp\\iced\\src\\rust\\iced-x86\\src";
let cmake = "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe";

let rules = "Work only under " + root + ". Read code with read_file and grep before editing. Match the existing Zydis C style: braces on their own line, ZyanStatus, ZYAN_MEMSET, no malloc, no libc in library .c files. Tests may use stdio. Copy the MIT header from include\\Zydis\\Utils.h onto new files. Do not commit, do not push, and do not add a Co-Authored-By trailer. Do not edit CMakeLists.txt, meson.build, msvc\\zydis\\Zydis.vcxproj, or include\\Zydis\\Zydis.h. Those are wired in a later step. Keep upstream decoder and encoder behavior. ";

phase("Compare");

let compare_base = "Read the files. Do not answer from memory. Seraph tree: " + root + ". iced-x86 tree: " + iced + ". Set add=true only when Seraph is missing an API iced-x86 has and a short C wrapper or formatter style would add it without replacing the Zydis decoder. Set add=false when Seraph or Zydis already has the behavior. evidence must quote a path and a symbol from each tree you opened. ";

let cjobs = [];
cjobs.push(#{
    prompt: compare_base + "id=nasm. iced formatter is nasm.rs and masm.rs. Seraph styles are in include\\Zydis\\Formatter.h. Zydis has ATT, INTEL, and INTEL_MASM. NASM is missing. MASM has open acceptance bugs (ret far vs retf, st1 vs st(1)).",
    label: "compare:nasm",
    capability_mode: "read-only",
    output_schema: gap_schema,
});
cjobs.push(#{
    prompt: compare_base + "id=assembler. iced code assembler is code_asm.rs (a.mov). Zydis encoder is include\\Zydis\\Encoder.h and takes a ZydisEncoderRequest. There is no in-tree C assembler. zasm is a different C++ project and is not in this tree.",
    label: "compare:assembler",
    capability_mode: "read-only",
    output_schema: gap_schema,
});
cjobs.push(#{
    prompt: compare_base + "id=block-encoder. iced block encoder is block_enc.rs. It rebases a block, widens short branches, and fixes RIP-relative displacements. Zydis encodes one instruction per ZydisEncoderEncodeInstruction call.",
    label: "compare:block-encoder",
    capability_mode: "read-only",
    output_schema: gap_schema,
});
cjobs.push(#{
    prompt: compare_base + "id=opcode-info. iced OpCodeInfo is encoder/op_code.rs. Zydis already stores instruction.opcode, instruction.opcode_map, instruction.encoding, and raw.modrm on ZydisDecodedInstruction. A thin getter is still missing. Do not ask for a made-up CPUID leaf table. Set add=true only for a getter over fields that already exist.",
    label: "compare:opcode-info",
    capability_mode: "read-only",
    output_schema: gap_schema,
});
cjobs.push(#{
    prompt: compare_base + "id=fast-formatter. iced fast formatter is formatter/fast.rs. Zydis formatter is callback-based in src\\Formatter.c. Set add=false. A second formatter ships only after a bench, and this run does not add it.",
    label: "compare:fast-formatter",
    capability_mode: "read-only",
    output_schema: gap_schema,
});
cjobs.push(#{
    prompt: compare_base + "id=already-present. Confirm Seraph already has ZydisGetConstantOffsets in include\\Zydis\\Utils.h and ZydisGetInstructionInfo in include\\Zydis\\InstructionInfo.h, plus the decoder, encoder, and Intel, ATT, and INTEL_MASM formatters. Set add=false.",
    label: "compare:already-present",
    capability_mode: "read-only",
    output_schema: gap_schema,
});

let compared = parallel(cjobs);

let do_nasm = true;
let do_asm = true;
let do_block = true;
let do_opcode = true;
let compare_notes = "";

for r in compared {
    if r != () && r.success && r.output.id != () {
        let bit = r.output.id;
        if r.output.evidence != () {
            compare_notes += bit;
            compare_notes += ": ";
            compare_notes += r.output.evidence;
            compare_notes += "\n";
        }
        if r.output.add == false && r.output.evidence != () && r.output.evidence != "" {
            if bit == "nasm" { do_nasm = false; }
            if bit == "opcode-info" { do_opcode = false; }
            log("skip " + bit);
        }
    }
}
log(compare_notes);

let block_api = "Use this exact API, no malloc. include\\Zydis\\BlockEncoder.h and src\\BlockEncoder.c. ";
block_api += "typedef struct ZydisBlockSlot_ { ZyanU8 kind; ZyanU32 label_id; ZyanU32 branch_label; ZyanU8 branch_operand; ZydisEncoderRequest request; const ZyanU8* bytes; ZyanU8 byte_count; } ZydisBlockSlot; ";
block_api += "kind 0 is an instruction request, kind 1 is raw bytes. label_id 0 means the slot defines no label. branch_label 0 means the request carries its own immediate. ";
block_api += "ZYDIS_BLOCK_ENCODER_DONT_FIX_BRANCHES is 1. ZydisBlockEncode(ZydisMachineMode mode, ZyanU64 base_ip, const ZydisBlockSlot* slots, ZyanU8 slot_count, ZyanU8* out, ZyanUSize out_cap, ZyanUSize* written, ZyanU32 options). ";
block_api += "First pass uses the request as given. If a branch_label target falls outside rel8 and options does not contain DONT_FIX, widen a short jmp (EB rel8) or short jcc (7x rel8) to the near form and repeat until stable. Then fix RIP-relative memory displacements from the final layout. DONT_FIX returns ZYAN_STATUS_FAILED when a short branch cannot reach. ";
block_api += "Write tests\\block_encoder.c. Cases: a forward jmp that stays EB; a forward jmp that must widen; DONT_FIX on that wide case fails; a rip-relative mov whose displacement changes after a widen; a db byte sequence with a label. Print SeraphBlockEncoder: ok on success.";

let asm_api = "Use this exact API. include\\Zydis\\Assembler.h and src\\Assembler.c. It fills ZydisBlockSlot values and calls ZydisBlockEncode, so include BlockEncoder.h and do not reimplement widening. ";
asm_api += "typedef struct ZydisAsm_ { ZydisMachineMode machine_mode; ZydisBlockSlot slots[32]; ZyanU8 count; ZyanBool lock_next; } ZydisAsm; ";
asm_api += "ZydisAsmInit(ZydisAsm*, ZydisMachineMode). ZydisAsmMovRegReg, ZydisAsmMovRegImm, ZydisAsmMovRegMem(asm, dest, base, index, scale, disp), ZydisAsmAddRegReg, ZydisAsmRet, ZydisAsmDb(asm, bytes, size), ZydisAsmLabel(asm, id) defines a label on the next slot, ZydisAsmJmpLabel(asm, id), ZydisAsmLock(asm) sets lock on the next instruction. ";
asm_api += "ZydisAsmEncode(asm, base_ip, out, cap, written) calls ZydisBlockEncode. 32 slots maximum, return ZYAN_STATUS_INSUFFICIENT_BUFFER_SIZE past that. ";
asm_api += "Write tests\\assembler.c that assembles mov rax, rcx; mov rax, 0x1337; mov rax, [rbx+rcx*8+0x10]; lock add; a forward jmp to a label; then decodes the bytes with Zydis and checks mnemonics. Print SeraphAssembler: ok.";

phase("Implement");
let ijobs = [];
if do_nasm {
    let p = rules;
    p += "Add ZYDIS_FORMATTER_STYLE_NASM after ZYDIS_FORMATTER_STYLE_INTEL_MASM in include\\Zydis\\Formatter.h and set MAX_VALUE to it. Update REQUIRED_BITS. Add the preset in src\\Formatter.c FORMATTER_PRESETS. Implement the style beside INTEL_MASM in src\\FormatterIntel.c and declare it in include\\Zydis\\Internal\\FormatterIntel.h. ";
    p += "NASM prints memory as dword [rax], not dword ptr. Hex immediates use a 0x prefix. ret far (opcode CB) prints retf. st1 prints st1. Also fix INTEL_MASM so CB prints retf and st1 prints st(1). ";
    p += "Write tests\\nasm_formatter.c. Decode 90 and expect nasm text nop. Decode 48 C7 C0 37 13 00 00 and expect mov rax, 0x1337. Decode 48 8B 00 and expect the memory operand to contain [rax] and not the substring ptr. Decode CB in 64-bit mode with the MASM style and expect retf. Print SeraphNasmFormatter: ok.";
    ijobs.push(#{ prompt: p, label: "impl:nasm", capability_mode: "execute", output_schema: note_schema });
}
if do_block {
    let p = rules + block_api;
    ijobs.push(#{ prompt: p, label: "impl:block-encoder", capability_mode: "execute", output_schema: note_schema });
}
if do_asm {
    let p = rules + asm_api;
    ijobs.push(#{ prompt: p, label: "impl:assembler", capability_mode: "execute", output_schema: note_schema });
}
if do_opcode {
    let p = rules;
    p += "Add include\\Zydis\\OpcodeInfo.h and src\\OpcodeInfo.c. typedef struct ZydisEncodingInfo_ { ZydisInstructionEncoding encoding; ZydisOpcodeMap opcode_map; ZyanU8 opcode; ZyanBool has_modrm; ZyanU8 modrm; ZydisISASet isa_set; ZydisISAExt isa_ext; } ZydisEncodingInfo; ";
    p += "ZydisGetEncodingInfo(const ZydisDecodedInstruction*, ZydisEncodingInfo*). Copy the fields that already exist on the decoded instruction. Do not invent CPUID leaves. ";
    p += "Write tests\\opcode_info.c. Decode 48 C7 C0 37 13 00 00 and expect opcode 0xC7, has_modrm true, isa set or ext non-zero. Decode 90 and expect opcode 0x90 and has_modrm false. Print SeraphOpcodeInfo: ok.";
    ijobs.push(#{ prompt: p, label: "impl:opcode-info", capability_mode: "execute", output_schema: note_schema });
}

if ijobs.len() == 0 {
    complete(#{ summary: "Compare found nothing to add.", notes: compare_notes });
}

let implemented = parallel(ijobs);
let impl_notes = "";
for r in implemented {
    if r != () && r.success && r.output.summary != () {
        impl_notes += r.output.summary;
        impl_notes += "\n";
    } else {
        impl_notes += "implement slot failed\n";
    }
}
log(impl_notes);

phase("Register");
let reg_prompt = "Edit only build registration under " + root + ". If a listed source file exists, add it. Do not rewrite the APIs. ";
reg_prompt += "CMakeLists.txt: add src\\BlockEncoder.c and include\\Zydis\\BlockEncoder.h next to the encoder sources when the encoder feature is on. Add src\\OpcodeInfo.c and its header next to the decoder sources. Include the new public headers from include\\Zydis\\Zydis.h under the same feature guards. ";
reg_prompt += "Add tests\\nasm_formatter.c, tests\\block_encoder.c, tests\\assembler.c, and tests\\opcode_info.c as executables and add_test entries when those files exist, gated by ZYDIS_FEATURE_DECODER AND NOT ZYAN_NO_LIBC AND NOT ZYDIS_MINIMAL_MODE. Assembler and block tests also need the encoder. ";
reg_prompt += "Mirror the new library .c files into meson.build and msvc\\zydis\\Zydis.vcxproj. Mirror the test executables into tests\\meson.build under the existing minimal.disabled() guard. Do not commit.";
let registered = agent(reg_prompt, #{
    label: "register",
    capability_mode: "execute",
    output_schema: note_schema,
});
if registered == () || registered.success == false {
    await_user("verification", "Build registration failed.");
}

phase("Test");
let test_prompt = "In " + root + " configure " + root + "\\build if needed with " + cmake + " -S . -B build -DZYDIS_BUILD_TESTS=ON -DZYAN_DEV_MODE=ON. Build the Release config. Run ctest -C Release --output-on-failure. The regression script already clears FORCE_COLOR. Set green=true only if ctest exits 0. summary must be the ctest summary line. Do not commit.";
let tested = agent(test_prompt, #{
    label: "test",
    capability_mode: "execute",
    output_schema: report_schema,
});
if tested == () || tested.success == false || tested.output.green != true {
    phase("Fix");
    let why = "ctest failed";
    if tested != () && tested.success && tested.output.summary != () {
        why = tested.output.summary;
    }
    agent("ctest in " + root + " failed. Detail: " + why + ". Fix the new Seraph sources so Release ctest exits 0. You may edit the new files and the build lists. Do not weaken or delete upstream tests. Do not commit.", #{
        label: "fix",
        capability_mode: "execute",
    });
    tested = agent(test_prompt, #{
        label: "retest",
        capability_mode: "execute",
        output_schema: report_schema,
    });
}
if tested == () || tested.success == false || tested.output.green != true || tested.output.summary == () || tested.output.summary == "" {
    await_user("verification", "ctest is still red. No push.");
}

phase("Prove");
let vjobs = [];
if do_nasm {
    vjobs.push(#{
        prompt: "Re-run " + root + "\\build\\Release\\SeraphNasmFormatter.exe yourself. Set real=true only if you saw it print SeraphNasmFormatter: ok and the process exited 0. summary must quote that line. Also confirm include\\Zydis\\Formatter.h contains ZYDIS_FORMATTER_STYLE_NASM by reading it.",
        label: "prove:nasm",
        capability_mode: "execute",
        output_schema: verdict_schema,
    });
}
if do_block {
    vjobs.push(#{
        prompt: "Re-run " + root + "\\build\\Release\\SeraphBlockEncoder.exe yourself. Set real=true only if you saw SeraphBlockEncoder: ok and exit 0. summary must quote that line. Confirm src\\BlockEncoder.c contains no malloc call by searching it.",
        label: "prove:block",
        capability_mode: "execute",
        output_schema: verdict_schema,
    });
}
if do_asm {
    vjobs.push(#{
        prompt: "Re-run " + root + "\\build\\Release\\SeraphAssembler.exe yourself. Set real=true only if you saw SeraphAssembler: ok and exit 0. summary must quote that line.",
        label: "prove:assembler",
        capability_mode: "execute",
        output_schema: verdict_schema,
    });
}
if do_opcode {
    vjobs.push(#{
        prompt: "Re-run " + root + "\\build\\Release\\SeraphOpcodeInfo.exe yourself. Set real=true only if you saw SeraphOpcodeInfo: ok and exit 0. summary must quote that line. Reject the change if the source invents CPUID leaf numbers that are not read from a Zydis field.",
        label: "prove:opcode",
        capability_mode: "execute",
        output_schema: verdict_schema,
    });
}

let verdicts = parallel(vjobs);
let proved = 0;
let prove_notes = "";
let vi = 0;
for v in verdicts {
    vi += 1;
    if v != () && v.success && v.output.real == true && v.output.summary != () && v.output.summary != "" {
        proved += 1;
        prove_notes += v.output.summary;
        prove_notes += "\n";
    }
}
log(proved.to_string() + "/" + vi.to_string() + " proved");
if proved != vi || vi == 0 {
    await_user("verification", "A critic did not quote a passing test. No push.");
}

phase("Ship");
let ship_prompt = "Repo " + root + " origin is https://github.com/kuro1337WStuff/seraph.git branch master. ctest summary: " + tested.output.summary + ". Critic lines: " + prove_notes + ". Update docs\\SERAPH_FEATURES.md so nasm-formatter, assembler, block-encoder, and opcode-cpuid say done only for the ones whose tests printed ok. Commit with the repo git identity. Do not add a Co-Authored-By trailer or any agent attribution. Do not create an empty commit. Before push, set GH_TOKEN from `gh auth token -u kuro1337WStuff` and push origin master. Set real=true only if the push succeeded. summary includes the ctest line.";
let shipped = agent(ship_prompt, #{
    label: "ship",
    capability_mode: "execute",
    output_schema: verdict_schema,
});
if shipped == () || shipped.success == false || shipped.output.real != true {
    await_user("verification", "Tests passed but the push did not.");
}

complete(#{
    summary: shipped.output.summary,
    compare: compare_notes,
    proved: proved,
    pushed: true,
});
