#include "pptxrtf.h"

#include <QColorSpace>
#include <QColorTransform>
#include <QHash>
#include <QRegularExpression>

#include <optional>

namespace pptx {

QList<int> RtfDocument::paragraphStarts() const
{
    QList<int> starts;
    int at = 0;
    for (int k = 0; k < paragraphs.size(); ++k) {
        starts << at + markerLengths.value(k);
        int units = 0;
        for (const Run &run : paragraphs.at(k).runs)
            units += int(run.text.size());
        at += markerLengths.value(k) + units + 1;
    }
    return starts;
}

namespace {

// \ansicpg1252, which is all ProPresenter writes: the bytes 0x80-0x9F differ from
// Latin-1; the rest are the same.
char16_t windows1252(uchar b)
{
    static const char16_t high[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    };
    if (b >= 0x80 && b <= 0x9F)
        return high[b - 0x80];
    return char16_t(b);
}

// Apple's Generic RGB, which macOS writes some colours in: the primaries of the
// Trinitron tubes Macs once had, D65 white, a gamma of 1.8. Near sRGB, but not the same,
// so converted.
const QColorSpace &genericRgb()
{
    static const QColorSpace space(QPointF(0.3127, 0.3290), QPointF(0.630, 0.340), QPointF(0.295, 0.605),
                                   QPointF(0.155, 0.077), QColorSpace::TransferFunction::Gamma, 1.8f);
    return space;
}

QColor converted(const QColorSpace &from, double r, double g, double b, double alpha)
{
    if (!from.isValid())
        return QColor::fromRgbF(float(r), float(g), float(b), float(alpha));
    const QColorTransform transform = from.transformationToColorSpace(QColorSpace(QColorSpace::SRgb));
    QColor colour = transform.map(QColor::fromRgbF(float(r), float(g), float(b)));
    colour.setAlphaF(float(alpha));
    return colour;
}

// A colour of the expanded table: values in 1/100000, alpha last if given.
std::optional<QColor> expandedColor(const QString &space, const QList<int> &v)
{
    const auto f = [&v](int k) { return double(v.at(k)) / 100000; };
    if (space == u"cssrgb" || space == u"csdevicergb") {
        if (v.size() < 3)
            return std::nullopt;
        return QColor::fromRgbF(float(f(0)), float(f(1)), float(f(2)), float(v.size() >= 4 ? f(3) : 1));
    }
    if (space == u"csgenericrgb" || space == u"csdisplayp3") {
        if (v.size() < 3)
            return std::nullopt;
        const double alpha = v.size() >= 4 ? f(3) : 1;
        return converted(space == u"csgenericrgb" ? genericRgb() : QColorSpace(QColorSpace::DisplayP3), f(0), f(1), f(2), alpha);
    }
    if (space == u"csgray" || space == u"csgenericgray") {
        if (v.isEmpty())
            return std::nullopt;
        return QColor::fromRgbF(float(f(0)), float(f(0)), float(f(0)), float(v.size() >= 2 ? f(1) : 1));
    }
    return std::nullopt;
}

bool isLetter(uchar c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

bool isDigit(uchar c)
{
    return c >= '0' && c <= '9';
}

class Reader
{
public:
    explicit Reader(const QByteArray &bytes) : m_bytes(bytes) {}

    RtfDocument parse();

private:
    enum class Destination { Text, FontTable, ColorTable, ExpandedColorTable, ListText, Skip };

    struct CharacterState
    {
        int font = 0;
        int halfPoints = 24;
        std::optional<int> milliPoints;
        bool bold = false;
        bool italic = false;
        bool underline = false;
        int underlineColor = 0;
        bool strike = false;
        int color = 0;
        int background = 0;
        int spacingTwips = 0;
        int baselineHalfPoints = 0;
        int superscript = 0;
        RunStyle::Capitalization caps = RunStyle::NoCapitalization;

        bool operator==(const CharacterState &other) const = default;
    };

    struct ParagraphState
    {
        Paragraph::Alignment alignment = Paragraph::Left;
        int leftTwips = 0;
        int firstTwips = 0;
        int rightTwips = 0;
        int spaceBeforeTwips = 0;
        int spaceAfterTwips = 0;
        int lineTwips = 0;
        bool lineMultiple = false;
        int leadingTwips = 0;
        QList<int> tabStops;
        int defaultTabTwips = 0;
        bool isListItem = false;
    };

    struct GroupState
    {
        Destination destination = Destination::Text;
        CharacterState character;
        ParagraphState paragraph;
        int unicodeSkip = 1;
    };

    uchar at(qsizetype k) const { return uchar(m_bytes.at(k)); }
    void endGroup();
    void controlWord();
    void apply(const QByteArray &word, std::optional<int> n);
    void plainCharacter(uchar c);
    void appendUnit(char16_t unit);
    void appendToRun(char16_t unit, bool marker);
    void flushText();
    void paragraphBreak();
    void endParagraph(bool final);
    void finishFontName();
    void skipFallback(int count);
    std::optional<uchar> hexByte();
    bool nextWordIsKnownDestination() const;
    std::optional<QColor> color(int index) const;
    RunStyle runStyle(const CharacterState &ch) const;
    static std::optional<Paragraph::LineSpacing> lineSpacing(const ParagraphState &pa);

    const QByteArray m_bytes;
    qsizetype m_i = 0;
    GroupState m_state;
    QList<GroupState> m_stack;

    // Tables
    QHash<int, QString> m_fonts;
    QStringList m_fontOrder;
    int m_currentFont = 0;
    QString m_fontNameBuffer;
    QList<std::optional<QColor>> m_colors;
    struct { int r = 0, g = 0, b = 0; bool set = false; } m_colorBuffer;
    QList<std::optional<QColor>> m_expanded;
    QString m_expandedSpace;
    QList<int> m_expandedValues;

    // Output
    QList<Paragraph> m_paragraphs;
    QList<int> m_markerLengths;
    QList<Run> m_runs;
    QString m_pendingText;
    std::optional<CharacterState> m_pendingStyle;
    QString m_listMarker;
    int m_markerUnits = 0;
    ParagraphState m_paragraphOpenState;
    bool m_sawText = false;
    int m_documentDefaultTab = 0;
    std::optional<GroupState> m_documentEnd;
};

RtfDocument Reader::parse()
{
    if (!m_bytes.startsWith("{\\rtf")) {
        const QString words = QString::fromUtf8(m_bytes);
        RtfDocument plain;
        for (const QString &line : words.split(u'\n')) {
            Paragraph paragraph;
            if (!line.isEmpty())
                paragraph.runs << Run {line, RunStyle()};
            plain.paragraphs << paragraph;
            plain.markerLengths << 0;
        }
        return plain;
    }
    while (m_i < m_bytes.size()) {
        const uchar c = at(m_i);
        switch (c) {
        case '{':
            flushText();
            m_stack.append(m_state);
            ++m_i;
            break;
        case '}':
            flushText();
            endGroup();
            ++m_i;
            break;
        case '\\':
            controlWord();
            break;
        case 0x0A:
        case 0x0D:
            ++m_i;
            break;
        default:
            plainCharacter(c);
            ++m_i;
        }
    }
    flushText();
    endParagraph(true);
    RtfDocument document;
    document.paragraphs = m_paragraphs;
    document.markerLengths = m_markerLengths;
    document.fontNames = m_fontOrder;
    if (document.fontNames.isEmpty()) {
        QList<int> keys = m_fonts.keys();
        std::sort(keys.begin(), keys.end());
        for (int key : keys)
            document.fontNames << m_fonts.value(key);
    }
    return document;
}

void Reader::endGroup()
{
    if (m_state.destination == Destination::FontTable)
        finishFontName();
    // The document's own group closing: what was in force at its end is what the last
    // paragraph has, not the state from before the document began.
    if (m_stack.size() == 1)
        m_documentEnd = m_state;
    if (!m_stack.isEmpty())
        m_state = m_stack.takeLast();
}

void Reader::controlWord()
{
    ++m_i; // the backslash
    if (m_i >= m_bytes.size())
        return;
    const uchar c = at(m_i);

    // Control symbols: a backslash and one character that is not a letter.
    if (!isLetter(c)) {
        ++m_i;
        switch (c) {
        case '\\':
        case '{':
        case '}':
            plainCharacter(c);
            break;
        case 0x0A:
        case 0x0D:
            // Cocoa writes a paragraph's end as a backslash before the line break.
            paragraphBreak();
            break;
        case '~':
            appendUnit(0x00A0);
            break;
        case '_':
            appendUnit(0x2011);
            break;
        case '-':
            break; // an optional hyphen: not shown unless the word breaks there
        case '\'':
            if (const std::optional<uchar> byte = hexByte())
                appendUnit(windows1252(*byte));
            break;
        case '*':
            // "What follows is a destination that may be ignored": skipped, unless it is
            // one of the few this reader knows (looked at by the word after it).
            if (!nextWordIsKnownDestination())
                m_state.destination = Destination::Skip;
            break;
        case 0x09:
            appendUnit(0x09);
            break;
        default:
            break;
        }
        return;
    }

    // A control word: letters, then perhaps a signed number, then perhaps one space.
    const qsizetype start = m_i;
    while (m_i < m_bytes.size() && isLetter(at(m_i)))
        ++m_i;
    const QByteArray word = m_bytes.mid(start, m_i - start);
    std::optional<int> number;
    if (m_i < m_bytes.size() && (at(m_i) == '-' || isDigit(at(m_i)))) {
        const qsizetype numberStart = m_i;
        ++m_i;
        while (m_i < m_bytes.size() && isDigit(at(m_i)))
            ++m_i;
        bool ok = false;
        const int value = m_bytes.mid(numberStart, m_i - numberStart).toInt(&ok);
        if (ok)
            number = value;
    }
    if (m_i < m_bytes.size() && at(m_i) == ' ')
        ++m_i;
    apply(word, number);
}

void Reader::apply(const QByteArray &word, std::optional<int> n)
{
    // Destinations first: these change where the words of the group go.
    if (word == "fonttbl") {
        m_state.destination = Destination::FontTable;
        return;
    }
    if (word == "colortbl") {
        m_state.destination = Destination::ColorTable;
        m_colors.clear();
        return;
    }
    if (word == "expandedcolortbl") {
        m_state.destination = Destination::ExpandedColorTable;
        m_expanded.clear();
        return;
    }
    if (word == "listtext") {
        m_state.destination = Destination::ListText;
        return;
    }
    static const QList<QByteArray> skipped = {"info", "stylesheet", "pict", "NeXTGraphic", "header", "footer", "footnote",
                                              "fldinst", "listtable", "listoverridetable", "themedata",
                                              "colorschememapping", "latentstyles", "generator"};
    if (skipped.contains(word)) {
        m_state.destination = Destination::Skip;
        return;
    }
    if (m_state.destination == Destination::Skip)
        return;

    if (m_state.destination == Destination::FontTable) {
        if (word == "f") {
            finishFontName();
            m_currentFont = n.value_or(0);
        }
        return;
    }
    if (m_state.destination == Destination::ColorTable) {
        if (word == "red") {
            m_colorBuffer.r = n.value_or(0);
            m_colorBuffer.set = true;
        } else if (word == "green") {
            m_colorBuffer.g = n.value_or(0);
            m_colorBuffer.set = true;
        } else if (word == "blue") {
            m_colorBuffer.b = n.value_or(0);
            m_colorBuffer.set = true;
        }
        return;
    }
    if (m_state.destination == Destination::ExpandedColorTable) {
        static const QList<QByteArray> spaces = {"cssrgb", "csgray", "csgenericrgb", "csgenericgray", "csdisplayp3", "csdevicergb"};
        if (spaces.contains(word)) {
            m_expandedSpace = QString::fromLatin1(word);
            m_expandedValues.clear();
        } else if (word == "c") {
            m_expandedValues << n.value_or(0);
        }
        return;
    }

    CharacterState ch = m_state.character;
    ParagraphState pa = m_state.paragraph;
    // Paragraphs
    if (word == "par") {
        paragraphBreak();
        return;
    } else if (word == "line") {
        appendUnit(0x2028);
    } else if (word == "tab") {
        appendUnit(0x09);
    } else if (word == "pard") {
        pa = ParagraphState();
        pa.defaultTabTwips = m_documentDefaultTab;
    } else if (word == "deftab") {
        m_documentDefaultTab = n.value_or(0);
        pa.defaultTabTwips = m_documentDefaultTab;
    } else if (word == "pardeftab") {
        pa.defaultTabTwips = n.value_or(0);
    } else if (word == "ql" || word == "qn") {
        pa.alignment = Paragraph::Left;
    } else if (word == "qc") {
        pa.alignment = Paragraph::Center;
    } else if (word == "qr") {
        pa.alignment = Paragraph::Right;
    } else if (word == "qj") {
        pa.alignment = Paragraph::Justified;
    } else if (word == "li") {
        pa.leftTwips = n.value_or(0);
    } else if (word == "fi") {
        pa.firstTwips = n.value_or(0);
    } else if (word == "ri") {
        pa.rightTwips = n.value_or(0);
    } else if (word == "sb") {
        pa.spaceBeforeTwips = n.value_or(0);
    } else if (word == "sa") {
        pa.spaceAfterTwips = n.value_or(0);
    } else if (word == "sl") {
        pa.lineTwips = n.value_or(0);
    } else if (word == "slmult") {
        pa.lineMultiple = n.value_or(0) != 0;
    } else if (word == "slleading") {
        pa.leadingTwips = n.value_or(0);
    } else if (word == "tx") {
        pa.tabStops << n.value_or(0);
    } else if (word == "ls") {
        pa.isListItem = true;
    // Characters
    } else if (word == "plain") {
        ch = CharacterState();
    } else if (word == "f") {
        ch.font = n.value_or(0);
    } else if (word == "fs") {
        ch.halfPoints = n.value_or(24);
        ch.milliPoints.reset();
    } else if (word == "fsmilli") {
        ch.milliPoints = n;
    } else if (word == "b") {
        ch.bold = n.value_or(1) != 0;
    } else if (word == "i") {
        ch.italic = n.value_or(1) != 0;
    } else if (word == "ul" || word == "uld" || word == "uldb" || word == "ulth" || word == "ulw" || word == "uldash"
               || word == "uldashd" || word == "uldashdd" || word == "ulwave" || word == "ulhwave" || word == "ululdbwave") {
        ch.underline = n.value_or(1) != 0;
    } else if (word == "ulnone") {
        ch.underline = false;
    } else if (word == "ulc") {
        ch.underlineColor = n.value_or(0);
    } else if (word == "strike" || word == "striked") {
        ch.strike = n.value_or(1) != 0;
    } else if (word == "cf") {
        ch.color = n.value_or(0);
    } else if (word == "cb" || word == "highlight") {
        ch.background = n.value_or(0);
    } else if (word == "expnd") {
        // Cocoa writes both, \expnd (quarter points) and then \expndtw (twips, finer),
        // so the second has the last word.
        ch.spacingTwips = n.value_or(0) * 5;
    } else if (word == "expndtw") {
        ch.spacingTwips = n.value_or(0);
    } else if (word == "up") {
        ch.baselineHalfPoints = n.value_or(6);
    } else if (word == "dn") {
        ch.baselineHalfPoints = -n.value_or(6);
    } else if (word == "super") {
        ch.superscript = 1;
    } else if (word == "sub") {
        ch.superscript = -1;
    } else if (word == "nosupersub") {
        ch.superscript = 0;
    } else if (word == "caps") {
        ch.caps = n.value_or(1) != 0 ? RunStyle::AllCaps : RunStyle::NoCapitalization;
    } else if (word == "AppleTypeServicesF") {
        // A font feature, as macOS writes one: its type times 65536 and its selector.
        // KNOWN (ProToPPTX): ProPresenter's "small caps" is lower case type 37, selector
        // 1. A bare \AppleTypeServices ends the features.
        if (n && ((*n >> 16) == 37 || (*n >> 16) == 38))
            ch.caps = (*n & 0xFFFF) == 1 ? RunStyle::SmallCaps : RunStyle::NoCapitalization;
    } else if (word == "AppleTypeServices") {
        if (ch.caps == RunStyle::SmallCaps)
            ch.caps = RunStyle::NoCapitalization;
    } else if (word == "scaps") {
        ch.caps = n.value_or(1) != 0 ? RunStyle::SmallCaps : RunStyle::NoCapitalization;
    // Unicode
    } else if (word == "uc") {
        m_state.unicodeSkip = std::max(0, n.value_or(1));
    } else if (word == "u") {
        if (n) {
            int v = *n;
            if (v < 0)
                v += 65536;
            appendUnit(char16_t(v & 0xFFFF));
            skipFallback(m_state.unicodeSkip);
        }
    }
    m_state.character = ch;
    m_state.paragraph = pa;
}

void Reader::plainCharacter(uchar c)
{
    switch (m_state.destination) {
    case Destination::FontTable:
        if (c == ';')
            finishFontName();
        else
            m_fontNameBuffer.append(QChar(windows1252(c)));
        break;
    case Destination::ColorTable:
        if (c == ';') {
            m_colors << (m_colorBuffer.set ? std::optional<QColor>(QColor(m_colorBuffer.r, m_colorBuffer.g, m_colorBuffer.b))
                                           : std::nullopt);
            m_colorBuffer = {};
        }
        break;
    case Destination::ExpandedColorTable:
        if (c == ';') {
            m_expanded << expandedColor(m_expandedSpace, m_expandedValues);
            m_expandedSpace.clear();
            m_expandedValues.clear();
        }
        break;
    case Destination::Skip:
        break;
    case Destination::Text:
    case Destination::ListText:
        appendUnit(windows1252(c));
        break;
    }
}

void Reader::appendUnit(char16_t unit)
{
    switch (m_state.destination) {
    case Destination::ListText:
        m_listMarker.append(QChar(unit));
        // The marker is part of the text as ProPresenter counts it.
        appendToRun(unit, true);
        break;
    case Destination::Text:
        appendToRun(unit, false);
        break;
    default:
        break;
    }
}

void Reader::appendToRun(char16_t unit, bool marker)
{
    if (marker) {
        // Counted, but kept apart from the words: see RtfDocument::markerLengths.
        ++m_markerUnits;
        return;
    }
    if (m_pendingStyle && !(*m_pendingStyle == m_state.character) && !m_pendingText.isEmpty())
        flushText();
    if (m_pendingText.isEmpty()) {
        m_pendingStyle = m_state.character;
        if (!m_sawText)
            m_paragraphOpenState = m_state.paragraph;
    }
    m_sawText = true;
    m_pendingText.append(QChar(unit));
}

void Reader::flushText()
{
    if (m_pendingText.isEmpty() || !m_pendingStyle)
        return;
    const RunStyle style = runStyle(*m_pendingStyle);
    if (!m_runs.isEmpty() && m_runs.last().style == style)
        m_runs.last().text += m_pendingText;
    else
        m_runs << Run {m_pendingText, style};
    m_pendingText.clear();
}

void Reader::paragraphBreak()
{
    flushText();
    endParagraph(false);
}

void Reader::endParagraph(bool final)
{
    // The paragraph's settings are those in force where its text began, or at its end if
    // it has none (an empty line).
    const GroupState closing = final ? m_documentEnd.value_or(m_state) : m_state;
    const ParagraphState pa = m_sawText ? m_paragraphOpenState : closing.paragraph;
    Paragraph paragraph;
    paragraph.alignment = pa.alignment;
    paragraph.lineSpacing = lineSpacing(pa);
    paragraph.extraLeading = pa.leadingTwips / 20.0;
    paragraph.spaceBefore = pa.spaceBeforeTwips / 20.0;
    paragraph.spaceAfter = pa.spaceAfterTwips / 20.0;
    paragraph.leftIndent = pa.leftTwips / 20.0;
    paragraph.firstLineIndent = pa.firstTwips / 20.0;
    paragraph.rightIndent = pa.rightTwips / 20.0;
    // (Cocoa writes stops left of the margin, \tx-100, that macOS ignores.)
    for (int stop : pa.tabStops) {
        if (stop > 0)
            paragraph.tabStops << stop / 20.0;
    }
    paragraph.defaultTabInterval = pa.defaultTabTwips / 20.0;
    paragraph.runs = m_runs;
    paragraph.endStyle = runStyle(closing.character);
    if (pa.isListItem || m_markerUnits > 0)
        paragraph.bullet = bulletFromMarker(m_listMarker);
    // Cocoa ends the last paragraph without a \par, so what follows the last break is
    // always a paragraph, if an empty one: "A", a break and nothing is two lines, the
    // second empty, as macOS lays it out.
    m_paragraphs << paragraph;
    m_markerLengths << m_markerUnits;
    m_runs.clear();
    m_listMarker.clear();
    m_markerUnits = 0;
    m_sawText = false;
}

void Reader::finishFontName()
{
    const QString name = m_fontNameBuffer.trimmed();
    if (!name.isEmpty()) {
        m_fonts.insert(m_currentFont, name);
        if (!m_fontOrder.contains(name))
            m_fontOrder << name;
    }
    m_fontNameBuffer.clear();
}

void Reader::skipFallback(int count)
{
    // After a Unicode character's \u and its number come `count` characters for readers
    // that do not know Unicode: a character, or a \'hh, each counting one.
    int left = count;
    while (left > 0 && m_i < m_bytes.size()) {
        if (at(m_i) == '\\' && m_i + 1 < m_bytes.size() && at(m_i + 1) == '\'')
            m_i += 4;
        else if (at(m_i) == '{' || at(m_i) == '}' || at(m_i) == '\\')
            return;
        else
            ++m_i;
        --left;
    }
}

std::optional<uchar> Reader::hexByte()
{
    if (m_i + 2 > m_bytes.size())
        return std::nullopt;
    bool ok = false;
    const int v = m_bytes.mid(m_i, 2).toInt(&ok, 16);
    if (!ok)
        return std::nullopt;
    m_i += 2;
    return uchar(v);
}

bool Reader::nextWordIsKnownDestination() const
{
    qsizetype j = m_i;
    if (j >= m_bytes.size() || at(j) != '\\')
        return false;
    ++j;
    const qsizetype start = j;
    while (j < m_bytes.size() && isLetter(at(j)))
        ++j;
    return m_bytes.mid(start, j - start) == "expandedcolortbl";
}

std::optional<QColor> Reader::color(int index) const
{
    if (index >= 0 && index < m_expanded.size() && m_expanded.at(index))
        return m_expanded.at(index);
    if (index >= 0 && index < m_colors.size())
        return m_colors.at(index);
    return std::nullopt;
}

RunStyle Reader::runStyle(const CharacterState &ch) const
{
    RunStyle style;
    style.size = ch.milliPoints ? *ch.milliPoints / 1000.0 : ch.halfPoints / 2.0;
    style.fontName = m_fonts.value(ch.font, m_fontOrder.value(0, QStringLiteral("Helvetica")));
    style.fontFamily.clear();
    style.bold = ch.bold;
    style.italic = ch.italic;
    style.underline = ch.underline;
    if (ch.underlineColor != 0)
        style.underlineColor = color(ch.underlineColor);
    style.strikethrough = ch.strike;
    // Colour 0 is "automatic": macOS draws it black.
    style.fill = color(ch.color).value_or(QColor(Qt::black));
    style.characterSpacing = ch.spacingTwips / 20.0;
    style.baselineOffset = ch.baselineHalfPoints / 2.0;
    style.superscript = ch.superscript;
    style.capitalization = ch.caps;
    if (ch.background != 0)
        style.highlight = color(ch.background);
    return style;
}

std::optional<Paragraph::LineSpacing> Reader::lineSpacing(const ParagraphState &pa)
{
    if (pa.lineTwips == 0)
        return std::nullopt;
    if (pa.lineMultiple)
        return Paragraph::LineSpacing {Paragraph::LineSpacing::Multiple, pa.lineTwips / 240.0};
    if (pa.lineTwips > 0)
        return Paragraph::LineSpacing {Paragraph::LineSpacing::AtLeast, pa.lineTwips / 20.0};
    return Paragraph::LineSpacing {Paragraph::LineSpacing::Exactly, -pa.lineTwips / 20.0};
}

} // namespace

RtfDocument readRtf(const QByteArray &rtf)
{
    Reader reader(rtf);
    return reader.parse();
}

std::optional<Paragraph::Bullet> bulletFromMarker(const QString &marker)
{
    const QString m = marker.trimmed();
    if (m.isEmpty())
        return std::nullopt;
    static const QList<std::pair<QRegularExpression, QString>> schemes = {
        {QRegularExpression(QStringLiteral("^(\\d+)\\.$")), QStringLiteral("arabicPeriod")},
        {QRegularExpression(QStringLiteral("^(\\d+)\\)$")), QStringLiteral("arabicParenR")},
        {QRegularExpression(QStringLiteral("^\\((\\d+)\\)$")), QStringLiteral("arabicParenBoth")},
        {QRegularExpression(QStringLiteral("^(\\d+)$")), QStringLiteral("arabicPlain")},
    };
    for (const auto &[pattern, scheme] : schemes) {
        const QRegularExpressionMatch match = pattern.match(m);
        if (match.hasMatch()) {
            Paragraph::Bullet bullet;
            bullet.numberScheme = scheme;
            bullet.startAt = std::max(1, match.captured(1).toInt());
            return bullet;
        }
    }
    if (m.size() == 2 && m.at(0).isLetter() && m.at(1) == u'.') {
        Paragraph::Bullet bullet;
        bullet.numberScheme = m.at(0).isUpper() ? QStringLiteral("alphaUcPeriod") : QStringLiteral("alphaLcPeriod");
        bullet.startAt = std::max(1, int(m.at(0).toLower().unicode()) - 96);
        return bullet;
    }
    Paragraph::Bullet bullet;
    bullet.character = m.left(1);
    return bullet;
}

} // namespace pptx
