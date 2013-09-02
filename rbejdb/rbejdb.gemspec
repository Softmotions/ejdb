Gem::Specification.new do |spec|
  spec.name = 'rbejdb'
  spec.version = '0.9'
  spec.summary = 'Ruby binding for EJDB database engine.'
  spec.author = 'Softmotions'
  spec.homepage = 'http://ejdb.org'

  spec.required_ruby_version = '>= 1.9.1'
  spec.files = Dir['src/*'] + Dir['extconf.rb']
  spec.platform = Gem::Platform::RUBY
  spec.require_paths = ['.', 'src']
  spec.extensions = Dir['extconf.rb']
end