# VIBE Examples

This directory contains example VIBE configuration files and usage examples.

## Valid Configuration Files

### simple.vibe
Basic VIBE file demonstrating core features:
- Simple key-value pairs
- Nested objects
- Arrays of scalar values

### config.vibe
Complex real-world configuration showing:
- Deep nesting
- Multiple data types
- Arrays and objects
- Comments
- Path-style values

### web_server.vibe
Web server configuration example with:
- Virtual hosts
- SSL configuration
- Logging settings
- Rate limiting

### database.vibe
Database configuration showing:
- Connection pooling
- Replica sets
- Migration settings

## Invalid Configuration Files (For Testing)

These files demonstrate what **NOT** to do in VIBE. They will fail to parse with helpful error messages.

### invalid_array_with_objects.vibe
❌ **Will fail**: Demonstrates why objects cannot be placed inside arrays
- Shows the common mistake of trying to create arrays of objects
- Includes the correct alternative using named objects
- Error: `"Objects cannot be placed inside arrays. Use named objects instead."`

### invalid_nested_arrays.vibe
❌ **Will fail**: Demonstrates why arrays cannot be nested
- Shows attempted multi-dimensional arrays
- Includes correct alternatives for different use cases
- Error: `"Arrays cannot be nested inside other arrays."`

**Why these restrictions?** See [The Stability Paradox](../docs/Stability_Paradox.md) for the design rationale. Arrays of objects create unstable configurations through index-based references, ambiguous merging, and lack of inherent identity. VIBE forces you to use named objects for stability.

## C Examples

### example.c
Complete working example showing how to:
- Parse VIBE files
- Access values using dot notation
- Handle errors
- Print configuration structure

## Running Examples

### Valid examples (will succeed):
```bash
cd ..
make
./vibe_example examples/simple.vibe
./vibe_example examples/config.vibe
./vibe_example examples/web_server.vibe
./vibe_example examples/database.vibe
```

### Invalid examples (will fail with error messages):
```bash
./vibe_example examples/invalid_array_with_objects.vibe
# Error: Objects cannot be placed inside arrays. Use named objects instead.

./vibe_example examples/invalid_nested_arrays.vibe
# Error: Arrays cannot be nested inside other arrays.
```

## Creating Your Own

1. Create a `.vibe` file with your configuration
2. Use the parser to load it
3. Access values with `vibe_get_*()` functions

See `example.c` for a complete reference implementation.
