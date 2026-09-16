// Prints the evaluated event trace of a node in tests/features.yarn, in the format of
// tests/OfficialTrace, for tests/parity.py:
//     driver NODE [option index...]
// Choices are consumed in order; after that the first available option is chosen.
#define THREADBOBBER_IMPLEMENTATION
#include "threadbobber.h"
#include "features_program.h"
#include <stdio.h>
#include <stdlib.h>

static ThreadBobberDialogue Dialogue;
static ThreadBobberStringStorage Strings;

static uint32_t Random(ThreadBobberDialogue *dialogue, uint32_t bound)
{
    (void)dialogue;
    return bound / 2;
}

static const ThreadBobberLibrary Library = {NULL, NULL, Random};

static void Values(const ThreadBobberValue *values, unsigned count)
{
    for (unsigned i = 0; i < count; i++)
    {
        if (i)
        {
            putchar('|');
        }
        if (values[i].Type == ThreadBobberValueType_Number)
        {
            printf("%.9g", (double)ThreadBobber_GetNumber(values[i]));
        }
        else if (values[i].Type == ThreadBobberValueType_Boolean)
        {
            printf("%s", values[i].Bits ? "True" : "False");
        }
        else
        {
            printf("%s", ThreadBobber_GetString(&Dialogue, values[i].Bits));
        }
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    int nextChoice = 2;
    int node = argc > 1 ? ThreadBobber_FindNode(&features_program, argv[1]) : -1;
    if (node < 0 || !ThreadBobber_Initialise(&Dialogue, &features_program, &Library) ||
        !ThreadBobber_AttachStringStorage(&Dialogue, &Strings) ||
        !ThreadBobber_SetNode(&Dialogue, (unsigned)node))
    {
        fputs("usage: driver NODE [option index...]\n", stderr);
        return 1;
    }
    for (unsigned resumes = 0; resumes < 1024; resumes++)
    {
        ThreadBobberState state = ThreadBobber_Run(&Dialogue);
        if (state == ThreadBobberState_Complete)
        {
            puts("D");
            return 0;
        }
        if (state == ThreadBobberState_WaitingForLine)
        {
            printf("L|%s|", ThreadBobber_GetLineID(&Dialogue, Dialogue.Line));
            Values(Dialogue.Substitutions, Dialogue.SubstitutionCount);
        }
        else if (state == ThreadBobberState_WaitingForCommand)
        {
            char text[1024];
            if (ThreadBobber_GetCurrentCommandText(&Dialogue, text, sizeof text) >= sizeof text)
            {
                return 2;
            }
            printf("C|%s\n", text);
        }
        else if (state == ThreadBobberState_WaitingForOptions)
        {
            unsigned selected = THREADBOBBER_NONE;
            for (unsigned i = 0; i < Dialogue.OptionCount; i++)
            {
                const ThreadBobberOption *option = &Dialogue.Options[i];
                printf("O|%s|%u|", ThreadBobber_GetLineID(&Dialogue, option->Line),
                       option->IsAvailable);
                Values(option->Substitutions, option->SubstitutionCount);
                if (selected == THREADBOBBER_NONE && option->IsAvailable)
                {
                    selected = i;
                }
            }
            if (nextChoice < argc)
            {
                selected = (unsigned)atoi(argv[nextChoice++]);
            }
            if (!ThreadBobber_SetSelectedOption(&Dialogue, selected))
            {
                fputs("The scenario selected an unavailable or missing option.\n", stderr);
                return 3;
            }
            continue;
        }
        else
        {
            return 4;
        }
        if (!ThreadBobber_Continue(&Dialogue))
        {
            return 5;
        }
    }
    return 6;
}
