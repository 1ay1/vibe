# ============================================================================
# libvibe — build system
# ============================================================================
# Common targets:
#   make            build the libraries, the example, and the `vibe` CLI
#   make lib        build static + shared libraries only
#   make test       parse the example documents (smoke test)
#   make test-suite run the unit test suite
#   make conformance build + run the language conformance suite
#   make cli        build the `vibe` command-line tool
#   make install    install headers, libraries, CLI, and pkg-config file
#   make clean      remove build artifacts
#   make help       list every target
#
# Keep VERSION / SOVERSION in sync with vibe.h (VIBE_VERSION_*).
# ============================================================================

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -std=c11 -O2 -g
LDFLAGS  ?=
AR       ?= ar

VERSION   := 1.1.0
SOVERSION := 1

# ---- install layout (override on the command line, e.g. PREFIX=/usr) --------
PREFIX       ?= /usr/local
LIBDIR       ?= $(PREFIX)/lib
INCLUDEDIR   ?= $(PREFIX)/include
BINDIR       ?= $(PREFIX)/bin
PKGCONFIGDIR ?= $(LIBDIR)/pkgconfig
DESTDIR      ?=

# ---- artifacts --------------------------------------------------------------
LIB_OBJ    := vibe.o
PIC_OBJ    := vibe.lo
STATIC_LIB := libvibe.a

EXAMPLE_BIN := vibe_example
EXAMPLE_OBJ := examples/example.o
TEST_BIN    := vibe_test
TEST_OBJ    := tests/test.o
CLI_BIN     := vibe
CLI_OBJ     := tools/vibe.o
CONF_BIN    := tests/conformance/run
PARSER_TOOL_BIN := vibe_parser_tool
PARSER_TOOL_OBJ := vibe_parser_tool.o

HEADERS := vibe.h

# Flags that must survive a CFLAGS override on the command line.
PIC_FLAGS := -fPIC -fvisibility=hidden -DVIBE_BUILD_SHARED

# ---- platform-specific shared-library naming/linking ------------------------
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  SHLIB_EXT      := dylib
  SHARED_REAL    := libvibe.$(VERSION).$(SHLIB_EXT)
  SHARED_SONAME  := libvibe.$(SOVERSION).$(SHLIB_EXT)
  SHARED_LINK    := libvibe.$(SHLIB_EXT)
  SHARED_LDFLAGS := -dynamiclib -install_name $(LIBDIR)/$(SHARED_SONAME) \
                    -compatibility_version $(SOVERSION) -current_version $(VERSION)
else
  SHLIB_EXT      := so
  SHARED_REAL    := libvibe.so.$(VERSION)
  SHARED_SONAME  := libvibe.so.$(SOVERSION)
  SHARED_LINK    := libvibe.so
  SHARED_LDFLAGS := -shared -Wl,-soname,$(SHARED_SONAME)
endif

.PHONY: all lib cli conformance test test-suite test-all run demo \
        clean help install uninstall parser_tool docs

# Default: libraries + example + CLI (no platform surprises beyond the shared lib).
all: lib $(EXAMPLE_BIN) $(CLI_BIN)

lib: $(STATIC_LIB) $(SHARED_REAL)

cli: $(CLI_BIN)

# ---- object files -----------------------------------------------------------
$(LIB_OBJ): vibe.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

$(PIC_OBJ): vibe.c $(HEADERS)
	$(CC) $(CFLAGS) $(PIC_FLAGS) -c -o $@ $<

examples/example.o: examples/example.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

tests/test.o: tests/test.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

tools/vibe.o: tools/vibe.c $(HEADERS)
	$(CC) $(CFLAGS) -I. -c -o $@ $<

$(PARSER_TOOL_OBJ): vibe_parser_tool.c $(HEADERS)
	$(CC) $(CFLAGS) -c -o $@ $<

# ---- libraries --------------------------------------------------------------
$(STATIC_LIB): $(LIB_OBJ)
	$(AR) rcs $@ $^

$(SHARED_REAL): $(PIC_OBJ)
	$(CC) $(SHARED_LDFLAGS) -o $@ $^ $(LDFLAGS)
	ln -sf $(SHARED_REAL) $(SHARED_SONAME)
	ln -sf $(SHARED_SONAME) $(SHARED_LINK)

# ---- executables (link the static library) ----------------------------------
$(EXAMPLE_BIN): $(EXAMPLE_OBJ) $(STATIC_LIB)
	$(CC) $(EXAMPLE_OBJ) $(STATIC_LIB) -o $@ $(LDFLAGS)

$(TEST_BIN): $(TEST_OBJ) $(STATIC_LIB)
	$(CC) $(TEST_OBJ) $(STATIC_LIB) -o $@ $(LDFLAGS)

$(CLI_BIN): $(CLI_OBJ) $(STATIC_LIB)
	$(CC) $(CLI_OBJ) $(STATIC_LIB) -o $@ $(LDFLAGS)

$(CONF_BIN): tests/conformance/run.c $(STATIC_LIB) $(HEADERS)
	$(CC) $(CFLAGS) -I. -o $@ tests/conformance/run.c $(STATIC_LIB) $(LDFLAGS)

parser_tool: $(PARSER_TOOL_BIN)
$(PARSER_TOOL_BIN): $(PARSER_TOOL_OBJ) $(STATIC_LIB)
	$(CC) $(PARSER_TOOL_OBJ) $(STATIC_LIB) -o $@ $(LDFLAGS) -lncurses -lpanel
	@echo "Built $(PARSER_TOOL_BIN) (interactive TUI)."

# ---- pkg-config -------------------------------------------------------------
vibe.pc: vibe.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@LIBDIR@|$(LIBDIR)|g' \
	    -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|g' \
	    -e 's|@VERSION@|$(VERSION)|g' $< > $@

# ---- documentation (regenerate the website's Markdown-backed pages) ---------
# Requires python3 + the `markdown` package (pip install markdown).
docs:
	python3 tools/gen_docs.py SPECIFICATION.md docs/specification.html \
		"VIBE Specification" "The complete, normative specification for the VIBE configuration format."
	python3 tools/gen_docs.py docs/Stability_Paradox.md docs/Stability_Paradox.html \
		"The Stability Paradox" "Why VIBE refuses features on purpose."
	@echo "Regenerated docs/specification.html and docs/Stability_Paradox.html"

# ---- tests ------------------------------------------------------------------
test: $(EXAMPLE_BIN)
	@echo "=== Parsing example documents ==="
	@./$(EXAMPLE_BIN) examples/simple.vibe     >/dev/null && echo "  ok  simple.vibe"
	@./$(EXAMPLE_BIN) examples/config.vibe     >/dev/null && echo "  ok  config.vibe"
	@./$(EXAMPLE_BIN) examples/web_server.vibe >/dev/null && echo "  ok  web_server.vibe"
	@./$(EXAMPLE_BIN) examples/database.vibe   >/dev/null && echo "  ok  database.vibe"
	@echo "All example documents parsed."

test-suite: $(TEST_BIN)
	@echo "=== Unit test suite ==="
	@./$(TEST_BIN)

conformance: $(CONF_BIN)
	@./$(CONF_BIN) tests/conformance

test-all: test test-suite conformance
	@echo "All tests completed."

run: $(EXAMPLE_BIN)
	./$(EXAMPLE_BIN) examples/simple.vibe

demo: $(CLI_BIN)
	@echo "=== vibe fmt examples/simple.vibe ==="
	@./$(CLI_BIN) fmt examples/simple.vibe

# ---- install / uninstall ----------------------------------------------------
install: lib $(CLI_BIN) vibe.pc
	@echo "Installing libvibe $(VERSION) to $(DESTDIR)$(PREFIX)"
	install -d $(DESTDIR)$(INCLUDEDIR) $(DESTDIR)$(LIBDIR) \
	           $(DESTDIR)$(BINDIR) $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 vibe.h $(DESTDIR)$(INCLUDEDIR)/
	install -m 644 $(STATIC_LIB) $(DESTDIR)$(LIBDIR)/
	install -m 755 $(SHARED_REAL) $(DESTDIR)$(LIBDIR)/
	ln -sf $(SHARED_REAL) $(DESTDIR)$(LIBDIR)/$(SHARED_SONAME)
	ln -sf $(SHARED_SONAME) $(DESTDIR)$(LIBDIR)/$(SHARED_LINK)
	install -m 755 $(CLI_BIN) $(DESTDIR)$(BINDIR)/
	install -m 644 vibe.pc $(DESTDIR)$(PKGCONFIGDIR)/
	@echo "Done. You may need to run 'ldconfig'."

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/vibe.h
	rm -f $(DESTDIR)$(LIBDIR)/$(STATIC_LIB)
	rm -f $(DESTDIR)$(LIBDIR)/$(SHARED_REAL)
	rm -f $(DESTDIR)$(LIBDIR)/$(SHARED_SONAME)
	rm -f $(DESTDIR)$(LIBDIR)/$(SHARED_LINK)
	rm -f $(DESTDIR)$(BINDIR)/$(CLI_BIN)
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/vibe.pc
	@echo "Uninstalled libvibe."

# ---- housekeeping -----------------------------------------------------------
clean:
	rm -f $(LIB_OBJ) $(PIC_OBJ) $(EXAMPLE_OBJ) $(TEST_OBJ) $(CLI_OBJ) $(PARSER_TOOL_OBJ)
	rm -f $(STATIC_LIB) libvibe.so libvibe.so.* libvibe.*.dylib libvibe.dylib
	rm -f $(EXAMPLE_BIN) $(TEST_BIN) $(CLI_BIN) $(CONF_BIN) $(PARSER_TOOL_BIN)
	rm -f vibe.pc
	rm -f *.gcov *.gcda *.gcno
	@echo "Cleaned."

help:
	@echo "libvibe $(VERSION) — build system"
	@echo ""
	@echo "Targets:"
	@echo "  all          libraries + example + CLI (default)"
	@echo "  lib          static (libvibe.a) + shared ($(SHARED_REAL))"
	@echo "  cli          the 'vibe' command-line tool"
	@echo "  test         parse the example documents"
	@echo "  test-suite   run the unit test suite"
	@echo "  conformance  run the language conformance suite"
	@echo "  test-all     run every test"
	@echo "  docs         regenerate the Markdown-backed website pages"
	@echo "  parser_tool  interactive TUI (requires ncurses)"
	@echo "  install      install to \$$PREFIX (default /usr/local)"
	@echo "  uninstall    remove installed files"
	@echo "  clean        remove build artifacts"
	@echo ""
	@echo "Install example:  make && sudo make install PREFIX=/usr"
	@echo "Consume it:       cc app.c \$$(pkg-config --cflags --libs vibe)"
