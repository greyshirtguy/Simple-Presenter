#include "proconvert.h"

#include "fontresolver.h"
#include "rtf.h"
#include "rtfwriter.h"
#include "workspacefiles.h"

#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QPointF>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <climits>

namespace proconvert {

namespace {

using Text = rv::data::Graphics::Text;
using Attributes = rv::data::Graphics::Text::Attributes;
using DataLink = rv::data::Slide::Element::DataLink;

int toQtVerticalAlignment(Text::VerticalAlignment alignment)
{
    switch (alignment) {
    case Text::VERTICAL_ALIGNMENT_TOP:
        return Qt::AlignTop;
    case Text::VERTICAL_ALIGNMENT_BOTTOM:
        return Qt::AlignBottom;
    default:
        return Qt::AlignVCenter;
    }
}

void addFontHint(RtfDefaults *defaults, const rv::data::Font &font)
{
    if (!font.name().empty() && !font.family().empty())
        defaults->familyForPostScriptName.insert(QString::fromStdString(font.name()), QString::fromStdString(font.family()));
}

Qt::Alignment toQtAlignment(Attributes::Alignment alignment)
{
    switch (alignment) {
    case Attributes::ALIGNMENT_CENTER:
        return Qt::AlignHCenter;
    case Attributes::ALIGNMENT_RIGHT:
        return Qt::AlignRight;
    case Attributes::ALIGNMENT_JUSTIFIED:
        return Qt::AlignJustify;
    default:
        return Qt::AlignLeft;
    }
}

// The format the file keeps beside the text, which is that of the start of the text.
// It is all there is to go by for a text box with nothing in it yet.
TextRun runFromAttributes(const Attributes &attributes)
{
    TextRun run;
    const rv::data::Font &font = attributes.font();
    const ResolvedFont resolved = resolvePostScriptName(QString::fromStdString(font.name()),
                                                        QString::fromStdString(font.family()));
    run.fontName = QString::fromStdString(font.name());
    run.family = resolved.family;
    run.bold = resolved.bold || font.bold();
    run.italic = resolved.italic || font.italic();
    run.size = font.size() > 0 ? font.size() : 60;
    run.fill = attributes.has_text_solid_fill() ? toColor(attributes.text_solid_fill()) : QColor(Qt::white);
    run.capitalization = int(attributes.capitalization());
    run.underline = attributes.underline_style().style() != Attributes::Underline::STYLE_NONE;
    run.strikethrough = attributes.strikethrough_style().style() != Attributes::Underline::STYLE_NONE;
    run.kerning = attributes.kerning();
    run.superscript = attributes.superscript();
    // A percentage of the font size, negative when the glyphs are filled as well.
    if (attributes.stroke_width() != 0) {
        run.strokeWidth = qAbs(attributes.stroke_width()) / 100.0 * run.size;
        run.fillVisible = attributes.stroke_width() < 0;
        run.stroke = toColor(attributes.stroke_color());
    }
    return run;
}

struct CapsRange
{
    int start;
    int end; // exclusive
    int capitalization;
};

// Gives each stretch of text the capitalisation of the range covering it. Ranges count
// characters through the whole text, a paragraph break counting as one.
void applyCapitalization(RichText *text, const QList<CapsRange> &ranges)
{
    const auto capsAt = [&ranges](int position) {
        for (const CapsRange &range : ranges) {
            if (position >= range.start && position < range.end)
                return range.capitalization;
        }
        return int(TextRun::NoCapitalization);
    };

    int offset = 0;
    for (TextParagraph &paragraph : text->paragraphs) {
        QList<TextRun> pieces;
        for (const TextRun &run : std::as_const(paragraph.runs)) {
            const int runStart = offset;
            const int runEnd = offset + int(run.text.size());
            // Cut the run wherever a range begins or ends inside it.
            QList<int> cuts {runStart, runEnd};
            for (const CapsRange &range : ranges) {
                for (const int edge : {range.start, range.end}) {
                    if (edge > runStart && edge < runEnd && !cuts.contains(edge))
                        cuts.append(edge);
                }
            }
            std::sort(cuts.begin(), cuts.end());
            for (qsizetype i = 0; i + 1 < cuts.size() || (i == 0 && runStart == runEnd); ++i) {
                TextRun piece = run;
                const int from = cuts.at(i);
                const int to = runStart == runEnd ? from : cuts.at(i + 1);
                piece.text = run.text.mid(from - runStart, to - from);
                piece.capitalization = capsAt(from);
                if (!pieces.isEmpty() && pieces.last().sameFormat(piece))
                    pieces.last().text += piece.text;
                else
                    pieces.append(piece);
                if (runStart == runEnd)
                    break;
            }
            offset = runEnd;
        }
        paragraph.runs = pieces;
        ++offset; // the paragraph break
    }
}

QString joined(const QStringList &lines, const QString &separator)
{
    QStringList kept;
    for (const QString &line : lines) {
        if (!line.trimmed().isEmpty())
            kept << line.trimmed();
    }
    return kept.join(separator);
}

QString onePerLine(const QString &text, bool words)
{
    QStringList parts;
    if (words) {
        parts = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    } else {
        for (const QChar c : text) {
            if (!c.isSpace())
                parts << QString(c);
        }
    }
    return parts.join(u'\n');
}

// What linking to another element's text can do to it on the way.
QString linkTransformed(const QString &text, int transform)
{
    using Link = DataLink::AlternateElementText;
    switch (transform) {
    case Link::TEXT_TRANSFORM_OPTION_REMOVE_LINE_RETURNS:
        return joined(text.split(u'\n'), QStringLiteral(" "));
    case Link::TEXT_TRANSFORM_OPTION_ONE_WORD_PER_LINE:
        return onePerLine(text, true);
    case Link::TEXT_TRANSFORM_OPTION_ONE_CHARACTER_PER_LINE:
        return onePerLine(text, false);
    default:
        return text;
    }
}

// What a text element can do to its own text, however it came by it.
QString elementTransformed(const QString &text, int transform, const QString &delimiter)
{
    switch (transform) {
    case Text::TRANSFORM_SINGLE_LINE:
        return joined(text.split(u'\n'), QStringLiteral(" "));
    case Text::TRANSFORM_ONE_WORD_PER_LINE:
        return onePerLine(text, true);
    case Text::TRANSFORM_ONE_CHARACTER_PER_LINE:
        return onePerLine(text, false);
    case Text::TRANSFORM_REPLACE_LINE_RETURNS:
        return joined(text.split(u'\n'), delimiter);
    default:
        return text;
    }
}

void insertShadow(QVariantMap *map, const QString &prefix, const rv::data::Graphics::Shadow &shadow)
{
    const QColor base = toColor(shadow.color());
    QColor drawn = base;
    drawn.setAlphaF(base.alphaF() * qBound(0.0, shadow.opacity(), 1.0));
    // The angle is measured anticlockwise with y pointing up; slides have y pointing down.
    const double angle = qDegreesToRadians(shadow.angle());
    map->insert(prefix + "Enabled", shadow.enable());
    map->insert(prefix + "Color", drawn);
    map->insert(prefix + "OffsetX", shadow.offset() * qCos(angle));
    map->insert(prefix + "OffsetY", -shadow.offset() * qSin(angle));
    map->insert(prefix + "Radius", shadow.radius());
    map->insert(prefix + "Angle", shadow.angle());
    map->insert(prefix + "Offset", shadow.offset());
}

// A name for a kind of link this app does not act on, to show that it is there.
QString otherLinkLabel(const DataLink &link)
{
    switch (link.PropertyType_case()) {
    case DataLink::kTicker: return QStringLiteral("Ticker");
    case DataLink::kTimerText: return QStringLiteral("Timer");
    case DataLink::kClockText: return QStringLiteral("Clock");
    case DataLink::kSlideText: return QStringLiteral("Slide text");
    case DataLink::kCcliText: return QStringLiteral("CCLI");
    case DataLink::kGroupName: return QStringLiteral("Group name");
    case DataLink::kPresentationNotes: return QStringLiteral("Presentation notes");
    case DataLink::kPlaylistItem: return QStringLiteral("Playlist item");
    case DataLink::kVideoCountdown: return QStringLiteral("Video countdown");
    case DataLink::kAudioCountdown: return QStringLiteral("Audio countdown");
    case DataLink::kStageMessage: return QStringLiteral("Stage message");
    case DataLink::kSlideCount: return QStringLiteral("Slide count");
    case DataLink::kSlideLabelText: return QStringLiteral("Slide label");
    default: return QString();
    }
}

// The same for a visibility condition.
QString otherConditionLabel(const DataLink::VisibilityLink::Condition &condition)
{
    using Condition = DataLink::VisibilityLink::Condition;
    switch (condition.ConditionType_case()) {
    case Condition::kTimerVisibility: return QStringLiteral("Timer “%1”").arg(QString::fromStdString(condition.timer_visibility().timer_name()));
    case Condition::kVideoCountdownVisibility: return QStringLiteral("Video countdown");
    case Condition::kAudioCountdownVisibility: return QStringLiteral("Audio countdown");
    case Condition::kCaptureSessionVisibility: return QStringLiteral("Capture");
    case Condition::kVideoInputVisibility: return QStringLiteral("Video input");
    default: return QStringLiteral("Something else");
    }
}

QVariantMap toElementMap(const rv::data::Slide::Element &slideElement)
{
    const rv::data::Graphics::Element &element = slideElement.element();
    const rv::data::Graphics::Fill &fill = element.fill();
    QVariantMap map {
        {"id", QString::fromStdString(element.uuid().string())},
        {"name", QString::fromStdString(element.name())},
        {"x", element.bounds().origin().x()},
        {"y", element.bounds().origin().y()},
        {"width", element.bounds().size().width()},
        {"height", element.bounds().size().height()},
        {"rotation", element.rotation()},
        {"opacity", element.opacity()},
        {"locked", element.locked()},
        {"hidden", element.hidden()},
        {"fillOn", fill.enable()},
        {"fillKind", fill.has_color() ? QStringLiteral("color") : fill.has_gradient() ? QStringLiteral("gradient")
                     : fill.has_media() ? QStringLiteral("media")
                     : fill.FillType_case() == rv::data::Graphics::Fill::FILLTYPE_NOT_SET ? QStringLiteral("none")
                     : QStringLiteral("other")},
        {"fillEnabled", fill.enable() && fill.has_color()},
        {"fillColor", toColor(fill.color())},
        {"fillLinesOnly", element.text_line_mask().enabled()},
        {"lineMaskStyle", int(element.text_line_mask().mask_style())},
        {"lineMaskWidthOffset", element.text_line_mask().width_offset()},
        {"lineMaskHeightOffset", element.text_line_mask().height_offset()},
        {"lineMaskHorizontalOffset", element.text_line_mask().horizontal_offset()},
        {"lineMaskVerticalOffset", element.text_line_mask().vertical_offset()},
        {"strokeOn", element.stroke().enable()},
        {"strokeEnabled", element.stroke().enable() && element.stroke().width() > 0},
        {"strokeColor", toColor(element.stroke().color())},
        {"strokeWidth", element.stroke().width()},
    };
    insertShadow(&map, QStringLiteral("shadow"), element.shadow());
    insertShadow(&map, QStringLiteral("textShadow"), element.text().shadow());

    const Text &text = element.text();
    map.insert("text", QVariant::fromValue(readText(text)));
    map.insert("verticalAlignment", toQtVerticalAlignment(text.vertical_alignment()));
    map.insert("marginLeft", text.margins().left());
    map.insert("marginTop", text.margins().top());
    map.insert("marginRight", text.margins().right());
    map.insert("marginBottom", text.margins().bottom());
    map.insert("textTransform", int(text.transform()));
    map.insert("textTransformDelimiter", QString::fromStdString(text.transformdelimiter()));

    map.insert("linkKind", QStringLiteral("none"));
    map.insert("linkElementId", QString());
    map.insert("linkElementName", QString());
    map.insert("linkTransform", 0);
    map.insert("linkLabel", QString());
    map.insert("visibilityRules", false);
    map.insert("visibilityCriterion", 0);
    map.insert("visibilityConditions", QVariantList());
    for (const DataLink &link : slideElement.data_links()) {
        if (link.has_alternate_text()) {
            map.insert("linkKind", QStringLiteral("element"));
            map.insert("linkElementId", QString::fromStdString(link.alternate_text().other_element_uuid().string()));
            map.insert("linkElementName", QString::fromStdString(link.alternate_text().other_element_name()));
            map.insert("linkTransform", int(link.alternate_text().text_transform()));
        } else if (link.has_visibility_link()) {
            QVariantList conditions;
            for (int index = 0; index < link.visibility_link().conditions_size(); ++index) {
                const auto &condition = link.visibility_link().conditions(index);
                using Criterion = DataLink::VisibilityLink::Condition::ElementVisibility;
                if (condition.has_element_visibility()) {
                    const auto &other = condition.element_visibility();
                    conditions.append(QVariantMap {
                        {"kind", QStringLiteral("element")},
                        {"elementId", QString::fromStdString(other.other_element_uuid().string())},
                        {"elementName", QString::fromStdString(other.other_element_name())},
                        {"hasText", other.visibility_criterion() == Criterion::ELEMENT_VISIBILITY_CRITERION_HAS_TEXT},
                    });
                } else {
                    conditions.append(QVariantMap {
                        {"kind", QStringLiteral("other")},
                        {"index", index},
                        {"label", otherConditionLabel(condition)},
                    });
                }
            }
            map.insert("visibilityRules", true);
            map.insert("visibilityCriterion", int(link.visibility_link().visibility_criterion()));
            map.insert("visibilityConditions", conditions);
        } else if (!otherLinkLabel(link).isEmpty()) {
            map.insert("linkKind", QStringLiteral("other"));
            map.insert("linkLabel", otherLinkLabel(link));
        }
    }
    return map;
}

// The element a link or a visibility rule refers to: by id, or failing that by name,
// which is how a rule copied from another slide still finds its target.
qsizetype findElement(const QVariantList &elements, const QString &id, const QString &name)
{
    for (qsizetype i = 0; i < elements.size(); ++i) {
        if (!id.isEmpty() && elements.at(i).toMap().value("id").toString() == id)
            return i;
    }
    for (qsizetype i = 0; i < elements.size(); ++i) {
        if (!name.isEmpty() && elements.at(i).toMap().value("name").toString() == name)
            return i;
    }
    return -1;
}

rv::data::Slide::Element *findSlideElement(rv::data::Slide *slide, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (rv::data::Slide::Element &element : *slide->mutable_elements()) {
        if (element.element().uuid().string() == wanted)
            return &element;
    }
    return nullptr;
}

void applyShadow(rv::data::Graphics::Shadow *shadow, const QString &prefix, const QVariantMap &changes)
{
    if (changes.contains(prefix + "Enabled"))
        shadow->set_enable(changes.value(prefix + "Enabled").toBool());
    if (changes.contains(prefix + "Color")) {
        // The colour as drawn: how see-through it is is the shadow's opacity.
        const QColor drawn = changes.value(prefix + "Color").value<QColor>();
        QColor solid = drawn;
        solid.setAlphaF(1);
        setColor(shadow->mutable_color(), solid);
        shadow->set_opacity(drawn.alphaF());
    }
    if (changes.contains(prefix + "Angle"))
        shadow->set_angle(changes.value(prefix + "Angle").toDouble());
    if (changes.contains(prefix + "Offset"))
        shadow->set_offset(qMax(0.0, changes.value(prefix + "Offset").toDouble()));
    if (changes.contains(prefix + "Radius"))
        shadow->set_radius(qMax(0.0, changes.value(prefix + "Radius").toDouble()));
}

bool anyKeyStartsWith(const QVariantMap &map, const QString &prefix)
{
    const auto from = map.lowerBound(prefix);
    return from != map.constEnd() && from.key().startsWith(prefix);
}

// Removes the links for which `matches` holds.
template <typename Matches>
void removeLinks(rv::data::Slide::Element *element, Matches matches)
{
    for (int i = element->data_links_size() - 1; i >= 0; --i) {
        if (matches(element->data_links(i)))
            element->mutable_data_links()->DeleteSubrange(i, 1);
    }
}

} // namespace

bool readPresentation(const QString &path, rv::data::Presentation *presentation, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot open %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    if (!presentation->ParseFromArray(data.constData(), int(data.size()))) {
        *error = QStringLiteral("%1 is not a ProPresenter 7 presentation").arg(QFileInfo(path).fileName());
        return false;
    }
    return true;
}

QString writePresentation(const QString &path, const rv::data::Presentation &presentation)
{
    std::string bytes;
    if (!presentation.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode %1").arg(QFileInfo(path).fileName());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size())
        || !file.commit())
        return QStringLiteral("Cannot write %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return {};
}

QColor toColor(const rv::data::Color &color)
{
    const auto unit = [](float v) { return qBound(0.0f, v, 1.0f); };
    return QColor::fromRgbF(unit(color.red()), unit(color.green()), unit(color.blue()), unit(color.alpha()));
}

void setColor(rv::data::Color *target, const QColor &color)
{
    target->set_red(color.redF());
    target->set_green(color.greenF());
    target->set_blue(color.blueF());
    target->set_alpha(color.alphaF());
}

RichText readText(const Text &text)
{
    const Attributes &attributes = text.attributes();
    // A text box with nothing in it still has a format, for what is typed into it or
    // linked into it: one empty paragraph says so.
    const auto blank = [&attributes] {
        RichText rich;
        TextParagraph paragraph;
        paragraph.alignment = toQtAlignment(attributes.paragraph_style().alignment());
        paragraph.runs.append(runFromAttributes(attributes));
        rich.paragraphs.append(paragraph);
        return rich;
    };
    if (text.rtf_data().empty())
        return blank();
    RtfDefaults defaults;
    if (attributes.has_text_solid_fill())
        defaults.textColor = toColor(attributes.text_solid_fill());
    addFontHint(&defaults, attributes.font());
    QList<CapsRange> ranges;
    for (const auto &custom : attributes.custom_attributes()) {
        if (custom.has_original_font())
            addFontHint(&defaults, custom.original_font());
        if (custom.has_capitalization())
            ranges.append({custom.range().start(), custom.range().end(), int(custom.capitalization())});
    }
    RichText rich = parseRtf(QByteArray::fromStdString(text.rtf_data()), defaults);
    if (rich.paragraphs.isEmpty())
        return blank();

    // With no ranges, the capitalisation given for the text as a whole applies to all of it.
    if (ranges.isEmpty() && attributes.capitalization() != Attributes::CAPITALIZATION_NONE)
        ranges.append({0, INT_MAX, int(attributes.capitalization())});
    if (!ranges.isEmpty())
        applyCapitalization(&rich, ranges);
    return rich;
}

void writeText(Text *text, const RichText &rich)
{
    // Ranges are tied to character positions. The ones this app does not understand
    // (a chord, a gradient over part of the text) can stay only while the characters
    // do: when the text is being given a new format but is otherwise the same.
    QList<Attributes::CustomAttribute> kept;
    if (readText(*text).plainText() == rich.plainText()) {
        for (const auto &custom : text->attributes().custom_attributes()) {
            switch (custom.Attribute_case()) {
            // These are written afresh below, or describe a scaling of the old font.
            case Attributes::CustomAttribute::kCapitalization:
            case Attributes::CustomAttribute::kOriginalFont:
            case Attributes::CustomAttribute::kOriginalFontSize:
            case Attributes::CustomAttribute::kFontScaleFactor:
                break;
            default:
                kept.append(custom);
                break;
            }
        }
    }

    text->set_rtf_data(writeRtf(rich).toStdString());

    Attributes *attributes = text->mutable_attributes();
    const TextRun first = rich.firstRun();
    const QString fontName = first.fontName.isEmpty() ? postScriptNameFor(first.family, first.bold, first.italic)
                                                      : first.fontName;
    rv::data::Font *font = attributes->mutable_font();
    font->set_name(fontName.toStdString());
    font->set_family(first.family.toStdString());
    font->set_size(first.size);
    font->set_bold(first.bold);
    font->set_italic(first.italic);
    font->clear_face();

    attributes->set_capitalization(Attributes::Capitalization(first.capitalization));
    // A gradient or media fill for the text is not something the text itself can say;
    // it stays as it is, and only a plain colour is kept in step with the text.
    if (attributes->fill_case() == Attributes::kTextSolidFill || attributes->fill_case() == Attributes::FILL_NOT_SET)
        setColor(attributes->mutable_text_solid_fill(), first.fill);
    attributes->mutable_underline_style()->set_style(first.underline ? Attributes::Underline::STYLE_SINGLE
                                                                    : Attributes::Underline::STYLE_NONE);
    attributes->mutable_strikethrough_style()->set_style(first.strikethrough ? Attributes::Underline::STYLE_SINGLE
                                                                            : Attributes::Underline::STYLE_NONE);
    attributes->set_kerning(first.kerning);
    attributes->set_superscript(first.superscript);
    // The stroke is a percentage of the font size, negative when the glyphs are filled too.
    if (first.strokeWidth > 0 && first.size > 0) {
        const double percent = first.strokeWidth / first.size * 100;
        attributes->set_stroke_width(first.fillVisible ? -percent : percent);
        setColor(attributes->mutable_stroke_color(), first.stroke);
    } else {
        attributes->set_stroke_width(0);
        attributes->clear_stroke_color();
    }
    if (!rich.paragraphs.isEmpty()) {
        const Qt::Alignment alignment = rich.paragraphs.first().alignment;
        Attributes::Paragraph *paragraph = attributes->mutable_paragraph_style();
        // "Natural" alignment is left for left-to-right text; keep it if that is what was there.
        const bool natural = paragraph->alignment() == Attributes::ALIGNMENT_NATURAL;
        paragraph->set_alignment(alignment & Qt::AlignHCenter ? Attributes::ALIGNMENT_CENTER
                                 : alignment & Qt::AlignRight ? Attributes::ALIGNMENT_RIGHT
                                 : alignment & Qt::AlignJustify ? Attributes::ALIGNMENT_JUSTIFIED
                                 : natural ? Attributes::ALIGNMENT_NATURAL : Attributes::ALIGNMENT_LEFT);
    }

    // Ranges count characters through the whole text, a paragraph break counting as one,
    // and must stay inside it: there is no break after the last paragraph.
    attributes->clear_custom_attributes();
    QList<CapsRange> capitalized;
    const auto capitalize = [&capitalized](int start, int end, int capitalization) {
        if (capitalization == TextRun::NoCapitalization || end <= start)
            return;
        // Neighbouring stretches of the same capitalisation share a range.
        if (!capitalized.isEmpty() && capitalized.last().end == start && capitalized.last().capitalization == capitalization)
            capitalized.last().end = end;
        else
            capitalized.append({start, end, capitalization});
    };
    struct FontRange
    {
        int start;
        int end;
        const TextRun *run;
    };
    QList<FontRange> fonts;

    int offset = 0;
    for (qsizetype p = 0; p < rich.paragraphs.size(); ++p) {
        const TextParagraph &paragraph = rich.paragraphs.at(p);
        const int paragraphStart = offset;
        for (const TextRun &run : paragraph.runs) {
            const int end = offset + int(run.text.size());
            capitalize(offset, end, run.capitalization);
            // The RTF names fonts by PostScript name only. The family of the first is
            // kept above; the family of any other goes with its range, where
            // ProPresenter keeps the font a stretch of text was set in.
            if (end > offset && !run.fontName.isEmpty() && run.fontName != fontName)
                fonts.append({offset, end, &run});
            offset = end;
        }
        if (p + 1 < rich.paragraphs.size()) {
            // A paragraph with nothing in it has only its break to carry its capitalisation.
            if (offset == paragraphStart && !paragraph.runs.isEmpty())
                capitalize(offset, offset + 1, paragraph.runs.first().capitalization);
            ++offset;
        }
    }

    for (const CapsRange &range : std::as_const(capitalized)) {
        auto *custom = attributes->add_custom_attributes();
        custom->mutable_range()->set_start(range.start);
        custom->mutable_range()->set_end(range.end);
        custom->set_capitalization(Attributes::Capitalization(range.capitalization));
    }
    for (const FontRange &range : std::as_const(fonts)) {
        auto *custom = attributes->add_custom_attributes();
        custom->mutable_range()->set_start(range.start);
        custom->mutable_range()->set_end(range.end);
        rv::data::Font *original = custom->mutable_original_font();
        original->set_name(range.run->fontName.toStdString());
        original->set_family(range.run->family.toStdString());
        original->set_size(range.run->size);
        original->set_bold(range.run->bold);
        original->set_italic(range.run->italic);
    }
    for (const auto &custom : std::as_const(kept))
        *attributes->add_custom_attributes() = custom;
}

QVariantMap toSlideMap(const rv::data::Slide &slide, const QString &label)
{
    QVariantList elements;
    for (const rv::data::Slide::Element &element : slide.elements())
        elements.append(toElementMap(element));

    // The text each element shows: its own, or that of the element it is linked to,
    // then put through its own transform.
    QList<RichText> shown;
    for (const QVariant &entry : std::as_const(elements)) {
        const QVariantMap element = entry.toMap();
        const RichText own = element.value("text").value<RichText>();
        RichText display = own;
        const auto restyled = [&own](const QString &text) {
            return RichText::plain(text, own.firstRun(),
                                   own.paragraphs.isEmpty() ? Qt::AlignHCenter : own.paragraphs.first().alignment);
        };
        if (element.value("linkKind").toString() == QLatin1String("element")) {
            const qsizetype source = findElement(elements, element.value("linkElementId").toString(),
                                                 element.value("linkElementName").toString());
            if (source >= 0) {
                const QString text = elements.at(source).toMap().value("text").value<RichText>().plainText();
                display = restyled(linkTransformed(text, element.value("linkTransform").toInt()));
            }
        }
        const int transform = element.value("textTransform").toInt();
        if (transform != Text::TRANSFORM_NONE)
            display = restyled(elementTransformed(display.plainText(), transform,
                                                  element.value("textTransformDelimiter").toString()));
        shown.append(display);
    }

    QStringList texts;
    for (qsizetype i = 0; i < elements.size(); ++i) {
        QVariantMap element = elements.at(i).toMap();
        bool visible = !element.value("hidden").toBool();
        if (visible && element.value("visibilityRules").toBool()) {
            // A rule about something this app does not track counts as met.
            int met = 0;
            const QVariantList conditions = element.value("visibilityConditions").toList();
            for (const QVariant &entry : conditions) {
                const QVariantMap condition = entry.toMap();
                if (condition.value("kind").toString() != QLatin1String("element")) {
                    ++met;
                    continue;
                }
                const qsizetype other = findElement(elements, condition.value("elementId").toString(),
                                                    condition.value("elementName").toString());
                const bool hasText = other >= 0 && !shown.at(other).plainText().trimmed().isEmpty();
                if (hasText == condition.value("hasText").toBool())
                    ++met;
            }
            switch (element.value("visibilityCriterion").toInt()) {
            case DataLink::VisibilityLink::VISIBILITY_CRITERION_ANY:
                visible = conditions.isEmpty() || met > 0;
                break;
            case DataLink::VisibilityLink::VISIBILITY_CRITERION_NONE:
                visible = met == 0;
                break;
            default:
                visible = met == conditions.size();
                break;
            }
        }
        element.insert("displayText", QVariant::fromValue(shown.at(i)));
        element.insert("hasText", !shown.at(i).isEmpty());
        element.insert("visible", visible);
        elements[i] = element;
        // The slide's words are those of the elements that show, once each: an element
        // that repeats another's text adds nothing to them.
        const bool repeats = element.value("linkKind").toString() == QLatin1String("element")
            && findElement(elements, element.value("linkElementId").toString(), element.value("linkElementName").toString()) >= 0;
        const QString text = shown.at(i).plainText().trimmed();
        if (visible && !repeats && !text.isEmpty())
            texts << text;
    }

    const bool hasSize = slide.size().width() > 0 && slide.size().height() > 0;
    return {
        {"width", hasSize ? slide.size().width() : 1920.0},
        {"height", hasSize ? slide.size().height() : 1080.0},
        {"drawsBackground", slide.draws_background_color()},
        {"backgroundColor", toColor(slide.background_color())},
        {"label", label},
        {"group", QString()},
        {"groupColor", QString()},
        {"groupStart", false},
        {"mediaName", QString()},
        {"mediaForeground", false},
        {"timerActions", QVariantList()},
        {"plainText", texts.join(u'\n')},
        {"elements", elements},
    };
}

bool applyChanges(rv::data::Slide *slide, const QString &elementId, const QVariantMap &changes)
{
    rv::data::Slide::Element *slideElement = findSlideElement(slide, elementId);
    if (!slideElement)
        return false;
    rv::data::Graphics::Element *element = slideElement->mutable_element();
    const auto number = [&changes](const char *key) { return changes.value(QLatin1String(key)).toDouble(); };
    const auto name = [slide](const QString &id) {
        const rv::data::Slide::Element *other = findSlideElement(slide, id);
        return other ? other->element().name() : std::string();
    };

    if (changes.contains("x"))
        element->mutable_bounds()->mutable_origin()->set_x(number("x"));
    if (changes.contains("y"))
        element->mutable_bounds()->mutable_origin()->set_y(number("y"));
    if (changes.contains("width"))
        element->mutable_bounds()->mutable_size()->set_width(qMax(1.0, number("width")));
    if (changes.contains("height"))
        element->mutable_bounds()->mutable_size()->set_height(qMax(1.0, number("height")));
    if (changes.contains("opacity"))
        element->set_opacity(qBound(0.0, number("opacity"), 1.0));
    if (changes.contains("locked"))
        element->set_locked(changes.value("locked").toBool());
    if (changes.contains("hidden"))
        element->set_hidden(changes.value("hidden").toBool());
    if (changes.contains("name")) {
        const std::string renamed = changes.value("name").toString().trimmed().toStdString();
        if (!renamed.empty()) {
            element->set_name(renamed);
            // The other elements' links and rules name this one as well as identify it.
            const std::string id = element->uuid().string();
            for (rv::data::Slide::Element &other : *slide->mutable_elements()) {
                for (DataLink &link : *other.mutable_data_links()) {
                    if (link.has_alternate_text() && link.alternate_text().other_element_uuid().string() == id)
                        link.mutable_alternate_text()->set_other_element_name(renamed);
                    if (!link.has_visibility_link())
                        continue;
                    for (auto &condition : *link.mutable_visibility_link()->mutable_conditions()) {
                        if (condition.has_element_visibility()
                            && condition.element_visibility().other_element_uuid().string() == id)
                            condition.mutable_element_visibility()->set_other_element_name(renamed);
                    }
                }
            }
        }
    }

    if (changes.contains("fillColor"))
        setColor(element->mutable_fill()->mutable_color(), changes.value("fillColor").value<QColor>());
    if (changes.contains("fillOn")) {
        element->mutable_fill()->set_enable(changes.value("fillOn").toBool());
        // Turned on with nothing to fill with, it is filled with black.
        if (element->fill().enable() && element->fill().FillType_case() == rv::data::Graphics::Fill::FILLTYPE_NOT_SET)
            setColor(element->mutable_fill()->mutable_color(), Qt::black);
    }
    if (changes.contains("fillLinesOnly"))
        element->mutable_text_line_mask()->set_enabled(changes.value("fillLinesOnly").toBool());

    if (changes.contains("strokeColor"))
        setColor(element->mutable_stroke()->mutable_color(), changes.value("strokeColor").value<QColor>());
    if (changes.contains("strokeWidth"))
        element->mutable_stroke()->set_width(qMax(0.0, number("strokeWidth")));
    if (changes.contains("strokeOn")) {
        element->mutable_stroke()->set_enable(changes.value("strokeOn").toBool());
        if (element->stroke().enable() && element->stroke().width() <= 0)
            element->mutable_stroke()->set_width(3);
    }

    if (anyKeyStartsWith(changes, QStringLiteral("shadow")))
        applyShadow(element->mutable_shadow(), QStringLiteral("shadow"), changes);
    if (anyKeyStartsWith(changes, QStringLiteral("textShadow")))
        applyShadow(element->mutable_text()->mutable_shadow(), QStringLiteral("textShadow"), changes);

    if (changes.contains("verticalAlignment")) {
        const int alignment = changes.value("verticalAlignment").toInt();
        element->mutable_text()->set_vertical_alignment(alignment & Qt::AlignTop ? Text::VERTICAL_ALIGNMENT_TOP
                                                        : alignment & Qt::AlignBottom ? Text::VERTICAL_ALIGNMENT_BOTTOM
                                                        : Text::VERTICAL_ALIGNMENT_MIDDLE);
    }
    if (changes.contains("marginLeft"))
        element->mutable_text()->mutable_margins()->set_left(qMax(0.0, number("marginLeft")));
    if (changes.contains("marginTop"))
        element->mutable_text()->mutable_margins()->set_top(qMax(0.0, number("marginTop")));
    if (changes.contains("marginRight"))
        element->mutable_text()->mutable_margins()->set_right(qMax(0.0, number("marginRight")));
    if (changes.contains("marginBottom"))
        element->mutable_text()->mutable_margins()->set_bottom(qMax(0.0, number("marginBottom")));

    // Where the text comes from
    if (changes.contains("linkKind") || changes.contains("linkElementId") || changes.contains("linkTransform")) {
        DataLink *existing = nullptr;
        for (DataLink &link : *slideElement->mutable_data_links()) {
            if (link.has_alternate_text()) {
                existing = &link;
                break;
            }
        }
        const QString kind = changes.value("linkKind", existing ? QStringLiteral("element") : QStringLiteral("none")).toString();
        if (kind == QLatin1String("none")) {
            removeLinks(slideElement, [](const DataLink &link) { return link.has_alternate_text(); });
        } else if (kind == QLatin1String("element")) {
            auto *alternate = (existing ? existing : slideElement->add_data_links())->mutable_alternate_text();
            if (changes.contains("linkElementId")) {
                const QString source = changes.value("linkElementId").toString();
                alternate->mutable_other_element_uuid()->set_string(source.toStdString());
                alternate->set_other_element_name(name(source));
            }
            if (changes.contains("linkTransform"))
                alternate->set_text_transform(DataLink::AlternateElementText::TextTransformOption(
                    qBound(0, changes.value("linkTransform").toInt(), 3)));
        }
    }

    // When it is shown
    if (changes.contains("visibilityRules") && !changes.value("visibilityRules").toBool()) {
        removeLinks(slideElement, [](const DataLink &link) { return link.has_visibility_link(); });
    } else if (changes.contains("visibilityRules") || changes.contains("visibilityCriterion")
               || changes.contains("visibilityConditions")) {
        DataLink *existing = nullptr;
        for (DataLink &link : *slideElement->mutable_data_links()) {
            if (link.has_visibility_link()) {
                existing = &link;
                break;
            }
        }
        auto *rules = (existing ? existing : slideElement->add_data_links())->mutable_visibility_link();
        if (changes.contains("visibilityCriterion"))
            rules->set_visibility_criterion(DataLink::VisibilityLink::VisibilityCriterion(
                qBound(0, changes.value("visibilityCriterion").toInt(), 2)));
        if (changes.contains("visibilityConditions")) {
            const DataLink::VisibilityLink before = *rules;
            rules->clear_conditions();
            const QVariantList conditions = changes.value("visibilityConditions").toList();
            for (const QVariant &entry : conditions) {
                const QVariantMap condition = entry.toMap();
                if (condition.value("kind").toString() == QLatin1String("element")) {
                    using Criterion = DataLink::VisibilityLink::Condition::ElementVisibility;
                    const QString other = condition.value("elementId").toString();
                    const std::string otherName = name(other);
                    auto *added = rules->add_conditions()->mutable_element_visibility();
                    added->mutable_other_element_uuid()->set_string(other.toStdString());
                    added->set_other_element_name(otherName.empty() ? condition.value("elementName").toString().toStdString()
                                                                    : otherName);
                    added->set_visibility_criterion(condition.value("hasText").toBool()
                                                        ? Criterion::ELEMENT_VISIBILITY_CRITERION_HAS_TEXT
                                                        : Criterion::ELEMENT_VISIBILITY_CRITERION_HAS_NO_TEXT);
                } else {
                    // A condition on something this app does not track goes back as it was.
                    const int index = condition.value("index", -1).toInt();
                    if (index >= 0 && index < before.conditions_size())
                        *rules->add_conditions() = before.conditions(index);
                }
            }
        }
    }
    return true;
}

QString uniqueElementName(const rv::data::Slide &slide, const QString &base)
{
    const auto taken = [&slide](const QString &name) {
        for (const rv::data::Slide::Element &element : slide.elements()) {
            if (QString::fromStdString(element.element().name()) == name)
                return true;
        }
        return false;
    };
    QString name = base;
    for (int n = 2; taken(name); ++n)
        name = QStringLiteral("%1 %2").arg(base).arg(n);
    return name;
}

rv::data::Slide::Element makeTextElement(const rv::data::Slide &slide, const rv::data::Slide::Element *like)
{
    const bool hasSize = slide.size().width() > 0 && slide.size().height() > 0;
    const double slideWidth = hasSize ? slide.size().width() : 1920;
    const double slideHeight = hasSize ? slide.size().height() : 1080;

    rv::data::Slide::Element result;
    result.set_info(rv::data::Slide::Element::INFO_IS_TEXT_ELEMENT);
    // The settings ProPresenter gives a text box that does not scroll.
    result.mutable_text_scroller()->set_scroll_rate(0.5);
    result.mutable_text_scroller()->set_should_repeat(true);
    result.mutable_text_scroller()->set_repeat_distance(0.05);

    rv::data::Graphics::Element *element = result.mutable_element();
    element->mutable_uuid()->set_string(workspace::newUuid());
    element->set_name(uniqueElementName(slide, QStringLiteral("Text")).toStdString());
    element->set_opacity(1);

    // In the middle of the slide, stepping down and right while that place is taken.
    const double width = qRound(slideWidth * 0.5);
    const double height = qRound(slideHeight * 0.2);
    double x = qRound((slideWidth - width) / 2);
    double y = qRound((slideHeight - height) / 2);
    const auto taken = [&slide](double x, double y) {
        for (const rv::data::Slide::Element &other : slide.elements()) {
            const auto &origin = other.element().bounds().origin();
            if (qAbs(origin.x() - x) < 1 && qAbs(origin.y() - y) < 1)
                return true;
        }
        return false;
    };
    while (taken(x, y) && y + 30 + height < slideHeight) {
        x += 30;
        y += 30;
    }
    element->mutable_bounds()->mutable_origin()->set_x(x);
    element->mutable_bounds()->mutable_origin()->set_y(y);
    element->mutable_bounds()->mutable_size()->set_width(width);
    element->mutable_bounds()->mutable_size()->set_height(height);

    // A rectangle, as a path on the unit square.
    rv::data::Graphics::Path *path = element->mutable_path();
    path->set_closed(true);
    for (const QPointF &corner : {QPointF(0, 0), QPointF(1, 0), QPointF(1, 1), QPointF(0, 1)}) {
        auto *point = path->add_points();
        for (rv::data::Graphics::Point *part : {point->mutable_point(), point->mutable_q0(), point->mutable_q1()}) {
            part->set_x(corner.x());
            part->set_y(corner.y());
        }
    }
    path->mutable_shape()->set_type(rv::data::Graphics::Path::Shape::TYPE_RECTANGLE);

    // No fill, stroke or shadow to begin with, but set up so that turning one on shows.
    setColor(element->mutable_fill()->mutable_color(), QColor(0, 0, 0, 128));
    element->mutable_stroke()->set_width(3);
    setColor(element->mutable_stroke()->mutable_color(), Qt::white);
    for (rv::data::Graphics::Shadow *shadow : {element->mutable_shadow(), element->mutable_text()->mutable_shadow()}) {
        shadow->set_angle(315);
        shadow->set_offset(5);
        shadow->set_radius(5);
        setColor(shadow->mutable_color(), Qt::black);
        shadow->set_opacity(0.75);
    }
    element->mutable_feather();

    rv::data::Graphics::Text *text = element->mutable_text();
    TextRun format;
    Qt::Alignment alignment = Qt::AlignHCenter;
    if (like && like->element().has_text()) {
        const RichText model = readText(like->element().text());
        format = model.firstRun();
        if (!model.paragraphs.isEmpty())
            alignment = model.paragraphs.first().alignment;
        // Its text shadow too, since that is part of how the text looks.
        *text->mutable_shadow() = like->element().text().shadow();
    } else {
        format.family = QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
        format.fontName = postScriptNameFor(format.family, false, false);
        format.size = qRound(slideHeight / 15);
        format.fill = Qt::white;
    }
    text->set_vertical_alignment(Text::VERTICAL_ALIGNMENT_MIDDLE);
    text->mutable_margins();
    text->set_is_superscript_standardized(true);
    text->mutable_attributes()->mutable_paragraph_style()->set_line_height_multiple(1);
    writeText(text, RichText::plain(QStringLiteral("Text"), format, alignment));
    return result;
}

} // namespace proconvert
