# frozen_string_literal: true

Gem::Specification.new do |s|
  s.name        = "vibe-lang"
  s.version     = "1.2.0"
  s.summary     = "VIBE config language as a native Ruby C-extension"
  s.description = "A native Ruby C-extension for libvibe. The single-header C " \
                  "library is compiled from source (vendored), so there is no " \
                  "runtime .so dependency. Provides a VIBE() heredoc DSL."
  s.authors     = ["1ay1"]
  s.homepage    = "https://github.com/1ay1/vibe"
  s.license     = "MIT"
  s.required_ruby_version = ">= 2.7"

  s.files = %w[
    vibe.c
    vibe_dsl.rb
    extconf.rb
    vendor/vibe.h
    vendor/vibe_impl.c
    README.md
  ]
  s.extensions = ["extconf.rb"]
  s.require_paths = ["."]

  s.metadata = {
    "source_code_uri" => "https://github.com/1ay1/vibe",
    "bug_tracker_uri" => "https://github.com/1ay1/vibe/issues",
  }
end
