// Vibe.java — JNI native bindings for libvibe (a TRUE native method interface,
// not the FFM/Panama bindings in bindings/java/).
//
// The `native` methods below are implemented in vibe_jni.c, compiled to
// libvibejni.so and loaded via System.load. The JVM <-> C boundary is the
// JNI ABI backed by compiled native code that links libvibe.a.
//
//   javac Vibe.java
//   javah is gone; header comes from `javac -h .`
//   gcc ... -> libvibejni.so
//   java -Djava.library.path=. Vibe
public final class Vibe {
    static {
        String lib = System.getProperty("vibe.jni.path");
        if (lib != null) {
            System.load(lib);
        } else {
            System.loadLibrary("vibejni");
        }
    }

    // ---- native declarations (implemented in vibe_jni.c) ----------------
    public static native String version();

    /** Parse VIBE text. Returns an opaque handle (a VibeValue* as a long),
     *  or throws VibeException on parse error. */
    public static native long parse(String text) throws VibeException;

    public static native String getString(long handle, String path);
    public static native long   getInt(long handle, String path);
    public static native double getFloat(long handle, String path);
    public static native boolean getBool(long handle, String path);
    public static native int    arraySize(long handle, String path);
    public static native String emit(long handle);
    public static native void   free(long handle);

    public static final class VibeException extends RuntimeException {
        public VibeException(String m) { super(m); }
    }

    // ---- self-test ------------------------------------------------------
    public static void main(String[] args) throws Exception {
        java.nio.file.Path sample = java.nio.file.Paths.get(
            System.getProperty("vibe.sample", "../../sample.vibe"));
        String text = new String(java.nio.file.Files.readAllBytes(sample),
                                 java.nio.charset.StandardCharsets.UTF_8);

        check(version().equals("1.1.0"), "version");

        long doc = parse(text);
        check(getString(doc, "name").equals("libvibe"), "name");
        check(getInt(doc, "answer") == 42, "answer");
        check(Math.abs(getFloat(doc, "pi") - 3.14159) < 1e-9, "pi");
        check(getBool(doc, "enabled"), "enabled");
        check(getString(doc, "server.host").equals("localhost"), "host");
        check(getInt(doc, "server.port") == 8080, "port");
        check(arraySize(doc, "ports") == 3, "ports");
        check(emit(doc).contains("libvibe"), "emit");

        boolean threw = false;
        try { parse("name {"); } catch (VibeException e) { threw = true; }
        check(threw, "expected VibeException on malformed input");

        free(doc);
        System.out.println("ALL OK (java native / JNI)");
    }

    private static void check(boolean cond, String what) {
        if (!cond) { System.err.println("FAIL: " + what); System.exit(1); }
    }
}
