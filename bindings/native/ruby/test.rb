#!/usr/bin/env ruby
# Smoke test for the NATIVE Ruby C-extension (require "vibe").
$LOAD_PATH.unshift(__dir__)
require "vibe"

sample = File.read(File.expand_path("../../sample.vibe", __dir__))

raise "version" unless Vibe.version == "1.1.0"

doc = Vibe.parse(sample)
raise "name"    unless doc.get_string("name") == "libvibe"
raise "answer"  unless doc.get_int("answer") == 42
raise "pi"      unless (doc.get_float("pi") - 3.14159).abs < 1e-9
raise "enabled" unless doc.get_bool("enabled") == true
raise "host"    unless doc.get_string("server.host") == "localhost"
raise "port"    unless doc.get_int("server.port") == 8080
raise "ports"   unless doc.array_size("ports") == 3
raise "emit"    unless doc.emit.include?("libvibe")

begin
  Vibe.parse("name {")
  raise "expected Vibe::Error on malformed input"
rescue Vibe::Error
end

puts "ALL OK (ruby native / C-extension)"
