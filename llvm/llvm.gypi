{
    'includes': [
        '../build/common.gypi',
    ],
    'variables': {
        'sources': [
            'InitializeLLVM.cpp',
            'LLVMAPI.cpp',
            'log.cpp',
            'CompilerState.cpp',
            'IntrinsicRepository.cpp',
            'CommonValues.cpp',
            'Output.cpp',
            'LLVMCompile.cpp',
            'StackMaps.cpp',
            'LLVMLink.cpp',
            'LLVMDisasContext.cpp',
            'TcgGenerator.cpp',
        ],
        'llvmlog_level': 0,
    },
}
