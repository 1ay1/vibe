# vibe_dsl.rb — VIBE as native Ruby syntax.
#
# Ruby's most native embedding is the squiggly HEREDOC handed to a kernel-level
# VIBE() method, returning an object you navigate with plain method calls:
#
#     require "vibe"
#     require_relative "vibe_dsl"
#
#     cfg = VIBE(<<~VIBE)
#       name    my-service
#       port    8080
#       tls     true
#       origins [ https://a.example  https://b.example ]
#       db {
#         host  localhost
#         port  5432
#       }
#     VIBE
#
#     cfg.name        # => "my-service"   (method access via method_missing)
#     cfg.port        # => 8080
#     cfg.db.host     # => "localhost"    (nested)
#     cfg[:db][:port] # => 5432           (or symbol/string indexing)
#     cfg.origins[0]  # => "https://a.example"
#     cfg.origins.each { |o| use(o) }     # it's a real Array
#
# `<<~VIBE ... VIBE` is genuine Ruby heredoc syntax, so the whole document lives
# inline in your source and reads as native Ruby. Backed by the native C
# extension (require "vibe").
#
# SPDX-License-Identifier: MIT
require "vibe"

module Vibe
  # A read-only view over a materialized VIBE object. Method calls and []
  # both resolve keys; nested hashes are wrapped, arrays keep their elements
  # (hash elements wrapped) so navigation is uniform.
  class Node
    def initialize(hash)
      @h = hash
    end

    def self.wrap(v)
      case v
      when Hash  then Node.new(v)
      when Array then v.map { |e| wrap(e) }
      else v
      end
    end

    def [](key)
      Node.wrap(@h[key.to_s])
    end

    def key?(key)
      @h.key?(key.to_s)
    end

    def keys
      @h.keys
    end

    def to_h
      @h
    end

    def each(&blk)
      @h.each { |k, v| blk.call(k, Node.wrap(v)) }
    end

    def respond_to_missing?(name, _include_private = false)
      @h.key?(name.to_s) || super
    end

    def method_missing(name, *args)
      key = name.to_s
      if @h.key?(key)
        Node.wrap(@h[key])
      else
        super
      end
    end

    def inspect
      "#<Vibe::Node #{@h.inspect}>"
    end
  end

  # Parse VIBE text into a navigable Node (Hash root) or the raw value.
  def self.load(text)
    Node.wrap(Vibe.parse(text).to_native)
  end
end

# The native-syntax entry point: a global VIBE() so a heredoc reads inline.
module Kernel
  def VIBE(text)
    Vibe.load(text)
  end

  # Also allow "...".to_vibe on any String for a fluent alternative.
  private def _vibe_noop; end
end

class String
  # `<<~VIBE ... VIBE.to_vibe` reads as native too.
  def to_vibe
    Vibe.load(self)
  end
end
