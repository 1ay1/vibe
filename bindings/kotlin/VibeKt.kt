// libvibe — Kotlin binding. Reuses the JVM's Foreign Function & Memory API
// (java.lang.foreign) from Kotlin, using invokeWithArguments (MethodHandle's
// polymorphic invoke/invokeExact don't map cleanly to Kotlin).
//
//   kotlinc VibeKt.kt -include-runtime -d vibe.jar
//   java --enable-native-access=ALL-UNNAMED -jar vibe.jar

import java.lang.foreign.*
import java.lang.foreign.ValueLayout.*
import java.lang.invoke.MethodHandle
import java.nio.file.Files
import java.nio.file.Path

val LINKER: Linker = Linker.nativeLinker()
val ARENA: Arena = Arena.ofShared()
lateinit var LOOKUP: SymbolLookup

fun h(name: String, fd: FunctionDescriptor): MethodHandle =
    LINKER.downcallHandle(LOOKUP.find(name).orElseThrow(), fd)

fun cstr(s: String): MemorySegment = ARENA.allocateFrom(s)
fun str(p: MemorySegment): String? =
    if (p.address() == 0L) null else p.reinterpret(Long.MAX_VALUE).getString(0)

fun main() {
    val libPath = System.getenv("VIBE_LIB") ?: "../../libvibe.so"
    LOOKUP = SymbolLookup.libraryLookup(libPath, ARENA)

    val version = h("vibe_version", FunctionDescriptor.of(ADDRESS))
    val parse = h("vibe_parse", FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS))
    val getStr = h("vibe_get_string", FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS))
    val getInt = h("vibe_get_int", FunctionDescriptor.of(JAVA_LONG, ADDRESS, ADDRESS))
    val getFloat = h("vibe_get_float", FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, ADDRESS))
    val getBool = h("vibe_get_bool", FunctionDescriptor.of(JAVA_BOOLEAN, ADDRESS, ADDRESS))
    val getArr = h("vibe_get_array", FunctionDescriptor.of(ADDRESS, ADDRESS, ADDRESS))
    val arrSize = h("vibe_array_size", FunctionDescriptor.of(JAVA_LONG, ADDRESS))
    val emit = h("vibe_emit", FunctionDescriptor.of(ADDRESS, ADDRESS))
    val vfree = h("vibe_value_free", FunctionDescriptor.ofVoid(ADDRESS))

    val errLayout = MemoryLayout.structLayout(
        JAVA_BOOLEAN.withName("has_error"),
        MemoryLayout.paddingLayout(3),
        JAVA_INT.withName("code"),
        ADDRESS.withName("message"),
        JAVA_INT.withName("line"),
        JAVA_INT.withName("column")
    )

    val sample = System.getenv("VIBE_SAMPLE") ?: "../sample.vibe"
    val bytes = Files.readAllBytes(Path.of(sample))
    val data = ARENA.allocate((bytes.size + 1).toLong())
    MemorySegment.copy(bytes, 0, data, JAVA_BYTE, 0, bytes.size)

    val err = ARENA.allocate(errLayout)
    val v = parse.invokeWithArguments(data, bytes.size.toLong(), err) as MemorySegment
    if (v.address() == 0L) {
        println("FAILED (kotlin): parse error"); kotlin.system.exitProcess(1)
    }

    var ok = true
    fun check(name: String, got: Any?, want: Any?) {
        val pass = got == want
        if (!pass) ok = false
        println("  [${if (pass) "ok " else "BAD"}] $name = $got")
    }

    check("version", str(version.invokeWithArguments() as MemorySegment), "1.2.0")
    check("name", str(getStr.invokeWithArguments(v, cstr("name")) as MemorySegment), "libvibe")
    check("answer", getInt.invokeWithArguments(v, cstr("answer")) as Long, 42L)
    val pi = Math.round((getFloat.invokeWithArguments(v, cstr("pi")) as Double) * 100000.0) / 100000.0
    check("pi", pi, 3.14159)
    check("enabled", getBool.invokeWithArguments(v, cstr("enabled")) as Boolean, true)
    check("server.host", str(getStr.invokeWithArguments(v, cstr("server.host")) as MemorySegment), "localhost")
    check("server.port", getInt.invokeWithArguments(v, cstr("server.port")) as Long, 8080L)
    val arr = getArr.invokeWithArguments(v, cstr("ports")) as MemorySegment
    val n = if (arr.address() == 0L) 0L else arrSize.invokeWithArguments(arr) as Long
    check("len(ports)", n, 3L)

    val raw = emit.invokeWithArguments(v) as MemorySegment
    val emitted = str(raw)
    if (emitted != null && emitted.contains("libvibe")) println("  [ok ] emit() round-trips")
    else { ok = false; println("  [BAD] emit() did not round-trip") }

    val bad = ARENA.allocateFrom("name {")
    val err2 = ARENA.allocate(errLayout)
    val badv = parse.invokeWithArguments(bad, 6L, err2) as MemorySegment
    if (badv.address() == 0L) println("  [ok ] rejects malformed input")
    else { ok = false; println("  [BAD] malformed input did not raise") }

    vfree.invokeWithArguments(v)
    println(if (ok) "ALL OK (kotlin)" else "FAILED (kotlin)")
    kotlin.system.exitProcess(if (ok) 0 else 1)
}
