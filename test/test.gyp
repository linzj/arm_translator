{
    'includes': [
        'test.gypi',
    ],
    'targets': [
        {
            'target_name': 'gen_bison',
            'type': 'none',
            'actions': [
                {
                    'action_name': 'generate_parser',
                    'inputs': ['<(bison_source)'],
                    'outputs': [
                        '<(bison_gen_source)',
                        '<(bison_gen_header)',
                    ],
                    'action': [
                        'bison',
                        '-y', 
                        '-v', 
                        '-o', 
                        '<(bison_gen_source)',
                        '--defines=<(bison_gen_header)',
                        '<(bison_source)',
                    ]
                },
            ],
            'message': 'generating bison parser'
        },
        {
            'target_name': 'gen_flex',
            'type': 'none',
            'actions': [
                {
                    'action_name': 'generate_scanner',
                    'inputs': ['<(flex_source)'],
                    'outputs': [
                        '<(flex_gen_source)',
                    ],
                    'action': [
                        'flex',
                        '--never-interactive',
                        '--outfile=<(flex_gen_source)',
                        '<(flex_source)'
                    ]
                },
            ],
            'message': 'generating flex scanner'
        },
        {
            'target_name': 'testQEMU',
            'type': 'executable',
            'sources': [ '<@(sources)',],
            'include_dirs': [
                '.',
                '<(DEPTH)/qemu',
                '<(DEPTH)/llvm',
            ],
            'defines': [
                'LLVMLOG_LEVEL=<(llvmlog_level)',
            ],
            'dependencies': [
                'gen_bison',
                'gen_flex',
                '<(DEPTH)/llvm/llvm.gyp:libllvm',
                '<(DEPTH)/qemu/qemu.gyp:libqemu',
            ],
        },
    ],
}
