Gem::Specification.new do |spec|
  spec.name = 'rbejdb'
  spec.version = '0.9'
  spec.summary = 'Ruby binding for EJDB database engine.'
  spec.author = 'Softmotions'
  spec.homepage = 'http://ejdb.org'
  spec.license = 'LGPL'

  spec.required_ruby_version = '>= 1.9.1'
  spec.files = Dir['ext/**/*']
  spec.platform = Gem::Platform::RUBY
  spec.require_paths = ['ext']
  spec.extensions = Dir['ext/rbejdb/extconf.rb']

  spec.has_rdoc = true
  spec.rdoc_options = %w[
    --exclude .*\.o
    --exclude .*\.so
  ]
end