#lang racket/base
;; libvibe — Racket binding via ffi/unsafe (Racket standard library).
;;
;;   VIBE_LIB=/path/libvibe.so racket vibe.rkt

(require ffi/unsafe
         racket/file)

(define lib (ffi-lib (or (getenv "VIBE_LIB") "../../libvibe.so")))
(define (fn name type) (get-ffi-obj name lib type))

(define vibe-version    (fn "vibe_version"          (_fun -> _string)))
(define vibe-parse      (fn "vibe_parse"            (_fun _bytes _size _pointer -> _pointer)))
(define vibe-get-string (fn "vibe_get_string"       (_fun _pointer _string -> _string)))
(define vibe-get-int    (fn "vibe_get_int"          (_fun _pointer _string -> _int64)))
(define vibe-get-float  (fn "vibe_get_float"        (_fun _pointer _string -> _double)))
(define vibe-get-bool   (fn "vibe_get_bool"         (_fun _pointer _string -> _stdbool)))
(define vibe-get-array  (fn "vibe_get_array"        (_fun _pointer _string -> _pointer)))
(define vibe-array-size (fn "vibe_array_size"       (_fun _pointer -> _size)))
(define vibe-emit       (fn "vibe_emit"             (_fun _pointer -> _pointer)))
(define vibe-free       (fn "vibe_free"             (_fun _pointer -> _void)))
(define vibe-value-free (fn "vibe_value_free"       (_fun _pointer -> _void)))

(define ok #t)
(define (check name got want)
  (unless (equal? got want) (set! ok #f))
  (printf "  [~a] ~a = ~s\n" (if (equal? got want) "ok " "BAD") name got))

(define sample (or (getenv "VIBE_SAMPLE") "../sample.vibe"))
(define data (file->bytes sample))
(define v (vibe-parse data (bytes-length data) #f))
(when (eq? v #f)
  (printf "FAILED (racket): parse error\n")
  (exit 1))

(check "version" (vibe-version) "1.2.0")
(check "name" (vibe-get-string v "name") "libvibe")
(check "answer" (vibe-get-int v "answer") 42)
(check "pi" (/ (round (* (vibe-get-float v "pi") 100000)) 100000) 3.14159)
(check "enabled" (vibe-get-bool v "enabled") #t)
(check "server.host" (vibe-get-string v "server.host") "localhost")
(check "server.port" (vibe-get-int v "server.port") 8080)
(define arr (vibe-get-array v "ports"))
(check "len(ports)" (if arr (vibe-array-size arr) 0) 3)

(define raw (vibe-emit v))
(define emitted (if raw (cast raw _pointer _string) ""))
(cond
  [(and emitted (regexp-match? #rx"libvibe" emitted))
   (printf "  [ok ] emit() round-trips\n")]
  [else (set! ok #f) (printf "  [BAD] emit() did not round-trip\n")])
(when raw (vibe-free raw))

(define bad (vibe-parse #"name {" 6 #f))
(if (eq? bad #f)
    (printf "  [ok ] rejects malformed input\n")
    (begin (set! ok #f) (printf "  [BAD] malformed input did not raise\n")))

(vibe-value-free v)
(printf "~a\n" (if ok "ALL OK (racket)" "FAILED (racket)"))
(exit (if ok 0 1))
