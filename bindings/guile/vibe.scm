;; libvibe — GNU Guile binding via (system foreign) — Guile standard library.
;;
;;   VIBE_LIB=/path/libvibe.so guile vibe.scm

(use-modules (system foreign)
             (rnrs bytevectors)
             (ice-9 binary-ports)
             (ice-9 format)
             (srfi srfi-13))

(define lib (dynamic-link (or (getenv "VIBE_LIB") "../../libvibe.so")))
(define (fn ret name args) (pointer->procedure ret (dynamic-func name lib) args))

(define vibe-version    (fn '* "vibe_version" '()))
(define vibe-parse      (fn '* "vibe_parse" (list '* size_t '*)))
(define vibe-get-string (fn '* "vibe_get_string" (list '* '*)))
(define vibe-get-int    (fn int64 "vibe_get_int" (list '* '*)))
(define vibe-get-float  (fn double "vibe_get_float" (list '* '*)))
(define vibe-get-bool   (fn int8 "vibe_get_bool" (list '* '*)))
(define vibe-get-array  (fn '* "vibe_get_array" (list '* '*)))
(define vibe-array-size (fn size_t "vibe_array_size" (list '*)))
(define vibe-emit       (fn '* "vibe_emit" (list '*)))
(define vibe-free       (fn void "vibe_free" (list '*)))
(define vibe-value-free (fn void "vibe_value_free" (list '*)))

(define (cstr s) (string->pointer s))
(define (get-str v path)
  (let ((p (vibe-get-string v (cstr path))))
    (if (null-pointer? p) "" (pointer->string p))))

(define ok #t)
(define (check name got want)
  (unless (equal? got want) (set! ok #f))
  (format #t "  [~a] ~a = ~s\n" (if (equal? got want) "ok " "BAD") name got))

(define sample (or (getenv "VIBE_SAMPLE") "../sample.vibe"))
(define data (call-with-input-file sample get-bytevector-all))
(define v (vibe-parse (bytevector->pointer data) (bytevector-length data) %null-pointer))
(when (null-pointer? v)
  (format #t "FAILED (guile): parse error\n")
  (exit 1))

(check "version" (pointer->string (vibe-version)) "1.2.0")
(check "name" (get-str v "name") "libvibe")
(check "answer" (vibe-get-int v (cstr "answer")) 42)
(check "pi" (/ (round (* (vibe-get-float v (cstr "pi")) 100000)) 100000) 3.14159)
(check "enabled" (not (= 0 (vibe-get-bool v (cstr "enabled")))) #t)
(check "server.host" (get-str v "server.host") "localhost")
(check "server.port" (vibe-get-int v (cstr "server.port")) 8080)
(define arr (vibe-get-array v (cstr "ports")))
(check "len(ports)" (if (null-pointer? arr) 0 (vibe-array-size arr)) 3)

(define raw (vibe-emit v))
(define emitted (if (null-pointer? raw) "" (pointer->string raw)))
(cond
  [(string-contains emitted "libvibe")
   (format #t "  [ok ] emit() round-trips\n")]
  [else (set! ok #f) (format #t "  [BAD] emit() did not round-trip\n")])
(unless (null-pointer? raw) (vibe-free raw))

(define bad (vibe-parse (string->pointer "name {") 6 %null-pointer))
(if (null-pointer? bad)
    (format #t "  [ok ] rejects malformed input\n")
    (begin (set! ok #f) (format #t "  [BAD] malformed input did not raise\n")))

(vibe-value-free v)
(format #t "~a\n" (if ok "ALL OK (guile)" "FAILED (guile)"))
(exit (if ok 0 1))
