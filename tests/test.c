// ThreadBobber tests. Build and run with `make test`; both capacity profiles are exercised.
// Use --list-tests to list cases or --filter=features.* to run one suite.
#include "utest.h"
#define THREADBOBBER_IMPLEMENTATION
#include "threadbobber.h"
#include "vmtest_program.h"
#include "features_program.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// utest.h copies both sides of ASSERT_EQ into variables before comparing them, so states and
// table identifiers are compared as unsigned to satisfy -Wsign-compare in both profiles.
#define ASSERT_UEQ(x, y) ASSERT_EQ((unsigned)(x), (unsigned)(y))

#ifdef THREADBOBBER_PROFILE_EXPANDED
#define PROFILE_NAME "expanded"
#else
#define PROFILE_NAME "compact"
#endif

static unsigned commandsSeen, lastCommand, lastValue, flagValue;
static float hostNumber = 42;
static char lastLabel[32];

// Reject unexpected command IDs, argument counts and types.
static int HandleCommand(ThreadBobberDialogue *dialogue, unsigned id, const ThreadBobberValue *a,
                         unsigned parameterCount)
{
    commandsSeen++;
    lastCommand = id;
    if (id == VMTEST_COMMAND_sync_cmd)
    {
        if (parameterCount != 1 || a[0].Type != ThreadBobberValueType_Number)
        {
            return -1;
        }
        lastValue = (unsigned)ThreadBobber_GetNumber(a[0]);
        return 0;
    }
    if (id == VMTEST_COMMAND_async_cmd)
    {
        if (parameterCount != 1 || a[0].Type != ThreadBobberValueType_String)
        {
            return -1;
        }
        snprintf(lastLabel, sizeof lastLabel, "%s", ThreadBobber_GetString(dialogue, a[0].Bits));
        return 1;
    }
    if (id == VMTEST_COMMAND_flag_cmd)
    {
        if (parameterCount != 1 || a[0].Type != ThreadBobberValueType_Boolean)
        {
            return -1;
        }
        flagValue = a[0].Bits;
        return 0;
    }
    return -1;
}

static int HostFunction(ThreadBobberDialogue *dialogue, unsigned id, const ThreadBobberValue *a,
                        unsigned parameterCount, ThreadBobberValue *r)
{
    (void)dialogue;
    if (id == VMTEST_FUNCTION_host_number && parameterCount == 0)
    {
        *r = ThreadBobber_CreateNumber(hostNumber);
        return 1;
    }
    if (id == VMTEST_FUNCTION_host_flag && parameterCount == 1)
    {
        *r = ThreadBobber_CreateBoolean((unsigned)ThreadBobber_GetNumber(a[0]) == 1);
        return 1;
    }
    return 0;
}

static uint32_t HalfRandom(ThreadBobberDialogue *dialogue, uint32_t bound)
{
    (void)dialogue;
    return bound / 2;
}

static const ThreadBobberLibrary host = {HandleCommand, HostFunction, HalfRandom};
static ThreadBobberDialogue dialogue;
static char text[256];

static int StartDialogue(unsigned node)
{
    return ThreadBobber_Initialise(&dialogue, &vmtest_program, &host) &&
           ThreadBobber_SetNode(&dialogue, node);
}

// Return the error from setup or one Run call; a normal yield returns None.
static unsigned RunProgramError(const ThreadBobberProgram *program)
{
    if (ThreadBobber_Initialise(&dialogue, program, &host) && ThreadBobber_SetNode(&dialogue, 0))
    {
        ThreadBobber_Run(&dialogue);
    }
    return dialogue.State == ThreadBobberState_Error ? dialogue.Error : ThreadBobberError_None;
}

UTEST(vm, commands)
{
    ASSERT_TRUE(StartDialogue(VMTEST_NODE_Commands));
    commandsSeen = 0;
    ASSERT_UEQ(ThreadBobber_Run(&dialogue), ThreadBobberState_WaitingForLine);
    ASSERT_UEQ(dialogue.Line, VMTEST_LINE_between_commands);
    ASSERT_EQ(commandsSeen, 1u);
    ASSERT_UEQ(lastCommand, VMTEST_COMMAND_sync_cmd);
    ASSERT_EQ(lastValue, 7u);
    ASSERT_TRUE(ThreadBobber_Continue(&dialogue));
    ASSERT_UEQ(ThreadBobber_Run(&dialogue), ThreadBobberState_WaitingForCommand);
    ASSERT_EQ(commandsSeen, 2u);
    ASSERT_STREQ(lastLabel, "pause");
    ASSERT_LT(dialogue.Command, vmtest_program.CommandCount);
    ASSERT_EQ(vmtest_program.Commands[dialogue.Command].FunctionID, VMTEST_COMMAND_async_cmd);
    /* Snapshot while waiting on the command, then resume the copy. */
    unsigned char snap[THREADBOBBER_SNAPSHOT_MAX];
    unsigned len = ThreadBobber_SaveState(&dialogue, snap, sizeof snap);
    ASSERT_GT(len, 0u);
    ThreadBobberDialogue copy;
    ASSERT_TRUE(ThreadBobber_Initialise(&copy, &vmtest_program, &host));
    memcpy(copy.Variables, dialogue.Variables, sizeof copy.Variables);
    ASSERT_TRUE(ThreadBobber_RestoreState(&copy, snap, len));
    ASSERT_EQ(copy.State, ThreadBobberState_WaitingForCommand);
    ASSERT_EQ(copy.Command, dialogue.Command);
    hostNumber = 9;
    ASSERT_TRUE(ThreadBobber_Continue(&copy));
    ASSERT_UEQ(ThreadBobber_Run(&copy), ThreadBobberState_WaitingForLine);
    ASSERT_UEQ(copy.Line, VMTEST_LINE_after_commands);
    ASSERT_EQ(flagValue, 1u);
    ThreadBobber_GetCurrentLineText(&copy, text, sizeof text);
    ASSERT_STREQ(text, "After: commands 9");
    ASSERT_FALSE(ThreadBobber_SetSelectedOption(&dialogue, 0));
    ASSERT_EQ(dialogue.State, ThreadBobberState_Error);
}

UTEST(vm, limits)
{
    ASSERT_TRUE(StartDialogue(VMTEST_NODE_Loop));
    ASSERT_UEQ(ThreadBobber_Run(&dialogue), ThreadBobberState_Error);
    ASSERT_EQ(dialogue.Error, ThreadBobberError_InstructionLimit);
    ASSERT_UEQ(dialogue.InstructionCount, THREADBOBBER_STEP_BUDGET);
    ASSERT_STREQ(ThreadBobber_GetErrorMessage(dialogue.Error),
                 "The instruction limit was exceeded.");
    ASSERT_TRUE(StartDialogue(VMTEST_NODE_Recurse));
    ASSERT_UEQ(ThreadBobber_Run(&dialogue), ThreadBobberState_Error);
    ASSERT_EQ(dialogue.Error, ThreadBobberError_CallStackOverflow);
    // The false branch skips the option block.
    ASSERT_TRUE(StartDialogue(VMTEST_NODE_Empty));
    ASSERT_UEQ(ThreadBobber_Run(&dialogue), ThreadBobberState_Complete);
    ASSERT_FALSE(ThreadBobber_SetNode(&dialogue, 999));
    ASSERT_EQ(dialogue.State, ThreadBobberState_Error);
    ASSERT_EQ(dialogue.Error, ThreadBobberError_InvalidNode);
    ASSERT_EQ(ThreadBobber_FindNode(&vmtest_program, "Empty"), VMTEST_NODE_Empty);
    ASSERT_LT(ThreadBobber_FindNode(&vmtest_program, "Nope"), 0);
    // Reject an oversized variable table and a node extending past the bytecode.
    ThreadBobberProgram bad = vmtest_program;
    bad.VariableCount = THREADBOBBER_MAX_VARS + 1;
    ASSERT_FALSE(ThreadBobber_Initialise(&dialogue, &bad, &host));
    static ThreadBobberNode nodes[1];
    nodes[0] = vmtest_program.Nodes[0];
    nodes[0].Start = (ThreadBobberId)vmtest_program.CodeSize;
    nodes[0].Length = 1;
    bad = vmtest_program;
    bad.Nodes = nodes;
    bad.NodeCount = 1;
    ASSERT_FALSE(ThreadBobber_Initialise(&dialogue, &bad, &host));
    static uint8_t junk[] = {0xFE};
    nodes[0] = vmtest_program.Nodes[0];
    nodes[0].Start = 0;
    nodes[0].Length = sizeof junk;
    bad.Code = junk;
    bad.CodeSize = sizeof junk;
    ASSERT_UEQ(RunProgramError(&bad), ThreadBobberError_InvalidOpcode);
    static uint8_t truncated[2] = {ThreadBobberOpcode_JumpTo, 0x10};
    nodes[0].Length = 2;
    bad.Code = truncated;
    bad.CodeSize = 2;
    ASSERT_UEQ(RunProgramError(&bad), ThreadBobberError_InvalidProgramCounter);
    static uint8_t underflow[1] = {ThreadBobberOpcode_Pop};
    nodes[0].Length = 1;
    bad.Code = underflow;
    bad.CodeSize = 1;
    ASSERT_UEQ(RunProgramError(&bad), ThreadBobberError_StackUnderflow);
    static uint8_t overflow[2 * (THREADBOBBER_STACK + 1)];
    for (unsigned i = 0; i < THREADBOBBER_STACK + 1; i++)
    {
        overflow[i * 2] = ThreadBobberOpcode_PushBool;
        overflow[i * 2 + 1] = 1;
    }
    nodes[0].Length = 2 * (THREADBOBBER_STACK + 1);
    bad.Code = overflow;
    bad.CodeSize = sizeof overflow;
    ASSERT_UEQ(RunProgramError(&bad), ThreadBobberError_StackOverflow);
    static uint8_t badvar[3] = {ThreadBobberOpcode_PushVariable, 0xFF, 0x7F};
    nodes[0].Length = 3;
    bad.Code = badvar;
    bad.CodeSize = 3;
    ASSERT_UEQ(RunProgramError(&bad), ThreadBobberError_InvalidVariable);
    static uint8_t badfn[6] = {ThreadBobberOpcode_PushFloat, 0, 0, 0, 0,
                               ThreadBobberOpcode_CallFunc};
    nodes[0].Length = 6;
    bad.Code = badfn;
    bad.CodeSize = 6; /* truncated call operand */
    ASSERT_UEQ(RunProgramError(&bad), ThreadBobberError_InvalidProgramCounter);
}

static const ThreadBobberLibrary EmptyLibrary = {0};

UTEST(regressions, invalid_tables)
{
    ThreadBobberDialogue d;
    ThreadBobberProgram program = vmtest_program;
    program.Variables = NULL;
    ASSERT_FALSE(ThreadBobber_Initialise(&d, &program, &EmptyLibrary));
    ASSERT_EQ(d.State, ThreadBobberState_Error);
    program = vmtest_program;
    program.Commands = NULL;
    ASSERT_FALSE(ThreadBobber_Initialise(&d, &program, &EmptyLibrary));
    program = vmtest_program;
    program.Lines = NULL;
    ASSERT_FALSE(ThreadBobber_Initialise(&d, &program, &EmptyLibrary));
    program = vmtest_program;
    program.Functions = NULL;
    ASSERT_FALSE(ThreadBobber_Initialise(&d, &program, &EmptyLibrary));
    ASSERT_FALSE(ThreadBobber_Initialise(NULL, &vmtest_program, &EmptyLibrary));
}

UTEST(regressions, number_formatting)
{
    ThreadBobberDialogue d;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &vmtest_program, &EmptyLibrary));
    const float inputs[] = {-42.0f, -0.25f, 2000000000.0f, NAN, INFINITY, -INFINITY};
    const char *expected[] = {"-42", "-0.25", "2e+09", "NaN", "Infinity", "-Infinity"};
    for (unsigned i = 0; i < sizeof inputs / sizeof inputs[0]; i++)
    {
        ThreadBobberValue values[] = {ThreadBobber_CreateNumber(inputs[i]),
                                      ThreadBobber_CreateBoolean(1), ThreadBobber_CreateString(0)};
        char actual[100];
        char wanted[100];
        snprintf(wanted, sizeof wanted, "Result: %s True ", expected[i]);
        memset(actual, 0x7f, sizeof actual);
        ThreadBobber_ExpandSubstitutions(&d, VMTEST_LINE_expr_result, values, 3, actual,
                                         sizeof actual);
        ASSERT_STREQ(actual, wanted);
    }
    ThreadBobberValue values[] = {ThreadBobber_CreateNumber(42), ThreadBobber_CreateBoolean(1),
                                  ThreadBobber_CreateString(0)};
    char singleByte = 'x';
    ASSERT_EQ(
        ThreadBobber_ExpandSubstitutions(&d, VMTEST_LINE_expr_result, values, 3, &singleByte, 1),
        0u);
    ASSERT_EQ(singleByte, '\0');
    ASSERT_EQ(ThreadBobber_ExpandSubstitutions(&d, VMTEST_LINE_expr_result, values, 3, NULL, 0),
              0u);
}

static void AppendNumber(uint8_t *code, unsigned *length, float number)
{
    uint32_t bits;
    memcpy(&bits, &number, sizeof bits);
    code[(*length)++] = ThreadBobberOpcode_PushFloat;
    for (unsigned i = 0; i < 4; i++)
    {
        code[(*length)++] = (uint8_t)(bits >> (8 * i));
    }
}

// Install the requested builtin at function index 0 and execute the supplied arguments.
static ThreadBobberState RunBuiltin(uint8_t *code, unsigned length, unsigned builtin,
                                    unsigned parameterCount, float *result)
{
    code[length++] = ThreadBobberOpcode_CallFunc;
    code[length++] = 0;
    code[length++] = 0;
    code[length++] = ThreadBobberOpcode_Stop;
    ThreadBobberNode node = {0, 0, (ThreadBobberId)length, THREADBOBBER_NONE, 0};
    ThreadBobberFunction function = {(uint8_t)builtin, 0, (uint8_t)parameterCount,
                                     ThreadBobberValueType_Number};
    ThreadBobberProgram program = vmtest_program;
    program.Nodes = &node;
    program.NodeCount = 1;
    program.Code = code;
    program.CodeSize = length;
    program.Functions = &function;
    program.FunctionCount = 1;
    ThreadBobberDialogue d;
    if (!ThreadBobber_Initialise(&d, &program, &EmptyLibrary) || !ThreadBobber_SetNode(&d, 0))
    {
        return ThreadBobberState_Error;
    }
    ThreadBobberState state = ThreadBobber_Run(&d);
    if (state == ThreadBobberState_Complete)
    {
        if (d.StackCount != 1 || d.Stack[0].Type != ThreadBobberValueType_Number)
        {
            return ThreadBobberState_Error;
        }
        *result = ThreadBobber_GetNumber(d.Stack[0]);
    }
    return state;
}

// The count is explicit so callers can test malformed argument counts.
static ThreadBobberState Evaluate(unsigned builtin, unsigned parameterCount, float input,
                                  float count, float *result)
{
    uint8_t code[16];
    unsigned length = 0;
    if (parameterCount)
    {
        AppendNumber(code, &length, input);
    }
    AppendNumber(code, &length, count);
    return RunBuiltin(code, length, builtin, parameterCount, result);
}

static ThreadBobberState Modulo(float x, float y, float *result)
{
    uint8_t code[24];
    unsigned length = 0;
    AppendNumber(code, &length, x);
    AppendNumber(code, &length, y);
    AppendNumber(code, &length, 2);
    return RunBuiltin(code, length, ThreadBobberFunction_NumberModulo, 2, result);
}

UTEST(regressions, numeric_limits)
{
    float result = 0;
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Floor, 1, FLT_MAX, 1, &result),
               ThreadBobberState_Complete);
    ASSERT_EQ(result, FLT_MAX);
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Round, 1, INFINITY, 1, &result),
               ThreadBobberState_Complete);
    ASSERT_TRUE(isinf(result));
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Round, 1, -2.5f, 1, &result),
               ThreadBobberState_Complete);
    ASSERT_EQ(result, -2.f);
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Int, 1, NAN, 1, &result), ThreadBobberState_Complete);
    ASSERT_TRUE(isnan(result));
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Floor, 1, 0, NAN, &result), ThreadBobberState_Error);
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Floor, 1, 0, -1, &result), ThreadBobberState_Error);
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Floor, 1, 0, 1.5f, &result), ThreadBobberState_Error);
    ASSERT_UEQ(Evaluate(ThreadBobberFunction_Random, 0, 0, 0, &result), ThreadBobberState_Error);
}

UTEST(regressions, modulo_rounds_operands_to_integers)
{
    // Cover half-to-even rounding, signs, float precision boundaries and Int32 limits.
    static const float cases[][3] = {
        {11.5f, 5, 2},
        {12.5f, 5, 2},
        {13.5f, 5, 4},
        {10.4f, 3, 1},
        {0.5f, 3, 0},
        {0.75f, 3, 1},
        {-7, 3, -1},
        {7, -3, 1},
        {-7, -3, -1},
        {-7.5f, 2, 0},
        {5, 2.5f, 1},
        {5.5f, 2.5f, 0},
        {4194304.5f, 2, 0},
        {4194305.5f, 4, 2},
        {8388609.f, 2, 1},
        {2147483520.f, 1000, 520},
        {-2147483648.f, 3, -2},
        {-2147483648.f, -1, 0},
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
        float result = 1;
        ASSERT_UEQ(Modulo(cases[i][0], cases[i][1], &result), ThreadBobberState_Complete);
        ASSERT_EQ(result, cases[i][2]);
    }
    // Reject divisors that round to zero and operands outside the finite Int32 range.
    static const float failing[][2] = {
        {5, 0}, {5, 0.4f}, {5, -0.5f}, {5, NAN}, {INFINITY, 2}, {2147483648.f, 2}, {-3e9f, 2},
    };
    for (unsigned i = 0; i < sizeof failing / sizeof failing[0]; i++)
    {
        float result = 1;
        ASSERT_UEQ(Modulo(failing[i][0], failing[i][1], &result), ThreadBobberState_Error);
    }
}

UTEST(regressions, rejects_truncated_snapshots_and_invalid_resume_positions)
{
    ThreadBobberDialogue d;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &vmtest_program, &EmptyLibrary));
    ASSERT_TRUE(ThreadBobber_SetNode(&d, VMTEST_NODE_Expressions));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForLine);
    unsigned char state[THREADBOBBER_SNAPSHOT_MAX];
    unsigned length = ThreadBobber_SaveState(&d, state, sizeof state);
    ASSERT_GT(length, 0u);
    ThreadBobberDialogue before = d;
    for (unsigned truncated = 0; truncated < length; truncated++)
    {
        ASSERT_FALSE(ThreadBobber_RestoreState(&d, state, truncated));
        ASSERT_EQ(memcmp(&d, &before, sizeof d), 0);
    }
    // Reject an out-of-range node without changing the dialogue.
    unsigned char invalidNode[THREADBOBBER_SNAPSHOT_MAX];
    memcpy(invalidNode, state, length);
    memset(invalidNode + 1, 0xFF, sizeof(ThreadBobberId));
    ASSERT_FALSE(ThreadBobber_RestoreState(&d, invalidNode, length));
    ASSERT_EQ(memcmp(&d, &before, sizeof d), 0);
    // Reject a program counter inside the first instruction's operand.
    unsigned offset = 2 + sizeof(ThreadBobberId);
    memset(state + offset, 0, sizeof(ThreadBobberId));
    state[offset] = 1;
    ASSERT_FALSE(ThreadBobber_RestoreState(&d, state, length));
    ASSERT_EQ(memcmp(&d, &before, sizeof d), 0);
}

UTEST(regressions, sampled_finite_floats_round_trip)
{
    ThreadBobberDialogue d;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &vmtest_program, &EmptyLibrary));
    uint32_t seed = 17;
    for (unsigned i = 0; i < 4096; i++)
    {
        seed = seed * 1664525u + 1013904223u;
        float input;
        memcpy(&input, &seed, sizeof input);
        if (!isfinite(input))
        {
            continue;
        }
        ThreadBobberValue values[] = {ThreadBobber_CreateNumber(input),
                                      ThreadBobber_CreateBoolean(1), ThreadBobber_CreateString(0)};
        char line[128];
        ThreadBobber_ExpandSubstitutions(&d, VMTEST_LINE_expr_result, values, 3, line, sizeof line);
        float restored = strtof(line + strlen("Result: "), NULL);
        ASSERT_EQ_MSG(memcmp(&input, &restored, sizeof input), 0, line);
    }
}

static const ThreadBobberLibrary Library = {NULL, NULL, HalfRandom};

// Values and options are compared field by field: their padding bytes are not defined.
static int SameValues(const ThreadBobberValue *a, const ThreadBobberValue *b, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        if (a[i].Type != b[i].Type || a[i].Bits != b[i].Bits)
        {
            return 0;
        }
    }
    return 1;
}

static int SameOptions(const ThreadBobberOption *a, const ThreadBobberOption *b, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        if (a[i].Line != b[i].Line || a[i].Destination != b[i].Destination ||
            a[i].IsAvailable != b[i].IsAvailable ||
            a[i].SubstitutionCount != b[i].SubstitutionCount ||
            !SameValues(a[i].Substitutions, b[i].Substitutions, a[i].SubstitutionCount))
        {
            return 0;
        }
    }
    return 1;
}

UTEST(features, option_substitutions)
{
    ThreadBobberDialogue d;
    char text[100];
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_SetNode(&d, FEATURES_NODE_OptionExpressions));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForOptions);
    ASSERT_EQ(d.OptionCount, 2);
    ASSERT_TRUE(d.Options[0].IsAvailable);
    ASSERT_FALSE(d.Options[1].IsAvailable);
    ASSERT_TRUE(ThreadBobber_SetValue(&d, FEATURES_VARIABLE_price, ThreadBobber_CreateNumber(99)));
    ThreadBobber_GetOptionText(&d, 0, text, sizeof text);
    ASSERT_STREQ(text, "Buy tea for 7");
    ThreadBobber_GetOptionText(&d, 1, text, sizeof text);
    ASSERT_STREQ(text, "Sell tea for 5");
    unsigned char saved[THREADBOBBER_SNAPSHOT_MAX];
    unsigned length = ThreadBobber_SaveState(&d, saved, sizeof saved);
    ASSERT_GT(length, 0u);
    ThreadBobberDialogue before = d;
    for (unsigned i = 0; i < length; i++)
    {
        ASSERT_FALSE(ThreadBobber_RestoreState(&d, saved, i));
        ASSERT_EQ(memcmp(&before, &d, sizeof d), 0);
    }
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_RestoreState(&d, saved, length));
    ThreadBobber_GetOptionText(&d, 1, text, sizeof text);
    ASSERT_STREQ(text, "Sell tea for 5");
    ASSERT_FALSE(ThreadBobber_SetSelectedOption(&d, 1));
    ASSERT_TRUE(ThreadBobber_SetSelectedOption(&d, 0));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForLine);
    ThreadBobber_GetCurrentLineText(&d, text, sizeof text);
    ASSERT_STREQ(text, "Purchased.");
}

UTEST(features, command_text)
{
    ThreadBobberDialogue d;
    char text[100];
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_SetNode(&d, FEATURES_NODE_CommandExpressions));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForCommand);
    ASSERT_EQ(ThreadBobber_GetCurrentCommandText(&d, NULL, 0), 17u);
    ASSERT_EQ(ThreadBobber_GetCurrentCommandText(&d, text, 5), 17u);
    ASSERT_STREQ(text, "give");
    ThreadBobber_GetCurrentCommandText(&d, text, sizeof text);
    ASSERT_STREQ(text, "give \"tea\" 8 True");
    unsigned char state[THREADBOBBER_SNAPSHOT_MAX];
    unsigned length = ThreadBobber_SaveState(&d, state, sizeof state);
    ASSERT_GT(length, 0u);
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_RestoreState(&d, state, length));
    ASSERT_EQ(d.State, ThreadBobberState_WaitingForCommand);
    ThreadBobber_GetCurrentCommandText(&d, text, sizeof text);
    ASSERT_STREQ(text, "give \"tea\" 8 True");
    ASSERT_TRUE(ThreadBobber_Continue(&d));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForCommand);
    ThreadBobber_GetCurrentCommandText(&d, text, sizeof text);
    ASSERT_STREQ(text, "wait 1");
    // Loading the earlier save must also replace a different pending command.
    ASSERT_TRUE(ThreadBobber_RestoreState(&d, state, length));
    ThreadBobber_GetCurrentCommandText(&d, text, sizeof text);
    ASSERT_STREQ(text, "give \"tea\" 8 True");
    ASSERT_TRUE(ThreadBobber_Continue(&d));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForCommand);
    ThreadBobber_GetCurrentCommandText(&d, text, sizeof text);
    ASSERT_STREQ(text, "wait 1");
    ASSERT_TRUE(ThreadBobber_Continue(&d));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_Complete);
}

UTEST(features, smart_variables)
{
    ThreadBobberDialogue d;
    ThreadBobberValue value;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_SetNode(&d, FEATURES_NODE_SmartVariables));
    ASSERT_TRUE(ThreadBobber_EvaluateSmartVariable(&d, "$double_price", &value));
    ASSERT_EQ(ThreadBobber_GetNumber(value), 14.f);
    ASSERT_EQ(d.State, ThreadBobberState_Running);
    ASSERT_UEQ(d.ProgramCounter, 0);
    ASSERT_UEQ(d.Node, FEATURES_NODE_SmartVariables);
    ASSERT_EQ(d.StackCount, 0);
    // Separate host evaluations each get a fresh instruction budget.
    ASSERT_UEQ(ThreadBobber_RunFor(&d, 1), ThreadBobberState_Running);
    ThreadBobberId slicedPc = d.ProgramCounter;
    unsigned slicedStack = d.StackCount;
    for (unsigned i = 0; i < THREADBOBBER_STEP_BUDGET; i++)
    {
        ASSERT_TRUE(ThreadBobber_EvaluateSmartVariable(&d, "$double_price", &value));
        ASSERT_EQ(ThreadBobber_GetNumber(value), 14.f);
    }
    ASSERT_EQ(d.ProgramCounter, slicedPc);
    ASSERT_UEQ(d.StackCount, slicedStack);
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForLine);
    ASSERT_EQ(ThreadBobber_GetNumber(d.Substitutions[0]), 14.f);
    ThreadBobberId pc = d.ProgramCounter, node = d.Node;
    ASSERT_TRUE(ThreadBobber_SetValue(&d, FEATURES_VARIABLE_price, ThreadBobber_CreateNumber(12)));
    ASSERT_TRUE(ThreadBobber_EvaluateSmartVariable(&d, "$double_price", &value));
    ASSERT_EQ(ThreadBobber_GetNumber(value), 24.f);
    ASSERT_EQ(d.State, ThreadBobberState_WaitingForLine);
    ASSERT_EQ(d.ProgramCounter, pc);
    ASSERT_EQ(d.Node, node);
    ASSERT_EQ(d.EvaluationCount, 0);
    ASSERT_EQ(ThreadBobber_GetNumber(d.Substitutions[0]), 14.f);
    ASSERT_FALSE(ThreadBobber_EvaluateSmartVariable(&d, "Numbers", &value));
    ASSERT_TRUE(ThreadBobber_Continue(&d));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForLine);
    ASSERT_EQ(ThreadBobber_GetNumber(d.Substitutions[0]), 18.f);
}

static int EvaluateInCallback(ThreadBobberDialogue *d, unsigned id, const ThreadBobberValue *args,
                              unsigned count)
{
    (void)id;
    (void)args;
    (void)count;
    ThreadBobberValue result;
    const char *name = ThreadBobber_GetString(d, d->Program->Nodes[1].Name);
    for (unsigned i = 0; i < THREADBOBBER_STEP_BUDGET; i++)
    {
        if (!ThreadBobber_EvaluateSmartVariable(d, name, &result))
        {
            return -1;
        }
    }
    return 0;
}

UTEST(features, callback_evaluation_budget)
{
    const uint8_t code[] = {ThreadBobberOpcode_RunCommand,
                            0,
                            0,
                            ThreadBobberOpcode_Stop,
                            ThreadBobberOpcode_PushBool,
                            1,
                            ThreadBobberOpcode_Stop};
    ThreadBobberNode nodes[] = {vmtest_program.Nodes[0], vmtest_program.Nodes[1]};
    nodes[0].Start = 0;
    nodes[0].Length = 4;
    nodes[1].Start = 4;
    nodes[1].Length = 3;
    nodes[1].IsSmartVariable = 1;
    ThreadBobberProgram program = vmtest_program;
    program.Code = code;
    program.CodeSize = sizeof code;
    program.Nodes = nodes;
    program.NodeCount = 2;
    const ThreadBobberLibrary library = {EvaluateInCallback, NULL, NULL};
    ThreadBobberDialogue d;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &program, &library));
    ASSERT_TRUE(ThreadBobber_SetNode(&d, 0));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_Error);
    ASSERT_EQ(d.Error, ThreadBobberError_InstructionLimit);
    ASSERT_UEQ(d.InstructionCount, THREADBOBBER_STEP_BUDGET);
    ASSERT_EQ(d.ExecutionActive, 0);
}

UTEST(features, string_storage)
{
    ThreadBobberDialogue d;
    ThreadBobberStringStorage storage;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_AttachStringStorage(&d, &storage));
    ASSERT_TRUE(ThreadBobber_SetNode(&d, FEATURES_NODE_Strings));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForOptions);
    char text[128];
    for (unsigned i = 0; i < 64; i++)
    {
        ThreadBobberValue value;
        snprintf(text, sizeof text, "temporary %u", i);
        ASSERT_TRUE(ThreadBobber_StoreString(&d, text, &value));
    }
    ThreadBobber_GetOptionText(&d, 0, text, sizeof text);
    ASSERT_STREQ(text, "tea! buy");
    unsigned char strings[THREADBOBBER_STRING_SNAPSHOT_MAX], state[THREADBOBBER_SNAPSHOT_MAX];
    uint32_t variables[THREADBOBBER_MAX_VARS];
    memcpy(variables, d.Variables, sizeof variables);
    unsigned stringLength = ThreadBobber_SaveStrings(&d, strings, sizeof strings);
    unsigned length = ThreadBobber_SaveState(&d, state, sizeof state);
    ASSERT_GT(stringLength, 0u);
    ASSERT_GT(length, 0u);
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_AttachStringStorage(&d, &storage));
    ASSERT_FALSE(ThreadBobber_RestoreState(&d, state, length));
    ASSERT_TRUE(ThreadBobber_RestoreStrings(&d, strings, stringLength));
    memcpy(d.Variables, variables, sizeof variables);
    ASSERT_TRUE(ThreadBobber_RestoreState(&d, state, length));
    ThreadBobber_GetOptionText(&d, 0, text, sizeof text);
    ASSERT_STREQ(text, "tea! buy");
    ASSERT_TRUE(ThreadBobber_SetSelectedOption(&d, 0));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForLine);
    ThreadBobber_GetCurrentLineText(&d, text, sizeof text);
    ASSERT_STREQ(text, "tea! 7 True True");
    ASSERT_TRUE(ThreadBobber_SetNode(&d, FEATURES_NODE_StringStress));
    ASSERT_UEQ(ThreadBobber_Run(&d), ThreadBobberState_WaitingForLine);
    ThreadBobber_GetCurrentLineText(&d, text, sizeof text);
    ASSERT_EQ(strlen(text), 32u);
    for (unsigned i = 0; i < 32; i++)
    {
        ASSERT_EQ(text[i], 'x');
    }
    ASSERT_EQ(storage.Pinned, 0u);
}

UTEST(features, string_limits)
{
    ThreadBobberDialogue d;
    ThreadBobberStringStorage storage;
    ThreadBobberValue value;
    char text[THREADBOBBER_STRING_BYTES + 1];
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_FALSE(ThreadBobber_StoreString(&d, "hello", &value));
    ASSERT_EQ(d.Error, ThreadBobberError_StringStorageFull);
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_AttachStringStorage(&d, &storage));
    memset(text, 'x', sizeof text - 1);
    text[sizeof text - 1] = 0;
    ASSERT_FALSE(ThreadBobber_StoreString(&d, text, &value));
    ASSERT_EQ(d.Error, ThreadBobberError_StringTooLong);
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &features_program, &Library));
    ASSERT_TRUE(ThreadBobber_AttachStringStorage(&d, &storage));
    // Every occupied slot is rooted on the stack, so none may be reclaimed.
    for (unsigned i = 0; i < THREADBOBBER_STRING_SLOTS; i++)
    {
        snprintf(text, sizeof text, "root %u", i);
        ASSERT_TRUE(ThreadBobber_StoreString(&d, text, &value));
        d.Stack[d.StackCount++] = value;
    }
    ASSERT_FALSE(ThreadBobber_StoreString(&d, "one too many", &value));
    ASSERT_EQ(d.Error, ThreadBobberError_StringStorageFull);
}

UTEST(features, evaluation_limits)
{
    ThreadBobberProgram program = features_program;
    uint8_t code[] = {ThreadBobberOpcode_EvaluateVariable, 0, 0, 0, 0};
    ThreadBobberNode node = {features_program.Nodes[FEATURES_NODE_OptionExpressions].Name,
                            0, 3, THREADBOBBER_NONE, 1};
    program.Code = code;
    program.CodeSize = sizeof code;
    program.Nodes = &node;
    program.NodeCount = 1;
    ThreadBobberDialogue d;
    ThreadBobberValue value;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &program, &Library));
    ASSERT_FALSE(ThreadBobber_EvaluateSmartVariable(&d, "OptionExpressions", &value));
    ASSERT_EQ(d.Error, ThreadBobberError_EvaluationOverflow);
    code[0] = ThreadBobberOpcode_PushBool;
    code[1] = 0;
    code[2] = ThreadBobberOpcode_JumpIfFalse;
    code[3] = 2;
    code[4] = 0;
    node.Length = sizeof code;
    ASSERT_TRUE(ThreadBobber_Initialise(&d, &program, &Library));
    ASSERT_FALSE(ThreadBobber_EvaluateSmartVariable(&d, "OptionExpressions", &value));
    ASSERT_EQ(d.Error, ThreadBobberError_InstructionLimit);
    ASSERT_UEQ(d.InstructionCount, THREADBOBBER_STEP_BUDGET);
}

UTEST(features, cooperative_execution)
{
    // Compare event payloads and storage after each yield for these five nodes.
    const char *nodes[] = {"Strings", "StringStress", "SmartVariables", "Saliency", "Fairness"};
    for (unsigned n = 0; n < sizeof nodes / sizeof nodes[0]; n++)
    {
        ThreadBobberDialogue full, sliced;
        ThreadBobberStringStorage fullStrings, slicedStrings;
        ASSERT_TRUE(ThreadBobber_Initialise(&full, &features_program, &Library));
        ASSERT_TRUE(ThreadBobber_Initialise(&sliced, &features_program, &Library));
        ASSERT_TRUE(ThreadBobber_AttachStringStorage(&full, &fullStrings));
        ASSERT_TRUE(ThreadBobber_AttachStringStorage(&sliced, &slicedStrings));
        unsigned node = (unsigned)ThreadBobber_FindNode(&features_program, nodes[n]);
        ASSERT_TRUE(ThreadBobber_SetNode(&full, node));
        ASSERT_TRUE(ThreadBobber_SetNode(&sliced, node));
        for (unsigned event = 0; event < 100; event++)
        {
            ThreadBobberState expected = ThreadBobber_Run(&full), actual;
            unsigned calls = 0;
            do
            {
                actual = ThreadBobber_RunFor(&sliced, 1);
                calls++;
                ASSERT_LE(calls, (unsigned)THREADBOBBER_STEP_BUDGET);
            } while (actual == ThreadBobberState_Running);
            ASSERT_UEQ(actual, expected);
            ASSERT_TRUE(actual != ThreadBobberState_Error);
            ASSERT_EQ(full.InstructionCount, sliced.InstructionCount);
            ASSERT_EQ(memcmp(full.Variables, sliced.Variables, sizeof full.Variables), 0);
            ASSERT_EQ(memcmp(&fullStrings, &slicedStrings, sizeof fullStrings), 0);
            if (actual == ThreadBobberState_Complete)
            {
                break;
            }
            if (actual == ThreadBobberState_WaitingForOptions)
            {
                ASSERT_EQ(full.OptionCount, sliced.OptionCount);
                ASSERT_TRUE(SameOptions(full.Options, sliced.Options, full.OptionCount));
                ASSERT_TRUE(ThreadBobber_SetSelectedOption(&full, 0));
                ASSERT_TRUE(ThreadBobber_SetSelectedOption(&sliced, 0));
            }
            else
            {
                ASSERT_EQ(full.Line, sliced.Line);
                ASSERT_EQ(full.SubstitutionCount, sliced.SubstitutionCount);
                ASSERT_TRUE(
                    SameValues(full.Substitutions, sliced.Substitutions, full.SubstitutionCount));
                ASSERT_TRUE(ThreadBobber_Continue(&full));
                ASSERT_TRUE(ThreadBobber_Continue(&sliced));
            }
        }
        ASSERT_EQ(sliced.State, ThreadBobberState_Complete);
    }
}

UTEST_STATE();

int main(int argc, const char *const argv[])
{
    printf("ThreadBobber %s profile: sizeof(ThreadBobberDialogue) = %u\n", PROFILE_NAME,
           (unsigned)sizeof(ThreadBobberDialogue));
    return utest_main(argc, argv);
}
