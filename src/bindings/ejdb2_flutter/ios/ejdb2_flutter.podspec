#
# To learn more about a Podspec see http://guides.cocoapods.org/syntax/podspec.html.
# Run `pod lib lint ejdb2_flutter.podspec' to validate before publishing.
#
Pod::Spec.new do |s|
  s.name             = 'ejdb2_flutter'
  s.version          = '1.0.10'
  s.summary          = 'Embeddable JSON Database engine EJDB http://ejdb.org Flutter binding'
  s.description      = <<-DESC
Embeddable JSON Database engine EJDB http://ejdb.org Flutter binding
                       DESC
  s.homepage         = 'https://ejdb.org'
  s.license          = { :file => '../LICENSE' }
  s.author           = { 'Softmotions Ltd.' => 'info@softmotions.com' }
  s.source           = { :path => '.' }
  s.source_files = 'Classes/**/*'
  s.static_framework = true
  s.dependency 'Flutter'
  s.dependency 'PathKit'
  s.dependency 'EJDB2'
  s.platform = :ios, '9.0'

  # Flutter.framework does not contain a i386 slice. Only x86_64 simulators are supported.
  s.pod_target_xcconfig = {
    'DEFINES_MODULE' => 'YES',
    'VALID_ARCHS[sdk=iphonesimulator*]' => 'x86_64'
  }
  s.swift_version = '5.1'
end
