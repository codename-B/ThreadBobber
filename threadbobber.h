/*
Copyright (c) 2026, codename-B
SPDX-License-Identifier: MIT

ThreadBobber 1.0.0 - a small C runtime for Yarn Spinner 3.2.2 dialogue.

Independent implementation; not an official Yarn Spinner package.
https://github.com/YarnSpinnerTool/YarnSpinner

Define THREADBOBBER_IMPLEMENTATION before including this header in
exactly one C file.

See README.md for conversion, integration and compatibility details.
*/

#ifndef THREADBOBBER_H
#define THREADBOBBER_H

#define THREADBOBBER_VERSION "1.0.0"
#define THREADBOBBER_VERSION_MAJOR 1
#define THREADBOBBER_VERSION_MINOR 0
#define THREADBOBBER_VERSION_PATCH 0
/// The Yarn Spinner release whose compiler output this runtime executes.
#define THREADBOBBER_YARN_VERSION "3.2.2"

#include <stdint.h>
#ifdef __cplusplus
extern "C"
{
#endif

// All translation units, including generated programs, must use the same capacities.
#if defined(THREADBOBBER_PROFILE_EXPANDED)
#ifndef THREADBOBBER_STACK
#define THREADBOBBER_STACK 64
#endif
#ifndef THREADBOBBER_CALLS
#define THREADBOBBER_CALLS 32
#endif
#ifndef THREADBOBBER_OPTIONS
#define THREADBOBBER_OPTIONS 16
#endif
#ifndef THREADBOBBER_SUBSTITUTIONS
#define THREADBOBBER_SUBSTITUTIONS 8
#endif
#ifndef THREADBOBBER_COMMAND_ARGS
#define THREADBOBBER_COMMAND_ARGS 8
#endif
#ifndef THREADBOBBER_MAX_VARS
#define THREADBOBBER_MAX_VARS 4096
#endif
#ifndef THREADBOBBER_STEP_BUDGET
#define THREADBOBBER_STEP_BUDGET 65536
#endif
#ifndef THREADBOBBER_ID_TYPE
#define THREADBOBBER_ID_TYPE uint32_t
#endif
#else

#ifndef THREADBOBBER_STACK
#define THREADBOBBER_STACK 16
#endif
#ifndef THREADBOBBER_CALLS
#define THREADBOBBER_CALLS 4
#endif
#ifndef THREADBOBBER_OPTIONS
#define THREADBOBBER_OPTIONS 6
#endif
#ifndef THREADBOBBER_SUBSTITUTIONS
#define THREADBOBBER_SUBSTITUTIONS 4
#endif
#ifndef THREADBOBBER_COMMAND_ARGS
#define THREADBOBBER_COMMAND_ARGS 4
#endif
#ifndef THREADBOBBER_MAX_VARS
#define THREADBOBBER_MAX_VARS 48
#endif
#ifndef THREADBOBBER_STEP_BUDGET
#define THREADBOBBER_STEP_BUDGET 2048
#endif
#ifndef THREADBOBBER_ID_TYPE
#define THREADBOBBER_ID_TYPE uint16_t
#endif
#endif

#ifndef THREADBOBBER_STRING_SLOTS
#define THREADBOBBER_STRING_SLOTS 8
#endif
#ifndef THREADBOBBER_STRING_BYTES
#define THREADBOBBER_STRING_BYTES 128
#endif
#if THREADBOBBER_STRING_SLOTS < 1 || THREADBOBBER_STRING_SLOTS > 32 || THREADBOBBER_STRING_BYTES < 2
#error "String storage requires one to 32 slots and room for a terminating null byte."
#endif

#ifndef THREADBOBBER_CANDIDATES
#define THREADBOBBER_CANDIDATES 8
#endif
#if THREADBOBBER_CANDIDATES < 1 || THREADBOBBER_CANDIDATES > 255
#error "The saliency candidate capacity must fit in a byte."
#endif

#ifndef THREADBOBBER_EVALUATIONS
#define THREADBOBBER_EVALUATIONS 4
#endif
#if THREADBOBBER_EVALUATIONS < 1 || THREADBOBBER_EVALUATIONS > 255
#error "The smart-variable evaluation capacity must fit in a byte."
#endif

#if THREADBOBBER_STACK < 1 || THREADBOBBER_STACK > 255 || THREADBOBBER_CALLS < 1 ||                \
    THREADBOBBER_CALLS > 255 || THREADBOBBER_OPTIONS < 1 || THREADBOBBER_OPTIONS > 255 ||          \
    THREADBOBBER_SUBSTITUTIONS < 1 || THREADBOBBER_SUBSTITUTIONS > 255 ||                          \
    THREADBOBBER_COMMAND_ARGS < 4 || THREADBOBBER_COMMAND_ARGS > 255
#error "ThreadBobber capacities must fit in a byte; command capacity must be at least four."
#endif

/// An index into a program table. All translation units must use the same profile.
typedef THREADBOBBER_ID_TYPE ThreadBobberId;
/// Indicates that a line, command or tracking variable is not present.
#define THREADBOBBER_NONE ((ThreadBobberId)(~((ThreadBobberId)0)))

/// Instructions emitted by the converter. Operands use little-endian byte order.
enum
{
    ThreadBobberOpcode_JumpTo = 1,
    ThreadBobberOpcode_PeekAndJump,
    ThreadBobberOpcode_RunLine,
    ThreadBobberOpcode_RunCommand,
    ThreadBobberOpcode_AddOption,
    ThreadBobberOpcode_ShowOptions,
    ThreadBobberOpcode_PushString,
    ThreadBobberOpcode_PushFloat,
    ThreadBobberOpcode_PushBool,
    ThreadBobberOpcode_JumpIfFalse,
    ThreadBobberOpcode_Pop,
    ThreadBobberOpcode_CallFunc,
    ThreadBobberOpcode_PushVariable,
    ThreadBobberOpcode_StoreVariable,
    ThreadBobberOpcode_Stop,
    ThreadBobberOpcode_RunNode,
    ThreadBobberOpcode_PeekAndRunNode,
    ThreadBobberOpcode_DetourToNode,
    ThreadBobberOpcode_PeekAndDetourToNode,
    ThreadBobberOpcode_Return,
    ThreadBobberOpcode_EvaluateVariable,
    ThreadBobberOpcode_AddSaliencyCandidate,
    ThreadBobberOpcode_SelectSaliencyCandidate,
    ThreadBobberOpcode_Count
};

/// The type of a value stored on the stack or in variable storage.
typedef enum
{
    ThreadBobberValueType_None = 0,
    ThreadBobberValueType_Number = 1,
    ThreadBobberValueType_Boolean = 2,
    ThreadBobberValueType_String = 3
} ThreadBobberValueType;

/// A number, boolean or string identifier. Numbers contain the bits of an IEEE 754 float.
typedef struct
{
    uint8_t Type;
    uint32_t Bits;
} ThreadBobberValue;

/// A named node, with its bytecode range and optional visit-tracking variable.
typedef struct
{
    ThreadBobberId Name, Start, Length, TrackingVariable;
    uint8_t IsSmartVariable;
} ThreadBobberNode;

/// A variable declaration and its initial value. Hash identifies the variable name.
typedef struct
{
    ThreadBobberId Name;
    uint8_t Type;
    uint32_t Hash, InitialValue;
} ThreadBobberVariable;

/// A line of dialogue. Text and ID are indices into the program string table.
typedef struct
{
    ThreadBobberId Text, ID;
} ThreadBobberLine;

/// A command sent to the game. FunctionID identifies its declaration; Site identifies its
/// position in the node.
typedef struct
{
    uint8_t FunctionID, ParameterCount, Site, IsText;
    ThreadBobberValue Parameters[THREADBOBBER_COMMAND_ARGS];
    ThreadBobberId Text;
    uint8_t SubstitutionCount;
} ThreadBobberCommand;

/// A function declaration. ReturnType describes the value returned to dialogue.
typedef struct
{
    uint8_t Builtin, FunctionID, ParameterCount, ReturnType;
} ThreadBobberFunction;

/// A compiled dialogue program. The game owns these constant tables for the lifetime of every
/// dialogue that uses them.
/// Tables must be trusted; do not cast untrusted bytes to ThreadBobberProgram.
typedef struct
{
    const char *Strings;
    const ThreadBobberId *StringOffsets;
    ThreadBobberId StringCount;
    const uint8_t *Code;
    uint32_t CodeSize;
    const ThreadBobberNode *Nodes;
    ThreadBobberId NodeCount;
    const ThreadBobberVariable *Variables;
    ThreadBobberId VariableCount;
    const ThreadBobberLine *Lines;
    ThreadBobberId LineCount;
    const ThreadBobberCommand *Commands;
    ThreadBobberId CommandCount;
    const ThreadBobberFunction *Functions;
    ThreadBobberId FunctionCount;
    uint32_t Hash;
} ThreadBobberProgram;

/// Functions provided by the runtime. Zero identifies a function supplied by the game.
enum
{
    ThreadBobberFunction_None = 0,
    ThreadBobberFunction_NumberAdd,
    ThreadBobberFunction_NumberMinus,
    ThreadBobberFunction_NumberMultiply,
    ThreadBobberFunction_NumberDivide,
    ThreadBobberFunction_NumberModulo,
    ThreadBobberFunction_NumberUnaryMinus,
    ThreadBobberFunction_NumberEqualTo,
    ThreadBobberFunction_NumberNotEqualTo,
    ThreadBobberFunction_NumberGreaterThan,
    ThreadBobberFunction_NumberLessThan,
    ThreadBobberFunction_NumberGreaterThanOrEqualTo,
    ThreadBobberFunction_NumberLessThanOrEqualTo,
    ThreadBobberFunction_BoolAnd,
    ThreadBobberFunction_BoolOr,
    ThreadBobberFunction_BoolXor,
    ThreadBobberFunction_BoolNot,
    ThreadBobberFunction_BoolEqualTo,
    ThreadBobberFunction_BoolNotEqualTo,
    ThreadBobberFunction_StringEqualTo,
    ThreadBobberFunction_StringNotEqualTo,
    ThreadBobberFunction_Visited,
    ThreadBobberFunction_VisitedCount,
    ThreadBobberFunction_Random,
    ThreadBobberFunction_RandomRange,
    ThreadBobberFunction_Dice,
    ThreadBobberFunction_Min,
    ThreadBobberFunction_Max,
    ThreadBobberFunction_Round,
    ThreadBobberFunction_Floor,
    ThreadBobberFunction_Ceil,
    ThreadBobberFunction_Inc,
    ThreadBobberFunction_Dec,
    ThreadBobberFunction_Int,
    ThreadBobberFunction_Decimal,
    ThreadBobberFunction_RoundPlaces,
    ThreadBobberFunction_RandomRangeFloat,
    ThreadBobberFunction_ValueEqualTo,
    ThreadBobberFunction_ValueNotEqualTo,
    ThreadBobberFunction_HasAnyContent,
    ThreadBobberFunction_StringAdd,
    ThreadBobberFunction_ConvertString,
    ThreadBobberFunction_ConvertNumber,
    ThreadBobberFunction_ConvertBoolean,
    ThreadBobberFunction_Count
};

/// Describes whether dialogue is running, waiting for the game, complete or in error.
typedef enum
{
    ThreadBobberState_Idle = 0,
    ThreadBobberState_Running,
    ThreadBobberState_WaitingForLine,
    ThreadBobberState_WaitingForOptions,
    ThreadBobberState_WaitingForCommand,
    ThreadBobberState_Complete,
    ThreadBobberState_Error
} ThreadBobberState;

/// The reason dialogue could not continue.
typedef enum
{
    ThreadBobberError_None = 0,
    ThreadBobberError_InvalidProgram,
    ThreadBobberError_InvalidNode,
    ThreadBobberError_InvalidProgramCounter,
    ThreadBobberError_InvalidOpcode,
    ThreadBobberError_StackOverflow,
    ThreadBobberError_StackUnderflow,
    ThreadBobberError_TypeMismatch,
    ThreadBobberError_CallStackOverflow,
    ThreadBobberError_TooManyOptions,
    ThreadBobberError_InstructionLimit,
    ThreadBobberError_InvalidFunction,
    ThreadBobberError_HostError,
    ThreadBobberError_InvalidState,
    ThreadBobberError_InvalidVariable,
    ThreadBobberError_EvaluationOverflow,
    ThreadBobberError_TooManyCandidates,
    ThreadBobberError_StringStorageFull,
    ThreadBobberError_StringTooLong,
    ThreadBobberError_Count
} ThreadBobberError;

/// An option to present to the player. IsAvailable indicates whether the option may be
/// selected.
typedef struct
{
    ThreadBobberId Line, Destination;
    uint8_t IsAvailable, SubstitutionCount;
    ThreadBobberValue Substitutions[THREADBOBBER_SUBSTITUTIONS];
} ThreadBobberOption;

/// The node and byte offset to return to after a detour.
typedef struct
{
    ThreadBobberId Node, ProgramCounter;
} ThreadBobberCallSite;

/// A suspended expression evaluation. Values below StackBase belong to its caller.
typedef struct
{
    ThreadBobberId Node, ProgramCounter;
    uint8_t StackBase;
} ThreadBobberEvaluation;

/// An eligible item awaiting content selection. Variable stores its view count.
typedef struct
{
    ThreadBobberId Variable, Destination;
    uint16_t Complexity;
} ThreadBobberSaliencyCandidate;
struct ThreadBobberDialogue;

/// Functions supplied by the game. Any unused handler may be NULL.
/// Callbacks may access variables, create strings and call EvaluateSmartVariable,
/// but must not otherwise run the dialogue.
typedef struct
{
    /// Receives a command. Return zero when complete, a positive value to wait,
    /// or a negative value to stop dialogue with HostError. Resume with Continue.
    int (*CommandHandler)(struct ThreadBobberDialogue *dialogue, unsigned functionID,
                          const ThreadBobberValue *parameters, unsigned parameterCount);

    /// Calls a function. Write its return value to result and return non-zero.
    /// Return zero if the function could not be called.
    int (*FunctionHandler)(struct ThreadBobberDialogue *dialogue, unsigned functionID,
                           const ThreadBobberValue *parameters, unsigned parameterCount,
                           ThreadBobberValue *result);

    /// Returns a uniformly distributed integer from zero to bound, inclusive.
    /// The game owns the random state and is responsible for saving it.
    uint32_t (*RandomHandler)(struct ThreadBobberDialogue *dialogue, uint32_t bound);
} ThreadBobberLibrary;

/// Optional storage for strings created during dialogue. Attach one store to one
/// dialogue. Unreferenced slots are reclaimed when another string is created.
typedef struct
{
    uint32_t Allocated, Pinned;
    char Entries[THREADBOBBER_STRING_SLOTS][THREADBOBBER_STRING_BYTES];
} ThreadBobberStringStorage;

/// The execution state and variable storage for one conversation. The game allocates this
/// structure; ThreadBobber does not allocate memory.
/// Treat its fields as read-only apart from Variables.
typedef struct ThreadBobberDialogue
{
    const ThreadBobberProgram *Program;
    const ThreadBobberLibrary *Library;
    ThreadBobberStringStorage *Strings;
    uint32_t Variables[THREADBOBBER_MAX_VARS];
    ThreadBobberValue Stack[THREADBOBBER_STACK];
    ThreadBobberCallSite CallStack[THREADBOBBER_CALLS];
    ThreadBobberOption Options[THREADBOBBER_OPTIONS];
    ThreadBobberValue Substitutions[THREADBOBBER_SUBSTITUTIONS];
    ThreadBobberId Node, ProgramCounter, Line, Command;
    uint8_t State, Error, StackCount, CallStackCount, OptionCount, SubstitutionCount;
    uint32_t InstructionCount, RemainingInstructions;
    ThreadBobberEvaluation Evaluations[THREADBOBBER_EVALUATIONS];
    uint8_t EvaluationCount, CandidateCount, ExecutionActive;
    ThreadBobberSaliencyCandidate Candidates[THREADBOBBER_CANDIDATES];
} ThreadBobberDialogue;

/// Initialises dialogue and copies the program's initial values into variable storage.
/// Returns non-zero on success. The program and library must remain valid while
/// dialogue uses them. A failed initialisation leaves dialogue in the Error state.
int ThreadBobber_Initialise(ThreadBobberDialogue *dialogue, const ThreadBobberProgram *program,
                            const ThreadBobberLibrary *library);

/// Attaches and clears caller-owned string storage while dialogue is Idle.
/// Returns zero in any other state. The store must remain alive with dialogue.
int ThreadBobber_AttachStringStorage(ThreadBobberDialogue *dialogue,
                                     ThreadBobberStringStorage *storage);

/// Copies a UTF-8 string into bounded storage. Keep the returned value in variable
/// storage or the VM stack before creating another string. External handles and
/// GetString pointers are borrowed until the next allocation unless rooted there.
/// Returns zero and sets an error if no slot is available or the string is too long.
int ThreadBobber_StoreString(ThreadBobberDialogue *dialogue, const char *text,
                             ThreadBobberValue *result);

/// String storage is saved separately from variables and execution state. Restore
/// strings first, then the matching variables and execution snapshot.
enum
{
    THREADBOBBER_STRING_SNAPSHOT_MAX = 8 + THREADBOBBER_STRING_SLOTS * THREADBOBBER_STRING_BYTES
};

/// Saves attached string storage. Returns the number of bytes written, or zero
/// when storage is missing, dialogue is running or the buffer is too small.
unsigned ThreadBobber_SaveStrings(const ThreadBobberDialogue *dialogue, unsigned char *output,
                                  unsigned capacity);
/// Restores string storage saved with the same capacities. Returns zero without
/// changing storage when the input is invalid or dialogue is running.
int ThreadBobber_RestoreStrings(ThreadBobberDialogue *dialogue, const unsigned char *input,
                                unsigned length);

/// Sets the node to run and clears the previous execution state. Variables are retained.
/// Returns non-zero if the node exists. Call Run to begin delivering content.
int ThreadBobber_SetNode(ThreadBobberDialogue *dialogue, unsigned node);

/// Gets the index of a node by name, or -1 if no matching node exists.
int ThreadBobber_FindNode(const ThreadBobberProgram *program, const char *name);

/// Runs dialogue until it delivers a line, options or an asynchronous command,
/// completes, or encounters an error. Returns the resulting state. Calling Run
/// while dialogue is waiting does not advance it.
ThreadBobberState ThreadBobber_Run(ThreadBobberDialogue *dialogue);

/// Runs up to instructionCount instructions before returning control to the game.
/// Running means more work remains; call RunFor again on a later frame. Host
/// callbacks and nested content queries complete before yielding, so this is an
/// instruction quota rather than a hard time limit. Zero performs no work.
ThreadBobberState ThreadBobber_RunFor(ThreadBobberDialogue *dialogue, unsigned instructionCount);

/// Marks the current line or command as complete. Returns non-zero on success.
/// Call Run to advance dialogue. Calling this method in another state is an error.
int ThreadBobber_Continue(ThreadBobberDialogue *dialogue);

/// Selects an available option by its index in Options. Returns non-zero on success.
/// An unavailable or out-of-range index leaves the option set unchanged.
int ThreadBobber_SetSelectedOption(ThreadBobberDialogue *dialogue, unsigned option);

/// Gets a program or runtime-created string, or an empty string for an invalid identifier.
/// A runtime-created string is borrowed until the next allocation unless its value is rooted.
const char *ThreadBobber_GetString(const ThreadBobberDialogue *dialogue, unsigned id);

/// Gets the base-language text for a line, before substitutions are expanded.
const char *ThreadBobber_GetLineText(const ThreadBobberDialogue *dialogue, unsigned line);

/// Gets the stable Yarn line ID, including its line: prefix.
const char *ThreadBobber_GetLineID(const ThreadBobberDialogue *dialogue, unsigned line);

/// Expands numeric substitution placeholders into a caller-owned buffer.
/// Returns the number of bytes written, excluding the terminating null byte.
/// Output is truncated to capacity - 1 bytes and terminated when capacity is
/// non-zero. Missing substitutions are shown as ?. Markup is not parsed.
unsigned ThreadBobber_ExpandSubstitutions(const ThreadBobberDialogue *dialogue, unsigned line,
                                          const ThreadBobberValue *substitutions, unsigned count,
                                          char *output, unsigned capacity);

/// Gets the current line with its substitutions expanded. Uses the same buffer
/// and return-value conventions as ExpandSubstitutions.
unsigned ThreadBobber_GetCurrentLineText(const ThreadBobberDialogue *dialogue, char *output,
                                         unsigned capacity);

/// Gets the current text command with its substitutions expanded. Returns the number
/// of bytes required, excluding the terminating null byte. Pass NULL and zero to
/// measure the output. When capacity is non-zero, output is terminated and truncated
/// to capacity - 1 bytes. Check the return value before executing a command.
unsigned ThreadBobber_GetCurrentCommandText(const ThreadBobberDialogue *dialogue, char *output,
                                            unsigned capacity);

/// Gets an option's text with the substitutions captured when the option was added.
/// Uses the same buffer and return-value conventions as ExpandSubstitutions.
unsigned ThreadBobber_GetOptionText(const ThreadBobberDialogue *dialogue, unsigned option,
                                    char *output, unsigned capacity);

/// Gets a number from a numeric value. Booleans yield one or zero; other types yield zero.
float ThreadBobber_GetNumber(ThreadBobberValue value);

/// Creates a numeric value without allocating memory.
ThreadBobberValue ThreadBobber_CreateNumber(float number);

/// Creates a boolean value. Any non-zero argument becomes true.
ThreadBobberValue ThreadBobber_CreateBoolean(int boolean);

/// Creates a string value from an existing identifier in the program string table.
ThreadBobberValue ThreadBobber_CreateString(unsigned id);

/// Evaluates a smart variable by name without advancing the surrounding dialogue.
/// Reads current stored values and shares the instruction budget during Run.
/// Returns zero on failure. Evaluation errors stop dialogue in the Error state.
int ThreadBobber_EvaluateSmartVariable(ThreadBobberDialogue *dialogue, const char *name,
                                       ThreadBobberValue *result);

/// Gets a variable's value. Returns zero if the variable does not exist.
int ThreadBobber_GetValue(const ThreadBobberDialogue *dialogue, unsigned variable,
                          ThreadBobberValue *result);

/// Sets a variable's value. Returns zero if its type or string identifier is invalid.
int ThreadBobber_SetValue(ThreadBobberDialogue *dialogue, unsigned variable,
                          ThreadBobberValue value);

/// Gets the number of completed visits to a node. Untracked nodes return zero.
unsigned ThreadBobber_GetVisitCount(const ThreadBobberDialogue *dialogue, unsigned node);

/// Gets a diagnostic message for an error code.
const char *ThreadBobber_GetErrorMessage(unsigned error);

/// The maximum number of bytes required to save execution state.
/// Variable storage, the program hash and game state are saved separately.
enum
{
    THREADBOBBER_SNAPSHOT_HEADER_SIZE = 6 + 4 * (int)sizeof(ThreadBobberId),
    THREADBOBBER_SNAPSHOT_MAX = THREADBOBBER_SNAPSHOT_HEADER_SIZE + THREADBOBBER_STACK * 5 +
                                THREADBOBBER_CALLS * ((int)sizeof(ThreadBobberId) * 2) +
                                THREADBOBBER_OPTIONS * ((int)sizeof(ThreadBobberId) * 2 + 2 +
                                                        THREADBOBBER_SUBSTITUTIONS * 5) +
                                THREADBOBBER_SUBSTITUTIONS * 5
};

/// Saves execution state to a buffer. Returns the number of bytes written, or
/// zero if the buffer is too small or dialogue is running or in error.
unsigned ThreadBobber_SaveState(const ThreadBobberDialogue *dialogue, unsigned char *output,
                                unsigned capacity);

/// Restores execution state saved with the same program, major version and
/// capacity profile. Returns zero without changing dialogue if the state is invalid.
/// The game must check its saved program hash before calling this method.
int ThreadBobber_RestoreState(ThreadBobberDialogue *dialogue, const unsigned char *input,
                              unsigned length);

#ifdef __cplusplus
}
#endif
#endif

/* -----------------------------------------------------------------------------
    Implementation */

#if defined(THREADBOBBER_IMPLEMENTATION) && !defined(THREADBOBBER_IMPLEMENTATION_DONE)
#define THREADBOBBER_IMPLEMENTATION_DONE

#include <string.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef __cplusplus
#define THREADBOBBER_STATIC_ASSERT static_assert
#else
#define THREADBOBBER_STATIC_ASSERT _Static_assert
#endif

THREADBOBBER_STATIC_ASSERT(sizeof(float) == 4 && FLT_RADIX == 2 && FLT_MANT_DIG == 24 &&
                               FLT_MAX_EXP == 128,
                           "ThreadBobber requires IEEE 754 single-precision floats.");
THREADBOBBER_STATIC_ASSERT(sizeof(ThreadBobberId) == 2 || sizeof(ThreadBobberId) == 4,
                           "ThreadBobber identifiers must contain 16 or 32 bits.");
#undef THREADBOBBER_STATIC_ASSERT

static float BitsToFloat(uint32_t b)
{
    float f;
    memcpy(&f, &b, 4);
    return f;
}

static uint32_t FloatToBits(float f)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

float ThreadBobber_GetNumber(ThreadBobberValue value)
{
    return value.Type == ThreadBobberValueType_Number    ? BitsToFloat(value.Bits)
           : value.Type == ThreadBobberValueType_Boolean ? (value.Bits ? 1.f : 0.f)
                                                         : 0.f;
}

ThreadBobberValue ThreadBobber_CreateNumber(float f)
{
    ThreadBobberValue value = {ThreadBobberValueType_Number, FloatToBits(f)};
    return value;
}

ThreadBobberValue ThreadBobber_CreateBoolean(int b)
{
    ThreadBobberValue value = {ThreadBobberValueType_Boolean, b ? 1u : 0u};
    return value;
}

ThreadBobberValue ThreadBobber_CreateString(unsigned id)
{
    ThreadBobberValue value = {ThreadBobberValueType_String, id};
    return value;
}

static int IsTrue(ThreadBobberValue value)
{
    return value.Type == ThreadBobberValueType_Number ? BitsToFloat(value.Bits) != 0.f
           : value.Type == ThreadBobberValueType_Boolean
               ? value.Bits != 0
               : value.Type == ThreadBobberValueType_String;
}

static void SetError(ThreadBobberDialogue *dialogue, unsigned error)
{
    if (dialogue->State != ThreadBobberState_Error)
    {
        dialogue->State = ThreadBobberState_Error;
        dialogue->Error = (uint8_t)error;
    }
}

const char *ThreadBobber_GetErrorMessage(unsigned e)
{
    static const char *const names[ThreadBobberError_Count] = {
        "No error.",
        "The program is invalid.",
        "The node does not exist.",
        "The program counter is outside the node.",
        "The instruction is not supported.",
        "The value stack is full.",
        "The value stack is empty.",
        "The value has an unexpected type.",
        "The detour stack is full.",
        "The option set exceeds its capacity.",
        "The instruction limit was exceeded.",
        "The function does not exist.",
        "The game could not handle the command or function.",
        "Dialogue cannot perform this operation in its current state.",
        "The variable does not exist.",
        "The smart-variable evaluation stack is full.",
        "The saliency candidate list is full.",
        "Dynamic string storage is unavailable or full.",
        "The string exceeds its storage capacity."};
    return e < ThreadBobberError_Count ? names[e] : "?";
}

#define DynamicStringBit UINT32_C(0x80000000)

static int IsValidString(const ThreadBobberDialogue *dialogue, uint32_t id)
{
    if (!(id & DynamicStringBit))
    {
        return dialogue->Program && id < dialogue->Program->StringCount;
    }
    unsigned slot = id & ~DynamicStringBit;
    return dialogue->Strings && slot < THREADBOBBER_STRING_SLOTS &&
           (dialogue->Strings->Allocated & (UINT32_C(1) << slot));
}

const char *ThreadBobber_GetString(const ThreadBobberDialogue *dialogue, unsigned id)
{
    if (id & DynamicStringBit)
    {
        return IsValidString(dialogue, id) ? dialogue->Strings->Entries[id & ~DynamicStringBit]
                                           : "";
    }
    const ThreadBobberProgram *program = dialogue->Program;
    return program && id < program->StringCount ? program->Strings + program->StringOffsets[id]
                                                : "";
}

const char *ThreadBobber_GetLineText(const ThreadBobberDialogue *dialogue, unsigned line)
{
    const ThreadBobberProgram *program = dialogue->Program;
    return program && line < program->LineCount
               ? ThreadBobber_GetString(dialogue, program->Lines[line].Text)
               : "";
}

const char *ThreadBobber_GetLineID(const ThreadBobberDialogue *dialogue, unsigned line)
{
    const ThreadBobberProgram *program = dialogue->Program;
    return program && line < program->LineCount
               ? ThreadBobber_GetString(dialogue, program->Lines[line].ID)
               : "";
}

static int IsValidProgram(const ThreadBobberProgram *program)
{
    if (!program || !program->Strings || !program->StringOffsets || !program->Code ||
        !program->Nodes || !program->NodeCount || program->VariableCount > THREADBOBBER_MAX_VARS)
    {
        return 0;
    }
    if ((program->VariableCount && !program->Variables) ||
        (program->LineCount && !program->Lines) || (program->CommandCount && !program->Commands) ||
        (program->FunctionCount && !program->Functions))
    {
        return 0;
    }
    for (unsigned i = 0; i < program->NodeCount; i++)
    {
        const ThreadBobberNode *n = &program->Nodes[i];
        if (n->Name >= program->StringCount || (uint32_t)n->Start > program->CodeSize ||
            n->Length > program->CodeSize - n->Start ||
            (n->TrackingVariable != THREADBOBBER_NONE &&
             n->TrackingVariable >= program->VariableCount))
        {
            return 0;
        }
    }
    for (unsigned i = 0; i < program->VariableCount; i++)
    {
        if (program->Variables[i].Type < ThreadBobberValueType_Number ||
            program->Variables[i].Type > ThreadBobberValueType_String ||
            program->Variables[i].Name >= program->StringCount ||
            (program->Variables[i].Type == ThreadBobberValueType_String &&
             program->Variables[i].InitialValue >= program->StringCount))
        {
            return 0;
        }
    }
    for (unsigned i = 0; i < program->LineCount; i++)
    {
        if (program->Lines[i].Text >= program->StringCount ||
            program->Lines[i].ID >= program->StringCount)
        {
            return 0;
        }
    }
    for (unsigned i = 0; i < program->CommandCount; i++)
    {
        if (program->Commands[i].ParameterCount > THREADBOBBER_COMMAND_ARGS ||
            program->Commands[i].Text >= program->StringCount ||
            program->Commands[i].SubstitutionCount > THREADBOBBER_SUBSTITUTIONS)
        {
            return 0;
        }
    }
    for (unsigned i = 0; i < program->FunctionCount; i++)
    {
        if (program->Functions[i].Builtin >= ThreadBobberFunction_Count ||
            program->Functions[i].ParameterCount > THREADBOBBER_COMMAND_ARGS ||
            program->Functions[i].ReturnType > ThreadBobberValueType_String)
        {
            return 0;
        }
    }
    return 1;
}

int ThreadBobber_Initialise(ThreadBobberDialogue *dialogue, const ThreadBobberProgram *program,
                            const ThreadBobberLibrary *host)
{
    if (!dialogue)
    {
        return 0;
    }
    memset(dialogue, 0, sizeof *dialogue);
    dialogue->Line = dialogue->Command = THREADBOBBER_NONE;
    if (!IsValidProgram(program) || !host)
    {
        SetError(dialogue, ThreadBobberError_InvalidProgram);
        return 0;
    }
    dialogue->Program = program;
    dialogue->Library = host;
    dialogue->State = ThreadBobberState_Idle;
    dialogue->Line = dialogue->Command = THREADBOBBER_NONE;
    for (unsigned i = 0; i < program->VariableCount; i++)
    {
        dialogue->Variables[i] = program->Variables[i].InitialValue;
    }
    return 1;
}

int ThreadBobber_AttachStringStorage(ThreadBobberDialogue *dialogue,
                                     ThreadBobberStringStorage *storage)
{
    if (!dialogue || !storage || dialogue->Strings || !dialogue->Program ||
        dialogue->State != ThreadBobberState_Idle)
    {
        return 0;
    }
    memset(storage, 0, sizeof *storage);
    dialogue->Strings = storage;
    return 1;
}

static uint32_t StringMask(ThreadBobberValue value)
{
    unsigned slot = value.Bits & ~DynamicStringBit;
    return value.Type == ThreadBobberValueType_String && (value.Bits & DynamicStringBit) &&
                   slot < THREADBOBBER_STRING_SLOTS
               ? UINT32_C(1) << slot
               : 0;
}

static uint32_t ReferencedStrings(const ThreadBobberDialogue *dialogue)
{
    uint32_t mask = dialogue->Strings->Pinned;
    for (unsigned i = 0; i < dialogue->Program->VariableCount; i++)
    {
        ThreadBobberValue value = {dialogue->Program->Variables[i].Type, dialogue->Variables[i]};
        mask |= StringMask(value);
    }
    for (unsigned i = 0; i < dialogue->StackCount; i++)
    {
        mask |= StringMask(dialogue->Stack[i]);
    }
    for (unsigned i = 0; i < dialogue->SubstitutionCount; i++)
    {
        mask |= StringMask(dialogue->Substitutions[i]);
    }
    for (unsigned i = 0; i < dialogue->OptionCount; i++)
    {
        for (unsigned j = 0; j < dialogue->Options[i].SubstitutionCount; j++)
        {
            mask |= StringMask(dialogue->Options[i].Substitutions[j]);
        }
    }
    return mask;
}

int ThreadBobber_StoreString(ThreadBobberDialogue *dialogue, const char *text,
                             ThreadBobberValue *result)
{
    if (!dialogue || !dialogue->Program || !text || !result)
    {
        return 0;
    }
    ThreadBobberStringStorage *storage = dialogue->Strings;
    if (!storage)
    {
        SetError(dialogue, ThreadBobberError_StringStorageFull);
        return 0;
    }
    unsigned length = 0;
    while (length < THREADBOBBER_STRING_BYTES && text[length])
    {
        length++;
    }
    if (length == THREADBOBBER_STRING_BYTES)
    {
        SetError(dialogue, ThreadBobberError_StringTooLong);
        return 0;
    }
    for (unsigned i = 0; i < THREADBOBBER_STRING_SLOTS; i++)
    {
        if ((storage->Allocated & (UINT32_C(1) << i)) && !strcmp(storage->Entries[i], text))
        {
            *result = ThreadBobber_CreateString(DynamicStringBit | i);
            return 1;
        }
    }
    uint32_t live = ReferencedStrings(dialogue);
    for (unsigned i = 0; i < THREADBOBBER_STRING_SLOTS; i++)
    {
        if (!(live & (UINT32_C(1) << i)))
        {
            memmove(storage->Entries[i], text, length + 1);
            storage->Allocated = live | (UINT32_C(1) << i);
            *result = ThreadBobber_CreateString(DynamicStringBit | i);
            return 1;
        }
    }
    SetError(dialogue, ThreadBobberError_StringStorageFull);
    return 0;
}

int ThreadBobber_FindNode(const ThreadBobberProgram *program, const char *name)
{
    if (!program || !name)
    {
        return -1;
    }
    for (unsigned i = 0; i < program->NodeCount; i++)
    {
        if (!strcmp(program->Strings + program->StringOffsets[program->Nodes[i].Name], name))
        {
            return (int)i;
        }
    }
    return -1;
}

static void ResetExecution(ThreadBobberDialogue *dialogue)
{
    dialogue->EvaluationCount = dialogue->CandidateCount = 0;
    dialogue->StackCount = dialogue->CallStackCount = dialogue->OptionCount =
        dialogue->SubstitutionCount = 0;
    dialogue->Line = dialogue->Command = THREADBOBBER_NONE;
}

int ThreadBobber_SetNode(ThreadBobberDialogue *dialogue, unsigned node)
{
    if (!dialogue->Program)
    {
        return 0;
    }
    if (node >= dialogue->Program->NodeCount)
    {
        SetError(dialogue, ThreadBobberError_InvalidNode);
        return 0;
    }
    ResetExecution(dialogue);
    dialogue->Node = (ThreadBobberId)node;
    dialogue->ProgramCounter = 0;
    dialogue->State = ThreadBobberState_Running;
    dialogue->Error = 0;
    return 1;
}

static int Push(ThreadBobberDialogue *dialogue, ThreadBobberValue value)
{
    if (dialogue->StackCount >= THREADBOBBER_STACK)
    {
        SetError(dialogue, ThreadBobberError_StackOverflow);
        return 0;
    }
    dialogue->Stack[dialogue->StackCount++] = value;
    return 1;
}

static int Pop(ThreadBobberDialogue *dialogue, ThreadBobberValue *value)
{
    if (!dialogue->StackCount ||
        (dialogue->EvaluationCount &&
         dialogue->StackCount <= dialogue->Evaluations[dialogue->EvaluationCount - 1].StackBase))
    {
        SetError(dialogue, ThreadBobberError_StackUnderflow);
        return 0;
    }
    *value = dialogue->Stack[--dialogue->StackCount];
    return 1;
}

static int Peek(ThreadBobberDialogue *dialogue, ThreadBobberValue *value)
{
    if (!dialogue->StackCount ||
        (dialogue->EvaluationCount &&
         dialogue->StackCount <= dialogue->Evaluations[dialogue->EvaluationCount - 1].StackBase))
    {
        SetError(dialogue, ThreadBobberError_StackUnderflow);
        return 0;
    }
    *value = dialogue->Stack[dialogue->StackCount - 1];
    return 1;
}

static float VarNumber(const ThreadBobberDialogue *dialogue, unsigned var)
{
    return BitsToFloat(dialogue->Variables[var]);
}

// Yarn records a visit when execution leaves a node, not when it enters one.
static void LeaveNode(ThreadBobberDialogue *dialogue, unsigned node)
{
    const ThreadBobberNode *n = &dialogue->Program->Nodes[node];
    if (n->TrackingVariable != THREADBOBBER_NONE)
    {
        dialogue->Variables[n->TrackingVariable] =
            FloatToBits(VarNumber(dialogue, n->TrackingVariable) + 1.f);
    }
}

static void Unwind(ThreadBobberDialogue *dialogue)
{
    LeaveNode(dialogue, dialogue->Node);
    while (dialogue->CallStackCount)
    {
        dialogue->CallStackCount--;
        LeaveNode(dialogue, dialogue->CallStack[dialogue->CallStackCount].Node);
    }
}

static int PopSubstitutions(ThreadBobberDialogue *dialogue, unsigned count)
{
    if (count > THREADBOBBER_SUBSTITUTIONS)
    {
        SetError(dialogue, ThreadBobberError_TypeMismatch);
        return 0;
    }
    for (unsigned i = count; i-- > 0;)
    {
        if (!Pop(dialogue, &dialogue->Substitutions[i]))
        {
            return 0;
        }
    }
    dialogue->SubstitutionCount = (uint8_t)count;
    return 1;
}

static unsigned NodeStringId(const ThreadBobberDialogue *dialogue, unsigned node)
{
    return dialogue->Program->Nodes[node].Name;
}

static int NodeByString(const ThreadBobberDialogue *dialogue, unsigned string_id)
{
    const ThreadBobberProgram *program = dialogue->Program;
    for (unsigned i = 0; i < program->NodeCount; i++)
    {
        if (NodeStringId(dialogue, i) == string_id)
        {
            return (int)i;
        }
    }
    return ThreadBobber_FindNode(program, ThreadBobber_GetString(dialogue, string_id));
}

static float Floorf(float x)
{
    return floorf(x);
}

static float Ceilf(float x)
{
    return ceilf(x);
}

static float Truncf(float x)
{
    return truncf(x);
}

static int IsIntegral(float x)
{
    return isfinite(x) && x == truncf(x);
}

// Math.Round in C# rounds ties to even.
static float RoundHalfEven(float x)
{
    if (!isfinite(x))
    {
        return x;
    }
    float f = Floorf(x), diff = x - f;
    if (diff > 0.5f)
    {
        return f + 1.f;
    }
    if (diff < 0.5f)
    {
        return f;
    }
    return fmodf(f, 2.f) != 0.f ? f + 1.f : f;
}

// Convert.ToInt32(float): round half to even, refusing NaN, infinities and values outside
// Int32. Works on the float's bit pattern, so on a core without floating point hardware it
// costs a few integer instructions instead of several soft-float calls.
static int ToInt32(float x, int32_t *result)
{
    uint32_t bits;
    memcpy(&bits, &x, sizeof bits);
    int exponent = (int)((bits >> 23) & 0xFF) - 127;
    uint32_t mantissa = (bits & 0x7FFFFF) | 0x800000, magnitude;
    if (exponent < -1)
    {
        magnitude = 0; // |x| < 0.5, including zero and subnormals
    }
    else if (exponent >= 31)
    {
        if (exponent == 31 && mantissa == 0x800000 && bits >> 31)
        {
            *result = INT32_MIN;
            return 1;
        }
        return 0; // NaN, an infinity, or beyond Int32
    }
    else if (exponent >= 23)
    {
        magnitude = mantissa << (exponent - 23); // already an integer
    }
    else
    {
        unsigned shift = (unsigned)(23 - exponent);
        uint32_t half = 1u << (shift - 1), fraction = mantissa & (2 * half - 1);
        magnitude = mantissa >> shift;
        if (fraction > half || (fraction == half && (magnitude & 1)))
        {
            magnitude++;
        }
    }
    *result = bits >> 31 ? -(int32_t)magnitude : (int32_t)magnitude;
    return 1;
}

static void NumberText(float number, char *buffer, unsigned capacity);

static int CallBuiltin(ThreadBobberDialogue *dialogue, unsigned id, const ThreadBobberValue *a,
                       unsigned parameterCount, ThreadBobberValue *r)
{
    float x = parameterCount > 0 ? ThreadBobber_GetNumber(a[0]) : 0.f,
          y = parameterCount > 1 ? ThreadBobber_GetNumber(a[1]) : 0.f;
    if ((id == ThreadBobberFunction_Random || id == ThreadBobberFunction_RandomRange ||
         id == ThreadBobberFunction_Dice || id == ThreadBobberFunction_RandomRangeFloat) &&
        !dialogue->Library->RandomHandler)
    {
        return 0;
    }
    int bx = parameterCount > 0 ? IsTrue(a[0]) : 0, by = parameterCount > 1 ? IsTrue(a[1]) : 0;
    switch (id)
    {
    case ThreadBobberFunction_RoundPlaces:
    {
        // Yarn converts the places argument to Int32, then calls Math.Round(double, int).
        float roundedPlaces = RoundHalfEven(y);
        if (!isfinite(roundedPlaces) || roundedPlaces < 0 || roundedPlaces > 15)
        {
            return 0;
        }
        static const double powers[] = {1,
                                        10,
                                        100,
                                        1000,
                                        10000,
                                        100000,
                                        1000000,
                                        10000000,
                                        100000000,
                                        1000000000,
                                        10000000000,
                                        100000000000,
                                        1000000000000,
                                        10000000000000,
                                        100000000000000,
                                        1000000000000000};
        double value = x;
        if (isfinite(x) && fabs(value) < 1e16)
        {
            double scale = powers[(unsigned)roundedPlaces];
            double scaled = value * scale;
            double lower = floor(scaled), fraction = scaled - lower;
            if (fraction > 0.5 || (fraction == 0.5 && fmod(lower, 2.0) != 0))
            {
                lower += 1;
            }
            value = lower / scale;
        }
        *r = ThreadBobber_CreateNumber((float)value);
        return 1;
    }
    case ThreadBobberFunction_RandomRangeFloat:
    {
        // Yarn Spinner 3.2.2 samples integer steps starting at the original float minimum.
        if (!isfinite(x) || !isfinite(y) || x < -2147483648.0 || x >= 2147483647.0 ||
            y < -2147483648.0 || y >= 2147483647.0)
        {
            return 0;
        }
        int64_t range = (int64_t)y - (int64_t)x + 1;
        if (range < 0 || range > INT32_MAX)
        {
            return 0;
        }
        *r = ThreadBobber_CreateNumber(
            x +
            (range ? (float)dialogue->Library->RandomHandler(dialogue, (uint32_t)range - 1) : 0));
        return 1;
    }
    case ThreadBobberFunction_ValueEqualTo:
    case ThreadBobberFunction_ValueNotEqualTo:
    {
        if (parameterCount != 2 || a[0].Type != a[1].Type)
        {
            return 0;
        }
        int equal = a[0].Type == ThreadBobberValueType_String
                        ? !strcmp(ThreadBobber_GetString(dialogue, a[0].Bits),
                                  ThreadBobber_GetString(dialogue, a[1].Bits))
                        : x == y;
        *r = ThreadBobber_CreateBoolean(id == ThreadBobberFunction_ValueEqualTo ? equal : !equal);
        return 1;
    }
    case ThreadBobberFunction_HasAnyContent:
    {
        if (parameterCount != 1 || a[0].Type != ThreadBobberValueType_String)
        {
            return 0;
        }
        const char *name = ThreadBobber_GetString(dialogue, a[0].Bits);
        if (ThreadBobber_FindNode(dialogue->Program, name) < 0)
        {
            *r = ThreadBobber_CreateBoolean(0);
            return 1;
        }
        const char prefix[] = "$ThreadBobber.HasAnyContent.";
        for (unsigned i = 0; i < dialogue->Program->NodeCount; i++)
        {
            const char *candidate =
                ThreadBobber_GetString(dialogue, dialogue->Program->Nodes[i].Name);
            if (!strncmp(candidate, prefix, sizeof prefix - 1) &&
                !strcmp(candidate + sizeof prefix - 1, name))
            {
                return ThreadBobber_EvaluateSmartVariable(dialogue, candidate, r);
            }
        }
        *r = ThreadBobber_CreateBoolean(1);
        return 1;
    }
    case ThreadBobberFunction_StringAdd:
    {
        if (parameterCount != 2 || a[0].Type != ThreadBobberValueType_String ||
            a[1].Type != ThreadBobberValueType_String)
        {
            return 0;
        }
        const char *left = ThreadBobber_GetString(dialogue, a[0].Bits);
        const char *right = ThreadBobber_GetString(dialogue, a[1].Bits);
        size_t first = strlen(left), second = strlen(right);
        if (first >= THREADBOBBER_STRING_BYTES || second >= THREADBOBBER_STRING_BYTES - first)
        {
            SetError(dialogue, ThreadBobberError_StringTooLong);
            return 0;
        }
        char text[THREADBOBBER_STRING_BYTES];
        memcpy(text, left, first);
        memcpy(text + first, right, second + 1);
        return ThreadBobber_StoreString(dialogue, text, r);
    }
    case ThreadBobberFunction_ConvertString:
    {
        if (parameterCount != 1)
        {
            return 0;
        }
        if (a[0].Type == ThreadBobberValueType_String)
        {
            *r = a[0];
            return 1;
        }
        if (a[0].Type == ThreadBobberValueType_Boolean)
        {
            return ThreadBobber_StoreString(dialogue, a[0].Bits ? "True" : "False", r);
        }
        char text[24];
        NumberText(x, text, sizeof text);
        return ThreadBobber_StoreString(dialogue, text, r);
    }
    case ThreadBobberFunction_ConvertNumber:
    case ThreadBobberFunction_ConvertBoolean:
    {
        if (parameterCount != 1)
        {
            return 0;
        }
        if (a[0].Type == ThreadBobberValueType_String)
        {
            const char *text = ThreadBobber_GetString(dialogue, a[0].Bits);
            while (isspace((unsigned char)*text))
            {
                text++;
            }
            if (id == ThreadBobberFunction_ConvertBoolean)
            {
                char word[6];
                unsigned i = 0;
                while (*text && !isspace((unsigned char)*text) && i < sizeof word - 1)
                {
                    word[i++] = (char)tolower((unsigned char)*text++);
                }
                word[i] = 0;
                while (isspace((unsigned char)*text))
                {
                    text++;
                }
                if (*text || (strcmp(word, "true") && strcmp(word, "false")))
                {
                    return 0;
                }
                *r = ThreadBobber_CreateBoolean(!strcmp(word, "true"));
                return 1;
            }
            const char *start = (*text == '+' || *text == '-') ? text + 1 : text;
            if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X'))
            {
                return 0;
            }
            char *end;
            x = strtof(text, &end);
            if (end == text)
            {
                return 0;
            }
            while (isspace((unsigned char)*end))
            {
                end++;
            }
            if (*end)
            {
                return 0;
            }
        }
        *r = id == ThreadBobberFunction_ConvertNumber ? ThreadBobber_CreateNumber(x)
                                                      : ThreadBobber_CreateBoolean(x != 0);
        return 1;
    }
    case ThreadBobberFunction_NumberAdd:
        *r = ThreadBobber_CreateNumber(x + y);
        return 1;
    case ThreadBobberFunction_NumberMinus:
        *r = ThreadBobber_CreateNumber(x - y);
        return 1;
    case ThreadBobberFunction_NumberMultiply:
        *r = ThreadBobber_CreateNumber(x * y);
        return 1;
    case ThreadBobberFunction_NumberDivide:
        *r = ThreadBobber_CreateNumber(x / y);
        return 1;
    case ThreadBobberFunction_NumberModulo:
    {
        // Yarn Spinner converts both operands to Int32 before taking the remainder, so
        // 11.5 % 5 is 2. A divisor that rounds to zero stops the dialogue.
        int32_t dividend, divisor;
        if (!ToInt32(x, &dividend) || !ToInt32(y, &divisor) || divisor == 0)
        {
            return 0;
        }
        *r = ThreadBobber_CreateNumber(divisor == -1 ? 0.f : (float)(dividend % divisor));
        return 1;
    }
    case ThreadBobberFunction_NumberUnaryMinus:
        *r = ThreadBobber_CreateNumber(-x);
        return 1;
    case ThreadBobberFunction_NumberEqualTo:
        *r = ThreadBobber_CreateBoolean(x == y);
        return 1;
    case ThreadBobberFunction_NumberNotEqualTo:
        *r = ThreadBobber_CreateBoolean(x != y);
        return 1;
    case ThreadBobberFunction_NumberGreaterThan:
        *r = ThreadBobber_CreateBoolean(x > y);
        return 1;
    case ThreadBobberFunction_NumberLessThan:
        *r = ThreadBobber_CreateBoolean(x < y);
        return 1;
    case ThreadBobberFunction_NumberGreaterThanOrEqualTo:
        *r = ThreadBobber_CreateBoolean(x >= y);
        return 1;
    case ThreadBobberFunction_NumberLessThanOrEqualTo:
        *r = ThreadBobber_CreateBoolean(x <= y);
        return 1;
    case ThreadBobberFunction_BoolAnd:
        *r = ThreadBobber_CreateBoolean(bx && by);
        return 1;
    case ThreadBobberFunction_BoolOr:
        *r = ThreadBobber_CreateBoolean(bx || by);
        return 1;
    case ThreadBobberFunction_BoolXor:
        *r = ThreadBobber_CreateBoolean(bx != by);
        return 1;
    case ThreadBobberFunction_BoolNot:
        *r = ThreadBobber_CreateBoolean(!bx);
        return 1;
    case ThreadBobberFunction_BoolEqualTo:
        *r = ThreadBobber_CreateBoolean(bx == by);
        return 1;
    case ThreadBobberFunction_BoolNotEqualTo:
        *r = ThreadBobber_CreateBoolean(bx != by);
        return 1;
    case ThreadBobberFunction_StringEqualTo:
    case ThreadBobberFunction_StringNotEqualTo:
    {
        if (parameterCount < 2 || a[0].Type != ThreadBobberValueType_String ||
            a[1].Type != ThreadBobberValueType_String)
        {
            return 0;
        }
        int eq = a[0].Bits == a[1].Bits || !strcmp(ThreadBobber_GetString(dialogue, a[0].Bits),
                                                   ThreadBobber_GetString(dialogue, a[1].Bits));
        *r = ThreadBobber_CreateBoolean(id == ThreadBobberFunction_StringEqualTo ? eq : !eq);
        return 1;
    }
    case ThreadBobberFunction_Visited:
    case ThreadBobberFunction_VisitedCount:
    {
        if (parameterCount < 1 || a[0].Type != ThreadBobberValueType_String)
        {
            return 0;
        }
        int node = NodeByString(dialogue, a[0].Bits);
        float count = node >= 0 ? (float)ThreadBobber_GetVisitCount(dialogue, (unsigned)node) : 0.f;
        *r = id == ThreadBobberFunction_Visited ? ThreadBobber_CreateBoolean(count > 0.f)
                                                : ThreadBobber_CreateNumber(count);
        return 1;
    }
    case ThreadBobberFunction_Random:
        *r = ThreadBobber_CreateNumber((float)dialogue->Library->RandomHandler(dialogue, 65535) /
                                       65536.f);
        return 1;
    case ThreadBobberFunction_RandomRange:
    {
        if (!isfinite(x) || !isfinite(y) || x < -2147483648.0 || x >= 2147483647.0 ||
            y < -2147483648.0 || y >= 2147483647.0)
        {
            return 0;
        }
        int64_t lo = (int64_t)x, hi = (int64_t)y;
        if (hi < lo)
        {
            int64_t t = lo;
            lo = hi;
            hi = t;
        }
        *r = ThreadBobber_CreateNumber(
            (float)(lo + (int64_t)dialogue->Library->RandomHandler(dialogue, (uint32_t)(hi - lo))));
        return 1;
    }
    case ThreadBobberFunction_Dice:
    {
        if (!isfinite(x) || x < -2147483648.0 || x >= 2147483647.0)
        {
            return 0;
        }
        long sides = (long)x;
        if (sides < 1)
        {
            sides = 1;
        }
        *r = ThreadBobber_CreateNumber(
            1.f + (float)dialogue->Library->RandomHandler(dialogue, (uint32_t)(sides - 1)));
        return 1;
    }
    case ThreadBobberFunction_Min:
        *r = ThreadBobber_CreateNumber(x < y ? x : y);
        return 1;
    case ThreadBobberFunction_Max:
        *r = ThreadBobber_CreateNumber(x > y ? x : y);
        return 1;
    case ThreadBobberFunction_Round:
        *r = ThreadBobber_CreateNumber(RoundHalfEven(x));
        return 1;
    case ThreadBobberFunction_Floor:
        *r = ThreadBobber_CreateNumber(Floorf(x));
        return 1;
    case ThreadBobberFunction_Ceil:
        *r = ThreadBobber_CreateNumber(Ceilf(x));
        return 1;
    case ThreadBobberFunction_Inc:
        *r = ThreadBobber_CreateNumber(IsIntegral(x) ? x + 1.f : Ceilf(x));
        return 1;
    case ThreadBobberFunction_Dec:
        *r = ThreadBobber_CreateNumber(IsIntegral(x) ? x - 1.f : Floorf(x));
        return 1;
    case ThreadBobberFunction_Int:
        *r = ThreadBobber_CreateNumber(Truncf(x));
        return 1;
    case ThreadBobberFunction_Decimal:
        *r = ThreadBobber_CreateNumber(x - Truncf(x));
        return 1;
    default:
        return 0;
    }
}

static int CallFunction(ThreadBobberDialogue *dialogue, unsigned functionIndex)
{
    const ThreadBobberProgram *program = dialogue->Program;
    if (functionIndex >= program->FunctionCount)
    {
        SetError(dialogue, ThreadBobberError_InvalidFunction);
        return 0;
    }
    const ThreadBobberFunction *f = &program->Functions[functionIndex];
    ThreadBobberValue count;
    if (!Pop(dialogue, &count))
    {
        return 0;
    }
    float argumentCount = ThreadBobber_GetNumber(count);
    if (!isfinite(argumentCount) || argumentCount < 0 ||
        argumentCount > THREADBOBBER_COMMAND_ARGS || argumentCount != truncf(argumentCount))
    {
        SetError(dialogue, ThreadBobberError_TypeMismatch);
        return 0;
    }
    unsigned parameterCount = (unsigned)argumentCount;
    if (count.Type != ThreadBobberValueType_Number || parameterCount != f->ParameterCount ||
        parameterCount > THREADBOBBER_COMMAND_ARGS)
    {
        SetError(dialogue, ThreadBobberError_TypeMismatch);
        return 0;
    }
    ThreadBobberValue args[THREADBOBBER_COMMAND_ARGS] = {{0, 0}};
    for (unsigned i = parameterCount; i-- > 0;)
    {
        if (!Pop(dialogue, &args[i]))
        {
            return 0;
        }
    }
    uint32_t pinned = dialogue->Strings ? dialogue->Strings->Pinned : 0;
    if (dialogue->Strings)
    {
        for (unsigned i = 0; i < parameterCount; i++)
        {
            dialogue->Strings->Pinned |= StringMask(args[i]);
        }
    }
    ThreadBobberValue result = {ThreadBobberValueType_None, 0};
    int ok;
    if (f->Builtin)
    {
        ok = CallBuiltin(dialogue, f->Builtin, args, parameterCount, &result);
    }
    else
    {
        ok = dialogue->Library->FunctionHandler
                 ? dialogue->Library->FunctionHandler(dialogue, f->FunctionID, args, parameterCount,
                                                      &result)
                 : 0;
    }
    if (dialogue->Strings)
    {
        dialogue->Strings->Pinned = pinned;
    }
    if (!ok)
    {
        SetError(dialogue, ThreadBobberError_HostError);
        return 0;
    }
    if (f->ReturnType != ThreadBobberValueType_None)
    {
        if (result.Type != f->ReturnType ||
            (result.Type == ThreadBobberValueType_String &&
             !IsValidString(dialogue, result.Bits)) ||
            (result.Type == ThreadBobberValueType_Boolean && result.Bits > 1))
        {
            SetError(dialogue, ThreadBobberError_TypeMismatch);
            return 0;
        }
        return Push(dialogue, result);
    }
    return 1;
}

static int EnterNode(ThreadBobberDialogue *dialogue, unsigned node)
{
    if (node >= dialogue->Program->NodeCount)
    {
        SetError(dialogue, ThreadBobberError_InvalidNode);
        return 0;
    }
    dialogue->Node = (ThreadBobberId)node;
    dialogue->ProgramCounter = 0;
    return 1;
}

static int BeginEvaluation(ThreadBobberDialogue *dialogue, unsigned node, unsigned next)
{
    if (node >= dialogue->Program->NodeCount || !dialogue->Program->Nodes[node].IsSmartVariable)
    {
        SetError(dialogue, ThreadBobberError_InvalidVariable);
        return 0;
    }
    if (dialogue->EvaluationCount >= THREADBOBBER_EVALUATIONS)
    {
        SetError(dialogue, ThreadBobberError_EvaluationOverflow);
        return 0;
    }
    ThreadBobberEvaluation *frame = &dialogue->Evaluations[dialogue->EvaluationCount++];
    frame->Node = dialogue->Node;
    frame->ProgramCounter = (ThreadBobberId)next;
    frame->StackBase = dialogue->StackCount;
    return EnterNode(dialogue, node);
}

static void EndEvaluation(ThreadBobberDialogue *dialogue)
{
    const ThreadBobberEvaluation *frame = &dialogue->Evaluations[dialogue->EvaluationCount - 1];
    if (dialogue->StackCount != frame->StackBase + 1)
    {
        SetError(dialogue, ThreadBobberError_TypeMismatch);
        return;
    }
    dialogue->Node = frame->Node;
    dialogue->ProgramCounter = frame->ProgramCounter;
    dialogue->EvaluationCount--;
}

static const uint8_t InstructionSizes[ThreadBobberOpcode_Count] = {
    0, 3, 1, 5, 3, 7, 1, 3, 5, 2, 3, 1, 3, 3, 3, 1, 3, 1, 3, 1, 1, 3, 7, 1};

static void RunInstruction(ThreadBobberDialogue *dialogue)
{
    const ThreadBobberProgram *program = dialogue->Program;
    const ThreadBobberNode *n = &program->Nodes[dialogue->Node];
    const uint8_t *code = program->Code + n->Start;
    unsigned pc = dialogue->ProgramCounter, len = n->Length;
    if (pc > len)
    {
        SetError(dialogue, ThreadBobberError_InvalidProgramCounter);
        return;
    }
    if (pc == len)
    {
        // Running off the end of a node behaves like Return.
        if (dialogue->EvaluationCount)
        {
            EndEvaluation(dialogue);
            return;
        }
        LeaveNode(dialogue, dialogue->Node);
        if (!dialogue->CallStackCount)
        {
            dialogue->State = ThreadBobberState_Complete;
            return;
        }
        dialogue->CallStackCount--;
        EnterNode(dialogue, dialogue->CallStack[dialogue->CallStackCount].Node);
        dialogue->ProgramCounter = dialogue->CallStack[dialogue->CallStackCount].ProgramCounter;
        return;
    }
    unsigned op = code[pc];
    if (op == 0 || op >= ThreadBobberOpcode_Count)
    {
        SetError(dialogue, ThreadBobberError_InvalidOpcode);
        return;
    }
    if (dialogue->EvaluationCount && op != ThreadBobberOpcode_PushString &&
        op != ThreadBobberOpcode_PushFloat && op != ThreadBobberOpcode_PushBool &&
        op != ThreadBobberOpcode_Pop && op != ThreadBobberOpcode_CallFunc &&
        op != ThreadBobberOpcode_PushVariable && op != ThreadBobberOpcode_EvaluateVariable &&
        op != ThreadBobberOpcode_JumpIfFalse && op != ThreadBobberOpcode_Stop)
    {
        SetError(dialogue, ThreadBobberError_InvalidOpcode);
        return;
    }
    unsigned width = InstructionSizes[op];
    if (pc + width > len)
    {
        SetError(dialogue, ThreadBobberError_InvalidProgramCounter);
        return;
    }
    unsigned a = width >= 3   ? (unsigned)code[pc + 1] | ((unsigned)code[pc + 2] << 8)
                 : width == 2 ? code[pc + 1]
                              : 0;
    unsigned b = width >= 5 ? (unsigned)code[pc + 3] | ((unsigned)code[pc + 4] << 8) : 0;
    unsigned c = width >= 7 ? (unsigned)code[pc + 5] | ((unsigned)code[pc + 6] << 8) : 0;
    unsigned next = pc + width;
    ThreadBobberValue value;
    switch (op)
    {
    // Jump to an instruction offset within the current node.
    case ThreadBobberOpcode_JumpTo:
        next = a;
        break;
    // Jump to the offset on top of the stack, leaving it in place.
    case ThreadBobberOpcode_PeekAndJump:
    {
        if (!Peek(dialogue, &value))
        {
            return;
        }
        if (value.Type != ThreadBobberValueType_Number)
        {
            SetError(dialogue, ThreadBobberError_TypeMismatch);
            return;
        }
        float destination = ThreadBobber_GetNumber(value);
        if (!isfinite(destination) || destination < 0 || destination > len ||
            destination != truncf(destination))
        {
            SetError(dialogue, ThreadBobberError_InvalidProgramCounter);
            return;
        }
        next = (unsigned)destination;
        break;
    }
    // Deliver a line to the game and wait for Continue.
    case ThreadBobberOpcode_RunLine:
        if (a >= program->LineCount)
        {
            SetError(dialogue, ThreadBobberError_InvalidProgram);
            return;
        }
        if (!PopSubstitutions(dialogue, b))
        {
            return;
        }
        dialogue->Line = (ThreadBobberId)a;
        dialogue->ProgramCounter = (ThreadBobberId)next;
        dialogue->State = ThreadBobberState_WaitingForLine;
        return;
    // Deliver a command. Text commands wait for the game; declared commands call the handler.
    case ThreadBobberOpcode_RunCommand:
    {
        if (a >= program->CommandCount)
        {
            SetError(dialogue, ThreadBobberError_InvalidProgram);
            return;
        }
        const ThreadBobberCommand *cmd = &program->Commands[a];
        dialogue->Command = (ThreadBobberId)a;
        dialogue->ProgramCounter = (ThreadBobberId)next;
        if (cmd->IsText)
        {
            if (!PopSubstitutions(dialogue, cmd->SubstitutionCount))
            {
                return;
            }
            dialogue->State = ThreadBobberState_WaitingForCommand;
            return;
        }
        int r = dialogue->Library->CommandHandler
                    ? dialogue->Library->CommandHandler(dialogue, cmd->FunctionID, cmd->Parameters,
                                                        cmd->ParameterCount)
                    : -1;
        if (dialogue->State == ThreadBobberState_Error)
        {
            return;
        }
        if (r < 0)
        {
            SetError(dialogue, ThreadBobberError_HostError);
            return;
        }
        if (r > 0)
        {
            dialogue->State = ThreadBobberState_WaitingForCommand;
        }
        else
        {
            dialogue->Command = THREADBOBBER_NONE;
        }
        return;
    }
    // Add an option to the pending set. Its substitutions and condition come from the stack.
    case ThreadBobberOpcode_AddOption:
    {
        if (a >= program->LineCount)
        {
            SetError(dialogue, ThreadBobberError_InvalidProgram);
            return;
        }
        if (dialogue->OptionCount >= THREADBOBBER_OPTIONS)
        {
            SetError(dialogue, ThreadBobberError_TooManyOptions);
            return;
        }
        if (!PopSubstitutions(dialogue, c & 0xFF))
        {
            return;
        }
        int enabled = 1;
        if (c & 0x100)
        {
            if (!Pop(dialogue, &value))
            {
                return;
            }
            enabled = IsTrue(value);
        }
        ThreadBobberOption *o = &dialogue->Options[dialogue->OptionCount++];
        o->Line = (ThreadBobberId)a;
        o->Destination = (ThreadBobberId)b;
        o->IsAvailable = (uint8_t)enabled;
        o->SubstitutionCount = dialogue->SubstitutionCount;
        memcpy(o->Substitutions, dialogue->Substitutions,
               o->SubstitutionCount * sizeof(ThreadBobberValue));
        dialogue->SubstitutionCount = 0;
        break;
    }
    // Deliver the pending options. An empty set ends the dialogue, as it does in Yarn.
    case ThreadBobberOpcode_ShowOptions:
        dialogue->ProgramCounter = (ThreadBobberId)next;
        if (!dialogue->OptionCount)
        {
            Unwind(dialogue);
            dialogue->State = ThreadBobberState_Complete;
            return;
        }
        dialogue->State = ThreadBobberState_WaitingForOptions;
        return;
    // Push a string from the program string table.
    case ThreadBobberOpcode_PushString:
        if (a >= program->StringCount)
        {
            SetError(dialogue, ThreadBobberError_InvalidProgram);
            return;
        }
        if (!Push(dialogue, ThreadBobber_CreateString(a)))
        {
            return;
        }
        break;
    // Push a number. The operand holds the float bits directly.
    case ThreadBobberOpcode_PushFloat:
    {
        uint32_t bits = (uint32_t)code[pc + 1] | ((uint32_t)code[pc + 2] << 8) |
                        ((uint32_t)code[pc + 3] << 16) | ((uint32_t)code[pc + 4] << 24);
        ThreadBobberValue f = {ThreadBobberValueType_Number, bits};
        if (!Push(dialogue, f))
        {
            return;
        }
        break;
    }
    // Push a boolean.
    case ThreadBobberOpcode_PushBool:
        if (!Push(dialogue, ThreadBobber_CreateBoolean(a)))
        {
            return;
        }
        break;
    // Jump if the value on top of the stack is false, leaving it in place.
    case ThreadBobberOpcode_JumpIfFalse:
        if (!Peek(dialogue, &value))
        {
            return;
        }
        if (!IsTrue(value))
        {
            next = a;
        }
        break;
    // Discard the value on top of the stack.
    case ThreadBobberOpcode_Pop:
        if (!Pop(dialogue, &value))
        {
            return;
        }
        break;
    // Call a runtime or game function with arguments from the stack.
    case ThreadBobberOpcode_CallFunc:
        if (!CallFunction(dialogue, a))
        {
            return;
        }
        break;
    // Push a variable's current value.
    case ThreadBobberOpcode_PushVariable:
    {
        if (a >= program->VariableCount)
        {
            SetError(dialogue, ThreadBobberError_InvalidVariable);
            return;
        }
        ThreadBobberValue x = {program->Variables[a].Type, dialogue->Variables[a]};
        if (!Push(dialogue, x))
        {
            return;
        }
        break;
    }
    // Store the value on top of the stack into a variable, leaving it in place.
    case ThreadBobberOpcode_StoreVariable:
        if (a >= program->VariableCount)
        {
            SetError(dialogue, ThreadBobberError_InvalidVariable);
            return;
        }
        if (!Peek(dialogue, &value))
        {
            return;
        }
        if (value.Type != program->Variables[a].Type)
        {
            SetError(dialogue, ThreadBobberError_TypeMismatch);
            return;
        }
        if (value.Type == ThreadBobberValueType_String && !IsValidString(dialogue, value.Bits))
        {
            SetError(dialogue, ThreadBobberError_InvalidProgram);
            return;
        }
        dialogue->Variables[a] = value.Bits;
        break;
    // Record a node-group member or line-group item whose condition passed.
    case ThreadBobberOpcode_AddSaliencyCandidate:
        if (a >= program->VariableCount ||
            program->Variables[a].Type != ThreadBobberValueType_Number || b > len ||
            !Pop(dialogue, &value))
        {
            SetError(dialogue, ThreadBobberError_InvalidProgram);
            return;
        }
        if (IsTrue(value))
        {
            if (dialogue->CandidateCount >= THREADBOBBER_CANDIDATES)
            {
                SetError(dialogue, ThreadBobberError_TooManyCandidates);
                return;
            }
            ThreadBobberSaliencyCandidate *candidate =
                &dialogue->Candidates[dialogue->CandidateCount++];
            candidate->Variable = (ThreadBobberId)a;
            candidate->Destination = (ThreadBobberId)b;
            candidate->Complexity = (uint16_t)c;
        }
        break;
    // Choose the least-viewed candidate, then the most complex, then a random one.
    case ThreadBobberOpcode_SelectSaliencyCandidate:
    {
        if (!dialogue->CandidateCount)
        {
            if (!Push(dialogue, ThreadBobber_CreateBoolean(0)))
            {
                return;
            }
            break;
        }
        float leastViews = INFINITY;
        unsigned complexity = 0, ties = 0;
        for (unsigned i = 0; i < dialogue->CandidateCount; i++)
        {
            const ThreadBobberSaliencyCandidate *candidate = &dialogue->Candidates[i];
            float views = VarNumber(dialogue, candidate->Variable);
            if (views < leastViews || (views == leastViews && candidate->Complexity > complexity))
            {
                leastViews = views;
                complexity = candidate->Complexity;
                ties = 1;
            }
            else if (views == leastViews && candidate->Complexity == complexity)
            {
                ties++;
            }
        }
        if (!ties || (ties > 1 && !dialogue->Library->RandomHandler))
        {
            SetError(dialogue, ThreadBobberError_HostError);
            return;
        }
        unsigned selected = ties == 1 ? 0 : dialogue->Library->RandomHandler(dialogue, ties - 1);
        if (selected >= ties)
        {
            SetError(dialogue, ThreadBobberError_HostError);
            return;
        }
        for (unsigned i = 0; i < dialogue->CandidateCount; i++)
        {
            const ThreadBobberSaliencyCandidate *candidate = &dialogue->Candidates[i];
            if (VarNumber(dialogue, candidate->Variable) != leastViews ||
                candidate->Complexity != complexity)
            {
                continue;
            }
            if (selected--)
            {
                continue;
            }
            if (!Push(dialogue, ThreadBobber_CreateNumber((float)candidate->Destination)) ||
                !Push(dialogue, ThreadBobber_CreateBoolean(1)))
            {
                return;
            }
            dialogue->Variables[candidate->Variable] = FloatToBits(leastViews + 1);
            break;
        }
        dialogue->CandidateCount = 0;
        break;
    }
    // Evaluate a smart variable in its own expression node, then resume here.
    case ThreadBobberOpcode_EvaluateVariable:
        BeginEvaluation(dialogue, a, next);
        return;
    // End the dialogue, or finish the current smart-variable evaluation.
    case ThreadBobberOpcode_Stop:
        if (dialogue->EvaluationCount)
        {
            EndEvaluation(dialogue);
            return;
        }
        Unwind(dialogue);
        dialogue->State = ThreadBobberState_Complete;
        return;
    // Jump to another node. The call stack is unchanged.
    case ThreadBobberOpcode_RunNode:
    case ThreadBobberOpcode_PeekAndRunNode:
    {
        unsigned target = a;
        if (op == ThreadBobberOpcode_PeekAndRunNode)
        {
            if (!Peek(dialogue, &value))
            {
                return;
            }
            if (value.Type != ThreadBobberValueType_String)
            {
                SetError(dialogue, ThreadBobberError_TypeMismatch);
                return;
            }
            int found = NodeByString(dialogue, value.Bits);
            if (found < 0)
            {
                SetError(dialogue, ThreadBobberError_InvalidNode);
                return;
            }
            target = (unsigned)found;
        }
        Unwind(dialogue);
        ResetExecution(dialogue);
        EnterNode(dialogue, target);
        return;
    }
    // Run another node, then return to the instruction after this one.
    case ThreadBobberOpcode_DetourToNode:
    case ThreadBobberOpcode_PeekAndDetourToNode:
    {
        unsigned target = a;
        if (op == ThreadBobberOpcode_PeekAndDetourToNode)
        {
            if (!Peek(dialogue, &value))
            {
                return;
            }
            if (value.Type != ThreadBobberValueType_String)
            {
                SetError(dialogue, ThreadBobberError_TypeMismatch);
                return;
            }
            int found = NodeByString(dialogue, value.Bits);
            if (found < 0)
            {
                SetError(dialogue, ThreadBobberError_InvalidNode);
                return;
            }
            target = (unsigned)found;
        }
        if (dialogue->CallStackCount >= THREADBOBBER_CALLS)
        {
            SetError(dialogue, ThreadBobberError_CallStackOverflow);
            return;
        }
        dialogue->CallStack[dialogue->CallStackCount].Node = dialogue->Node;
        dialogue->CallStack[dialogue->CallStackCount].ProgramCounter = (ThreadBobberId)next;
        dialogue->CallStackCount++;
        EnterNode(dialogue, target);
        return;
    }
    // Return from a detour, or end the dialogue if there is nothing to return to.
    case ThreadBobberOpcode_Return:
        LeaveNode(dialogue, dialogue->Node);
        if (!dialogue->CallStackCount)
        {
            dialogue->State = ThreadBobberState_Complete;
            return;
        }
        dialogue->CallStackCount--;
        EnterNode(dialogue, dialogue->CallStack[dialogue->CallStackCount].Node);
        dialogue->ProgramCounter = dialogue->CallStack[dialogue->CallStackCount].ProgramCounter;
        return;
    default:
        SetError(dialogue, ThreadBobberError_InvalidOpcode);
        return;
    }
    if (next > len)
    {
        SetError(dialogue, ThreadBobberError_InvalidProgramCounter);
        return;
    }
    dialogue->ProgramCounter = (ThreadBobberId)next;
}

static void RunBudgetedInstruction(ThreadBobberDialogue *dialogue)
{
    if (!dialogue->RemainingInstructions)
    {
        SetError(dialogue, ThreadBobberError_InstructionLimit);
        return;
    }
    dialogue->RemainingInstructions--;
    dialogue->InstructionCount++;
    uint8_t active = dialogue->ExecutionActive;
    dialogue->ExecutionActive = 1;
    RunInstruction(dialogue);
    dialogue->ExecutionActive = active;
}

int ThreadBobber_EvaluateSmartVariable(ThreadBobberDialogue *dialogue, const char *name,
                                       ThreadBobberValue *result)
{
    if (!dialogue || !dialogue->Program || !result || dialogue->State == ThreadBobberState_Error)
    {
        return 0;
    }
    int node = ThreadBobber_FindNode(dialogue->Program, name);
    if (node < 0 || !dialogue->Program->Nodes[node].IsSmartVariable)
    {
        return 0;
    }
    uint8_t state = dialogue->State, depth = dialogue->EvaluationCount;
    if (!dialogue->ExecutionActive)
    {
        dialogue->RemainingInstructions = THREADBOBBER_STEP_BUDGET;
    }
    if (!BeginEvaluation(dialogue, (unsigned)node, dialogue->ProgramCounter))
    {
        return 0;
    }
    dialogue->State = ThreadBobberState_Running;
    while (dialogue->EvaluationCount > depth && dialogue->State == ThreadBobberState_Running)
    {
        RunBudgetedInstruction(dialogue);
    }
    if (dialogue->State == ThreadBobberState_Error || !Pop(dialogue, result))
    {
        return 0;
    }
    dialogue->State = state;
    return 1;
}

ThreadBobberState ThreadBobber_Run(ThreadBobberDialogue *dialogue)
{
    if (dialogue->State != ThreadBobberState_Running)
    {
        return (ThreadBobberState)dialogue->State;
    }
    dialogue->RemainingInstructions = THREADBOBBER_STEP_BUDGET;
    while (dialogue->State == ThreadBobberState_Running)
    {
        RunBudgetedInstruction(dialogue);
    }
    return (ThreadBobberState)dialogue->State;
}

int ThreadBobber_Continue(ThreadBobberDialogue *dialogue)
{
    if (dialogue->State != ThreadBobberState_WaitingForLine &&
        dialogue->State != ThreadBobberState_WaitingForCommand)
    {
        SetError(dialogue, ThreadBobberError_InvalidState);
        return 0;
    }
    dialogue->State = ThreadBobberState_Running;
    dialogue->Line = dialogue->Command = THREADBOBBER_NONE;
    dialogue->SubstitutionCount = 0;
    return 1;
}

ThreadBobberState ThreadBobber_RunFor(ThreadBobberDialogue *dialogue, unsigned instructionCount)
{
    if (dialogue->State != ThreadBobberState_Running || !instructionCount)
    {
        return (ThreadBobberState)dialogue->State;
    }
    unsigned start = dialogue->InstructionCount;
    if (instructionCount > THREADBOBBER_STEP_BUDGET)
    {
        instructionCount = THREADBOBBER_STEP_BUDGET;
    }
    dialogue->RemainingInstructions = THREADBOBBER_STEP_BUDGET;
    while (dialogue->State == ThreadBobberState_Running &&
           dialogue->InstructionCount - start < instructionCount)
    {
        RunBudgetedInstruction(dialogue);
    }
    return (ThreadBobberState)dialogue->State;
}

int ThreadBobber_SetSelectedOption(ThreadBobberDialogue *dialogue, unsigned option)
{
    if (dialogue->State != ThreadBobberState_WaitingForOptions)
    {
        SetError(dialogue, ThreadBobberError_InvalidState);
        return 0;
    }
    if (option >= dialogue->OptionCount || !dialogue->Options[option].IsAvailable)
    {
        return 0;
    }
    unsigned destination = dialogue->Options[option].Destination;
    dialogue->OptionCount = 0;
    if (!Push(dialogue, ThreadBobber_CreateNumber((float)destination)) ||
        !Push(dialogue, ThreadBobber_CreateBoolean(1)))
    {
        return 0;
    }
    dialogue->State = ThreadBobberState_Running;
    return 1;
}

static unsigned AppendText(char *output, unsigned capacity, unsigned n, const char *s)
{
    while (*s)
    {
        if (output && n + 1 < capacity)
        {
            output[n] = *s;
        }
        n++;
        s++;
    }
    return n;
}

// Common short decimal values avoid the C library's general-purpose formatter.
// Verify the float round trip before choosing this representation.
static int ShortNumberText(float number, char *buffer, unsigned capacity)
{
    if (!isfinite(number))
    {
        return 0;
    }
    float magnitude = fabsf(number);
    uint32_t scale = 1;
    for (unsigned places = 0; places <= 4; places++, scale *= 10)
    {
        float scaled = magnitude * (float)scale;
        if (scaled >= 1e9f)
        {
            break;
        }
        uint32_t integer = (uint32_t)scaled;
        if ((float)integer != scaled || (float)integer / (float)scale != magnitude)
        {
            continue;
        }
        char reversed[16];
        unsigned digits = 0;
        do
        {
            reversed[digits++] = (char)('0' + integer % 10);
            integer /= 10;
        } while (integer || digits <= places);
        unsigned length = digits + (places ? 1 : 0) + (signbit(number) ? 1 : 0);
        if (length >= capacity)
        {
            return 0;
        }
        unsigned offset = 0;
        if (signbit(number))
        {
            buffer[offset++] = '-';
        }
        for (unsigned i = digits; i-- > 0;)
        {
            buffer[offset++] = reversed[i];
            if (places && i == places)
            {
                buffer[offset++] = '.';
            }
        }
        buffer[offset] = 0;
        return 1;
    }
    return 0;
}

// The fallback uses nine significant digits to preserve a single-precision value.
// Games that need locale-specific formatting should present substitutions themselves.
static void NumberText(float number, char *buffer, unsigned capacity)
{
    if (capacity == 0)
    {
        return;
    }
    if (ShortNumberText(number, buffer, capacity))
    {
        return;
    }
    const char *special = isnan(number)   ? "NaN"
                          : isinf(number) ? (number < 0 ? "-Infinity" : "Infinity")
                                          : NULL;
    if (special)
    {
        snprintf(buffer, capacity, "%s", special);
    }
    else
    {
        snprintf(buffer, capacity, "%.9g", (double)number);
    }
}

static unsigned ExpandText(const ThreadBobberDialogue *dialogue, const char *text,
                           const ThreadBobberValue *substitutions, unsigned count, char *output,
                           unsigned capacity, int command)
{
    if (!substitutions)
    {
        count = 0;
    }
    const char *s = text;
    unsigned n = 0;
    while (*s)
    {
        if (*s == '{' && s[1] >= '0' && s[1] <= '9' && s[2] == '}')
        {
            unsigned i = (unsigned)(s[1] - '0');
            // Commands replace the last occurrence of each marker, as Yarn does.
            if (command)
            {
                char marker[4] = {'{', s[1], '}', 0};
                if (strstr(s + 3, marker))
                {
                    if (output && n + 1 < capacity)
                    {
                        output[n] = *s;
                    }
                    n++;
                    s++;
                    continue;
                }
            }
            s += 3;
            char buf[24];
            if (i >= count)
            {
                n = AppendText(output, capacity, n, "?");
                continue;
            }
            if (substitutions[i].Type == ThreadBobberValueType_Number)
            {
                NumberText(ThreadBobber_GetNumber(substitutions[i]), buf, sizeof buf);
                n = AppendText(output, capacity, n, buf);
            }
            else if (substitutions[i].Type == ThreadBobberValueType_Boolean)
            {
                n = AppendText(output, capacity, n, substitutions[i].Bits ? "True" : "False");
            }
            else
            {
                n = AppendText(output, capacity, n,
                               ThreadBobber_GetString(dialogue, substitutions[i].Bits));
            }
            continue;
        }
        if (output && n + 1 < capacity)
        {
            output[n] = *s;
        }
        n++;
        s++;
    }
    if (output && capacity)
    {
        output[n < capacity ? n : capacity - 1] = 0;
    }
    return n;
}

unsigned ThreadBobber_ExpandSubstitutions(const ThreadBobberDialogue *dialogue, unsigned line,
                                          const ThreadBobberValue *substitutions, unsigned count,
                                          char *output, unsigned capacity)
{
    if (!output || !capacity)
    {
        return 0;
    }
    unsigned length = ExpandText(dialogue, ThreadBobber_GetLineText(dialogue, line), substitutions,
                                 count, output, capacity, 0);
    return length < capacity ? length : capacity - 1;
}

unsigned ThreadBobber_GetCurrentCommandText(const ThreadBobberDialogue *dialogue, char *output,
                                            unsigned capacity)
{
    if (!dialogue->Program || dialogue->Command >= dialogue->Program->CommandCount)
    {
        if (output && capacity)
        {
            output[0] = 0;
        }
        return 0;
    }
    const ThreadBobberCommand *command = &dialogue->Program->Commands[dialogue->Command];
    return ExpandText(dialogue, ThreadBobber_GetString(dialogue, command->Text),
                      dialogue->Substitutions, dialogue->SubstitutionCount, output, capacity, 1);
}

unsigned ThreadBobber_GetCurrentLineText(const ThreadBobberDialogue *dialogue, char *output,
                                         unsigned capacity)
{
    if (dialogue->Line == THREADBOBBER_NONE)
    {
        if (capacity)
        {
            output[0] = 0;
        }
        return 0;
    }
    return ThreadBobber_ExpandSubstitutions(dialogue, dialogue->Line, dialogue->Substitutions,
                                            dialogue->SubstitutionCount, output, capacity);
}

unsigned ThreadBobber_GetOptionText(const ThreadBobberDialogue *dialogue, unsigned option,
                                    char *output, unsigned capacity)
{
    if (option >= dialogue->OptionCount)
    {
        if (capacity)
        {
            output[0] = 0;
        }
        return 0;
    }
    const ThreadBobberOption *selected = &dialogue->Options[option];
    return ThreadBobber_ExpandSubstitutions(dialogue, selected->Line, selected->Substitutions,
                                            selected->SubstitutionCount, output, capacity);
}

int ThreadBobber_GetValue(const ThreadBobberDialogue *dialogue, unsigned var,
                          ThreadBobberValue *output)
{
    if (!dialogue->Program || !output || var >= dialogue->Program->VariableCount)
    {
        return 0;
    }
    output->Type = dialogue->Program->Variables[var].Type;
    output->Bits = dialogue->Variables[var];
    return 1;
}

int ThreadBobber_SetValue(ThreadBobberDialogue *dialogue, unsigned var, ThreadBobberValue value)
{
    if (!dialogue->Program || var >= dialogue->Program->VariableCount ||
        value.Type != dialogue->Program->Variables[var].Type)
    {
        return 0;
    }
    if (value.Type == ThreadBobberValueType_String && !IsValidString(dialogue, value.Bits))
    {
        return 0;
    }
    dialogue->Variables[var] =
        value.Type == ThreadBobberValueType_Boolean ? (value.Bits ? 1u : 0u) : value.Bits;
    return 1;
}

unsigned ThreadBobber_GetVisitCount(const ThreadBobberDialogue *dialogue, unsigned node)
{
    if (!dialogue->Program || node >= dialogue->Program->NodeCount)
    {
        return 0;
    }
    unsigned t = dialogue->Program->Nodes[node].TrackingVariable;
    if (t == THREADBOBBER_NONE)
    {
        return 0;
    }
    float f = VarNumber(dialogue, t);
    return f >= (double)UINT_MAX ? UINT_MAX : f > 0.f ? (unsigned)f : 0u;
}

static void PutId(unsigned char *o, ThreadBobberId value)
{
    for (unsigned k = 0; k < sizeof(ThreadBobberId); k++)
    {
        o[k] = (unsigned char)(value >> (8 * k));
    }
}

static ThreadBobberId GetId(const unsigned char *i)
{
    ThreadBobberId value = 0;
    for (unsigned k = 0; k < sizeof(ThreadBobberId); k++)
    {
        value |= (ThreadBobberId)i[k] << (8 * k);
    }
    return value;
}

static void WriteUInt32(unsigned char *o, uint32_t value)
{
    for (unsigned i = 0; i < 4; i++)
    {
        o[i] = (unsigned char)(value >> (8 * i));
    }
}

static uint32_t ReadUInt32(const unsigned char *i)
{
    return i[0] | ((uint32_t)i[1] << 8) | ((uint32_t)i[2] << 16) | ((uint32_t)i[3] << 24);
}

unsigned ThreadBobber_SaveStrings(const ThreadBobberDialogue *dialogue, unsigned char *output,
                                  unsigned capacity)
{
    if (!dialogue || !dialogue->Strings || !output || capacity < THREADBOBBER_STRING_SNAPSHOT_MAX ||
        dialogue->State == ThreadBobberState_Running || dialogue->Strings->Pinned)
    {
        return 0;
    }
    WriteUInt32(output, UINT32_C(0x53544231));
    WriteUInt32(output + 4, dialogue->Strings->Allocated);
    memset(output + 8, 0, THREADBOBBER_STRING_SNAPSHOT_MAX - 8);
    for (unsigned i = 0; i < THREADBOBBER_STRING_SLOTS; i++)
    {
        if (dialogue->Strings->Allocated & (UINT32_C(1) << i))
        {
            memcpy(output + 8 + i * THREADBOBBER_STRING_BYTES, dialogue->Strings->Entries[i],
                   strlen(dialogue->Strings->Entries[i]) + 1);
        }
    }
    return THREADBOBBER_STRING_SNAPSHOT_MAX;
}

int ThreadBobber_RestoreStrings(ThreadBobberDialogue *dialogue, const unsigned char *input,
                                unsigned length)
{
    if (!dialogue || !dialogue->Strings || !input || length != THREADBOBBER_STRING_SNAPSHOT_MAX ||
        dialogue->State == ThreadBobberState_Running || ReadUInt32(input) != UINT32_C(0x53544231))
    {
        return 0;
    }
    uint32_t allocated = ReadUInt32(input + 4);
    if ((allocated >> (THREADBOBBER_STRING_SLOTS - 1)) > 1)
    {
        return 0;
    }
    for (unsigned i = 0; i < THREADBOBBER_STRING_SLOTS; i++)
    {
        if ((allocated & (UINT32_C(1) << i)) &&
            !memchr(input + 8 + i * THREADBOBBER_STRING_BYTES, 0, THREADBOBBER_STRING_BYTES))
        {
            return 0;
        }
    }
    memcpy(dialogue->Strings->Entries, input + 8, THREADBOBBER_STRING_SNAPSHOT_MAX - 8);
    dialogue->Strings->Allocated = allocated;
    dialogue->Strings->Pinned = 0;
    return 1;
}

unsigned ThreadBobber_SaveState(const ThreadBobberDialogue *dialogue, unsigned char *output,
                                unsigned capacity)
{
    enum
    {
        IdSize = (int)sizeof(ThreadBobberId)
    };

    unsigned n = THREADBOBBER_SNAPSHOT_HEADER_SIZE + dialogue->StackCount * 5 +
                 dialogue->CallStackCount * (IdSize * 2) +
                 dialogue->OptionCount * (IdSize * 2 + 2 + THREADBOBBER_SUBSTITUTIONS * 5) +
                 dialogue->SubstitutionCount * 5;
    if (!dialogue->Program || !output || n > capacity || dialogue->CandidateCount ||
        dialogue->EvaluationCount || dialogue->State == ThreadBobberState_Running ||
        dialogue->State == ThreadBobberState_Error)
    {
        return 0;
    }
    unsigned char *o = output;
    *o++ = THREADBOBBER_VERSION_MAJOR;
    *o++ = (unsigned char)dialogue->State;
    PutId(o, dialogue->Node);
    o += IdSize;
    PutId(o, dialogue->ProgramCounter);
    o += IdSize;
    *o++ = dialogue->StackCount;
    *o++ = dialogue->CallStackCount;
    *o++ = dialogue->OptionCount;
    *o++ = dialogue->SubstitutionCount;
    PutId(o, dialogue->Line);
    o += IdSize;
    PutId(o, dialogue->Command);
    o += IdSize;
    for (unsigned i = 0; i < dialogue->StackCount; i++)
    {
        o[0] = dialogue->Stack[i].Type;
        WriteUInt32(o + 1, dialogue->Stack[i].Bits);
        o += 5;
    }
    for (unsigned i = 0; i < dialogue->CallStackCount; i++)
    {
        PutId(o, dialogue->CallStack[i].Node);
        PutId(o + IdSize, dialogue->CallStack[i].ProgramCounter);
        o += IdSize * 2;
    }
    for (unsigned i = 0; i < dialogue->OptionCount; i++)
    {
        PutId(o, dialogue->Options[i].Line);
        PutId(o + IdSize, dialogue->Options[i].Destination);
        o[IdSize * 2] = dialogue->Options[i].IsAvailable;
        o[IdSize * 2 + 1] = dialogue->Options[i].SubstitutionCount;
        o += IdSize * 2 + 2;
        for (unsigned k = 0; k < THREADBOBBER_SUBSTITUTIONS; k++)
        {
            ThreadBobberValue value = {0, 0};
            if (k < dialogue->Options[i].SubstitutionCount)
            {
                value = dialogue->Options[i].Substitutions[k];
            }
            o[0] = value.Type;
            WriteUInt32(o + 1, value.Bits);
            o += 5;
        }
    }
    for (unsigned i = 0; i < dialogue->SubstitutionCount; i++)
    {
        o[0] = dialogue->Substitutions[i].Type;
        WriteUInt32(o + 1, dialogue->Substitutions[i].Bits);
        o += 5;
    }
    return n;
}

static int IsValidValue(const ThreadBobberDialogue *dialogue, ThreadBobberValue value)
{
    return value.Type == ThreadBobberValueType_Number ||
           (value.Type == ThreadBobberValueType_Boolean && value.Bits <= 1) ||
           (value.Type == ThreadBobberValueType_String && IsValidString(dialogue, value.Bits));
}

// A saved program counter must point to an instruction or the end of a node.
static int IsInstructionBoundary(const ThreadBobberProgram *program, unsigned node, unsigned target)
{
    if (node >= program->NodeCount || target > program->Nodes[node].Length)
    {
        return 0;
    }
    const ThreadBobberNode *current = &program->Nodes[node];
    unsigned offset = 0;
    while (offset < target)
    {
        unsigned opcode = program->Code[current->Start + offset];
        if (opcode == 0 || opcode >= ThreadBobberOpcode_Count)
        {
            return 0;
        }
        offset += InstructionSizes[opcode];
    }
    return offset == target;
}

int ThreadBobber_RestoreState(ThreadBobberDialogue *dialogue, const unsigned char *in, unsigned len)
{
    enum
    {
        IdSize = (int)sizeof(ThreadBobberId)
    };

    const ThreadBobberProgram *program = dialogue->Program;
    if (!program || !in || len < THREADBOBBER_SNAPSHOT_HEADER_SIZE)
    {
        return 0;
    }
    const unsigned char *cur = in;
    if (*cur++ != THREADBOBBER_VERSION_MAJOR)
    {
        return 0;
    }
    unsigned state = *cur++;
    ThreadBobberId node = GetId(cur);
    cur += IdSize;
    ThreadBobberId pc = GetId(cur);
    cur += IdSize;
    unsigned stackCount = *cur++, callStackCount = *cur++, options = *cur++, substitutions = *cur++;
    ThreadBobberId line = GetId(cur);
    cur += IdSize;
    ThreadBobberId command = GetId(cur);
    cur += IdSize;
    if (state != ThreadBobberState_Idle && state != ThreadBobberState_WaitingForLine &&
        state != ThreadBobberState_WaitingForOptions &&
        state != ThreadBobberState_WaitingForCommand && state != ThreadBobberState_Complete)
    {
        return 0;
    }
    if (node >= program->NodeCount || pc > program->Nodes[node].Length ||
        stackCount > THREADBOBBER_STACK || callStackCount > THREADBOBBER_CALLS ||
        options > THREADBOBBER_OPTIONS || substitutions > THREADBOBBER_SUBSTITUTIONS)
    {
        return 0;
    }
    if (len != THREADBOBBER_SNAPSHOT_HEADER_SIZE + stackCount * 5 + callStackCount * (IdSize * 2) +
                   options * (IdSize * 2 + 2 + THREADBOBBER_SUBSTITUTIONS * 5) + substitutions * 5)
    {
        return 0;
    }
    if (!IsInstructionBoundary(program, node, pc))
    {
        return 0;
    }
    if (state == ThreadBobberState_WaitingForLine && line >= program->LineCount)
    {
        return 0;
    }
    if (state == ThreadBobberState_WaitingForCommand && command >= program->CommandCount)
    {
        return 0;
    }
    if (state == ThreadBobberState_WaitingForOptions && !options)
    {
        return 0;
    }
    ThreadBobberDialogue next = *dialogue;
    ResetExecution(&next);
    next.State = (uint8_t)state;
    next.Node = node;
    next.ProgramCounter = pc;
    next.StackCount = (uint8_t)stackCount;
    next.CallStackCount = (uint8_t)callStackCount;
    next.OptionCount = (uint8_t)options;
    next.SubstitutionCount = (uint8_t)substitutions;
    next.Line = line;
    next.Command = command;
    next.Error = 0;
    const unsigned char *i = in + THREADBOBBER_SNAPSHOT_HEADER_SIZE;
    for (unsigned k = 0; k < stackCount; k++)
    {
        next.Stack[k].Type = i[0];
        next.Stack[k].Bits = ReadUInt32(i + 1);
        if (!IsValidValue(&next, next.Stack[k]))
        {
            return 0;
        }
        i += 5;
    }
    for (unsigned k = 0; k < callStackCount; k++)
    {
        next.CallStack[k].Node = GetId(i);
        next.CallStack[k].ProgramCounter = GetId(i + IdSize);
        if (next.CallStack[k].Node >= program->NodeCount ||
            !IsInstructionBoundary(program, next.CallStack[k].Node,
                                   next.CallStack[k].ProgramCounter))
        {
            return 0;
        }
        i += IdSize * 2;
    }
    for (unsigned k = 0; k < options; k++)
    {
        next.Options[k].Line = GetId(i);
        next.Options[k].Destination = GetId(i + IdSize);
        next.Options[k].IsAvailable = i[IdSize * 2] ? 1 : 0;
        if (next.Options[k].Line >= program->LineCount ||
            !IsInstructionBoundary(program, node, next.Options[k].Destination))
        {
            return 0;
        }
        next.Options[k].SubstitutionCount = i[IdSize * 2 + 1];
        if (next.Options[k].SubstitutionCount > THREADBOBBER_SUBSTITUTIONS)
        {
            return 0;
        }
        i += IdSize * 2 + 2;
        for (unsigned s = 0; s < THREADBOBBER_SUBSTITUTIONS; s++)
        {
            next.Options[k].Substitutions[s].Type = i[0];
            next.Options[k].Substitutions[s].Bits = ReadUInt32(i + 1);
            if (s < next.Options[k].SubstitutionCount &&
                !IsValidValue(&next, next.Options[k].Substitutions[s]))
            {
                return 0;
            }
            i += 5;
        }
    }
    for (unsigned k = 0; k < substitutions; k++)
    {
        next.Substitutions[k].Type = i[0];
        next.Substitutions[k].Bits = ReadUInt32(i + 1);
        if (!IsValidValue(&next, next.Substitutions[k]))
        {
            return 0;
        }
        i += 5;
    }
    *dialogue = next;
    return 1;
}

#endif /* THREADBOBBER_IMPLEMENTATION */
