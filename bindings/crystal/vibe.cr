# libvibe — Crystal binding. Crystal's `lib` blocks bind C directly.
#
#   CRYSTAL_LIBRARY_PATH=/path LD_LIBRARY_PATH=/path crystal run vibe.cr

@[Link("vibe")]
lib LibVibe
  struct VibeError
    has_error : Bool
    code : Int32
    message : LibC::Char*
    line : Int32
    column : Int32
  end

  fun vibe_version : LibC::Char*
  fun vibe_parse(data : UInt8*, length : LibC::SizeT, err : VibeError*) : Void*
  fun vibe_get_string(v : Void*, path : LibC::Char*) : LibC::Char*
  fun vibe_get_int(v : Void*, path : LibC::Char*) : Int64
  fun vibe_get_float(v : Void*, path : LibC::Char*) : Float64
  fun vibe_get_bool(v : Void*, path : LibC::Char*) : Bool
  fun vibe_get_array(v : Void*, path : LibC::Char*) : Void*
  fun vibe_array_size(a : Void*) : LibC::SizeT
  fun vibe_emit(v : Void*) : LibC::Char*
  fun vibe_free(p : Void*) : Void
  fun vibe_value_free(v : Void*) : Void
  fun vibe_error_code_string(code : Int32) : LibC::Char*
end

module Vibe
  def self.version : String
    String.new(LibVibe.vibe_version)
  end

  class Doc
    def initialize(@ptr : Void*)
    end

    def get_string(path : String) : String
      p = LibVibe.vibe_get_string(@ptr, path)
      p.null? ? "" : String.new(p)
    end

    def get_int(path : String) : Int64
      LibVibe.vibe_get_int(@ptr, path)
    end

    def get_float(path : String) : Float64
      LibVibe.vibe_get_float(@ptr, path)
    end

    def get_bool(path : String) : Bool
      LibVibe.vibe_get_bool(@ptr, path)
    end

    def array_size(path : String) : Int32
      arr = LibVibe.vibe_get_array(@ptr, path)
      arr.null? ? 0 : LibVibe.vibe_array_size(arr).to_i32
    end

    def emit : String
      raw = LibVibe.vibe_emit(@ptr)
      return "" if raw.null?
      s = String.new(raw)
      LibVibe.vibe_free(raw.as(Void*))
      s
    end

    def free
      LibVibe.vibe_value_free(@ptr)
    end
  end

  def self.parse(data : Bytes) : Doc
    err = LibVibe::VibeError.new
    ptr = LibVibe.vibe_parse(data.to_unsafe, data.size, pointerof(err))
    if ptr.null?
      code = String.new(LibVibe.vibe_error_code_string(err.code))
      raise "VibeError: #{code} at #{err.line}:#{err.column}"
    end
    Doc.new(ptr)
  end
end

sample = ENV.fetch("VIBE_SAMPLE", "../sample.vibe")
doc = Vibe.parse(File.read(sample).to_slice)
ok = true

check = ->(name : String, got : String, want : String) do
  ok = false if got != want
  puts "  [#{got == want ? "ok " : "BAD"}] #{name} = #{got}"
end

ok = false unless Vibe.version == "1.1.0"
puts "  [#{Vibe.version == "1.1.0" ? "ok " : "BAD"}] version = #{Vibe.version}"
check.call("name", doc.get_string("name"), "libvibe")
ok = false unless doc.get_int("answer") == 42
puts "  [#{doc.get_int("answer") == 42 ? "ok " : "BAD"}] answer = #{doc.get_int("answer")}"
pi = (doc.get_float("pi") * 100000).round / 100000
ok = false unless pi == 3.14159
puts "  [#{pi == 3.14159 ? "ok " : "BAD"}] pi = #{pi}"
ok = false unless doc.get_bool("enabled")
puts "  [#{doc.get_bool("enabled") ? "ok " : "BAD"}] enabled = #{doc.get_bool("enabled")}"
check.call("server.host", doc.get_string("server.host"), "localhost")
ok = false unless doc.get_int("server.port") == 8080
puts "  [#{doc.get_int("server.port") == 8080 ? "ok " : "BAD"}] server.port = #{doc.get_int("server.port")}"
ok = false unless doc.array_size("ports") == 3
puts "  [#{doc.array_size("ports") == 3 ? "ok " : "BAD"}] len(ports) = #{doc.array_size("ports")}"

if doc.emit.includes?("libvibe")
  puts "  [ok ] emit() round-trips"
else
  ok = false
  puts "  [BAD] emit() did not round-trip"
end

begin
  Vibe.parse("name {".to_slice)
  ok = false
  puts "  [BAD] malformed input did not raise"
rescue
  puts "  [ok ] rejects malformed input"
end

puts(ok ? "ALL OK (crystal)" : "FAILED (crystal)")
exit(ok ? 0 : 1)
