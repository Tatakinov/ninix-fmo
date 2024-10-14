# frozen_string_literal: true

require_relative "lib/ninix_fmo/version"

Gem::Specification.new do |spec|
  spec.name = "ninix_fmo"
  spec.version = NinixFMO::VERSION
  spec.authors = ["Tatakinov"]
  spec.email = ["tatakinov@gmail.com"]

  spec.summary = "A gem for using FileMappingObject in ninix-kagari"
  spec.homepage = "https://tatakinov.github.io/"
  spec.license = "MIT"
  spec.required_ruby_version = ">= 2.6.0"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = "https://github.com/Tatakinov/ninix_fmo"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir.chdir(File.expand_path(__dir__)) do
    `git ls-files -z`.split("\x0").reject do |f|
      (f == __FILE__) || f.match(%r{\A(?:(?:bin|test|spec|features)/|\.(?:git|travis|circleci)|appveyor)})
    end
  end
  spec.bindir = "exe"
  spec.executables = spec.files.grep(%r{\Aexe/}) { |f| File.basename(f) }
  spec.require_paths = ["lib"]
  spec.extensions = %w{ext/ninix_fmo/extconf.rb}

  spec.add_development_dependency "bundler", "~> 1.5"
  spec.add_development_dependency "rake"
  spec.add_development_dependency "rake-compiler"
end
