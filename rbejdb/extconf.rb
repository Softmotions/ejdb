require "mkmf"

unless have_library('tcejdb')
  raise "EJDB C library is not installed!"
end

BUILD_DIR = 'build'

unless File.exists?(BUILD_DIR)
  Dir.mkdir BUILD_DIR
end

Dir.chdir BUILD_DIR

$CFLAGS << ' -Wall'
CONFIG['warnflags'].gsub!('-Wdeclaration-after-statement', '')
create_makefile("rbejdb", '../src')