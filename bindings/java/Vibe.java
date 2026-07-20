// libvibe — Java binding using the Foreign Function & Memory API (JEP 454,
// final since Java 22). No JNI, no native glue. Single-file launch:
//
//   java --enable-native-access=ALL-UNNAMED Vibe.java

import java.lang.foreign.*;
import java.lang.invoke.MethodHandle;
import java.nio.file.Files;
import java.nio.file.Path;

import static java.lang.foreign.ValueLayout.*;

public class Vibe {
    static final Linker LINKER = Linker.nativeLinker();
    static final Arena ARENA = Arena.ofShared();
    static SymbolLookup LOOKUP;

    static MethodHandle h(String name, FunctionDescriptor fd) {
        return LINKER.downcallHandle(LOOKUP.find(name).orElseThrow(), fd);
    }

    // C string helpers
    static MemorySegment cstr(String s) { return ARENA.allocateFrom(s); }
    static String str(MemorySegment p) {
        return p.address() == 0 ? null : p.reinterpret(Long.MAX_VALUE).getString(0);
    }

    public static void main(String[] args) throws Throwable {
        String libPath = System.getenv().getOrDefault("VIBE_LIB", "../../libvibe.so");
        LOOKUP = SymbolLookup.libraryLookup(libPath, ARENA);

        MethodHandle version = h("vibe_version", FunctionDescriptor.of(ADDRESS));
        MethodHandle parse = h("vibe_parse", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS));
        MethodHandle getStr = h("vibe_get_string", FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS));
        MethodHandle getInt = h("vibe_get_int", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS));
        MethodHandle getFloat = h("vibe_get_float", FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, ADDRESS));
        MethodHandle getBool = h("vibe_get_bool", FunctionDescriptor.of(JAVA_BOOLEAN, ADDRESS, ADDRESS));
        MethodHandle getArr = h("vibe_get_array", FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS));
        MethodHandle arrSize = h("vibe_array_size", FunctionDescriptor.of(JAVA_LONG, ADDRESS));
        MethodHandle emit = h("vibe_emit", FunctionDescriptor.of(ADDRESS, ADDRESS));
        MethodHandle vfree = h("vibe_value_free", FunctionDescriptor.ofVoid(ADDRESS));
        MethodHandle freeMem = h("vibe_free", FunctionDescriptor.ofVoid(ADDRESS));

        // VibeError layout: bool + pad(3) + int + ptr + int + int
        MemoryLayout errLayout = MemoryLayout.structLayout(
            JAVA_BOOLEAN.withName("has_error"),
            MemoryLayout.paddingLayout(3),
            JAVA_INT.withName("code"),
            ADDRESS.withName("message"),
            JAVA_INT.withName("line"),
            JAVA_INT.withName("column"));

        String sample = System.getenv().getOrDefault("VIBE_SAMPLE", "../sample.vibe");
        byte[] bytes = Files.readAllBytes(Path.of(sample));
        MemorySegment data = ARENA.allocate(bytes.length + 1);
        MemorySegment.copy(bytes, 0, data, JAVA_BYTE, 0, bytes.length);

        MemorySegment err = ARENA.allocate(errLayout);
        MemorySegment v = (MemorySegment) parse.invoke(data, (long) bytes.length, err);
        if (v.address() == 0) {
            System.out.println("FAILED (java): parse error");
            System.exit(1);
        }

        boolean[] ok = { true };

        String ver = str((MemorySegment) version.invoke());
        check(ok, "version", ver, "1.1.0");
        check(ok, "name", str((MemorySegment) getStr.invoke(v, cstr("name"))), "libvibe");
        check(ok, "answer", (long) getInt.invoke(v, cstr("answer")), 42L);
        double pi = Math.round((double) getFloat.invoke(v, cstr("pi")) * 100000.0) / 100000.0;
        check(ok, "pi", pi, 3.14159);
        check(ok, "enabled", (boolean) getBool.invoke(v, cstr("enabled")), true);
        check(ok, "server.host", str((MemorySegment) getStr.invoke(v, cstr("server.host"))), "localhost");
        check(ok, "server.port", (long) getInt.invoke(v, cstr("server.port")), 8080L);
        MemorySegment arr = (MemorySegment) getArr.invoke(v, cstr("ports"));
        long n = arr.address() == 0 ? 0 : (long) arrSize.invoke(arr);
        check(ok, "len(ports)", n, 3L);

        MemorySegment raw = (MemorySegment) emit.invoke(v);
        String emitted = str(raw);
        if (emitted != null && emitted.contains("libvibe")) {
            System.out.println("  [ok ] emit() round-trips");
        } else {
            ok[0] = false;
            System.out.println("  [BAD] emit() did not round-trip");
        }
        if (raw.address() != 0) freeMem.invoke(raw);

        MemorySegment bad = ARENA.allocateFrom("name {");
        MemorySegment err2 = ARENA.allocate(errLayout);
        MemorySegment badv = (MemorySegment) parse.invoke(bad, 6L, err2);
        if (badv.address() == 0) {
            System.out.println("  [ok ] rejects malformed input");
        } else {
            ok[0] = false;
            System.out.println("  [BAD] malformed input did not raise");
        }

        vfree.invoke(v);
        System.out.println(ok[0] ? "ALL OK (java)" : "FAILED (java)");
        System.exit(ok[0] ? 0 : 1);
    }

    static void check(boolean[] ok, String name, Object got, Object want) {
        boolean pass = java.util.Objects.equals(got, want);
        if (!pass) ok[0] = false;
        System.out.printf("  [%s] %s = %s%n", pass ? "ok " : "BAD", name, got);
    }
}
