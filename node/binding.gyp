{
  'targets': [
    {
      'target_name' : 'ejdb_native',
      'sources' : ['ejdb_native.cc'],
      'include_dirs': ['../tcejdb'],
      'libraries' : ['-L../tcejdb']
    }
  ]
}