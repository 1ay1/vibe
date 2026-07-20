# The VIBE Manifesto

*Configuration is the most-edited, least-respected file in your repository.
This is a manifesto for taking it seriously.*

---

## 1. Configuration is where software meets human judgment

Your code is reviewed, tested, typed, and versioned. Your configuration is
pasted from a Stack Overflow answer at 4 p.m. on a Friday.

Yet configuration is where the two most dangerous things in software live: the
values that differ between *works on my machine* and *production*, and the
switches that a tired human flips under pressure during an incident. A port. A
feature flag. A replica's hostname. The blast radius of a one-character config
edit is frequently larger than that of a thousand-line code change — because the
config edit ships instantly, unreviewed, and with no type checker standing
between the keystroke and the outage.

We decided this was unacceptable. VIBE is what a configuration format looks like
when you treat that Friday-afternoon edit as the most important line of code in
the building.

## 2. The problem was never syntax. It was *ambiguity.*

Every mainstream config format is technically fine and practically a minefield,
and always for the same reason: **the same bytes can mean two different things.**

- YAML reads `country: no` and gives you the boolean `false`. Norway is not a
  boolean. This bug has a name because it happens constantly.
- YAML reads `version: 2.0` as a float and drops your trailing zero; `2.1.0`
  stays a string. Your version numbers now have two types.
- JSON makes you count commas and forbids the comment that would explain *why*
  the value is `8080`.
- TOML has four ways to write a table and five to write a date.
- Every one of them lets you write `[ {…}, {…} ]` — an anonymous list of
  records — and then addresses those records by a position that any tidy-minded
  teammate can reorder, silently rebinding every reference in your fleet.

None of these are *syntax* problems. They are *meaning* problems. The format
looked at your text and **guessed**, and one day it guessed wrong at 3 a.m.

VIBE's founding refusal: **a format must never guess.** A token means exactly one
thing. `no` is the string `"no"`. `007` is the integer `7`. `10:30` is the string
`"10:30"`, not 630 seconds. If you want a boolean, you type `true`. The
[Norway problem](https://hitchdev.com/strictyaml/why/implicit-typing-removed/)
and its entire family are not bugs we patched — they are *impossible to express.*

## 3. The First Law: if it's worth structuring, it's worth naming

Here is the single sentence the whole language turns on:

> **An array must not contain an object or another array.**

Arrays hold scalars — numbers, strings, booleans. A bag of values. The instant
you want *structure*, you must give it a *name*.

This feels like a restriction until you understand what it abolishes. Consider
the pattern every other format invites:

```yaml
replicas:
  - host: db-east.internal    # I am replicas[0]
  - host: db-west.internal    # I am replicas[1]
```

These records have no identity. They are known only by their *position*. A script
says `replicas[1]` and means "the west one" — until someone alphabetizes the
list, and now `replicas[1]` is the east one, and the drain script drains the
wrong database, and nobody typed a hostname wrong. (This is a true story; it has
[its own essay](Stability_Paradox.md).)

VIBE makes you write what you actually mean:

```vibe
replicas {
  east { host db-east.internal }
  west { host db-west.internal }
}
```

Now `replicas.west` means the west one *forever*, regardless of order. Reordering
is a no-op. Diffs are minimal. Merges are defined. Identity is stable because
identity is **named**, not positional. The First Law doesn't take a feature away;
it takes a class of 3 a.m. pages away.

## 4. A format is defined by what it refuses

Feature lists are how formats rot. Every added feature is a new way for two
readers — human or parser — to disagree. So VIBE is defined by a list of things
it will **never** have, and each refusal buys back a specific guarantee:

| VIBE refuses | So that you get |
|--------------|-----------------|
| Objects inside arrays | Stable, named identity — no positional rebinding |
| Implicit type coercion | One token, one meaning — no Norway problem |
| Anchors, references, aliases | A value's meaning is readable *in place* — no spooky action |
| Variable substitution, includes | The file you read is the file that runs |
| Conditionals, templates, loops | Config is data, not a program you have to *run* to understand |
| Significant whitespace | You can never break a file by re-indenting it |
| Multiple ways to write one thing | One canonical form — sane diffs, merges, and tooling |

Refusal is a feature. Every "no" above is a bug that cannot happen to you.

## 5. Determinism is a promise, not an aspiration

Most specs describe a *happy path* and wave at the edges. VIBE makes four
promises and then **mechanically enforces every one of them:**

1. **One Parse.** The same bytes produce the same value tree in every conforming
   parser, forever. There are no ambiguous documents — not "few," *none*. Show us
   two conforming parsers that disagree on any input and that is a spec bug we
   will fix.
2. **One Canonical Form.** Every conforming emitter, given equal data, produces
   *byte-identical* output. `fmt` is deterministic; `parse(emit(x)) == x` and
   `emit(emit(x)) == emit(x)` are guarantees, checked in the test suite. Your
   diffs stay minimal because there is exactly one way to write a value.
3. **Fail Closed.** Malformed, oversized, or ill-formed-UTF-8 input is *rejected*
   with a stable error code — never truncated, never coerced, never guessed.
   Every parser enforces the same resource limits so untrusted input can't
   exhaust memory or the stack.
4. **Frozen Grammar.** The syntax is locked for all of VIBE 1.x. A document you
   write today parses identically under every future 1.x parser. No format
   should ever silently reinterpret a file you already shipped.

None of this is an honor system. A single, language-neutral
[conformance suite](../tests/conformance) — `valid/`, `invalid/`, and
`canonical/` — decides what "conforming" means, byte for byte, in any language.
If a parser can't pass it, it isn't VIBE.

## 6. Boring on purpose

VIBE is not clever. That is the highest compliment we can pay a config format.

The parser is a single translation unit with an explicit-stack loop — deeply
nested input can't overflow the C call stack. The library is one header you can
drop into any project. There is no runtime, no plugin system, no expression
evaluator, no Turing tarpit hiding in your settings file. When you read a VIBE
document, the structure you see *is* the structure the program gets. There is
nothing behind the curtain because there is no curtain.

Clever formats fail in clever ways at the worst possible time. VIBE fails
immediately, loudly, and at parse time — which is to say, it fails while you're
still looking at it, not while your users are.

## 7. Know when *not* to use it

A manifesto that claims universal applicability is marketing, not engineering.
VIBE is for **configuration that humans write and read by hand.** That is the
whole job, and it intends to be the best possible tool for exactly that job.

It is *not* a wire format. For machine-to-machine data interchange — APIs,
message payloads, anything where a program is the only reader — use JSON. It's
ubiquitous, it's fine, we mean it.

And if you've reached genuine, irreducible complexity — you truly need
computation to *generate* your configuration — then you have outgrown a config
*format* and want a config *language* like CUE or Jsonnet, or just plain code.
That's not a failure of VIBE; it's VIBE telling you the truth about your problem
instead of hiding the complexity inside a `.yaml` file where it will ambush the
next person.

## The bet, in one sentence

**Configuration should be the safest file in your repository, not the scariest —
and the way you get there is by refusing every feature that lets the format guess
what you meant.**

Everything a value looks like, it is. Everything structured has a name. The same
bytes always mean the same thing. That's the whole vibe.

*Pass the vibe check.* 🌊

---

*Ready? → [Get started](https://github.com/1ay1/vibe#-quick-start) · [Specification](../SPECIFICATION.md)
· [The Stability Paradox](Stability_Paradox.md) · [Language bindings](https://github.com/1ay1/vibe/tree/main/bindings)*
