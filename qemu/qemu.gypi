{
    'includes': [
        '../build/common.gypi',
    ],
    'variables': {
        'sources': [
            'translate.c',
            'helper.c',
            'neon_helper.c',
            'iwmmxt_helper.c',
            'aes.c',
            'crypto_helper.c',
            'softfloat.c',
            'crc32c.c',
            'cpuinit.c',
            'compatglib.c',
            'host-utils.c',
            'QEMUDisasContext.cpp',
        ],
        'llvmlog_level': 0,
    },
}
