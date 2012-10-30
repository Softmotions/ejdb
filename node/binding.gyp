{
  'targets': [
    {
      'target_name' : 'ejdb_native',
      'sources' : [
            'ejdb_native.cc',
            'ejdb_logging.cc'
       ],
      'include_dirs': ['../tcejdb'],
      'libraries' : [
            '-L../../tcejdb',
            '-Wl,-Bstatic -ltcejdb',
            '-Wl,-Bdynamic',
            '-lbz2 -lz -lrt -lpthread -lm -lc'
      ],
      'cflags': [
            '-g',
            '-O0',
            '-fPIC',
            '-pedantic',
            '-Wno-variadic-macros',
            '-D_GNU_SOURCE',
            '-D_FILE_OFFSET_BITS=64',
            '-D_LARGEFILE_SOURCE'
       ],
       'cflags!': [ '-fno-exceptions' ],
       'cflags_cc!': [ '-fno-exceptions' ]
    }
  ]
}