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
                'qemu/helper.c',
                'qemu/neon_helper.c',
                'qemu/iwmmxt_helper.c',
                'qemu/aes.c',
                'qemu/crypto_helper.c',
                'qemu/softfloat.c',
                'qemu/crc32c.c',
             ],
            'dependencies': [
                '<(DEPTH)/llvm/llvm.gyp:libllvm',
            ]
        },
    ],
}
