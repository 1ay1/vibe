//! vibe — native Zig module wrapping libvibe.
//!
//! Unlike an FFI binding, this is a real Zig module you `@import`: it links
//! libvibe.a at build time (see build.zig) and exposes a small typed Zig API
//! over the C ABI (imported via @cImport of the real vibe.h).
//!
//! Build + test:  zig build test   (or: zig build run)

const std = @import("std");

const c = @cImport({
    @cInclude("vibe.h");
});

pub const Error = error{ ParseFailed, EmitFailed };

/// A parsed VIBE document. Call `deinit` to free.
pub const Doc = struct {
    root: *c.VibeValue,

    pub fn parse(data: []const u8) Error!Doc {
        var err: c.VibeError = undefined;
        const root = c.vibe_parse(data.ptr, data.len, &err);
        if (root == null) return Error.ParseFailed;
        return Doc{ .root = root.? };
    }

    pub fn deinit(self: Doc) void {
        c.vibe_value_free(self.root);
    }

    pub fn getString(self: Doc, path: [:0]const u8) ?[]const u8 {
        const s = c.vibe_get_string(self.root, path.ptr);
        if (s == null) return null;
        return std.mem.span(s);
    }

    pub fn getInt(self: Doc, path: [:0]const u8) i64 {
        return c.vibe_get_int(self.root, path.ptr);
    }

    pub fn getFloat(self: Doc, path: [:0]const u8) f64 {
        return c.vibe_get_float(self.root, path.ptr);
    }

    pub fn getBool(self: Doc, path: [:0]const u8) bool {
        return c.vibe_get_bool(self.root, path.ptr);
    }

    pub fn arraySize(self: Doc, path: [:0]const u8) usize {
        const arr = c.vibe_get_array(self.root, path.ptr);
        if (arr == null) return 0;
        return c.vibe_array_size(arr);
    }

    /// Caller owns the returned slice; free with `freeEmit`.
    pub fn emit(self: Doc) Error![]u8 {
        const s = c.vibe_emit(self.root);
        if (s == null) return Error.EmitFailed;
        return std.mem.span(s);
    }
};

pub fn freeEmit(s: []u8) void {
    c.vibe_free(s.ptr);
}

pub fn version() []const u8 {
    return std.mem.span(c.vibe_version());
}

// ---- self-test -------------------------------------------------------------
test "libvibe native module round-trips sample.vibe" {
    const sample = @embedFile("sample.vibe");

    try std.testing.expectEqualStrings("1.1.0", version());

    const doc = try Doc.parse(sample);
    defer doc.deinit();

    try std.testing.expectEqualStrings("libvibe", doc.getString("name").?);
    try std.testing.expectEqual(@as(i64, 42), doc.getInt("answer"));
    try std.testing.expect(@abs(doc.getFloat("pi") - 3.14159) < 1e-9);
    try std.testing.expect(doc.getBool("enabled"));
    try std.testing.expectEqualStrings("localhost", doc.getString("server.host").?);
    try std.testing.expectEqual(@as(i64, 8080), doc.getInt("server.port"));
    try std.testing.expectEqual(@as(usize, 3), doc.arraySize("ports"));

    const out = try doc.emit();
    defer freeEmit(out);
    try std.testing.expect(std.mem.indexOf(u8, out, "libvibe") != null);

    try std.testing.expectError(Error.ParseFailed, Doc.parse("name {"));
}

// A runnable entry point for `zig build run`.
pub fn main() !void {
    const sample = @embedFile("sample.vibe");
    const doc = try Doc.parse(sample);
    defer doc.deinit();
    if (!std.mem.eql(u8, doc.getString("name").?, "libvibe")) return error.Mismatch;
    if (doc.arraySize("ports") != 3) return error.Mismatch;
    const msg = "ALL OK (zig native / module)\n";
    _ = std.c.write(1, msg.ptr, msg.len);
}
