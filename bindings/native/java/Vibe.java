// Vibe.java — VIBE as native Java syntax.
//
// Java has no user-defined literals, so its most native embedding is the TEXT
// BLOCK (real Java 15+ syntax) handed to a fluent, typed navigator:
//
//     var cfg = Vibe.of("""
//         name    my-service
//         port    8080
//         tls     true
//         origins [ https://a.example  https://b.example ]
//         db {
//             host  localhost
//             port  5432
//         }
//         """);
//
//     String name = cfg.get("name").str();       // "my-service"
//     int    port = cfg.get("port").asInt();     // 8080
//     String host = cfg.get("db").get("host").str();   // nested
//     for (Vibe.Node o : cfg.get("origins")) use(o.str());
//
// This is a TRUE native binding: the native methods below are implemented in
// vibe_jni.c, compiled to libvibejni.so, linking libvibe.a.
//
//   javac -h . Vibe.java && gcc ... -> libvibejni.so
//   java -Djava.library.path=. Vibe
import java.util.Iterator;

public final class Vibe {
    static {
        String lib = System.getProperty("vibe.jni.path");
        if (lib != null) {
            System.load(lib);
        } else {
            System.loadLibrary("vibejni");
        }
    }

    // VibeType discriminants (mirror the C enum order in vibe.h).
    static final int T_NULL = 0, T_INTEGER = 1, T_FLOAT = 2, T_BOOLEAN = 3,
                     T_STRING = 4, T_ARRAY = 5, T_OBJECT = 6;

    // ---- native declarations (implemented in vibe_jni.c) ----------------
    public static native String version();

    /** Parse VIBE text. Returns an opaque handle (a VibeValue* as a long),
     *  or throws VibeException on parse error. */
    static native long parse(String text) throws VibeException;

    static native String getString(long handle, String path);
    static native long   getInt(long handle, String path);
    static native double getFloat(long handle, String path);
    static native boolean getBool(long handle, String path);
    static native int    arraySize(long handle, String path);
    static native String emit(long handle);
    static native void   free(long handle);

    // handle-based navigation
    static native long    child(long handle, String path);
    static native long    arrayElement(long handle, int index);
    static native int     nodeSize(long handle);
    static native int     typeOf(long handle);
    static native String  nodeString(long handle);
    static native long    nodeInt(long handle);
    static native double  nodeFloat(long handle);
    static native boolean nodeBool(long handle);

    public static final class VibeException extends RuntimeException {
        public VibeException(String m) { super(m); }
    }

    // ---- the native-syntax entry point ----------------------------------

    /** Parse a VIBE text block into a navigable document. The returned
     *  {@link Document} owns the native tree; close() (or try-with-resources)
     *  frees it. */
    public static Document of(String text) {
        return new Document(parse(text));
    }

    /** A whole parsed document: a Node that owns the native tree. */
    public static final class Document extends Node implements AutoCloseable {
        private boolean closed = false;
        Document(long root) { super(root); }
        @Override public void close() {
            if (!closed && handle != 0) { free(handle); closed = true; }
        }
    }

    /** A read-only view of a node. Absent nodes have handle 0 and read back as
     *  fallbacks — navigation never throws for a missing key. */
    public static class Node implements Iterable<Node> {
        final long handle;
        Node(long h) { this.handle = h; }

        public boolean exists()  { return handle != 0; }
        public int type()        { return typeOf(handle); }
        public boolean isNull()   { return handle == 0 || type() == T_NULL; }
        public boolean isInt()    { return type() == T_INTEGER; }
        public boolean isFloat()  { return type() == T_FLOAT; }
        public boolean isBool()   { return type() == T_BOOLEAN; }
        public boolean isString() { return type() == T_STRING; }
        public boolean isArray()  { return type() == T_ARRAY; }
        public boolean isObject() { return type() == T_OBJECT; }

        /** Key or dotted-path lookup: get("a").get("b") or get("a.b"). */
        public Node get(String path) { return new Node(handle == 0 ? 0 : child(handle, path)); }

        /** Array index. */
        public Node get(int index) { return new Node(handle == 0 ? 0 : arrayElement(handle, index)); }

        /** Array length (0 for non-arrays). */
        public int size() { return handle == 0 ? 0 : nodeSize(handle); }

        // Typed readers with fallbacks.
        public String str()               { return str(null); }
        public String str(String fb)       { return isString() ? nodeString(handle) : fb; }
        public long asInt()                { return asInt(0); }
        public long asInt(long fb)          { return isInt() ? nodeInt(handle) : fb; }
        public double asFloat()            { return asFloat(0.0); }
        public double asFloat(double fb)    {
            if (isFloat())   return nodeFloat(handle);
            if (isInt())     return (double) nodeInt(handle);
            return fb;
        }
        public boolean asBool()            { return asBool(false); }
        public boolean asBool(boolean fb)   { return isBool() ? nodeBool(handle) : fb; }

        @Override public Iterator<Node> iterator() {
            return new Iterator<>() {
                int i = 0, n = size();
                @Override public boolean hasNext() { return i < n; }
                @Override public Node next() { return get(i++); }
            };
        }
    }

    // ---- self-test ------------------------------------------------------
    public static void main(String[] args) throws Exception {
        check(version().equals("1.2.0"), "version");

        // Backward-compatible low-level path API still works.
        java.nio.file.Path sample = java.nio.file.Paths.get(
            System.getProperty("vibe.sample", "../../sample.vibe"));
        String text = new String(java.nio.file.Files.readAllBytes(sample),
                                 java.nio.charset.StandardCharsets.UTF_8);
        long doc = parse(text);
        check(getString(doc, "name").equals("libvibe"), "name");
        check(getInt(doc, "answer") == 42, "answer");
        check(arraySize(doc, "ports") == 3, "ports");
        check(emit(doc).contains("libvibe"), "emit");
        free(doc);

        // VIBE as native Java syntax: a text block + fluent navigator.
        try (Document cfg = Vibe.of("""
                name    my-service
                port    8080
                tls     true
                ratio   0.75
                origins [ https://a.example  https://b.example ]
                db {
                    host  localhost
                    port  5432
                }
                """)) {

            check(cfg.get("name").str().equals("my-service"), "block name");
            check(cfg.get("port").asInt() == 8080, "block port");
            check(cfg.get("tls").asBool(), "block tls");
            check(cfg.get("ratio").asFloat() == 0.75, "block ratio");
            check(cfg.get("db").get("host").str().equals("localhost"), "nested host");
            check(cfg.get("db.port").asInt() == 5432, "dotted path");

            Node origins = cfg.get("origins");
            check(origins.isArray(), "origins array");
            check(origins.size() == 2, "origins size");
            check(origins.get(0).str().equals("https://a.example"), "origins[0]");
            int seen = 0;
            for (Node o : origins) { check(o.str().startsWith("https://"), "iter"); seen++; }
            check(seen == 2, "iter count");

            // Missing keys are absent, not exceptions.
            check(!cfg.get("nope").exists(), "missing absent");
            check(cfg.get("nope").asInt(-1) == -1, "missing fallback");
        }

        boolean threw = false;
        try { Vibe.of("name {"); } catch (VibeException e) { threw = true; }
        check(threw, "expected VibeException on malformed input");

        System.out.println("ALL OK (java native / JNI, Vibe.of text block)");
    }

    private static void check(boolean cond, String what) {
        if (!cond) { System.err.println("FAIL: " + what); System.exit(1); }
    }
}
