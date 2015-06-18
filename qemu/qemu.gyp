{
    'includes': [
        'qemu.gypi',
    ],
    'targets': [
        {
            'target_name': 'libqemu',
            'type': 'static_library',
            'sources': [ '<@(sources)',],
            'include_dirs': [
                '.',
                '<(DEPTH)/llvm',
            ],
            'defines': [
                'LLVMLOG_LEVEL=<(llvmlog_level)',
            ],
            'direct_dependent_settings': {
                'include_dirs': [
                    '.',
                ],
                'defines': [
                    'LLVMLOG_LEVEL=<(llvmlog_level)',
                ],
            },
        },
    ],
}

