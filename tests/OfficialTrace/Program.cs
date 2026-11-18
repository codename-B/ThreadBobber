using System.Globalization;
using Yarn;

internal sealed class OfficialTrace(Dialogue dialogue, Queue<int> choices) : IDialogueResponder
{
    private static async Task Main(string[] args)
    {
        CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
        var dialogue = new Dialogue(new MemoryVariableStore());
        dialogue.Program = Yarn.Program.Parser.ParseFrom(File.ReadAllBytes(args[0]));
        dialogue.Responder = new OfficialTrace(dialogue, new Queue<int>(args.Skip(2).Select(int.Parse)));
        await dialogue.StartDialogue(args[1]);
    }

    public ValueTask HandleLine(Line line, CancellationToken token)
    {
        Console.WriteLine("L|" + line.ID + "|" + string.Join("|", line.Substitutions));
        return ValueTask.CompletedTask;
    }

    public ValueTask HandleCommand(Command command, CancellationToken token)
    {
        Console.WriteLine("C|" + command.Text);
        return ValueTask.CompletedTask;
    }

    public ValueTask<int> HandleOptions(OptionSet options, CancellationToken token)
    {
        foreach (var option in options.Options)
            Console.WriteLine("O|" + option.Line.ID + "|" + (option.IsAvailable ? "1" : "0")
                + "|" + string.Join("|", option.Line.Substitutions));
        var selected = choices.Count > 0 ? choices.Dequeue() : options.Options.First(o => o.IsAvailable).ID;
        if (selected != Dialogue.NoOptionSelected && !options.Options[selected].IsAvailable)
            throw new InvalidOperationException("The scenario selected an unavailable option.");
        return ValueTask.FromResult(selected);
    }

    public ValueTask HandleDialogueComplete()
    {
        Console.WriteLine("D");
        return ValueTask.CompletedTask;
    }

    public ValueTask HandleNodeStart(string node, CancellationToken token) => ValueTask.CompletedTask;
    public ValueTask HandleNodeComplete(string node, CancellationToken token) => ValueTask.CompletedTask;
    public ValueTask PrepareForLines(List<string> ids, CancellationToken token) => ValueTask.CompletedTask;
    public bool CanCallFunction(string name) => name is "visited" or "visited_count" or "has_any_content";

    public ValueTask<IConvertible> InvokeFunction(string name, IConvertible[] parameters, CancellationToken token)
    {
        var node = parameters[0].ToString(CultureInfo.InvariantCulture);
        return ValueTask.FromResult<IConvertible>(name switch {
            "visited" => dialogue.IsNodeVisited(node),
            "visited_count" => dialogue.GetNodeVisitCount(node),
            "has_any_content" => dialogue.HasAnyContent(node),
            _ => throw new ArgumentException(name)
        });
    }
}
