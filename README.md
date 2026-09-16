# ThreadBobber

<img src=".github/needle.svg" alt="A sewing needle and thread" width="100px;" align="right">

ThreadBobber runs [Yarn Spinner](https://github.com/YarnSpinnerTool/YarnSpinner)
dialogue in C. You write conversations with the Yarn Spinner tools you already
know, compile them with the official compiler, convert the result to constant C
tables, and run them in your game. When a conversation is running, ThreadBobber
hands your game lines to show, options for the player to choose from, and
commands to handle.

The runtime uses no dynamic memory allocation. With the compact profile, each
dialogue needs 720 bytes of RAM on a Game Boy Advance.

Single-file MIT licensed library for C/C++. See [threadbobber.h](threadbobber.h)
for the API reference.

ThreadBobber is an independent implementation of Yarn Spinner's virtual machine
and isn't an official Yarn Spinner package. It supports a documented subset of
Yarn Spinner 3.2.2: see [Compatibility](#compatibility) for limitations. For the Yarn
language itself, see the [Yarn Spinner documentation](https://docs.yarnspinner.dev/).


## Converting dialogue

Keep using `.yarn`, `.yarnproject` and `.ysls.json` files with the Yarn Spinner
editor tools. `threadbobber.py` runs the pinned official compiler and writes a
`.c` and `.h` file. It needs Python 3.10 and `ysc` 3.2.2 on `PATH`:

```bash
dotnet tool install --global YarnSpinner.Console --version 3.2.2
python threadbobber.py game.yarnproject --out src --prefix game
```

Commands and functions are declared in the project's `.ysls.json` file. The
converter writes a constant for each into the generated header. A declared
command with literal arguments calls the command handler; any other command
is delivered as text.


## Running dialogue

Copy `threadbobber.h` into your project, define `THREADBOBBER_IMPLEMENTATION`
in exactly one C file before including it, and compile your generated program
alongside it. Every other file, including generated programs, includes the
header without that define. Link with `-lm` where the toolchain needs it.

```c
#define THREADBOBBER_IMPLEMENTATION
#include "threadbobber.h"
#include "game_program.h"

ThreadBobberDialogue dialogue;
const ThreadBobberLibrary library = {0};
ThreadBobber_Initialise(&dialogue, &game_program, &library);
ThreadBobber_SetNode(&dialogue, ThreadBobber_FindNode(&game_program, "Start"));

for (;;) {
    char text[256];
    switch (ThreadBobber_Run(&dialogue)) {
    case ThreadBobberState_WaitingForLine:
        ThreadBobber_GetCurrentLineText(&dialogue, text, sizeof text);
        puts(text);
        ThreadBobber_Continue(&dialogue);
        break;
    case ThreadBobberState_WaitingForOptions:
        for (unsigned i = 0; i < dialogue.OptionCount; i++) {
            ThreadBobber_GetOptionText(&dialogue, i, text, sizeof text);
            printf("%u: %s\n", i + 1, text);
        }
        ThreadBobber_SetSelectedOption(&dialogue, 0);
        break;
    case ThreadBobberState_WaitingForCommand:
        ThreadBobber_GetCurrentCommandText(&dialogue, text, sizeof text);
        ThreadBobber_Continue(&dialogue);
        break;
    case ThreadBobberState_Complete:
        return 0;
    default:
        puts(ThreadBobber_GetErrorMessage(dialogue.Error));
        return 1;
    }
}
```


`ThreadBobber_Run()` returns when dialogue needs the game to act or execution
ends:

| State | Game action |
| --- | --- |
| `WaitingForLine` | Present the line, then call `Continue()`. |
| `WaitingForOptions` | Present the options, then call `SetSelectedOption()`. |
| `WaitingForCommand` | Handle the text command or finish the pending callback, then call `Continue()`. |
| `Complete` | The conversation ended. Call `SetNode()` to start another. |
| `Error` | Read `dialogue.Error`; the dialogue will not continue. |
| `Running` | Returned by `RunFor()` when more work remains. Call it again next frame. |

`ThreadBobber_RunFor(dialogue, n)` runs at most `n` instructions so a game can
spread work across frames. Callbacks, string operations and nested content
queries finish before yielding, so the quota bounds instructions, not time.

The game owns the program, library and dialogue, and they must remain valid
while in use. Treat the dialogue's fields as read-only apart from `Variables`.
Callbacks may read and write variables, create strings and call
`EvaluateSmartVariable()`, but must not otherwise run the dialogue. Generated
tables must be trusted; do not cast untrusted bytes to `ThreadBobberProgram`.


## Strings and saving

Dialogue that builds strings at runtime needs a `ThreadBobberStringStorage`
attached immediately after `Initialise()`. Values held in variables, on the
stack, in pending content or in active function arguments stay alive. Anything
else may be reclaimed by the next allocation, so store a value in a variable
before creating another string.

`SaveState()` writes execution state into a buffer of `THREADBOBBER_SNAPSHOT_MAX`
bytes while dialogue is idle, waiting or complete. Save the variables, the
program's `Hash`, the string store from `SaveStrings()` and your random state
alongside it.

To restore:

1. Call `Initialise()` with the same program and check the saved program hash.
2. Attach string storage and call `RestoreStrings()` if using runtime strings.
3. Copy the saved variables back and restore your random state.
4. Call `RestoreState()` with the execution snapshot.

Snapshots require a matching runtime major version, capacity profile and program.


## Capacity profiles

Every capacity has a `THREADBOBBER_` macro with a default from the profile.
All translation units, including generated programs, must use the same values.
Define `THREADBOBBER_PROFILE_EXPANDED` for the larger profile, and convert with
`--profile expanded`.

| Capacity | Macro suffix | Compact | Expanded |
| --- | --- | ---: | ---: |
| Variables | `MAX_VARS` | 48 | 4096 |
| Stack values | `STACK` | 16 | 64 |
| Nested detours | `CALLS` | 4 | 32 |
| Options in a set | `OPTIONS` | 6 | 16 |
| Substitutions | `SUBSTITUTIONS` | 4 | 8 |
| Command/function arguments | `COMMAND_ARGS` | 4 | 8 |
| Instructions per `Run()` | `STEP_BUDGET` | 2048 | 65536 |
| Table identifier width | `ID_TYPE` | `uint16_t` | `uint32_t` |
| Smart-variable nesting | `EVALUATIONS` | 4 | 4 |
| Saliency candidates | `CANDIDATES` | 8 | 8 |
| String slots | `STRING_SLOTS` | 8 | 8 |
| Bytes per string slot | `STRING_BYTES` | 128 | 128 |

Bytecode operands are 16 bits in both profiles, so a program has at most 65535
bytes of bytecode and 65535 entries per table.


## Compatibility

Programs must come from YarnSpinner.Console 3.2.2, the release named by
`THREADBOBBER_YARN_VERSION`. Generated headers reject a runtime with a different
`THREADBOBBER_VERSION_MAJOR`, and snapshots record that version too.

The runtime does not support `format` or `format_invariant` (the converter
rejects them), markup parsing, localisation, custom saliency strategies or node
header queries. Numbers are formatted in the C locale. Games that localise
dialogue should use the line ID and format `Substitutions` themselves.

Random numbers come from the game's handler and will not match .NET's sequence.
As in Yarn Spinner, `%` rounds both operands to integers (half to even) before
taking the remainder. A divisor that rounds to zero stops dialogue with
`ThreadBobberError_HostError`.


## Tests

Call `make test` to build and run the tests in both capacity profiles. The
test binaries use [utest.h](tests/utest.h), so `./test_compact --help` lists
their options. With `ysc` and .NET installed, `make parity` compares evaluated
traces against the official runtime and exercises the converter end to end, and
`make generate` regenerates the checked-in programs.


## Benchmarks

Game Boy Advance, compact profile, devkitARM GCC 16.1 at `-Os`, measured in the
NanoBoyAdvance emulator:

| | |
| --- | ---: |
| Dialogue state | 720 bytes |
| Optional string store | 1,032 bytes |
| Runtime code and constants | 11,395 bytes |
| Longest `RunFor` slice at a quota of 8 instructions | 2.1 ms |

On x86-64 the runtime executes a short conversation about 5x faster than the
Yarn Spinner C# runtime, both reusing one dialogue with the JIT warmed up.


## License

MIT. Yarn Spinner is developed by Yarn Spinner Pty. Ltd., Secret Lab Pty. Ltd.
and Yarn Spinner contributors, and is also MIT licensed. Thank you to the Yarn
Spinner team for making their tools available. The needle is
[Sewing needle](https://game-icons.net/1x1/lorc/sewing-needle.html) by Lorc from
[game-icons.net](https://game-icons.net), [CC BY 3.0](https://creativecommons.org/licenses/by/3.0/).
