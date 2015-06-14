{
    'includes': [
        './build/common.gypi',
    ],
    'targets': [
        {
            'target_name': 'main',
            'type': 'executable',
            'sources': [
                'main.cpp',
                'qemu/translate.c',
             ],
            'dependencies': [
                '<(DEPTH)/llvm/llvm.gyp:libllvm',
            ]
        },
    ],
}
