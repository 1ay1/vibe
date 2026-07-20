<?php
// libvibe — PHP binding via the FFI extension (PHP 7.4+).
//
//   php -d ffi.enable=1 -d extension=ffi vibe.php
//
// (In a long-running SAPI you would enable ffi in php.ini and preload the cdef.)

$libPath = getenv('VIBE_LIB') ?: '../../libvibe.so';

$ffi = FFI::cdef('
    const char* vibe_version(void);
    void* vibe_parse(const char*, size_t, void*);
    const char* vibe_get_string(void*, const char*);
    int64_t vibe_get_int(void*, const char*);
    double vibe_get_float(void*, const char*);
    bool vibe_get_bool(void*, const char*);
    void* vibe_get_array(void*, const char*);
    size_t vibe_array_size(void*);
    void* vibe_emit(void*);
    void vibe_free(void*);
    void vibe_value_free(void*);
    const char* vibe_error_code_string(int);
', $libPath);

$sample = getenv('VIBE_SAMPLE') ?: '../sample.vibe';
$data = file_get_contents($sample);

// A returned NULL pointer surfaces as PHP null (not a NULL CData), so guard both.
function isNullPtr($x) { return $x === null || FFI::isNull($x); }

$v = $ffi->vibe_parse($data, strlen($data), null);
if (isNullPtr($v)) {
    echo "FAILED (php): parse error\n";
    exit(1);
}

$ok = true;
function check(&$ok, $name, $got, $want) {
    $pass = $got === $want;
    if (!$pass) $ok = false;
    printf("  [%s] %s = %s\n", $pass ? "ok " : "BAD", $name, var_export($got, true));
}
function getStr($ffi, $v, $path) {
    // PHP-FFI auto-converts a `const char*` return into a PHP string (or null).
    $p = $ffi->vibe_get_string($v, $path);
    return $p ?? "";
}

check($ok, "version", $ffi->vibe_version(), "1.2.0");
check($ok, "name", getStr($ffi, $v, "name"), "libvibe");
check($ok, "answer", $ffi->vibe_get_int($v, "answer"), 42);
check($ok, "pi", round($ffi->vibe_get_float($v, "pi"), 5), 3.14159);
check($ok, "enabled", $ffi->vibe_get_bool($v, "enabled"), true);
check($ok, "server.host", getStr($ffi, $v, "server.host"), "localhost");
check($ok, "server.port", $ffi->vibe_get_int($v, "server.port"), 8080);
$arr = $ffi->vibe_get_array($v, "ports");
check($ok, "len(ports)", isNullPtr($arr) ? 0 : $ffi->vibe_array_size($arr), 3);

$raw = $ffi->vibe_emit($v);
$emitted = isNullPtr($raw) ? "" : FFI::string(FFI::cast('char*', $raw));
if (str_contains($emitted, "libvibe")) {
    echo "  [ok ] emit() round-trips\n";
} else {
    $ok = false;
    echo "  [BAD] emit() did not round-trip\n";
}
if (!isNullPtr($raw)) $ffi->vibe_free($raw);

$bad = $ffi->vibe_parse("name {", 6, null);
if (isNullPtr($bad)) {
    echo "  [ok ] rejects malformed input\n";
} else {
    $ok = false;
    echo "  [BAD] malformed input did not raise\n";
}

$ffi->vibe_value_free($v);
echo $ok ? "ALL OK (php)\n" : "FAILED (php)\n";
exit($ok ? 0 : 1);
