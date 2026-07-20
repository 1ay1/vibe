// libvibe — Zig binding. Zig imports the real C header directly with @cImport,
// so it sees the exact ABI with zero hand-written declarations.
//
//   zig run vibe.zig -I../.. -L../.. -lvibe -lc

const std = @import("std");
const c = @cImport({
    @cInclude("stdio.h");
    @cInclude("stdlib.h");
    @cInclude("vibe.h");
});

pub fn main() void {
    const penv = c.getenv("VIBE_SAMPLE");
    const path: [*c]const u8 = if (penv != null) penv else "../sample.vibe";
    const f = c.fopen(path, "rb") orelse {
        std.debug.print("FAILED (zig): cannot open sample\n", .{});
        std.process.exit(1);
    };
    _ = c.fseek(f, 0, c.SEEK_END);
    const size: usize = @intCast(c.ftell(f));
    _ = c.fseek(f, 0, c.SEEK_SET);
    const buf: [*]u8 = @ptrCast(c.malloc(size).?);
    const nread = c.fread(buf, 1, size, f);
    _ = c.fclose(f);
    const data: []const u8 = buf[0..nread];

    var err: c.VibeError = undefined;
    const v = c.vibe_parse(data.ptr, data.len, &err);
    if (v == null) {
        std.debug.print("FAILED (zig): parse error\n", .{});
        std.process.exit(1);
    }

    var ok = true;
    const p = std.debug.print;

    // version
    const ver = std.mem.span(c.vibe_version());
    ok = check_str("version", ver, "1.2.0") and ok;
    ok = check_str("name", std.mem.span(c.vibe_get_string(v, "name")), "libvibe") and ok;
    ok = check_int("answer", c.vibe_get_int(v, "answer"), 42) and ok;

    const pi = c.vibe_get_float(v, "pi");
    const pi_ok = @abs(pi - 3.14159) < 1e-9;
    if (!pi_ok) ok = false;
    p("  [{s}] pi = {d}\n", .{ if (pi_ok) "ok " else "BAD", pi });

    const enabled = c.vibe_get_bool(v, "enabled");
    if (!enabled) ok = false;
    p("  [{s}] enabled = {}\n", .{ if (enabled) "ok " else "BAD", enabled });

    ok = check_str("server.host", std.mem.span(c.vibe_get_string(v, "server.host")), "localhost") and ok;
    ok = check_int("server.port", c.vibe_get_int(v, "server.port"), 8080) and ok;

    const arr = c.vibe_get_array(v, "ports");
    const n = if (arr != null) c.vibe_array_size(arr) else 0;
    ok = check_int("len(ports)", @intCast(n), 3) and ok;

    const emitted = std.mem.span(c.vibe_emit(v));
    if (std.mem.indexOf(u8, emitted, "libvibe") != null) {
        p("  [ok ] emit() round-trips\n", .{});
    } else {
        ok = false;
        p("  [BAD] emit() did not round-trip\n", .{});
    }

    var e2: c.VibeError = undefined;
    const bad = c.vibe_parse("name {", 6, &e2);
    if (bad == null) {
        p("  [ok ] rejects malformed input\n", .{});
    } else {
        ok = false;
        p("  [BAD] malformed input did not raise\n", .{});
    }

    p("{s}\n", .{if (ok) "ALL OK (zig)" else "FAILED (zig)"});
    std.process.exit(if (ok) 0 else 1);
}

fn check_str(name: []const u8, got: []const u8, want: []const u8) bool {
    const pass = std.mem.eql(u8, got, want);
    std.debug.print("  [{s}] {s} = {s}\n", .{ if (pass) "ok " else "BAD", name, got });
    return pass;
}

fn check_int(name: []const u8, got: i64, want: i64) bool {
    const pass = got == want;
    std.debug.print("  [{s}] {s} = {d}\n", .{ if (pass) "ok " else "BAD", name, got });
    return pass;
}
