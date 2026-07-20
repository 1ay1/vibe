# libvibe — Julia binding using the built-in `ccall` FFI (no packages).
#
#   include("vibe.jl")
#   doc = Vibe.parse(read("config.vibe"))
#   Vibe.get_string(doc, "server.host")

module Vibe

const LIB = get(ENV, "VIBE_LIB", joinpath(@__DIR__, "..", "..", "libvibe.so"))

# Mirrors VibeError in vibe.h; Julia lays isbits structs out C-compatibly.
struct CError
    has_error::Bool
    code::Cint
    message::Ptr{Cchar}
    line::Cint
    column::Cint
end

struct VibeError <: Exception
    code::String
    message::String
    line::Int
    column::Int
end

version() = unsafe_string(ccall((:vibe_version, LIB), Cstring, ()))

struct Doc
    ptr::Ptr{Cvoid}
end

function parse(data::Vector{UInt8})
    err = Ref{CError}(CError(false, 0, C_NULL, 0, 0))
    ptr = ccall((:vibe_parse, LIB), Ptr{Cvoid},
                (Ptr{UInt8}, Csize_t, Ptr{CError}), data, length(data), err)
    if ptr == C_NULL
        e = err[]
        code = unsafe_string(ccall((:vibe_error_code_string, LIB), Cstring, (Cint,), e.code))
        msg = e.message == C_NULL ? "parse error" : unsafe_string(e.message)
        throw(VibeError(code, msg, Int(e.line), Int(e.column)))
    end
    Doc(ptr)
end
parse(s::AbstractString) = parse(Vector{UInt8}(s))

function get_string(d::Doc, path)
    p = ccall((:vibe_get_string, LIB), Cstring, (Ptr{Cvoid}, Cstring), d.ptr, path)
    p == C_NULL ? nothing : unsafe_string(p)
end
get_int(d::Doc, path) = ccall((:vibe_get_int, LIB), Int64, (Ptr{Cvoid}, Cstring), d.ptr, path)
get_float(d::Doc, path) = ccall((:vibe_get_float, LIB), Cdouble, (Ptr{Cvoid}, Cstring), d.ptr, path)
get_bool(d::Doc, path) = ccall((:vibe_get_bool, LIB), Bool, (Ptr{Cvoid}, Cstring), d.ptr, path)

function array_size(d::Doc, path)
    arr = ccall((:vibe_get_array, LIB), Ptr{Cvoid}, (Ptr{Cvoid}, Cstring), d.ptr, path)
    arr == C_NULL ? 0 : Int(ccall((:vibe_array_size, LIB), Csize_t, (Ptr{Cvoid},), arr))
end

function emit(d::Doc)
    raw = ccall((:vibe_emit, LIB), Ptr{Cchar}, (Ptr{Cvoid},), d.ptr)
    raw == C_NULL && return ""
    s = unsafe_string(raw)
    ccall((:vibe_free, LIB), Cvoid, (Ptr{Cvoid},), raw)
    s
end

free(d::Doc) = ccall((:vibe_value_free, LIB), Cvoid, (Ptr{Cvoid},), d.ptr)

end # module

if abspath(PROGRAM_FILE) == @__FILE__
    sample = get(ENV, "VIBE_SAMPLE", joinpath(@__DIR__, "..", "sample.vibe"))
    doc = Vibe.parse(read(sample))
    ok = Ref(true)
    function check(name, got, want)
        got != want && (ok[] = false)
        println("  [$(got == want ? "ok " : "BAD")] $name = $(repr(got))")
    end
    check("version", Vibe.version(), "1.1.0")
    check("name", Vibe.get_string(doc, "name"), "libvibe")
    check("answer", Vibe.get_int(doc, "answer"), 42)
    check("pi", round(Vibe.get_float(doc, "pi"), digits=5), 3.14159)
    check("enabled", Vibe.get_bool(doc, "enabled"), true)
    check("server.host", Vibe.get_string(doc, "server.host"), "localhost")
    check("server.port", Vibe.get_int(doc, "server.port"), 8080)
    check("len(ports)", Vibe.array_size(doc, "ports"), 3)
    if occursin("libvibe", Vibe.emit(doc))
        println("  [ok ] emit() round-trips")
    else
        ok[] = false; println("  [BAD] emit() did not round-trip")
    end
    try
        Vibe.parse("name {")
        ok[] = false; println("  [BAD] malformed input did not raise")
    catch e
        println("  [ok ] rejects malformed input: $(e.code)")
    end
    println(ok[] ? "ALL OK (julia)" : "FAILED (julia)")
    exit(ok[] ? 0 : 1)
end
