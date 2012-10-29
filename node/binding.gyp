{
  'targets': [
    {
      'target_name' : 'ejdb_native',
      'sources' : ['ejdb_native.cc'],
      'include_dirs': ['../tcejdb'],
      'libraries' : [
            '-L../../tcejdb',
            '-Wl,-Bstatic -ltcejdb',
            '-Wl,-Bdynamic',
            '-lbz2 -lz -lrt -lpthread -lm -lc'
      ],
      'cflags': [
            '-fPIC',
            '-D_FILE_OFFSET_BITS=64',
            '-D_LARGEFILE_SOURCE'
       ]
    }
  ]
}