#!/usr/bin/env ruby
# Smoke test: native Ruby C-extension + VIBE-as-native-Ruby-syntax.
$LOAD_PATH.unshift(__dir__)
require "vibe"
require_relative "vibe_dsl"

sample = File.read(File.expand_path("../../sample.vibe", __dir__))

# ---- low-level native extension still works ---------------------------
raise "version" unless Vibe.version == "1.2.0"
doc = Vibe.parse(sample)
raise "name"   unless doc.get_string("name") == "libvibe"
raise "answer" unless doc.get_int("answer") == 42
raise "ports"  unless doc.array_size("ports") == 3
raise "emit"   unless doc.emit.include?("libvibe")
raise "native" unless doc.to_native["server"]["port"] == 8080

# ---- VIBE as native Ruby syntax: the VIBE() heredoc DSL ---------------
cfg = VIBE(<<~VIBE)
  name    my-service
  port    8080
  tls     true
  ratio   0.75
  origins [ https://a.example  https://b.example ]
  db {
    host  localhost
    port  5432
  }
VIBE

# Method access via method_missing.
raise "dsl name"  unless cfg.name == "my-service"
raise "dsl port"  unless cfg.port == 8080
raise "dsl tls"   unless cfg.tls == true
raise "dsl ratio" unless cfg.ratio == 0.75
# Nested — dot access and symbol/string indexing.
raise "nested host" unless cfg.db.host == "localhost"
raise "nested port" unless cfg[:db][:port] == 5432
raise "str index"   unless cfg["db"]["host"] == "localhost"
# Arrays are real Arrays.
raise "arr class" unless cfg.origins.is_a?(Array)
raise "arr len"   unless cfg.origins.length == 2
raise "arr[0]"    unless cfg.origins[0] == "https://a.example"
raise "arr each"  unless cfg.origins.all? { |o| o.start_with?("https://") }
# Missing keys raise NoMethodError like any Ruby object.
begin
  cfg.nope
  raise "expected NoMethodError"
rescue NoMethodError
end
raise "key?" if cfg.key?(:nope)

# The String#to_vibe alias reads native too.
c2 = <<~VIBE.to_vibe
  service api
  workers 4
VIBE
raise "to_vibe" unless c2.service == "api" && c2.workers == 4

# Malformed input raises the native error type.
begin
  VIBE("name {")
  raise "expected Vibe::Error on malformed input"
rescue Vibe::Error
end

puts "ALL OK (ruby native / C-extension, VIBE() heredoc DSL)"
