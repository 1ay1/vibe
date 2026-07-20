// libvibe — C# binding via P/Invoke. Runs as a .NET file-based app:
//
//   LD_LIBRARY_PATH=/path dotnet run vibe.cs
//
// (or drop this in a console project as Program.cs).

using System;
using System.IO;
using System.Runtime.InteropServices;

string sample = Environment.GetEnvironmentVariable("VIBE_SAMPLE") ?? "../sample.vibe";
byte[] data = File.ReadAllBytes(sample);

IntPtr v = Native.vibe_parse(data, (nuint)data.Length, out VibeError err);
if (v == IntPtr.Zero)
{
    Console.WriteLine("FAILED (csharp): parse error");
    Environment.Exit(1);
}

bool ok = true;
void Check(string name, object got, object want)
{
    bool pass = Equals(got, want);
    if (!pass) ok = false;
    Console.WriteLine($"  [{(pass ? "ok " : "BAD")}] {name} = {got}");
}
string Str(IntPtr p) => p == IntPtr.Zero ? "" : Marshal.PtrToStringUTF8(p) ?? "";

Check("version", Str(Native.vibe_version()), "1.1.0");
Check("name", Str(Native.vibe_get_string(v, "name")), "libvibe");
Check("answer", Native.vibe_get_int(v, "answer"), 42L);
Check("pi", Math.Round(Native.vibe_get_float(v, "pi"), 5), 3.14159);
Check("enabled", Native.vibe_get_bool(v, "enabled"), true);
Check("server.host", Str(Native.vibe_get_string(v, "server.host")), "localhost");
Check("server.port", Native.vibe_get_int(v, "server.port"), 8080L);
IntPtr arr = Native.vibe_get_array(v, "ports");
Check("len(ports)", arr == IntPtr.Zero ? (nuint)0 : Native.vibe_array_size(arr), (nuint)3);

IntPtr raw = Native.vibe_emit(v);
string emitted = Str(raw);
if (emitted.Contains("libvibe")) Console.WriteLine("  [ok ] emit() round-trips");
else { ok = false; Console.WriteLine("  [BAD] emit() did not round-trip"); }
if (raw != IntPtr.Zero) Native.vibe_free(raw);

byte[] bad = System.Text.Encoding.UTF8.GetBytes("name {");
if (Native.vibe_parse(bad, (nuint)bad.Length, out _) == IntPtr.Zero)
    Console.WriteLine("  [ok ] rejects malformed input");
else { ok = false; Console.WriteLine("  [BAD] malformed input did not raise"); }

Native.vibe_value_free(v);
Console.WriteLine(ok ? "ALL OK (csharp)" : "FAILED (csharp)");
Environment.Exit(ok ? 0 : 1);

[StructLayout(LayoutKind.Sequential)]
struct VibeError
{
    [MarshalAs(UnmanagedType.I1)] public bool has_error;
    public int code;
    public IntPtr message;
    public int line;
    public int column;
}

static class Native
{
    const string L = "vibe";
    const CharSet U = CharSet.Ansi;
    [DllImport(L)] public static extern IntPtr vibe_version();
    [DllImport(L)] public static extern IntPtr vibe_parse(byte[] data, nuint len, out VibeError err);
    [DllImport(L, CharSet = U)] public static extern IntPtr vibe_get_string(IntPtr v, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(L, CharSet = U)] public static extern long vibe_get_int(IntPtr v, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(L, CharSet = U)] public static extern double vibe_get_float(IntPtr v, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(L, CharSet = U)] [return: MarshalAs(UnmanagedType.I1)] public static extern bool vibe_get_bool(IntPtr v, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(L, CharSet = U)] public static extern IntPtr vibe_get_array(IntPtr v, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(L)] public static extern nuint vibe_array_size(IntPtr a);
    [DllImport(L)] public static extern IntPtr vibe_emit(IntPtr v);
    [DllImport(L)] public static extern void vibe_free(IntPtr p);
    [DllImport(L)] public static extern void vibe_value_free(IntPtr v);
    [DllImport(L)] public static extern IntPtr vibe_error_code_string(int code);
}
