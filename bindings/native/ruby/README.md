# vibe-lang (native Ruby extension)

A native Ruby C-extension for [libvibe](https://github.com/1ay1/vibe) — the VIBE
config language. The single-header C library is compiled from source (vendored),
so there is no runtime `.so` dependency.

```ruby
require "vibe"
require_relative "vibe_dsl"

cfg = VIBE <<~VIBE
  port 8080
  name "web"
VIBE

cfg["port"]  # => 8080
cfg["name"]  # => "web"
```

## Install

```sh
gem install vibe-lang
```

## Build from a checkout

```sh
ruby extconf.rb && make
ruby test.rb
```

MIT © 1ay1 — https://github.com/1ay1/vibe
