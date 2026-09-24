pymod = import('python')
py_exe = pymod.find_installation('python3', required: tests)

tests_req = py_exe.found()

if tests_req and decoder.enabled()
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
