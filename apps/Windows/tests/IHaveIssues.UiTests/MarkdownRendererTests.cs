using System.Windows;
using System.Windows.Documents;
using IHaveIssues.Presentation;

namespace IHaveIssues.UiTests;

/// <summary>
/// Renders markdown to a <see cref="FlowDocument"/> and inspects the result.
/// </summary>
/// <remarks>
/// These run on an STA thread but need no <see cref="System.Windows.Application"/>: a document is
/// built, never laid out. Theme brushes are attached as resource references and simply stay
/// unresolved here, which is what makes the renderer testable without a running app.
/// </remarks>
public class MarkdownRendererTests
{
    // MARK: - Structure

    [Fact]
    public void EmptyInputProducesAnEmptyDocument() => OnSta(() =>
    {
        Assert.Empty(MarkdownRenderer.Render(null).Blocks);
        Assert.Empty(MarkdownRenderer.Render(string.Empty).Blocks);
        Assert.Empty(MarkdownRenderer.Render("   \n  ").Blocks);
    });

    [Fact]
    public void HeadingsHierarchyIsPreserved() => OnSta(() =>
    {
        var blocks = MarkdownRenderer.Render("# One\n\n## Two\n\n### Three").Blocks.OfType<Paragraph>().ToList();

        Assert.Equal(3, blocks.Count);
        Assert.Equal(["One", "Two", "Three"], blocks.Select(TextOf));
        // Each level is smaller than the one above it.
        Assert.True(blocks[0].FontSize > blocks[1].FontSize);
        Assert.True(blocks[1].FontSize > blocks[2].FontSize);
    });

    [Fact]
    public void EmphasisMapsToBoldItalicAndStrikethrough() => OnSta(() =>
    {
        var paragraph = Assert.IsType<Paragraph>(MarkdownRenderer.Render("**b** *i* ~~s~~").Blocks.FirstBlock);
        var spans = paragraph.Inlines.OfType<Span>().ToList();

        Assert.Contains(spans, span => span is Bold && TextOfInline(span) == "b");
        Assert.Contains(spans, span => span is Italic && TextOfInline(span) == "i");
        Assert.Contains(
            spans,
            span => span.TextDecorations == TextDecorations.Strikethrough && TextOfInline(span) == "s");
    });

    [Fact]
    public void ListsBecomeListsWithTheRightMarker() => OnSta(() =>
    {
        var ordered = Assert.IsType<List>(MarkdownRenderer.Render("1. one\n2. two").Blocks.FirstBlock);
        Assert.Equal(TextMarkerStyle.Decimal, ordered.MarkerStyle);
        Assert.Equal(2, ordered.ListItems.Count);

        var unordered = Assert.IsType<List>(MarkdownRenderer.Render("- one\n- two").Blocks.FirstBlock);
        Assert.Equal(TextMarkerStyle.Disc, unordered.MarkerStyle);
        Assert.Equal(2, unordered.ListItems.Count);
    });

    [Fact]
    public void OrderedListKeepsItsStartingNumber() => OnSta(() =>
    {
        var list = Assert.IsType<List>(MarkdownRenderer.Render("7. seven\n8. eight").Blocks.FirstBlock);
        Assert.Equal(7, list.StartIndex);
    });

    [Fact]
    public void FencedCodeKeepsItsTextAndUsesAMonospaceFont() => OnSta(() =>
    {
        var code = Assert.IsType<Paragraph>(
            MarkdownRenderer.Render("```\nvar x = 1;\nvar y = 2;\n```").Blocks.FirstBlock);

        Assert.Equal("var x = 1;\nvar y = 2;", TextOf(code));
        Assert.Contains("Consolas", code.FontFamily.Source, StringComparison.Ordinal);
    });

    [Fact]
    public void BlockQuoteBecomesABorderedSection() => OnSta(() =>
    {
        var quote = Assert.IsType<Section>(MarkdownRenderer.Render("> quoted").Blocks.FirstBlock);
        Assert.Equal(3, quote.BorderThickness.Left);
        Assert.Equal("quoted", TextOf(Assert.IsType<Paragraph>(quote.Blocks.FirstBlock)));
    });

    /// <summary>
    /// Issue text is prose where the line breaks are meant, so a single newline is a line break
    /// rather than being folded into a space as CommonMark would.
    /// </summary>
    [Fact]
    public void SingleNewlineIsALineBreak() => OnSta(() =>
    {
        var paragraph = Assert.IsType<Paragraph>(
            MarkdownRenderer.Render("The button is inert.\nIt should authenticate.").Blocks.FirstBlock);

        Assert.Single(paragraph.Inlines.OfType<LineBreak>());
    });

    // MARK: - Links

    [Fact]
    public void HttpLinkBecomesANavigableHyperlink() => OnSta(() =>
    {
        var paragraph = Assert.IsType<Paragraph>(
            MarkdownRenderer.Render("see [the issue](https://github.com/openbcm/i-have-issues/issues/412)")
                .Blocks.FirstBlock);

        var link = Assert.Single(paragraph.Inlines.OfType<Hyperlink>());
        Assert.Equal("https://github.com/openbcm/i-have-issues/issues/412", link.NavigateUri!.AbsoluteUri);
        Assert.Equal("the issue", TextOfInline(link));
    });

    [Fact]
    public void BareUrlBecomesAHyperlink() => OnSta(() =>
    {
        var paragraph = Assert.IsType<Paragraph>(
            MarkdownRenderer.Render("see https://example.com/x for details").Blocks.FirstBlock);

        Assert.Single(paragraph.Inlines.OfType<Hyperlink>());
    });

    /// <summary>
    /// A <c>.issues</c> file comes from a repository and may be written by anyone. A link this app
    /// would not open must not look clickable, and must never reach the shell.
    /// </summary>
    [Theory]
    [InlineData("file:///C:/Windows/System32/cmd.exe")]
    [InlineData("javascript:alert(1)")]
    [InlineData("ms-msdt:/id")]
    [InlineData("\\\\attacker\\share\\payload.exe")]
    public void UnsafeLinkSchemesAreRenderedAsPlainText(string url) => OnSta(() =>
    {
        var paragraph = Assert.IsType<Paragraph>(MarkdownRenderer.Render($"[click me]({url})").Blocks.FirstBlock);

        Assert.Empty(paragraph.Inlines.OfType<Hyperlink>());
        Assert.Equal("click me", TextOf(paragraph));
    });

    [Theory]
    [InlineData("https://example.com", true)]
    [InlineData("http://example.com", true)]
    [InlineData("mailto:dru@openbcm.com", true)]
    [InlineData("file:///C:/Windows/System32/cmd.exe", false)]
    [InlineData("javascript:alert(1)", false)]
    [InlineData("ftp://example.com", false)]
    public void OnlyWebAndMailSchemesAreConsideredSafe(string url, bool expected) =>
        Assert.Equal(expected, ExternalLink.IsSafe(new Uri(url)));

    [Fact]
    public void RelativeUriIsNotSafe() =>
        Assert.False(ExternalLink.IsSafe(new Uri("some/path", UriKind.Relative)));

    /// <summary>
    /// Images are never fetched: loading a remote image on open would report the reader's address
    /// and the time they opened the document to whoever wrote it.
    /// </summary>
    [Fact]
    public void ImagesRenderAsAltTextAndAreNeverLoaded() => OnSta(() =>
    {
        var paragraph = Assert.IsType<Paragraph>(
            MarkdownRenderer.Render("![a screenshot](https://tracker.example.com/pixel.png)").Blocks.FirstBlock);

        Assert.Empty(paragraph.Inlines.OfType<InlineUIContainer>());
        Assert.Contains("a screenshot", TextOf(paragraph), StringComparison.Ordinal);
    });

    // MARK: - Robustness

    /// <summary>Whatever an issue's notes contain, rendering must not throw.</summary>
    [Theory]
    [InlineData("- \n- \n")]
    [InlineData("> \n>\n")]
    [InlineData("```")]
    [InlineData("[](  )")]
    [InlineData("![]()")]
    [InlineData("***")]
    [InlineData("<div>raw html</div>")]
    [InlineData("| a | b |\n|---|---|\n| 1 | 2 |")]
    [InlineData("1. one\n   - nested\n     1. deeper")]
    [InlineData("**unclosed")]
    [InlineData("&amp; &#169; &nope;")]
    public void MalformedOrUnusualMarkdownStillRenders(string markdown) => OnSta(() =>
    {
        var document = MarkdownRenderer.Render(markdown);
        Assert.NotNull(document);
    });

    // MARK: - Helpers

    private static string TextOf(Paragraph paragraph) => new TextRange(paragraph.ContentStart, paragraph.ContentEnd).Text;

    private static string TextOfInline(System.Windows.Documents.Inline inline) =>
        new TextRange(inline.ContentStart, inline.ContentEnd).Text;

    /// <summary>WPF document objects have thread affinity and require a single-threaded apartment.</summary>
    private static void OnSta(Action body)
    {
        Exception? failure = null;
        var thread = new Thread(() =>
        {
            try
            {
                body();
            }
            catch (Exception error)
            {
                failure = error;
            }
        });

        thread.SetApartmentState(ApartmentState.STA);
        thread.Start();
        thread.Join();

        if (failure is not null)
        {
            throw new InvalidOperationException(failure.Message, failure);
        }
    }
}
