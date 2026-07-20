;; libvibe — CHICKEN Scheme binding (compiled FFI via foreign-lambda).
;;
;;   csc -C -I/path -L "-L/path -lvibe" vibe.scm -o vibe_chicken
;;   LD_LIBRARY_PATH=/path ./vibe_chicken

(import (chicken foreign)
        (chicken process-context)
        (chicken string)
        (chicken io))

(foreign-declare "#include \"vibe.h\"")

(define vibe-version    (foreign-lambda c-string "vibe_version"))
(define vibe-parse      (foreign-lambda c-pointer "vibe_parse" c-string size_t c-pointer))
(define vibe-get-string (foreign-lambda c-string "vibe_get_string" c-pointer c-string))
(define vibe-get-int    (foreign-lambda integer64 "vibe_get_int" c-pointer c-string))
(define vibe-get-float  (foreign-lambda double "vibe_get_float" c-pointer c-string))
(define vibe-get-bool   (foreign-lambda bool "vibe_get_bool" c-pointer c-string))
(define vibe-get-array  (foreign-lambda c-pointer "vibe_get_array" c-pointer c-string))
(define vibe-array-size (foreign-lambda size_t "vibe_array_size" c-pointer))
(define vibe-emit       (foreign-lambda c-string "vibe_emit" c-pointer))
(define vibe-value-free (foreign-lambda void "vibe_value_free" c-pointer))

(define ok #t)
(define (check name got want)
  (unless (equal? got want) (set! ok #f))
  (print "  [" (if (equal? got want) "ok " "BAD") "] " name " = " got))

(define sample (or (get-environment-variable "VIBE_SAMPLE") "../sample.vibe"))
(define data (call-with-input-file sample (lambda (p) (read-string #f p))))
(define v (vibe-parse data (string-length data) #f))
(when (not v)
  (print "FAILED (chicken): parse error")
  (exit 1))

(check "version" (vibe-version) "1.1.0")
(check "name" (vibe-get-string v "name") "libvibe")
(check "answer" (vibe-get-int v "answer") 42)
(check "pi" (/ (round (* (vibe-get-float v "pi") 100000)) 100000) 3.14159)
(check "enabled" (vibe-get-bool v "enabled") #t)
(check "server.host" (vibe-get-string v "server.host") "localhost")
(check "server.port" (vibe-get-int v "server.port") 8080)
(define arr (vibe-get-array v "ports"))
(check "len(ports)" (if arr (vibe-array-size arr) 0) 3)

(define emitted (vibe-emit v))
(if (and emitted (substring-index "libvibe" emitted))
    (print "  [ok ] emit() round-trips")
    (begin (set! ok #f) (print "  [BAD] emit() did not round-trip")))

(define bad (vibe-parse "name {" 6 #f))
(if (not bad)
    (print "  [ok ] rejects malformed input")
    (begin (set! ok #f) (print "  [BAD] malformed input did not raise")))

(vibe-value-free v)
(print (if ok "ALL OK (chicken)" "FAILED (chicken)"))
(exit (if ok 0 1))
