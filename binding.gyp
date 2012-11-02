{
    'variables' : {

    },

    'target_defaults': {
        'configurations': {
            'Debug': {
            },
            'Release':{
                'defines': [ 'NDEBUG' ],
            }
        },
        'conditions': [
            ['OS == "win"', {

            }, {
               'defines': [
                 '_LARGEFILE_SOURCE',
                 '_FILE_OFFSET_BITS=64',
                 '_GNU_SOURCE',
               ],
            }],
            [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
                'cflags': [ '-Wall', '-pedantic', '-fsigned-char', '-pthread', '-Wno-variadic-macros'],
                'cflags_cc!' : [ '-fno-exceptions' ],
                'libraries' : [
                    '-L../tcejdb',
                    '-Wl,-Bstatic -ltcejdb',
                    '-Wl,-Bdynamic',
                    '-lbz2 -lz -lrt -lpthread -lm -lc'
                ]
            }],
        ],
        'include_dirs' : ['tcejdb'],
    },

    'targets' : [
        {
            'target_name' : 'ejdb_native',
            'sources' : [
                'node/ejdb_native.cc',
                'node/ejdb_logging.cc'
            ]
        },
    ]
}