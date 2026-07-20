//! Build the native Zig libvibe module + run its tests.
//!
//!   zig build test    # runs the in-module test block
//!   zig build run     # runs main() -> prints ALL OK
//!
//! Links the static libvibe.a from the repo root and adds the root to the C
//! include path so @cImport("vibe.h") resolves. sample.vibe is wired in as a
//! named anonymous import so @embedFile("sample.vibe") works from this dir.

const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    // Repo root, three levels up from bindings/native/zig.
    const root = "../../..";

    const mod = b.createModule(.{
        .root_source_file = b.path("vibe.zig"),
        .target = target,
        .optimize = optimize,
    });
    // Make @embedFile("sample.vibe") resolve to the shared test doc.
    mod.addAnonymousImport("sample.vibe", .{
        .root_source_file = b.path("../../sample.vibe"),
    });
    mod.addIncludePath(b.path(root));
    mod.addObjectFile(b.path(root ++ "/libvibe.a"));
    mod.link_libc = true;

    const exe = b.addExecutable(.{ .name = "vibe-native", .root_module = mod });
    b.installArtifact(exe);

    const run = b.addRunArtifact(exe);
    b.step("run", "Run the native Zig smoke test").dependOn(&run.step);

    const tests = b.addTest(.{ .root_module = mod });
    const run_tests = b.addRunArtifact(tests);
    b.step("test", "Run the native Zig module tests").dependOn(&run_tests.step);
}
