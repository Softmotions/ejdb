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
                 '_UNICODE',
                 '_GLIBCXX_PERMIT_BACKWARD_HASH',
               ],
            }],
            [ 'OS=="linux" or OS=="freebsd" or OS=="openbsd" or OS=="solaris"', {
                'cflags': [ '-Wall', '-pedantic', '-fsigned-char', '-pthread', '-Wno-variadic-macros'],
                'cflags_cc!' : [ '-fno-exceptions' ],
                'libraries' : [
                    '-L../tcejdb',
                    '-Wl,-Bstatic -ltcejdb',
                    '-Wl,-Bdynamic',
                    '-lz -lpthread -lm -lc'
                ]
            }],
	    [ 'OS=="mac"', {
		'defines': ['_DARWIN_USE_64_BIT_INODE=1'],
		'cflags_cc!' : [ '-fno-exceptions' ],
		'xcode_settings': {	
			'GCC_ENABLE_CPP_EXCEPTIONS':'YES',
			'OTHER_CFLAGS': [       			
				'-fsigned-char', '-pthread', '-Wno-variadic-macros', '-fexceptions'
          		],
			'OTHER_LDFLAGS': [
				'-Wl,-search_paths_first',
				'-L./tcejdb/static',                 		
                    		'-lstcejdb -lz -lpthread -lm -lc'
			]						
		}
            }]        
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
