(* libvibe — OCaml binding using ctypes + ctypes-foreign.
 *
 *   ocamlfind ocamlopt -package ctypes,ctypes.foreign -linkpkg vibe.ml -o vibe_ml
 *   VIBE_LIB=/path/libvibe.so ./vibe_ml
 *)

open Ctypes
open Foreign

let lib =
  let path = try Sys.getenv "VIBE_LIB" with Not_found -> "../../libvibe.so" in
  Dl.dlopen ~filename:path ~flags:[ Dl.RTLD_NOW; Dl.RTLD_GLOBAL ]

let vibe_version = foreign ~from:lib "vibe_version" (void @-> returning string)
let vibe_parse =
  foreign ~from:lib "vibe_parse"
    (string @-> size_t @-> ptr void @-> returning (ptr void))
let vibe_get_string =
  foreign ~from:lib "vibe_get_string" (ptr void @-> string @-> returning string_opt)
let vibe_get_int =
  foreign ~from:lib "vibe_get_int" (ptr void @-> string @-> returning int64_t)
let vibe_get_float =
  foreign ~from:lib "vibe_get_float" (ptr void @-> string @-> returning double)
let vibe_get_bool =
  foreign ~from:lib "vibe_get_bool" (ptr void @-> string @-> returning bool)
let vibe_get_array =
  foreign ~from:lib "vibe_get_array" (ptr void @-> string @-> returning (ptr void))
let vibe_array_size =
  foreign ~from:lib "vibe_array_size" (ptr void @-> returning size_t)
let vibe_emit = foreign ~from:lib "vibe_emit" (ptr void @-> returning (ptr char))
let vibe_free = foreign ~from:lib "vibe_free" (ptr void @-> returning void)
let vibe_value_free =
  foreign ~from:lib "vibe_value_free" (ptr void @-> returning void)

let ok = ref true

let contains_sub s sub =
  let n = String.length s and m = String.length sub in
  let rec go i = i + m <= n && (String.sub s i m = sub || go (i + 1)) in
  m = 0 || go 0

let check name got want =
  if got <> want then ok := false;
  Printf.printf "  [%s] %s = %s\n" (if got = want then "ok " else "BAD") name got

let () =
  let sample = try Sys.getenv "VIBE_SAMPLE" with Not_found -> "../sample.vibe" in
  let data =
    let ic = open_in_bin sample in
    let n = in_channel_length ic in
    let s = really_input_string ic n in
    close_in ic; s
  in
  let v = vibe_parse data (Unsigned.Size_t.of_int (String.length data)) null in
  if is_null v then (print_endline "FAILED (ocaml): parse error"; exit 1);

  let gets path = match vibe_get_string v path with Some s -> s | None -> "" in
  check "version" (vibe_version ()) "1.2.0";
  check "name" (gets "name") "libvibe";
  check "answer" (Int64.to_string (vibe_get_int v "answer")) "42";
  let pi = Float.round (vibe_get_float v "pi" *. 100000.) /. 100000. in
  check "pi" (Printf.sprintf "%.5f" pi) "3.14159";
  check "enabled" (string_of_bool (vibe_get_bool v "enabled")) "true";
  check "server.host" (gets "server.host") "localhost";
  check "server.port" (Int64.to_string (vibe_get_int v "server.port")) "8080";
  let arr = vibe_get_array v "ports" in
  let n = if is_null arr then 0 else Unsigned.Size_t.to_int (vibe_array_size arr) in
  check "len(ports)" (string_of_int n) "3";

  let raw = vibe_emit v in
  let emitted = if is_null raw then "" else coerce (ptr char) string raw in
  if contains_sub emitted "libvibe" then
    print_endline "  [ok ] emit() round-trips"
  else (ok := false; print_endline "  [BAD] emit() did not round-trip");
  if not (is_null raw) then vibe_free (coerce (ptr char) (ptr void) raw);

  let bad = vibe_parse "name {" (Unsigned.Size_t.of_int 6) null in
  if is_null bad then print_endline "  [ok ] rejects malformed input"
  else (ok := false; print_endline "  [BAD] malformed input did not raise");

  vibe_value_free v;
  print_endline (if !ok then "ALL OK (ocaml)" else "FAILED (ocaml)");
  exit (if !ok then 0 else 1)
