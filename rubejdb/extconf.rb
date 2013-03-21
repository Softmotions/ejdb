require "mkmf"

BUILD_DIR = 'build'

unless File.exists?(BUILD_DIR)
  Dir.mkdir BUILD_DIR
end

Dir.chdir BUILD_DIR
create_makefile("rubejdb", '../src')