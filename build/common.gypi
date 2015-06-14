{
    'variables': {
      'clang%': 0,
      'conditions': [
        ['OS == "linux"',
            {
                'shared_library_cflags': [
                    '-fPIC',
                ]
            }
        ],
        ['OS == "win"',
            {
                'shared_library_cflags': [
                ],
            }
        ],
      ],
  },
  'conditions': [
      ['clang == 1', {
          'make_global_settings': [
              ['CXX','/usr/bin/clang++'],
              ['LINK','/usr/bin/clang++'],
          ],
      }, {}
      ],
  ],
  'target_defaults': {
      'default_configuration': 'Debug',
      'configurations': {
          'Common_Base': {
              'abstract': 1,
          },
          'Debug_Base': {
              'abstract': 1,
              'defines': [
                  'DEBUG=1',
                  'LOG_LEVEL=5',
              ],
              'conditions': [
                ['OS == "linux"',
                  {
                      'cflags': [
                        '-O0',
                        '-g3',
                      ],
                      'cflags_cc': [
                        '-std=c++11',
                      ],
                  }
                ],
              ],
           },
           'Debug':  {
               'inherit_from': ['Common_Base', 'Debug_Base'],
           },
           'Release_Base': {
              'abstract': 1,
              'defines': [
                  'NDEBUG',
              ],
              'conditions': [
                ['OS == "linux"',
                  {
                      'cflags': [
                        '-O2'
                      ],
                  }
                ],
              ],
           },
           'Release':  {
               'inherit_from': ['Common_Base', 'Release_Base'],
           },
      },
  },
}
