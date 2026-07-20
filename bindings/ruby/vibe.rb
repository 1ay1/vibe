#!/usr/bin/env ruby
# frozen_string_literal: true
#
# libvibe — Ruby binding using Fiddle (Ruby standard library, no gems).
#
#   require_relative "vibe"
#   doc = Vibe.parse(File.binread("config.vibe"))
#   host = doc.get_string("server.host")

require "fiddle"
require "fiddle/import"

module Vibe
  module LibC
    extend Fiddle::Importer
    lib = ENV["VIBE_LIB"] || File.expand_path("../../libvibe.so", __dir__)
    dlload lib
    extern "const char* vibe_version()"
    extern "void* vibe_parse(const char*, size_t, void*)"
    extern "const char* vibe_get_string(void*, const char*)"
    extern "long long vibe_get_int(void*, const char*)"
    extern "double vibe_get_float(void*, const char*)"
    extern "char vibe_get_bool(void*, const char*)"
    extern "void* vibe_get_array(void*, const char*)"
    extern "size_t vibe_array_size(void*)"
    extern "char* vibe_emit(void*)"
    extern "void vibe_free(void*)"
    extern "void vibe_value_free(void*)"
    extern "const char* vibe_error_code_string(int)"
  end

  class Error < StandardError
    attr_reader :code, :line, :column
    def initialize(code, message, line, column)
      @code = code
      @line = line
      @column = column
      super("#{message} (code #{code} at #{line}:#{column})")
    end
  end

  def self.version
    LibC.vibe_version.to_s
  end

  class Doc
    def initialize(ptr)
      @ptr = ptr
    end

    def get_string(path)
      p = LibC.vibe_get_string(@ptr, path)
      p.null? ? nil : p.to_s
    end

    def get_int(path)  = LibC.vibe_get_int(@ptr, path)
    def get_float(path) = LibC.vibe_get_float(@ptr, path)
    def get_bool(path) = LibC.vibe_get_bool(@ptr, path) != 0

    def array_size(path)
      arr = LibC.vibe_get_array(@ptr, path)
      arr.null? ? 0 : LibC.vibe_array_size(arr)
    end

    def emit
      raw = LibC.vibe_emit(@ptr)
      return "" if raw.null?
      s = raw.to_s
      LibC.vibe_free(raw)
      s
    end

    def free
      LibC.vibe_value_free(@ptr) unless @ptr.null?
      @ptr = Fiddle::NULL
    end
  end

  # VibeError { bool@0; int code@4; char* message@8; int line@16; int column@20 } = 24 bytes
  def self.parse(data)
    data = data.b
    buf = ("\0".b * 24)
    err = Fiddle::Pointer[buf]
    ptr = LibC.vibe_parse(data, data.bytesize, err)
    return Doc.new(ptr) unless ptr.null?

    code_num = buf[4, 4].unpack1("l")
    msg_addr = buf[8, 8].unpack1("J")
    line = buf[16, 4].unpack1("l")
    col  = buf[20, 4].unpack1("l")
    code = LibC.vibe_error_code_string(code_num).to_s
    msg  = msg_addr.zero? ? "parse error" : Fiddle::Pointer.new(msg_addr).to_s
    raise Error.new(code, msg, line, col)
  end
end

if __FILE__ == $PROGRAM_NAME
  sample = ENV["VIBE_SAMPLE"] || File.expand_path("../sample.vibe", __dir__)
  doc = Vibe.parse(File.binread(sample))
  ok = true
  checks = [
    ["version",     Vibe.version,              "1.1.0"],
    ["name",        doc.get_string("name"),    "libvibe"],
    ["answer",      doc.get_int("answer"),     42],
    ["pi",          doc.get_float("pi").round(5), 3.14159],
    ["enabled",     doc.get_bool("enabled"),   true],
    ["server.host", doc.get_string("server.host"), "localhost"],
    ["server.port", doc.get_int("server.port"), 8080],
    ["len(ports)",  doc.array_size("ports"),   3],
  ]
  checks.each do |name, got, want|
    ok = false if got != want
    puts "  [#{got == want ? 'ok ' : 'BAD'}] #{name} = #{got.inspect}"
  end
  if doc.emit.include?("libvibe")
    puts "  [ok ] emit() round-trips"
  else
    ok = false; puts "  [BAD] emit() did not round-trip"
  end
  begin
    Vibe.parse("name {")
    ok = false; puts "  [BAD] malformed input did not raise"
  rescue Vibe::Error => e
    puts "  [ok ] rejects malformed input: #{e.code}"
  end
  puts(ok ? "ALL OK (ruby)" : "FAILED (ruby)")
  exit(ok ? 0 : 1)
end
