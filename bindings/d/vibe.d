// libvibe — D binding (extern(C) against the shared library).
//
//   ldc2 vibe.d -L-L/path -L-lvibe -of=vibe_d   (or dmd)
//   LD_LIBRARY_PATH=/path ./vibe_d

import std.stdio, std.string, std.math, std.file, std.process, std.conv;

extern (C) {
    struct VibeError {
        bool has_error;
        int code;
        const(char)* message;
        int line;
        int column;
    }
    const(char)* vibe_version();
    void* vibe_parse(const(char)* data, size_t length, VibeError* err);
    const(char)* vibe_get_string(void* v, const(char)* path);
    long vibe_get_int(void* v, const(char)* path);
    double vibe_get_float(void* v, const(char)* path);
    bool vibe_get_bool(void* v, const(char)* path);
    void* vibe_get_array(void* v, const(char)* path);
    size_t vibe_array_size(void* a);
    char* vibe_emit(void* v);
    void vibe_free(void* p);
    void vibe_value_free(void* v);
    const(char)* vibe_error_code_string(int code);
}

string getStr(void* v, string path) {
    auto p = vibe_get_string(v, path.toStringz);
    return p is null ? "" : p.fromStringz.idup;
}

void main() {
    string sample = environment.get("VIBE_SAMPLE", "../sample.vibe");
    auto data = cast(string) std.file.read(sample);

    VibeError err;
    void* v = vibe_parse(data.ptr, data.length, &err);
    if (v is null) {
        writeln("FAILED (d): parse error");
        import core.stdc.stdlib : exit;
        exit(1);
    }

    bool ok = true;
    void check(T)(string name, T got, T want) {
        if (got != want) ok = false;
        writefln("  [%s] %s = %s", got == want ? "ok " : "BAD", name, got);
    }
    check!string("version", vibe_version().fromStringz.idup, "1.1.0");
    check!string("name", getStr(v, "name"), "libvibe");
    check!long("answer", vibe_get_int(v, "answer"), 42);
    check!double("pi", (vibe_get_float(v, "pi") * 100000).round / 100000, 3.14159);
    check!bool("enabled", vibe_get_bool(v, "enabled"), true);
    check!string("server.host", getStr(v, "server.host"), "localhost");
    check!long("server.port", vibe_get_int(v, "server.port"), 8080);
    auto arr = vibe_get_array(v, "ports");
    check!size_t("len(ports)", arr is null ? 0 : vibe_array_size(arr), 3);

    auto raw = vibe_emit(v);
    if (raw !is null && raw.fromStringz.indexOf("libvibe") >= 0) {
        writeln("  [ok ] emit() round-trips");
        vibe_free(raw);
    } else {
        ok = false;
        writeln("  [BAD] emit() did not round-trip");
    }

    VibeError e2;
    if (vibe_parse("name {".ptr, 6, &e2) is null)
        writeln("  [ok ] rejects malformed input");
    else {
        ok = false;
        writeln("  [BAD] malformed input did not raise");
    }

    vibe_value_free(v);
    writeln(ok ? "ALL OK (d)" : "FAILED (d)");
    import core.stdc.stdlib : exit;
    exit(ok ? 0 : 1);
}
