using System.Text;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Documents;
using System.Windows.Media;
using System.Windows.Navigation;
using Markdig;
using Markdig.Extensions.EmphasisExtras;
using Markdig.Syntax;
using Markdig.Syntax.Inlines;

// Markdig and WPF both call their document node types Block and Inline, and this file works with
// both at once. The aliases keep every signature saying which side of the translation it is on.
using MarkdigBlock = Markdig.Syntax.Block;
using MarkdigInline = Markdig.Syntax.Inlines.Inline;
using WpfBlock = System.Windows.Documents.Block;
using WpfInline = System.Windows.Documents.Inline;

namespace IHaveIssues.Presentation;

/// <summary>
/// Renders the markdown carried in an issue's body fields into a WPF <see cref="FlowDocument"/>.
/// </summary>
/// <remarks>
/// Markdig parses; this type does the rendering. A ready-made WPF renderer would have been less
/// code, but the maintained ones target .NET 5/6 at the newest and each would add a second
/// third-party licence to carry. Rendering here also means the output is styled with the Fluent
/// theme brushes rather than a library's own palette, and it follows a light/dark switch: every
/// themed brush is attached with
/// <see cref="FrameworkContentElement.SetResourceReference(DependencyProperty, object)"/>, which is
/// the code equivalent of a <c>DynamicResource</c>.
/// <para>
/// Images are deliberately not fetched — see <see cref="AppendLink"/>.
/// </para>
/// </remarks>
public static class MarkdownRenderer
{
    /// <summary>
    /// CommonMark, plus strikethrough, bare URLs becoming links, and a single newline meaning a
    /// line break.
    /// </summary>
    /// <remarks>
    /// The extensions are chosen, not taken wholesale — <c>UseAdvancedExtensions</c> would pull in
    /// footnotes, custom containers, and more, all of which would need rendering.
    /// <list type="bullet">
    /// <item><b>Strikethrough</b> because the format already uses it: the legacy markdown importer
    /// reads <c>~~Title~~</c> as a resolved entry, so <c>~~</c> must not show up as literal tildes
    /// in the same document.</item>
    /// <item><b>Soft line breaks as hard</b> because CommonMark folds a single newline into a
    /// space. Issue text is prose where the line breaks are meant — the Apple build preserves them
    /// too — so folding them would silently reflow steps, notes, and pasted output.</item>
    /// </list>
    /// </remarks>
    private static readonly MarkdownPipeline Pipeline = new MarkdownPipelineBuilder()
        .UseEmphasisExtras(EmphasisExtraOptions.Strikethrough)
        .UseAutoLinks()
        .UseSoftlineBreakAsHardlineBreak()
        .Build();

    private static readonly FontFamily MonospaceFont = new("Cascadia Mono, Consolas, Courier New, Global Monospace");

    // MARK: - Attached property

    /// <summary>
    /// The markdown to render. Set it on a <see cref="FlowDocumentScrollViewer"/> and its
    /// <see cref="FlowDocumentScrollViewer.Document"/> is replaced whenever the text changes.
    /// </summary>
    public static readonly DependencyProperty TextProperty = DependencyProperty.RegisterAttached(
        "Text",
        typeof(string),
        typeof(MarkdownRenderer),
        new PropertyMetadata(null, OnTextChanged));

    public static string? GetText(DependencyObject element) => (string?)element.GetValue(TextProperty);

    public static void SetText(DependencyObject element, string? value) => element.SetValue(TextProperty, value);

    private static void OnTextChanged(DependencyObject element, DependencyPropertyChangedEventArgs e)
    {
        if (element is FlowDocumentScrollViewer viewer)
        {
            viewer.Document = Render(e.NewValue as string);
        }
    }

    // MARK: - Document

    /// <summary>Renders markdown as a flow document. Empty text produces an empty document.</summary>
    public static FlowDocument Render(string? markdown)
    {
        var document = new FlowDocument
        {
            // Without these a FlowDocument lays itself out as a padded, multi-column page.
            PagePadding = new Thickness(0),
            ColumnWidth = double.PositiveInfinity,
            FontFamily = SystemFonts.MessageFontFamily,
            FontSize = SystemFonts.MessageFontSize
        };
        document.SetResourceReference(TextElement.ForegroundProperty, "TextFillColorPrimaryBrush");

        if (string.IsNullOrWhiteSpace(markdown))
        {
            return document;
        }

        foreach (var block in Markdig.Markdown.Parse(markdown, Pipeline))
        {
            AppendBlock(document.Blocks, block);
        }

        return document;
    }

    // MARK: - Blocks

    private static void AppendBlock(BlockCollection target, MarkdigBlock block)
    {
        switch (block)
        {
            case HeadingBlock heading:
                target.Add(BuildHeading(heading));
                break;

            case ParagraphBlock paragraph:
                target.Add(BuildParagraph(paragraph));
                break;

            case ListBlock list:
                target.Add(BuildList(list));
                break;

            case QuoteBlock quote:
                target.Add(BuildQuote(quote));
                break;

            case CodeBlock code:
                target.Add(BuildCodeBlock(code));
                break;

            case ThematicBreakBlock:
                target.Add(BuildRule());
                break;

            // An HTML block, or anything an extension produced that is not handled above, is shown
            // as its own source text rather than dropped.
            case LeafBlock leaf:
                target.Add(new Paragraph(new Run(ReadLines(leaf))) { Margin = new Thickness(0, 0, 0, 8) });
                break;

            case ContainerBlock container:
                foreach (var child in container)
                {
                    AppendBlock(target, child);
                }

                break;

            default:
                break;
        }
    }

    private static Paragraph BuildHeading(HeadingBlock heading)
    {
        var paragraph = new Paragraph
        {
            FontSize = heading.Level switch { 1 => 20, 2 => 17, 3 => 15, _ => 14 },
            FontWeight = FontWeights.SemiBold,
            Margin = new Thickness(0, heading.Level <= 2 ? 12 : 8, 0, 4)
        };

        AppendInlines(paragraph.Inlines, heading.Inline);
        return paragraph;
    }

    private static Paragraph BuildParagraph(ParagraphBlock block)
    {
        var paragraph = new Paragraph { Margin = new Thickness(0, 0, 0, 8) };
        AppendInlines(paragraph.Inlines, block.Inline);
        return paragraph;
    }

    private static List BuildList(ListBlock block)
    {
        var list = new List
        {
            MarkerStyle = block.IsOrdered ? TextMarkerStyle.Decimal : TextMarkerStyle.Disc,
            Margin = new Thickness(0, 0, 0, 8),
            Padding = new Thickness(20, 0, 0, 0)
        };

        if (block.IsOrdered && int.TryParse(block.OrderedStart, out var start))
        {
            list.StartIndex = Math.Max(start, 1);
        }

        foreach (var child in block)
        {
            var item = new ListItem();
            if (child is ContainerBlock itemBlock)
            {
                foreach (var nested in itemBlock)
                {
                    AppendBlock(item.Blocks, nested);
                }
            }

            // A ListItem with no blocks throws when the document is laid out.
            if (item.Blocks.Count == 0)
            {
                item.Blocks.Add(new Paragraph());
            }

            TrimTrailingMargin(item.Blocks);
            list.ListItems.Add(item);
        }

        return list;
    }

    private static Section BuildQuote(QuoteBlock block)
    {
        var section = new Section
        {
            BorderThickness = new Thickness(3, 0, 0, 0),
            Padding = new Thickness(10, 0, 0, 0),
            Margin = new Thickness(0, 0, 0, 8)
        };
        section.SetResourceReference(WpfBlock.BorderBrushProperty, "CardStrokeColorDefaultBrush");
        section.SetResourceReference(TextElement.ForegroundProperty, "TextFillColorSecondaryBrush");

        foreach (var child in block)
        {
            AppendBlock(section.Blocks, child);
        }

        if (section.Blocks.Count == 0)
        {
            section.Blocks.Add(new Paragraph());
        }

        TrimTrailingMargin(section.Blocks);
        return section;
    }

    private static Paragraph BuildCodeBlock(CodeBlock block)
    {
        var paragraph = new Paragraph(new Run(ReadLines(block).TrimEnd('\n')))
        {
            FontFamily = MonospaceFont,
            FontSize = 13,
            Padding = new Thickness(10, 8, 10, 8),
            Margin = new Thickness(0, 0, 0, 8)
        };
        paragraph.SetResourceReference(TextElement.BackgroundProperty, "CardBackgroundFillColorDefaultBrush");
        return paragraph;
    }

    private static Paragraph BuildRule()
    {
        var rule = new Paragraph
        {
            BorderThickness = new Thickness(0, 0, 0, 1),
            Margin = new Thickness(0, 4, 0, 12)
        };
        rule.SetResourceReference(WpfBlock.BorderBrushProperty, "CardStrokeColorDefaultBrush");
        return rule;
    }

    /// <summary>Drops the bottom margin of the last block so nested content does not double-space.</summary>
    private static void TrimTrailingMargin(BlockCollection blocks)
    {
        if (blocks.LastBlock is { } last)
        {
            last.Margin = new Thickness(last.Margin.Left, last.Margin.Top, last.Margin.Right, 0);
        }
    }

    private static string ReadLines(LeafBlock block)
    {
        var builder = new StringBuilder();
        for (var index = 0; index < block.Lines.Count; index++)
        {
            builder.Append(block.Lines.Lines[index].Slice.ToString()).Append('\n');
        }

        return builder.ToString();
    }

    // MARK: - Inlines

    private static void AppendInlines(InlineCollection target, ContainerInline? container)
    {
        if (container is null)
        {
            return;
        }

        foreach (var inline in container)
        {
            AppendInline(target, inline);
        }
    }

    private static void AppendInline(InlineCollection target, MarkdigInline inline)
    {
        switch (inline)
        {
            case LiteralInline literal:
                target.Add(new Run(literal.Content.ToString()));
                break;

            case EmphasisInline emphasis:
                target.Add(BuildEmphasis(emphasis));
                break;

            case CodeInline code:
                target.Add(BuildCodeSpan(code));
                break;

            case LinkInline link:
                AppendLink(target, link);
                break;

            case AutolinkInline autolink:
                target.Add(BuildHyperlink(autolink.Url, new Run(autolink.Url)));
                break;

            case LineBreakInline:
                target.Add(new LineBreak());
                break;

            case ContainerInline container:
                AppendInlines(target, container);
                break;

            // Raw HTML and entities are shown as written rather than interpreted.
            case LeafInline leaf:
                target.Add(new Run(leaf.ToString()));
                break;

            default:
                break;
        }
    }

    private static Span BuildEmphasis(EmphasisInline emphasis)
    {
        Span span = emphasis switch
        {
            { DelimiterChar: '~', DelimiterCount: 2 } => new Span { TextDecorations = TextDecorations.Strikethrough },
            { DelimiterCount: >= 2 } => new Bold(),
            _ => new Italic()
        };

        AppendInlines(span.Inlines, emphasis);
        return span;
    }

    private static Span BuildCodeSpan(CodeInline code)
    {
        var span = new Span(new Run(code.Content))
        {
            FontFamily = MonospaceFont,
            FontSize = 13
        };
        span.SetResourceReference(TextElement.BackgroundProperty, "CardBackgroundFillColorDefaultBrush");
        return span;
    }

    /// <summary>
    /// Renders a link, or an image as its alt text beside a link to the source.
    /// </summary>
    /// <remarks>
    /// Images are never fetched. An <c>.issues</c> file arrives from a repository and may have been
    /// written by anyone; loading its image URLs on open would report the reader's IP address and
    /// the moment they opened the document to whoever wrote it.
    /// </remarks>
    private static void AppendLink(InlineCollection target, LinkInline link)
    {
        var label = new Span();
        AppendInlines(label.Inlines, link);

        if (link.IsImage)
        {
            if (label.Inlines.Count == 0)
            {
                label.Inlines.Add(new Run("image"));
            }

            var caption = new Italic(label);
            caption.SetResourceReference(TextElement.ForegroundProperty, "TextFillColorSecondaryBrush");
            target.Add(caption);

            if (link.Url is { Length: > 0 } source)
            {
                target.Add(new Run(" "));
                target.Add(BuildHyperlink(source, new Run(source)));
            }

            return;
        }

        if (label.Inlines.Count == 0)
        {
            label.Inlines.Add(new Run(link.Url ?? string.Empty));
        }

        target.Add(BuildHyperlink(link.Url, label));
    }

    private static WpfInline BuildHyperlink(string? url, WpfInline label)
    {
        if (!Uri.TryCreate(url, UriKind.Absolute, out var uri) || !ExternalLink.IsSafe(uri))
        {
            // Not something this app will open, so it is text rather than a dead or dangerous link.
            return label;
        }

        var hyperlink = new Hyperlink(label) { NavigateUri = uri };
        hyperlink.RequestNavigate += OnRequestNavigate;
        return hyperlink;
    }

    private static void OnRequestNavigate(object sender, RequestNavigateEventArgs e)
    {
        ExternalLink.Open(e.Uri);
        e.Handled = true;
    }
}
