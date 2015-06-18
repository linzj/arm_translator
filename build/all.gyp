{
  'includes': [
    'common.gypi',
  ],
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'dependencies': [
          '<(DEPTH)/test/test.gyp:testQEMU',
      ]
    },
  ],
}
