{
    'includes': [
        'llvm.gypi',
    ],
    'targets': [
        {
            'target_name': 'libllvm',
            'type': 'static_library',
            'sources': [ '<@(sources)',],
            'include_dirs': [
                '.',
                '<(DEPTH)/qemu',
            ],
            'defines': [
                'LLVMLOG_LEVEL=<(llvmlog_level)',
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '.',
                    '<(DEPTH)/qemu',
                ],
                'libraries': [
                    '-ldl',
                    '-lpthread',
                    '-lz',
                ],
                'ldflags': [
                ],
                'defines': [
                    'LLVMLOG_LEVEL=<(llvmlog_level)',
                ],
            },
        },
    ],
}
