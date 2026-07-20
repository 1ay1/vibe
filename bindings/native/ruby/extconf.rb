# Builds the NATIVE Ruby C-extension for libvibe.
#
# Self-contained: the library is compiled from the vendored single-header
# vendor/vibe.h (via vendor/vibe_impl.c), so `gem install` builds anywhere with
# no prebuilt libvibe.a.
#
#   ruby extconf.rb && make && ruby test.rb
require "mkmf"

here = __dir__

# Compile against the vendored header, and build the library implementation TU
# alongside the extension. VPATH lets make find vendor/vibe_impl.c.
$INCFLAGS << " -I#{here}/vendor"
$VPATH << "#{here}/vendor"
$srcs = %w[vibe.c vibe_impl.c]

create_makefile("vibe")
