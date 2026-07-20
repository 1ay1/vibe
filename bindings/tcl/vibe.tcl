# libvibe — Tcl binding via the cffi extension.
#
#   # cffi: https://cffi.magicsplat.com  (AUR: tcl-cffi, or build from source)
#   VIBE_LIB=/path/libvibe.so tclsh vibe.tcl

package require cffi

set libpath [expr {[info exists ::env(VIBE_LIB)] ? $::env(VIBE_LIB) : "../../libvibe.so"}]
cffi::Wrapper create vibe $libpath

vibe stdcall vibe_version {} string
vibe stdcall vibe_parse {data bytes len size_t err {pointer nullok}} pointer
vibe stdcall vibe_get_string {v pointer path string} string
vibe stdcall vibe_get_int {v pointer path string} long
vibe stdcall vibe_get_float {v pointer path string} double
vibe stdcall vibe_get_bool {v pointer path string} schar
vibe stdcall vibe_get_array {v pointer path string} pointer
vibe stdcall vibe_array_size {a pointer} size_t
vibe stdcall vibe_emit {v pointer} pointer
vibe stdcall vibe_free {p pointer} void
vibe stdcall vibe_value_free {v pointer} void

set sample [expr {[info exists ::env(VIBE_SAMPLE)] ? $::env(VIBE_SAMPLE) : "../sample.vibe"}]
set fh [open $sample rb]
set data [read $fh]
close $fh

set v [vibe_parse $data [string length $data] NULL]
if {[cffi::pointer isnull $v]} {
    puts "FAILED (tcl): parse error"
    exit 1
}

set ok 1
proc check {name got want} {
    global ok
    set pass [expr {$got eq $want}]
    if {!$pass} { set ok 0 }
    puts "  \[[expr {$pass ? {ok } : {BAD}}]\] $name = $got"
}

check "version" [vibe_version] "1.1.0"
check "name" [vibe_get_string $v "name"] "libvibe"
check "answer" [vibe_get_int $v "answer"] 42
check "pi" [format %.5f [vibe_get_float $v "pi"]] "3.14159"
check "enabled" [expr {[vibe_get_bool $v "enabled"] != 0 ? 1 : 0}] 1
check "server.host" [vibe_get_string $v "server.host"] "localhost"
check "server.port" [vibe_get_int $v "server.port"] 8080
set arr [vibe_get_array $v "ports"]
check "len(ports)" [expr {[cffi::pointer isnull $arr] ? 0 : [vibe_array_size $arr]}] 3

set raw [vibe_emit $v]
set emitted [expr {[cffi::pointer isnull $raw] ? "" : [cffi::memory tostring! $raw]}]
if {[string first "libvibe" $emitted] >= 0} {
    puts "  \[ok \] emit() round-trips"
} else {
    set ok 0
    puts "  \[BAD\] emit() did not round-trip"
}
if {![cffi::pointer isnull $raw]} { vibe_free $raw }

set bad [vibe_parse "name {" 6 NULL]
if {[cffi::pointer isnull $bad]} {
    puts "  \[ok \] rejects malformed input"
} else {
    set ok 0
    puts "  \[BAD\] malformed input did not raise"
}

vibe_value_free $v
puts [expr {$ok ? "ALL OK (tcl)" : "FAILED (tcl)"}]
exit [expr {$ok ? 0 : 1}]
