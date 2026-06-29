# Context Switching Issue & Fix

## Problem Identified

The context switching was not resuming tasks after they yield. Tasks would run once, call `Yield()`, and then never resume their execution. The root cause was a critical assembly bug in the `Begin()` function.

## Root Cause: Undefined Behavior in Begin() Assembly

**Location**: [layer0-assembly/asm.S](layer0-assembly/asm.S), line ~52

**Original Code (UNSAFE)**:
```asm
ldr x30, [x30], 8
```

This instruction has **undefined behavior** according to ARM architecture specifications. You cannot:
- Load INTO a register using that same register as the address source
- Use post-index addressingwith the target register

The assembler even warns about this:
```
Warning: unpredictable transfer with writeback -- `ldr x30,[x30],8'
```

## Why This Breaks Context Switching

When `Begin()` attempts to restore a task's saved registers:

1. `Begin()` receives x0 = pointer to saved register buffer
2. `Begin()` uses x30 as a loop counter pointer via `mov x30, x0`
3. When loading registers in a loop: `ldr xN, [x30], 8`
4. **The final instruction `ldr x30, [x30], 8`** causes undefined behavior:
   - The CPU cannot reliably predict what happens
   - The loaded value might be corrupted or not loaded at all
   - This causes registers to have incorrect values
   - Tasks then crash or misbehave after resuming

## Solution

Replace the unsafe post-index loop with manual address incrementing. This avoids the problematic pattern entirely.

### Correct Implementation (from the test fix):

```asm
mov x30, x0        // x30 = pointer to register buffer

// Load all 31 registers with manual increment (no post-index on x30 itself)
ldr x0, [x30]
add x30, x30, #8
ldr x1, [x30]
add x30, x30, #8
// ... repeat for x2-x29 ...
ldr x29, [x30]
add x30, x30, #8

// Load x30 last (it's now safe since we use explicit increment)
ldr x30, [x30]

eret
```

**Key Differences:**
- Each load follows the pattern: `ldr xN, [x30]` (without post-index)
- Then explicitly: `add x30, x30, #8` (separate post-increment instruction)
- Final x30 load is safe because x30 contains the address, and the only side effect is loading INTO x30

## Impact on Context Switching

With this fix:
1. **Task context is correctly restored** - All registers (including x30) have their proper saved values
2. **Tasks can resume after Yield()** - The program counter and stack pointer are restored correctly
3. **Multiple context switches work** - Tasks can yield multiple times and resume properly
4. **Preempts are handled** - Exception handling in Begin() works as intended

## Testing Recommendations

1. **Run the context switch test** in [layer1-processes/processes.c]:
   - `context_test_task_A/B/C` each yield 3 times
   - Each should print "Before Yield" and "After Yield"
   - Without the fix: Only "Before Yield" prints
   - With the fix: Both "Before Yield" and "After Yield" print multiple times

2. **Expected output with fix**:
```
[A-0] Before Yield
(Master continues)
[A-0] After Yield
[A-1] Before Yield
(Master continues)
[A-1] After Yield
...
```

3. **Verify build without warnings**:
```bash
make PLATFORM=QEMU_VIRT 2>&1 | grep -i "unpredictable\|warning.*x30"
# Should have NO output (no warnings about x30)
```

## Files Modified

- [layer0-assembly/asm.S](layer0-assembly/asm.S) - Begin() function
- [layer1-processes/processes.c](layer1-processes/processes.c) - Added context switching test tasks
- [layer1-processes/syscall.c](layer1-processes/syscall.c) - Scheduler infrastructure (already working)

## Assembly Pattern Explanation

The corrected pattern avoids undefined behavior by:
1. **Separation of concerns**: Address increment is a separate instruction
2. **No self-referential operations**: Never modify a register while using it as a source
3. **Clear intent**: Manual increment makes it obvious what's happening
4. **Predictable behavior**: Pure register increments are always safe

This is a common pattern in bare-metal ARM assembly where you need to load consecutive values but avoid the undefined behavior corner cases.

## Related Documentation

- See [WHY_CONSOLE_FAILED_DETAILED.md](WHY_CONSOLE_FAILED_DETAILED.md) for related platform-specific issues
- ARM: Architecture Reference Manual section on load/store with writeback
- See existing `Save()` function for the inverse operation (storing registers)
