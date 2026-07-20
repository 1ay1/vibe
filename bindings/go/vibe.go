// libvibe — Go binding via cgo. Links the shared library built by `make`.
//
//   go run vibe.go          (from bindings/go/, with libvibe.so in repo root)
//
// The cgo preamble pulls in the real vibe.h, so Go sees the exact C ABI.

package main

/*
#cgo CFLAGS: -I${SRCDIR}/../..
#cgo LDFLAGS: -L${SRCDIR}/../.. -lvibe -Wl,-rpath,${SRCDIR}/../..
#include <stdlib.h>
#include "vibe.h"
*/
import "C"

import (
	"fmt"
	"math"
	"os"
	"strings"
	"unsafe"
)

type Doc struct{ ptr *C.VibeValue }

func parse(data []byte) (*Doc, error) {
	var err C.VibeError
	cdata := C.CBytes(data)
	defer C.free(cdata)
	ptr := C.vibe_parse((*C.char)(cdata), C.size_t(len(data)), &err)
	if ptr == nil {
		code := C.GoString(C.vibe_error_code_string(err.code))
		return nil, fmt.Errorf("%s at %d:%d", code, int(err.line), int(err.column))
	}
	return &Doc{ptr}, nil
}

func (d *Doc) getString(path string) string {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))
	r := C.vibe_get_string(d.ptr, cp)
	if r == nil {
		return ""
	}
	return C.GoString(r)
}

func (d *Doc) getInt(path string) int64 {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))
	return int64(C.vibe_get_int(d.ptr, cp))
}

func (d *Doc) getFloat(path string) float64 {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))
	return float64(C.vibe_get_float(d.ptr, cp))
}

func (d *Doc) getBool(path string) bool {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))
	return bool(C.vibe_get_bool(d.ptr, cp))
}

func (d *Doc) arraySize(path string) int {
	cp := C.CString(path)
	defer C.free(unsafe.Pointer(cp))
	a := C.vibe_get_array(d.ptr, cp)
	if a == nil {
		return 0
	}
	return int(C.vibe_array_size(a))
}

func (d *Doc) emit() string {
	raw := C.vibe_emit(d.ptr)
	if raw == nil {
		return ""
	}
	defer C.vibe_free(unsafe.Pointer(raw))
	return C.GoString(raw)
}

func (d *Doc) free() { C.vibe_value_free(d.ptr) }

func version() string { return C.GoString(C.vibe_version()) }

func main() {
	sample := os.Getenv("VIBE_SAMPLE")
	if sample == "" {
		sample = "../sample.vibe"
	}
	data, e := os.ReadFile(sample)
	if e != nil {
		panic(e)
	}
	doc, e := parse(data)
	if e != nil {
		panic(e)
	}
	defer doc.free()

	ok := true
	check := func(name string, got, want interface{}) {
		if got != want {
			ok = false
		}
		flag := "ok "
		if got != want {
			flag = "BAD"
		}
		fmt.Printf("  [%s] %s = %v\n", flag, name, got)
	}
	check("version", version(), "1.1.0")
	check("name", doc.getString("name"), "libvibe")
	check("answer", doc.getInt("answer"), int64(42))
	check("pi", math.Round(doc.getFloat("pi")*100000)/100000, 3.14159)
	check("enabled", doc.getBool("enabled"), true)
	check("server.host", doc.getString("server.host"), "localhost")
	check("server.port", doc.getInt("server.port"), int64(8080))
	check("len(ports)", doc.arraySize("ports"), 3)
	if strings.Contains(doc.emit(), "libvibe") {
		fmt.Println("  [ok ] emit() round-trips")
	} else {
		ok = false
		fmt.Println("  [BAD] emit() did not round-trip")
	}
	if _, e := parse([]byte("name {")); e != nil {
		fmt.Println("  [ok ] rejects malformed input")
	} else {
		ok = false
		fmt.Println("  [BAD] malformed input did not raise")
	}
	if ok {
		fmt.Println("ALL OK (go)")
	} else {
		fmt.Println("FAILED (go)")
		os.Exit(1)
	}
}

