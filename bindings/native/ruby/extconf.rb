# Builds the NATIVE Ruby C-extension for libvibe.
#
#   cd bindings/native/ruby
#   ruby extconf.rb && make
#   ruby test.rb
require "mkmf"

# Repo root holds vibe.h and libvibe.a (three levels up).
root = File.expand_path("../../..", __dir__)

$INCFLAGS << " -I#{root}"
# Link the static archive directly so there is no runtime .so dependency.
$LOCAL_LIBS << " #{root}/libvibe.a"

create_makefile("vibe")
