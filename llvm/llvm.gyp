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
                '<!(llvm-config --includedir)',
            ],
            'defines': [
                'LLVMLOG_LEVEL=<(llvmlog_level)',
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '.',
                    '<(DEPTH)/qemu',
                    '<!(llvm-config --includedir)',
                ],
                'libraries': [
                    '<!(llvm-config --libs)',
                    '-ldl',
                    '-lpthread',
                    '-lz',
                ],
                'ldflags': [
                    '<!(llvm-config --ldflags)',
                ],
                'defines': [
                    'LLVMLOG_LEVEL=<(llvmlog_level)',
                ],
            },
        },
    ],
}
