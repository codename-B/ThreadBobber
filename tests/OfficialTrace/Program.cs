namespace ThreadBobber.Tests
{
    using System.Globalization;
    using Yarn;

    /// <summary>
    /// Writes evaluated dialogue events from the pinned Yarn Spinner runtime.
    /// Usage: OfficialTrace PROGRAM.yarnc NODE [option index...]
    /// </summary>
    internal static class OfficialTrace
    {
        private static void Main(string[] args)
        {
            CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
            var choices = args.Skip(2).Select(int.Parse).ToArray();
            var nextChoice = 0;
            var dialogue = new Dialogue(new MemoryVariableStore());
            dialogue.SetProgram(Yarn.Program.Parser.ParseFrom(File.ReadAllBytes(args[0])));
            dialogue.LineHandler = line => Console.WriteLine(
                "L|" + line.ID + "|" + string.Join("|", line.Substitutions));
            dialogue.CommandHandler = command => Console.WriteLine("C|" + command.Text);
            dialogue.OptionsHandler = options =>
            {
                foreach (var option in options.Options)
                {
                    Console.WriteLine(
                        "O|" + option.Line.ID + "|" + (option.IsAvailable ? "1" : "0")
                        + "|" + string.Join("|", option.Line.Substitutions));
                }

                var selected = nextChoice < choices.Length
                    ? options.Options[choices[nextChoice++]]
                    : options.Options.First(option => option.IsAvailable);
                if (!selected.IsAvailable)
                {
                    throw new InvalidOperationException("The scenario selected an unavailable option.");
                }

                dialogue.SetSelectedOption(selected.ID);
            };
            dialogue.DialogueCompleteHandler = () => Console.WriteLine("D");
            dialogue.SetNode(args[1]);

            int resumes = 0;
            do
            {
                if (++resumes > 1024)
                {
                    throw new InvalidOperationException("The trace exceeded its continuation limit.");
                }

                dialogue.Continue();
            }
            while (dialogue.IsActive);
        }
    }
}
