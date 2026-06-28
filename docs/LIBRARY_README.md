# Library

This folder contains common utility functions used across all layers of the Galatea-NIX kernel.

## Structure

- **math.c / math.h** - Mathematical utility functions
  - `min_u64()` / `max_u64()` - Min/max for unsigned 64-bit integers
  - `min_int()` / `max_int()` - Min/max for signed integers

- **string.c / string.h** - String utility functions
  - `atoi_64()` - Convert string to 64-bit integer (supports negative numbers)
  - `str_to_hex()` - Convert hex string to integer
  - `strcmp_ret()` - Compare two strings for equality
  - `a2d()` - Convert ASCII character to digit (inline)
  - `is_empty()` - Check if string is empty (inline)
  - `is_hex()` - Check if string has "0x" prefix (inline)

## Usage

Include the appropriate header in your source files:

```c
#include "math.h"      // For mathematical functions
#include "string.h"    // For string functions
```

The library is automatically included in the build via the Makefile's `-Ilibrary` flag.
