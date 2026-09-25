pymod = import('python')
py_exe = pymod.find_installation('python3', required: tests)

tests_req = py_exe.found()

if tests_req and decoder.enabled() and minimal.disabled()
  constant_offsets = executable(
    'SeraphConstantOffsets',
    'constant_offsets.c',
    dependencies: [zydis_dep],
  )
  test('SeraphConstantOffsets', constant_offsets)
  instruction_info = executable(
    'SeraphInstructionInfo',
    'instruction_info.c',
    dependencies: [zydis_dep],
  )
  test('SeraphInstructionInfo', instruction_info)
  nasm_formatter = executable(
    'SeraphNasmFormatter',
    'nasm_formatter.c',
    dependencies: [zydis_dep],
  )
  test('SeraphNasmFormatter', nasm_formatter)
  opcode_info = executable(
    'SeraphOpcodeInfo',
    'opcode_info.c',
    dependencies: [zydis_dep],
  )
  test('SeraphOpcodeInfo', opcode_info)
  cpuid_flags = executable(
    'SeraphCpuidFlags',
    'cpuid_flags.c',
    dependencies: [zydis_dep],
  )
  test('SeraphCpuidFlags', cpuid_flags)
  condition_code = executable(
    'SeraphConditionCode',
    'condition_code.c',
    dependencies: [zydis_dep],
  )
  test('SeraphConditionCode', condition_code)
  if formatter.enabled()
    seraph_bench = executable(
      'SeraphBench',
      'formatter_bench.c',
      dependencies: [zydis_dep],
    )
    test('SeraphBench', seraph_bench, args: ['200'])
    seraph_vm_exit = executable(
      'SeraphVmExit',
      'vm_exit.c',
      dependencies: [zydis_dep],
    )
    test('SeraphVmExit', seraph_vm_exit)
  endif
  if encoder.enabled()
    block_encoder = executable(
      'SeraphBlockEncoder',
      'block_encoder.c',
      dependencies: [zydis_dep],
    )
    test('SeraphBlockEncoder', block_encoder)
    seraph_assembler = executable(
      'SeraphAssembler',
      'assembler.c',
      dependencies: [zydis_dep],
    )
    test('SeraphAssembler', seraph_assembler)
  endif
endif

if tests_req
  test(
    'ZydisRegression',
    py_exe,
    args: [
      files('regression.py'),
      'test',
      zydisinfo_exe,
    ],
    workdir: meson.current_source_dir(),
  )

  test(
    'ZydisRegressionEncoder',
    py_exe,
    args: [
      files('regression_encoder.py'),
      zydisfuzzreencoding_exe,
      zydisfuzzencoder_exe,
      zydistestencoderabsolute_exe,
    ],
    workdir: meson.current_source_dir(),
  )
endif

summary(
  {'tests': tests_req},
  section: 'Features',
)
