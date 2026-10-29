#include "pptxtext.h"

#include "fontresolver.h"
#include "richtext.h"
#include "textlayout.h"

#include <QFontMetricsF>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

#include <algorithm>

namespace pptx {

QFont fontOf(const RunStyle &style, double size)
{
    // The face the run names, as this app draws it (fontFor). Asked for as bold and as
    // italic as the face itself is, so that an installed face is drawn as itself: the
    // run's switches are PowerPoint's (pptximport), which for a face outside Office's
    // four ("Condensed Black") are off though the face is heavy.
    const ResolvedFont face = resolvePostScriptName(style.fontName, style.fontFamily);
    TextRun run;
    run.fontName = style.fontName;
    run.family = face.installed ? face.family : style.fontFamily;
    run.size = size;
    run.bold = face.installed ? face.bold : style.bold;
    run.italic = face.installed ? face.italic : style.italic;
    if (style.size > 0)
        run.kerning = style.characterSpacing * size / style.size;
    if (style.capitalization == RunStyle::AllCaps)
        run.capitalization = TextRun::AllCaps;
    else if (style.capitalization == RunStyle::SmallCaps)
        run.capitalization = TextRun::SmallCaps;
    return fontFor(run);
}

namespace {

// The height this app gives a line of these metrics before any spacing (textlayout.cpp)
double naturalHeight(double ascent, double descent, double leading)
{
    return qCeil(ascent + descent + leading);
}

double naturalHeight(const QFont &font)
{
    const QFontMetricsF metrics(font);
    return naturalHeight(metrics.ascent(), metrics.descent(), metrics.leading());
}

// The size a run is laid out at: a superscript small, as ProPresenter draws it.
// (KNOWN, ProToPPTX, against ProPresenter's exported slides: it does not make its line
// taller; A GUESS at 0.6 of the size.)
double laidSize(const RunStyle &style, double fontScale)
{
    return style.size * fontScale * (style.superscript != 0 ? 0.6 : 1);
}

const RunStyle &largestStyle(const Paragraph &paragraph)
{
    if (paragraph.runs.isEmpty())
        return paragraph.endStyle;
    const Run *largest = &paragraph.runs.first();
    for (const Run &run : paragraph.runs) {
        if (run.style.size > largest->style.size)
            largest = &run;
    }
    return largest->style;
}

} // namespace

LaidText layoutLines(const TextBody &body, double width, double fontScale)
{
    LaidText laid;
    if (width <= 0 || body.paragraphs.isEmpty())
        return laid;
    double y = 0;
    for (int k = 0; k < body.paragraphs.size(); ++k) {
        const Paragraph &p = body.paragraphs.at(k);
        if (k > 0)
            y += p.spaceBefore * fontScale;

        // The paragraph's words as one string, with each run's font over its stretch:
        // a list's marker and a tab first, as macOS draws a list; a line break as Qt's
        // line separator; an empty paragraph as a zero-width space in its end's style,
        // which gives it the height of a line, as macOS lays one out.
        QString text;
        QList<QTextLayout::FormatRange> formats;
        QList<std::pair<int, double>> sizes; // where each stretch starts, and its size
        const auto append = [&](const QString &piece, const RunStyle &style) {
            if (piece.isEmpty())
                return;
            QTextLayout::FormatRange range;
            range.start = int(text.size());
            range.length = int(piece.size());
            range.format.setFont(fontOf(style, laidSize(style, fontScale)));
            formats << range;
            sizes << std::make_pair(range.start, style.size * fontScale);
            text += piece;
        };
        if (p.bullet) {
            const RunStyle &first = p.runs.isEmpty() ? p.endStyle : p.runs.first().style;
            append((p.bullet->numberScheme.isEmpty() ? p.bullet->character : QString::number(p.bullet->startAt) + u'.')
                       + u'\t', first);
        }
        for (const Run &run : p.runs) {
            QString words = run.text;
            words.replace(QChar(0x2028), QChar::LineSeparator);
            words.replace(QChar(0x0B), QChar::LineSeparator);
            if (run.style.capitalization == RunStyle::AllCaps)
                words = words.toUpper();
            append(words, run.style);
        }
        if (text.isEmpty())
            append(QString(QChar(0x200B)), p.endStyle);

        QTextLayout layout(text);
        layout.setFormats(formats);
        QTextOption option;
        option.setWrapMode(body.wraps ? QTextOption::WordWrap : QTextOption::NoWrap);
        option.setUseDesignMetrics(true);
        // Only the stops the text gives, then the default interval (macOS's own default
        // stops end at 336 points, after which a tab breaks the line)
        QList<QTextOption::Tab> tabs;
        for (double stop : p.tabStops)
            tabs << QTextOption::Tab(stop * fontScale, QTextOption::LeftTab);
        if (p.bullet && p.leftIndent > 0)
            tabs << QTextOption::Tab(p.leftIndent * fontScale, QTextOption::LeftTab);
        std::sort(tabs.begin(), tabs.end(), [](const QTextOption::Tab &a, const QTextOption::Tab &b) {
            return a.position < b.position;
        });
        option.setTabs(tabs);
        option.setTabStopDistance((p.defaultTabInterval > 0 ? p.defaultTabInterval : 72) * fontScale);
        layout.setTextOption(option);

        const RunStyle &largest = largestStyle(p);
        const double paragraphNatural = naturalHeight(fontOf(largest, laidSize(largest, fontScale)));
        layout.beginLayout();
        bool first = true;
        while (true) {
            QTextLine line = layout.createLine();
            if (!line.isValid())
                break;
            const double indent = (p.leftIndent + (first ? p.firstLineIndent : 0)) * fontScale;
            line.setLineWidth(qMax(1.0, width - indent - p.rightIndent * fontScale));
            const double natural = naturalHeight(line.ascent(), line.descent(), line.leading());
            // How tall the line is, and where its baseline is in it
            double height = natural;
            double gap = 0;
            double baseline = line.ascent();
            if (p.lineSpacing) {
                const double value = p.lineSpacing->value;
                switch (p.lineSpacing->kind) {
                case Paragraph::LineSpacing::Multiple:
                    if (value > 1) {
                        // As extra space below each line, as ProPresenter places it
                        gap = (value - 1) * paragraphNatural;
                    } else {
                        height = natural * value;
                        baseline = height - line.descent();
                    }
                    break;
                case Paragraph::LineSpacing::Exactly:
                    height = value * fontScale;
                    baseline = height - line.descent();
                    break;
                case Paragraph::LineSpacing::AtLeast:
                    height = qMax(natural, value * fontScale);
                    baseline = height - line.descent();
                    break;
                }
            }
            // The largest size of what is on the line
            double size = 0;
            const int from = line.textStart();
            const int to = from + line.textLength();
            for (int s = 0; s < sizes.size(); ++s) {
                const int start = sizes.at(s).first;
                const int end = s + 1 < sizes.size() ? sizes.at(s + 1).first : int(text.size());
                if (start < to && end > from)
                    size = qMax(size, sizes.at(s).second);
            }
            laid.lines << LaidLine {k, size, y + baseline};
            y += height + gap + p.extraLeading * fontScale;
            first = false;
        }
        layout.endLayout();
        if (k < body.paragraphs.size() - 1)
            y += p.spaceAfter * fontScale;
    }
    laid.height = y;
    return laid;
}

double lineHeightRatio(const RunStyle &style)
{
    const double size = qMax(style.size, 1.0);
    return naturalHeight(fontOf(style, size)) / size;
}

double shrinkFactor(const TextBody &body, const QRectF &frame)
{
    const double width = frame.width() - body.insets.left() - body.insets.right();
    const double available = frame.height() - body.insets.top() - body.insets.bottom();
    if (width <= 0 || available <= 0)
        return 1;
    if (layoutLines(body, width).height <= available + 0.5)
        return 1;
    double low = 0.05;
    double high = 1;
    for (int k = 0; k < 14; ++k) {
        const double mid = (low + high) / 2;
        if (layoutLines(body, width, mid).height <= available + 0.5)
            low = mid;
        else
            high = mid;
    }
    return low;
}

} // namespace pptx
