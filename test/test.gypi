{
    'includes': [
        '../build/common.gypi',
    ],
    'variables': {
        'sources': [
            'Check.cpp',
            'IRContext.cpp',
            'IRContextInternal.cpp',
            'main.cpp',
            'RegisterAssign.cpp',
            'RegisterInit.cpp',
            'RegisterOperation.cpp',
            'dispatch_vex.S',
            '<(bison_gen_source)',
            '<(flex_gen_source)',
        ],
        'bison_source':  'TestParser.y',
        'bison_gen_header': '<(SHARED_INTERMEDIATE_DIR)/TestParser.h',
        'bison_gen_source': '<(SHARED_INTERMEDIATE_DIR)/TestParser.c',
        'flex_source': 'TestScanner.l',
        'flex_gen_source': '<(SHARED_INTERMEDIATE_DIR)/TestScanner.c',
        'llvmlog_level': 0,
    },
}


