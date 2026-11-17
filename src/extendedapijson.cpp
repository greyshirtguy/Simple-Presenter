#include "extendedapijson.h"

#include <QHash>
#include <QSet>
#include <QUrl>

#include <cmath>

namespace extendedapijson {

QString snake(const QString &camel)
{
    QString made;
    made.reserve(camel.size() + 4);
    for (const QChar letter : camel) {
        if (letter.isUpper()) {
            made += u'_';
            made += letter.toLower();
        } else {
            made += letter;
        }
    }
    return made;
}

QString camel(const QString &snake)
{
    QString made;
    made.reserve(snake.size());
    bool raise = false;
    for (const QChar letter : snake) {
        if (letter == u'_') {
            raise = true;
        } else {
            made += raise ? letter.toUpper() : letter;
            raise = false;
        }
    }
    return made;
}

QString colorText(const QColor &color)
{
    const QColor rgb = color.toRgb();
    const auto two = [](int value) { return QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')); };
    QString text = u'#' + two(rgb.red()) + two(rgb.green()) + two(rgb.blue());
    if (rgb.alpha() != 255)
        text += two(rgb.alpha());
    return text;
}

bool colorOf(const QVariant &value, QColor *color)
{
    if (value.typeId() == QMetaType::QColor) {
        *color = value.value<QColor>();
        return color->isValid();
    }
    if (value.typeId() != QMetaType::QString)
        return false;
    const QString text = value.toString().trimmed();
    if (text.startsWith(u'#')) {
        const QString digits = text.mid(1);
        for (const QChar digit : digits) {
            if (!((digit >= u'0' && digit <= u'9') || (digit.toLower() >= u'a' && digit.toLower() <= u'f')))
                return false;
        }
        const auto part = [&digits](int at, int length) {
            const int read = digits.mid(at, length).toInt(nullptr, 16);
            return length == 1 ? read * 17 : read;
        };
        if (digits.size() == 3 || digits.size() == 4) {
            *color = QColor(part(0, 1), part(1, 1), part(2, 1), digits.size() == 4 ? part(3, 1) : 255);
            return true;
        }
        if (digits.size() == 6 || digits.size() == 8) {
            *color = QColor(part(0, 2), part(2, 2), part(4, 2), digits.size() == 8 ? part(6, 2) : 255);
            return true;
        }
        return false;
    }
    // (By a name: "white", "transparent". Not Qt's reading of "#aarrggbb", dealt with
    // above the other way round.)
    const QColor named = QColor::fromString(text);
    if (!named.isValid())
        return false;
    *color = named;
    return true;
}

namespace {

// The properties whose value is one of a few words, which go in snake case as names do
bool isWord(const QString &camelKey)
{
    static const QSet<QString> words {QStringLiteral("kind"), QStringLiteral("fillKind"), QStringLiteral("linkKind"), QStringLiteral("shape")};
    return words.contains(camelKey);
}

QVariant plainOf(const QVariant &value, const QString &key)
{
    switch (value.typeId()) {
    case QMetaType::QVariantMap: {
        const QVariantMap map = value.toMap();
        QVariantMap made;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it)
            made.insert(snake(it.key()), plainOf(it.value(), it.key()));
        return made;
    }
    case QMetaType::QVariantList: {
        QVariantList made;
        for (const QVariant &each : value.toList())
            made.append(plainOf(each, QString()));
        return made;
    }
    case QMetaType::QColor:
        return colorText(value.value<QColor>());
    case QMetaType::QUrl: {
        const QUrl url = value.toUrl();
        return url.isLocalFile() ? url.toLocalFile() : url.toString();
    }
    case QMetaType::QString:
        return isWord(key) ? QVariant(snake(value.toString())) : value;
    default:
        break;
    }
    if (value.typeId() == qMetaTypeId<RichText>())
        return value.value<RichText>().plainText();
    return value;
}

QVariant namedOf(const QVariant &value, const QString &camelKey)
{
    switch (value.typeId()) {
    case QMetaType::QVariantMap: {
        const QVariantMap map = value.toMap();
        QVariantMap made;
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            const QString key = camel(it.key());
            made.insert(key, namedOf(it.value(), key));
        }
        return made;
    }
    case QMetaType::QVariantList: {
        QVariantList made;
        for (const QVariant &each : value.toList())
            made.append(namedOf(each, QString()));
        return made;
    }
    case QMetaType::QString:
        return isWord(camelKey) ? QVariant(camel(value.toString())) : value;
    default:
        return value;
    }
}

struct Word
{
    int number;
    const char *word;
};

QString wordOf(const QList<Word> &words, int number)
{
    for (const Word &each : words) {
        if (each.number == number)
            return QString::fromLatin1(each.word);
    }
    return QString::number(number);
}

int numberOf(const QList<Word> &words, const QVariant &value)
{
    if (value.typeId() == QMetaType::QString) {
        const QString asked = value.toString().trimmed().toLower();
        for (const Word &each : words) {
            if (asked == QLatin1String(each.word))
                return each.number;
        }
        // (A number written as text)
        bool isNumber = false;
        const int number = asked.toInt(&isNumber);
        if (!isNumber)
            return -1;
        for (const Word &each : words) {
            if (each.number == number)
                return number;
        }
        return -1;
    }
    if (!value.canConvert<double>())
        return -1;
    const int number = value.toInt();
    for (const Word &each : words) {
        if (each.number == number)
            return number;
    }
    return -1;
}

const QList<Word> &alignments()
{
    static const QList<Word> words {{Qt::AlignLeft, "left"}, {Qt::AlignHCenter, "center"}, {Qt::AlignRight, "right"}, {Qt::AlignJustify, "justify"}};
    return words;
}

const QList<Word> &verticals()
{
    static const QList<Word> words {{Qt::AlignTop, "top"}, {Qt::AlignVCenter, "middle"}, {Qt::AlignBottom, "bottom"}};
    return words;
}

const QList<Word> &capitals()
{
    static const QList<Word> words {{TextRun::NoCapitalization, "none"}, {TextRun::AllCaps, "all_caps"}, {TextRun::SmallCaps, "small_caps"},
                                    {TextRun::TitleCase, "title_case"}, {TextRun::StartCase, "start_case"}};
    return words;
}

const QList<Word> &playbacks()
{
    static const QList<Word> words {{0, "stop"}, {1, "loop"}, {2, "loop_count"}, {3, "loop_seconds"}};
    return words;
}

}

QVariant plain(const QVariant &value)
{
    return plainOf(value, QString());
}

QVariantMap named(const QVariantMap &api)
{
    return namedOf(api, QString()).toMap();
}

QString alignmentWord(int alignment)
{
    // (Only its horizontal part: Qt's alignments are both in one number)
    return wordOf(alignments(), alignment & int(Qt::AlignHorizontal_Mask));
}

int alignmentOf(const QVariant &value)
{
    if (value.typeId() == QMetaType::QString && value.toString().trimmed().toLower() == QLatin1String("centre"))
        return Qt::AlignHCenter;
    return numberOf(alignments(), value);
}

QString verticalWord(int alignment)
{
    return wordOf(verticals(), alignment & int(Qt::AlignVertical_Mask));
}

int verticalOf(const QVariant &value)
{
    if (value.typeId() == QMetaType::QString) {
        const QString asked = value.toString().trimmed().toLower();
        if (asked == QLatin1String("center") || asked == QLatin1String("centre"))
            return Qt::AlignVCenter;
    }
    return numberOf(verticals(), value);
}

QString capitalsWord(int capitalization)
{
    return wordOf(capitals(), capitalization);
}

int capitalsOf(const QVariant &value)
{
    return numberOf(capitals(), value);
}

QString playbackWord(int playback)
{
    return wordOf(playbacks(), playback);
}

int playbackOf(const QVariant &value)
{
    return numberOf(playbacks(), value);
}

// ---- text

QVariantMap format(const TextRun &run, Qt::Alignment alignment)
{
    QVariantMap made {
        {QStringLiteral("family"), run.family},
        {QStringLiteral("font_name"), run.fontName},
        {QStringLiteral("size"), run.size},
        {QStringLiteral("bold"), run.bold},
        {QStringLiteral("italic"), run.italic},
        {QStringLiteral("color"), colorText(run.fill)},
        {QStringLiteral("alignment"), alignmentWord(int(alignment))},
    };
    if (run.underline)
        made.insert(QStringLiteral("underline"), true);
    if (run.strikethrough)
        made.insert(QStringLiteral("strikethrough"), true);
    if (!qFuzzyIsNull(run.kerning))
        made.insert(QStringLiteral("kerning"), run.kerning);
    if (run.capitalization != TextRun::NoCapitalization)
        made.insert(QStringLiteral("capitalization"), capitalsWord(run.capitalization));
    if (run.strokeWidth > 0) {
        made.insert(QStringLiteral("stroke_color"), colorText(run.stroke));
        made.insert(QStringLiteral("stroke_width"), run.strokeWidth);
    }
    return made;
}

QVariantMap formatChanges(const QVariantMap &api, QString *error)
{
    QVariantMap made;
    for (auto it = api.constBegin(); it != api.constEnd(); ++it) {
        const QString key = camel(it.key());
        const QVariant &value = it.value();
        if (key == QLatin1String("color") || key == QLatin1String("strokeColor")) {
            QColor color;
            if (!colorOf(value, &color)) {
                *error = QStringLiteral("`%1` is not a colour: \"#rrggbb\", \"#rrggbbaa\" or a colour's name").arg(it.key());
                return {};
            }
            made.insert(key, color);
        } else if (key == QLatin1String("alignment")) {
            const int alignment = alignmentOf(value);
            if (alignment < 0) {
                *error = QStringLiteral("`alignment` is one of \"left\", \"center\", \"right\" and \"justify\"");
                return {};
            }
            made.insert(key, alignment);
        } else if (key == QLatin1String("capitalization")) {
            const int capitalization = capitalsOf(value);
            if (capitalization < 0) {
                *error = QStringLiteral("`capitalization` is one of \"none\", \"all_caps\", \"small_caps\", \"title_case\" and \"start_case\"");
                return {};
            }
            made.insert(key, capitalization);
        } else if (key == QLatin1String("family") || key == QLatin1String("fontName")) {
            if (value.toString().trimmed().isEmpty()) {
                *error = QStringLiteral("`%1` needs a font's name").arg(it.key());
                return {};
            }
            made.insert(key, value.toString());
        } else if (key == QLatin1String("size") || key == QLatin1String("kerning") || key == QLatin1String("strokeWidth")) {
            bool isNumber = false;
            const double number = value.toDouble(&isNumber);
            if (!isNumber || (key == QLatin1String("size") && number <= 0)) {
                *error = QStringLiteral("`%1` is a number").arg(it.key());
                return {};
            }
            made.insert(key, number);
        } else if (key == QLatin1String("bold") || key == QLatin1String("italic") || key == QLatin1String("underline")
                   || key == QLatin1String("strikethrough")) {
            if (value.typeId() != QMetaType::Bool) {
                *error = QStringLiteral("`%1` is true or false").arg(it.key());
                return {};
            }
            made.insert(key, value.toBool());
        } else {
            *error = QStringLiteral("A format has no `%1`: it has family, font_name, size, bold, italic, underline, strikethrough, "
                                    "color, alignment, capitalization, kerning, stroke_color and stroke_width").arg(it.key());
            return {};
        }
    }
    return made;
}

QVariantList paragraphs(const RichText &text)
{
    QVariantList made;
    for (const TextParagraph &paragraph : text.paragraphs) {
        QVariantList runs;
        for (const TextRun &run : paragraph.runs) {
            QVariantMap each = format(run, paragraph.alignment);
            each.remove(QStringLiteral("alignment"));
            each.insert(QStringLiteral("text"), run.text);
            runs.append(each);
        }
        made.append(QVariantMap {{QStringLiteral("alignment"), alignmentWord(int(paragraph.alignment))}, {QStringLiteral("runs"), runs}});
    }
    return made;
}

bool paragraphsOf(const QVariantList &api, const TextRun &like, Qt::Alignment alignment, RichText *text, QString *error)
{
    RichText made;
    for (const QVariant &entry : api) {
        // (A paragraph may be its words alone.)
        QVariantMap described = entry.toMap();
        if (entry.typeId() == QMetaType::QString)
            described.insert(QStringLiteral("runs"), QVariantList {QVariantMap {{QStringLiteral("text"), entry.toString()}}});
        if (!described.contains(QStringLiteral("runs")) && described.contains(QStringLiteral("text")))
            described.insert(QStringLiteral("runs"), QVariantList {QVariantMap {{QStringLiteral("text"), described.value(QStringLiteral("text"))}}});
        if (described.value(QStringLiteral("runs")).typeId() != QMetaType::QVariantList) {
            *error = QStringLiteral("Each of `paragraphs` is {alignment, runs: [{text, …}]}");
            return false;
        }
        TextParagraph paragraph;
        paragraph.alignment = alignment;
        if (described.contains(QStringLiteral("alignment"))) {
            const int asked = alignmentOf(described.value(QStringLiteral("alignment")));
            if (asked < 0) {
                *error = QStringLiteral("A paragraph's `alignment` is one of \"left\", \"center\", \"right\" and \"justify\"");
                return false;
            }
            paragraph.alignment = Qt::Alignment(asked);
        }
        for (const QVariant &given : described.value(QStringLiteral("runs")).toList()) {
            QVariantMap wanted = given.toMap();
            if (given.typeId() == QMetaType::QString)
                wanted.insert(QStringLiteral("text"), given.toString());
            TextRun run = like;
            // (Words on a line of their own: a break within the run would be a break
            // within the paragraph, which is not what a list of paragraphs is asking for.)
            run.text = wanted.take(QStringLiteral("text")).toString().replace(u'\n', u' ');
            const QVariantMap changes = formatChanges(wanted, error);
            if (!error->isEmpty())
                return false;
            if (changes.contains(QStringLiteral("alignment"))) {
                *error = QStringLiteral("An alignment is a paragraph's, not a run's");
                return false;
            }
            run.applyFormat(changes);
            paragraph.runs.append(run);
        }
        if (paragraph.runs.isEmpty()) {
            TextRun run = like;
            run.text.clear();
            paragraph.runs.append(run);
        }
        made.paragraphs.append(paragraph);
    }
    if (made.paragraphs.isEmpty()) {
        TextParagraph paragraph;
        paragraph.alignment = alignment;
        TextRun run = like;
        run.text.clear();
        paragraph.runs.append(run);
        made.paragraphs.append(paragraph);
    }
    *text = made;
    return true;
}

bool uniform(const RichText &text)
{
    const TextRun first = text.firstRun();
    const Qt::Alignment alignment = text.paragraphs.isEmpty() ? Qt::AlignLeft : text.paragraphs.first().alignment;
    for (const TextParagraph &paragraph : text.paragraphs) {
        if (paragraph.alignment != alignment)
            return false;
        for (const TextRun &run : paragraph.runs) {
            if (!run.sameFormat(first))
                return false;
        }
    }
    return true;
}

// ---- an element

namespace {

// The app's own names of what proconvert::applyChanges takes (its header lists them, and
// tst_extendedapijson reads its source to see that this list has every one it reads)
const QStringList &changeable()
{
    static const QStringList keys {
        QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height"), QStringLiteral("rotation"),
        QStringLiteral("opacity"), QStringLiteral("name"), QStringLiteral("locked"), QStringLiteral("hidden"),
        QStringLiteral("fillOn"), QStringLiteral("fillKind"), QStringLiteral("fillColor"), QStringLiteral("fillLinesOnly"),
        QStringLiteral("fillBlurAmount"), QStringLiteral("fillBlurSaturation"),
        QStringLiteral("fillGradientFrom"), QStringLiteral("fillGradientTo"), QStringLiteral("fillGradientAngle"),
        QStringLiteral("fillMediaPath"), QStringLiteral("fillMediaScale"), QStringLiteral("fillMediaSettings"),
        QStringLiteral("fillScreenId"), QStringLiteral("fillScreenName"), QStringLiteral("fillSlideNext"), QStringLiteral("fillInputIndex"),
        QStringLiteral("roundness"), QStringLiteral("featherOn"), QStringLiteral("featherRadius"),
        QStringLiteral("strokeOn"), QStringLiteral("strokeColor"), QStringLiteral("strokeWidth"),
        QStringLiteral("shadowEnabled"), QStringLiteral("shadowColor"), QStringLiteral("shadowSolidColor"), QStringLiteral("shadowOpacity"),
        QStringLiteral("shadowAngle"), QStringLiteral("shadowOffset"), QStringLiteral("shadowRadius"),
        QStringLiteral("textShadowEnabled"), QStringLiteral("textShadowColor"), QStringLiteral("textShadowSolidColor"),
        QStringLiteral("textShadowOpacity"), QStringLiteral("textShadowAngle"), QStringLiteral("textShadowOffset"),
        QStringLiteral("textShadowRadius"),
        QStringLiteral("verticalAlignment"), QStringLiteral("textScale"),
        QStringLiteral("marginLeft"), QStringLiteral("marginTop"), QStringLiteral("marginRight"), QStringLiteral("marginBottom"),
        QStringLiteral("chordsOn"), QStringLiteral("chordNotation"), QStringLiteral("chordColor"),
        QStringLiteral("linkKind"), QStringLiteral("linkElementId"), QStringLiteral("linkTransform"),
        QStringLiteral("linkTimerId"), QStringLiteral("linkTimerName"), QStringLiteral("linkTimerHours"), QStringLiteral("linkTimerMinutes"),
        QStringLiteral("linkTimerSeconds"), QStringLiteral("linkTimerHundredths"), QStringLiteral("linkColorTriggers"),
        QStringLiteral("linkSlideNext"), QStringLiteral("linkSlideSource"), QStringLiteral("linkSlideName"), QStringLiteral("linkSlidePreserve"),
        QStringLiteral("linkClockDate"), QStringLiteral("linkClockTime"), QStringLiteral("linkClock24"),
        QStringLiteral("linkVideoIgnoreLooping"), QStringLiteral("linkFlash"), QStringLiteral("linkFlashColor"),
        QStringLiteral("linkMarkerDestination"), QStringLiteral("linkMarkerWhich"), QStringLiteral("linkMarkerName"),
        QStringLiteral("linkMarkerTime"), QStringLiteral("linkMarkerColor"),
        QStringLiteral("visibilityRules"), QStringLiteral("visibilityCriterion"), QStringLiteral("visibilityConditions"),
    };
    return keys;
}

bool isColorKey(const QString &camelKey)
{
    return camelKey.endsWith(QLatin1String("Color")) || camelKey == QLatin1String("fillGradientFrom")
        || camelKey == QLatin1String("fillGradientTo");
}

// What of an element's map is for drawing alone, or is worked out from the rest, and is
// not said through the API. (`text` and `order` are put in again in the API's own form.)
const QSet<QString> &unsaid()
{
    static const QSet<QString> keys {
        QStringLiteral("displayText"), QStringLiteral("hasText"), QStringLiteral("fillShown"), QStringLiteral("fillEnabled"),
        QStringLiteral("strokeEnabled"), QStringLiteral("shadowOffsetX"), QStringLiteral("shadowOffsetY"),
        QStringLiteral("textShadowOffsetX"), QStringLiteral("textShadowOffsetY"), QStringLiteral("fillMediaSource"),
        QStringLiteral("fillMediaUrl"), QStringLiteral("words"), QStringLiteral("outline"), QStringLiteral("chordStyle"),
        QStringLiteral("visibilityTimed"), QStringLiteral("visibilityStaged"), QStringLiteral("textBox"), QStringLiteral("text"),
        QStringLiteral("fills"), QStringLiteral("builds"), QStringLiteral("buildSteps"), QStringLiteral("buildIn"), QStringLiteral("buildOut"),
    };
    return keys;
}

// The names beginning with `prefix`, but for `keep`, taken out: what says nothing while
// its switch is off
void drop(QVariantMap *map, const QString &prefix, const QStringList &keep = {})
{
    for (auto it = map->begin(); it != map->end();) {
        if (it.key().startsWith(prefix) && !keep.contains(it.key()))
            it = map->erase(it);
        else
            ++it;
    }
}

bool sameValue(const QVariant &now, const QVariant &wanted);

// Whether two of the API's values say the same, lists and maps all the way down: a
// number is a number however JSON wrote it (84 and 84.0)
bool sameAll(const QVariant &one, const QVariant &other)
{
    if (one.typeId() == QMetaType::QVariantList || other.typeId() == QMetaType::QVariantList) {
        const QVariantList a = one.toList();
        const QVariantList b = other.toList();
        if (one.typeId() != other.typeId() || a.size() != b.size())
            return false;
        for (qsizetype i = 0; i < a.size(); ++i) {
            if (!sameAll(a.at(i), b.at(i)))
                return false;
        }
        return true;
    }
    if (one.typeId() == QMetaType::QVariantMap || other.typeId() == QMetaType::QVariantMap) {
        const QVariantMap a = one.toMap();
        const QVariantMap b = other.toMap();
        if (one.typeId() != other.typeId() || a.keys() != b.keys())
            return false;
        for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
            if (!sameAll(it.value(), b.value(it.key())))
                return false;
        }
        return true;
    }
    return sameValue(one, other);
}

// The names of what an element is shown with and cannot be changed by: what it is, what
// is worked out, and the parts of it this app reads and has no way to set
bool readOnly(const QString &name)
{
    static const QSet<QString> names {
        QStringLiteral("id"), QStringLiteral("visible"), QStringLiteral("shape"), QStringLiteral("scroll"), QStringLiteral("box_fill"),
        QStringLiteral("fill_media_name"), QStringLiteral("fill_media_video"), QStringLiteral("link_element_name"),
        QStringLiteral("link_label"), QStringLiteral("link_picture"), QStringLiteral("link_timer_pattern"),
        QStringLiteral("link_clock_pattern"), QStringLiteral("link_timer_hundredths_under_minute"),
    };
    return names.contains(name) || name.startsWith(QLatin1String("line_mask")) || name.startsWith(QLatin1String("text_transform"));
}

bool sameValue(const QVariant &now, const QVariant &wanted)
{
    if (now.typeId() == QMetaType::QColor || wanted.typeId() == QMetaType::QColor)
        return now.value<QColor>().rgba() == wanted.value<QColor>().rgba() && now.isValid();
    const auto isNumber = [](const QVariant &value) {
        switch (value.typeId()) {
        case QMetaType::Int: case QMetaType::UInt: case QMetaType::LongLong: case QMetaType::ULongLong:
        case QMetaType::Double: case QMetaType::Float:
            return true;
        default:
            return false;
        }
    };
    if (isNumber(now) && isNumber(wanted))
        return std::abs(now.toDouble() - wanted.toDouble()) < 1e-9;
    return now.isValid() && now == wanted;
}

}

QStringList changeableProperties()
{
    QStringList made;
    for (const QString &key : changeable())
        made.append(snake(key));
    return made;
}

QVariantMap element(const QVariantMap &internal, int order, bool rich)
{
    QVariantMap map = internal;
    const RichText text = map.value(QStringLiteral("text")).value<RichText>();
    const bool textBox = map.value(QStringLiteral("textBox")).toBool();
    const int vertical = map.value(QStringLiteral("verticalAlignment")).toInt();
    for (const QString &key : unsaid())
        map.remove(key);

    // What says nothing while its switch is off
    const QString fillKind = map.value(QStringLiteral("fillKind")).toString();
    if (!map.value(QStringLiteral("fillOn")).toBool()) {
        drop(&map, QStringLiteral("fill"), {QStringLiteral("fillOn")});
        drop(&map, QStringLiteral("lineMask"));
        map.remove(QStringLiteral("boxFill"));
    } else {
        if (fillKind != QLatin1String("color"))
            map.remove(QStringLiteral("fillColor"));
        if (fillKind != QLatin1String("gradient"))
            drop(&map, QStringLiteral("fillGradient"));
        if (fillKind != QLatin1String("media"))
            drop(&map, QStringLiteral("fillMedia"));
        if (fillKind != QLatin1String("screen"))
            drop(&map, QStringLiteral("fillScreen"));
        if (fillKind != QLatin1String("slideImage"))
            map.remove(QStringLiteral("fillSlideNext"));
        if (fillKind != QLatin1String("input"))
            map.remove(QStringLiteral("fillInputIndex"));
        if (!map.value(QStringLiteral("fillLinesOnly")).toBool())
            drop(&map, QStringLiteral("lineMask"));
    }
    if (!map.value(QStringLiteral("strokeOn")).toBool())
        drop(&map, QStringLiteral("stroke"), {QStringLiteral("strokeOn")});
    if (!map.value(QStringLiteral("shadowEnabled")).toBool())
        drop(&map, QStringLiteral("shadow"), {QStringLiteral("shadowEnabled")});
    if (!map.value(QStringLiteral("textShadowEnabled")).toBool())
        drop(&map, QStringLiteral("textShadow"), {QStringLiteral("textShadowEnabled")});
    if (!map.value(QStringLiteral("featherOn")).toBool())
        map.remove(QStringLiteral("featherRadius"));
    if (map.value(QStringLiteral("linkKind")).toString() == QLatin1String("none"))
        drop(&map, QStringLiteral("link"), {QStringLiteral("linkKind")});
    if (!map.value(QStringLiteral("visibilityRules")).toBool())
        drop(&map, QStringLiteral("visibility"), {QStringLiteral("visibilityRules")});
    if (!map.value(QStringLiteral("chordsOn")).toBool())
        drop(&map, QStringLiteral("chord"), {QStringLiteral("chordsOn"), QStringLiteral("chords")});
    if (map.value(QStringLiteral("shape")).toString() != QLatin1String("roundedRectangle"))
        map.remove(QStringLiteral("roundness"));
    if (map.value(QStringLiteral("textTransform")).toInt() == 0) {
        map.remove(QStringLiteral("textTransform"));
        map.remove(QStringLiteral("textTransformDelimiter"));
    }

    QVariantMap made = plain(map).toMap();
    made.insert(QStringLiteral("order"), order);
    made.insert(QStringLiteral("vertical_alignment"), verticalWord(vertical));
    // Its text: the words, the format they begin in, and all of it run by run where
    // there is more than one format in it (or it is asked for)
    made.insert(QStringLiteral("text"), text.plainText());
    if (textBox || !text.isEmpty()) {
        const Qt::Alignment alignment = text.paragraphs.isEmpty() ? Qt::AlignLeft : text.paragraphs.first().alignment;
        made.insert(QStringLiteral("format"), format(text.firstRun(), alignment));
        if (rich || !uniform(text))
            made.insert(QStringLiteral("paragraphs"), paragraphs(text));
    }
    return made;
}

ElementChanges elementChanges(const QVariantMap &api, const QVariantMap &current)
{
    ElementChanges made;
    // What was sent, with the things of a `properties` map beside the rest
    QVariantMap sent = api;
    const QVariant inner = sent.take(QStringLiteral("properties"));
    if (inner.isValid() && !inner.isNull()) {
        if (inner.typeId() != QMetaType::QVariantMap) {
            made.error = QStringLiteral("`properties` is an object of properties by name");
            return made;
        }
        const QVariantMap properties = inner.toMap();
        for (auto it = properties.constBegin(); it != properties.constEnd(); ++it) {
            if (!sent.contains(it.key()))
                sent.insert(it.key(), it.value());
        }
    }
    // The element as the API shows it, for what can be read and not changed: sent back
    // as it is, it is passed over
    const QVariantMap shown = current.isEmpty() ? QVariantMap() : element(current, 0);

    for (auto it = sent.constBegin(); it != sent.constEnd(); ++it) {
        const QString &name = it.key();
        const QVariant &value = it.value();
        if (name == QLatin1String("text")) {
            if (value.typeId() != QMetaType::QString) {
                made.error = QStringLiteral("`text` is the element's words, as text");
                return made;
            }
            made.hasText = true;
            made.text = value.toString();
            continue;
        }
        if (name == QLatin1String("format")) {
            if (value.typeId() != QMetaType::QVariantMap) {
                made.error = QStringLiteral("`format` is an object: {family, size, bold, italic, color, alignment, …}");
                return made;
            }
            // A format is put on all the element's words. But one that has every
            // property the element was shown with is that format sent back, changed here
            // and there or not at all (what a program does that reads an element, changes
            // something and sends it all back): of that, only what is other than it was
            // shown with is put on the words, or a text of several looks would be made
            // one look by being read and sent back. A format of a few properties is
            // somebody asking for those, and each is put on every word, whether or not
            // the first words have it already: "white" asked of words that begin white
            // and go on red makes them all white.
            QVariantMap wanted = value.toMap();
            const QVariantMap had = shown.value(QStringLiteral("format")).toMap();
            bool sentBack = !had.isEmpty();
            for (auto each = had.constBegin(); sentBack && each != had.constEnd(); ++each)
                sentBack = wanted.contains(each.key());
            for (auto each = wanted.begin(); sentBack && each != wanted.end();) {
                if (had.contains(each.key()) && sameValue(had.value(each.key()), each.value()))
                    each = wanted.erase(each);
                else
                    ++each;
            }
            made.format = formatChanges(wanted, &made.error);
            if (!made.error.isEmpty())
                return made;
            continue;
        }
        if (name == QLatin1String("paragraphs")) {
            if (value.typeId() != QMetaType::QVariantList) {
                made.error = QStringLiteral("`paragraphs` is a list: [{alignment, runs: [{text, …}]}]");
                return made;
            }
            // (As the text is already, run for run: nothing to write)
            if (!current.isEmpty() && sameAll(QVariant(paragraphs(current.value(QStringLiteral("text")).value<RichText>())), value))
                continue;
            made.hasParagraphs = true;
            made.paragraphs = value.toList();
            continue;
        }
        if (name == QLatin1String("chords")) {
            if (value.typeId() != QMetaType::QVariantList) {
                made.error = QStringLiteral("`chords` is a list: [{at, name}]");
                return made;
            }
            if (!current.isEmpty() && sameAll(shown.value(name, QVariantList()), value))
                continue;
            made.hasChords = true;
            made.chords = value.toList();
            continue;
        }
        if (name == QLatin1String("order")) {
            bool isNumber = false;
            const int order = value.toInt(&isNumber);
            if (!isNumber || order < 0) {
                made.error = QStringLiteral("`order` is a place among the slide's elements, from 0 at the back");
                return made;
            }
            made.order = order;
            continue;
        }
        if (name == QLatin1String("kind")) {
            made.kind = camel(value.toString());
            continue;
        }
        if (name == QLatin1String("file")) {
            made.file = value.toString();
            continue;
        }

        const QString key = camel(name);
        if (!changeable().contains(key)) {
            // What can only be read: passed over where it says what is so (and for an
            // element about to be made, which may be another's sent as it was shown),
            // and refused where it says something else
            if (!readOnly(name) && !shown.contains(name)) {
                made.error = QStringLiteral("An element has no property `%1`. Those that can be changed are: %2; and text, format, "
                                            "paragraphs, chords and order.").arg(name, changeableProperties().join(QStringLiteral(", ")));
                return made;
            }
            if (shown.contains(name) && !sameAll(shown.value(name), value)) {
                made.error = QStringLiteral("`%1` can be read but not changed").arg(name);
                return made;
            }
            continue;
        }

        // The value in the app's own form
        QVariant wanted = value;
        if (isColorKey(key)) {
            QColor color;
            if (!colorOf(value, &color)) {
                made.error = QStringLiteral("`%1` is not a colour: \"#rrggbb\", \"#rrggbbaa\" or a colour's name").arg(name);
                return made;
            }
            wanted = color;
        } else if (key == QLatin1String("verticalAlignment")) {
            const int alignment = verticalOf(value);
            if (alignment < 0) {
                made.error = QStringLiteral("`vertical_alignment` is one of \"top\", \"middle\" and \"bottom\"");
                return made;
            }
            wanted = alignment;
        } else if (key == QLatin1String("fillKind") || key == QLatin1String("linkKind")) {
            wanted = camel(value.toString());
        } else if (key == QLatin1String("linkColorTriggers")) {
            QVariantList triggers;
            for (const QVariant &each : value.toList()) {
                QVariantMap trigger = each.toMap();
                QColor color;
                if (!colorOf(trigger.value(QStringLiteral("color")), &color)) {
                    made.error = QStringLiteral("Each of `link_color_triggers` is {time, color}");
                    return made;
                }
                trigger.insert(QStringLiteral("color"), color);
                triggers.append(trigger);
            }
            wanted = triggers;
        } else if (value.typeId() == QMetaType::QVariantMap) {
            wanted = named(value.toMap());
        } else if (value.typeId() == QMetaType::QVariantList) {
            QVariantList list;
            for (const QVariant &each : value.toList())
                list.append(each.typeId() == QMetaType::QVariantMap ? QVariant(named(each.toMap())) : each);
            wanted = list;
        } else if (key == QLatin1String("name")) {
            if (value.toString().trimmed().isEmpty()) {
                made.error = QStringLiteral("An element's `name` cannot be empty");
                return made;
            }
        }
        // (Only what is to be other than it is: what an element has already is not
        // written again. A map or a list is always handed on, to be dealt with by what
        // knows it.)
        if (!current.isEmpty() && current.contains(key) && wanted.typeId() != QMetaType::QVariantMap
            && wanted.typeId() != QMetaType::QVariantList && sameValue(current.value(key), wanted))
            continue;
        made.properties.insert(key, wanted);
    }
    return made;
}

// ---- a slide, a presentation

QVariantList actions(const QVariantList &internal)
{
    QVariantList made;
    for (const QVariant &each : internal) {
        QVariantMap action = each.toMap();
        // (For a menu's row and its picture, and a timer's set-up as the file has it,
        // which its set_… say in plain values)
        action.remove(QStringLiteral("label"));
        action.remove(QStringLiteral("picture"));
        action.remove(QStringLiteral("configuration"));
        made.append(plain(action));
    }
    return made;
}

QVariantMap slide(const QVariantMap &internal, int index, bool full)
{
    QVariantMap made {
        {QStringLiteral("id"), internal.value(QStringLiteral("id"))},
        {QStringLiteral("index"), index},
        {QStringLiteral("group"), internal.value(QStringLiteral("group")).toString()},
        {QStringLiteral("label"), internal.value(QStringLiteral("label")).toString()},
        {QStringLiteral("enabled"), !internal.value(QStringLiteral("disabled")).toBool()},
        {QStringLiteral("text"), internal.value(QStringLiteral("plainText")).toString()},
        {QStringLiteral("notes"), internal.value(QStringLiteral("notesText")).toString()},
        {QStringLiteral("width"), internal.value(QStringLiteral("width"))},
        {QStringLiteral("height"), internal.value(QStringLiteral("height"))},
        {QStringLiteral("background"), internal.value(QStringLiteral("drawsBackground")).toBool()
                                           ? colorText(internal.value(QStringLiteral("backgroundColor")).value<QColor>()) : QString()},
    };
    const QString labelColor = internal.value(QStringLiteral("labelColor")).toString();
    if (!labelColor.isEmpty())
        made.insert(QStringLiteral("label_color"), labelColor);
    const QString mediaName = internal.value(QStringLiteral("mediaName")).toString();
    if (!mediaName.isEmpty()) {
        const QVariantMap found = internal.value(QStringLiteral("media")).toMap();
        QVariantMap media {
            {QStringLiteral("name"), mediaName},
            {QStringLiteral("file"), found.value(QStringLiteral("path")).toString()},
            // (Its file is not to be found on this computer)
            {QStringLiteral("missing"), found.isEmpty()},
            {QStringLiteral("video"), internal.value(QStringLiteral("mediaVideo")).toBool()},
            {QStringLiteral("foreground"), internal.value(QStringLiteral("mediaForeground")).toBool()},
        };
        if (internal.value(QStringLiteral("mediaVideo")).toBool()) {
            const int playback = internal.value(QStringLiteral("mediaPlayback")).toInt();
            media.insert(QStringLiteral("playback"), playbackWord(playback));
            if (playback == 2)
                media.insert(QStringLiteral("loop_count"), internal.value(QStringLiteral("mediaLoopCount")));
            if (playback == 3)
                media.insert(QStringLiteral("loop_seconds"), internal.value(QStringLiteral("mediaLoopSeconds")));
        }
        if (found.contains(QStringLiteral("settings")))
            media.insert(QStringLiteral("settings"), plain(found.value(QStringLiteral("settings"))));
        made.insert(QStringLiteral("media"), media);
    }
    if (internal.contains(QStringLiteral("goToNext")))
        made.insert(QStringLiteral("go_to_next"), plain(internal.value(QStringLiteral("goToNext"))));
    made.insert(QStringLiteral("actions"), actions(internal.value(QStringLiteral("actions")).toList()));
    if (full) {
        QVariantList elements;
        int order = 0;
        for (const QVariant &each : internal.value(QStringLiteral("elements")).toList())
            elements.append(element(each.toMap(), order++));
        made.insert(QStringLiteral("elements"), elements);
    }
    return made;
}

QVariantMap themeSlide(const QVariantMap &internal, const QString &name, int index)
{
    QVariantList elements;
    int order = 0;
    for (const QVariant &each : internal.value(QStringLiteral("elements")).toList())
        elements.append(element(each.toMap(), order++));
    return {
        {QStringLiteral("id"), internal.value(QStringLiteral("id"))},
        {QStringLiteral("name"), name},
        {QStringLiteral("index"), index},
        {QStringLiteral("width"), internal.value(QStringLiteral("width"))},
        {QStringLiteral("height"), internal.value(QStringLiteral("height"))},
        {QStringLiteral("background"), internal.value(QStringLiteral("drawsBackground")).toBool()
                                           ? colorText(internal.value(QStringLiteral("backgroundColor")).value<QColor>()) : QString()},
        {QStringLiteral("elements"), elements},
    };
}

namespace {

// A timeline (timeline::describe) as the API has it: its length, whether it goes round,
// and its cues, each { place, time, name, track } with the slide's id or the action
QVariantMap timelineOf(const QVariantMap &timeline)
{
    QVariantList cues;
    for (const QVariant &each : timeline.value(QStringLiteral("cues")).toList()) {
        const QVariantMap cue = each.toMap();
        QVariantMap made {
            {QStringLiteral("place"), cue.value(QStringLiteral("place"))},
            {QStringLiteral("time"), cue.value(QStringLiteral("time"))},
            {QStringLiteral("name"), cue.value(QStringLiteral("name"))},
            {QStringLiteral("track"), cue.value(QStringLiteral("track"))},
        };
        if (cue.contains(QStringLiteral("action")))
            made.insert(QStringLiteral("action"), actions({cue.value(QStringLiteral("action"))}).value(0));
        else
            made.insert(QStringLiteral("slide"), cue.value(QStringLiteral("slideId")));
        cues.append(made);
    }
    return {
        {QStringLiteral("duration"), timeline.value(QStringLiteral("duration"))},
        {QStringLiteral("loop"), timeline.value(QStringLiteral("loop")).toBool()},
        {QStringLiteral("cues"), cues},
    };
}

}

QVariantMap presentation(const QVariantMap &document, const QString &library, const QList<QStringList> &groupSlides, Detail detail)
{
    const bool full = detail == Full;
    QVariantList groups;
    QHash<QString, QString> groupNames;
    const QVariantList documentGroups = document.value(QStringLiteral("groups")).toList();
    for (qsizetype i = 0; i < documentGroups.size(); ++i) {
        const QVariantMap group = documentGroups.at(i).toMap();
        groupNames.insert(group.value(QStringLiteral("id")).toString(), group.value(QStringLiteral("name")).toString());
        groups.append(QVariantMap {
            {QStringLiteral("id"), group.value(QStringLiteral("id"))},
            {QStringLiteral("name"), group.value(QStringLiteral("name"))},
            {QStringLiteral("color"), group.value(QStringLiteral("color"))},
            {QStringLiteral("slides"), groupSlides.value(i)},
        });
    }
    QVariantList arrangements;
    for (const QVariant &each : document.value(QStringLiteral("arrangementList")).toList()) {
        const QVariantMap arrangement = each.toMap();
        QStringList names;
        const QStringList ids = arrangement.value(QStringLiteral("groups")).toStringList();
        for (const QString &id : ids)
            names.append(groupNames.value(id));
        arrangements.append(QVariantMap {
            {QStringLiteral("id"), arrangement.value(QStringLiteral("id"))},
            {QStringLiteral("name"), arrangement.value(QStringLiteral("name"))},
            {QStringLiteral("groups"), ids},
            {QStringLiteral("group_names"), names},
        });
    }
    // One for each cue, in the presentation's own order: a cue that shows more than one
    // slide (ProPresenter makes none, but the file allows it) is the first of them
    QVariantList slides;
    QString last;
    int index = 0;
    for (const QVariant &each : document.value(QStringLiteral("slides")).toList()) {
        const QVariantMap map = each.toMap();
        const QString id = map.value(QStringLiteral("id")).toString();
        if (!slides.isEmpty() && id == last)
            continue;
        last = id;
        slides.append(slide(map, index++, full));
    }
    QVariantMap copyright = plain(document.value(QStringLiteral("ccli"))).toMap();
    QVariantMap made {
        {QStringLiteral("uuid"), document.value(QStringLiteral("uuid"))},
        {QStringLiteral("name"), document.value(QStringLiteral("name"))},
        {QStringLiteral("library"), library},
        {QStringLiteral("file"), document.value(QStringLiteral("path"))},
        {QStringLiteral("destination"), document.value(QStringLiteral("destination")).toString() == QLatin1String("announcements")
                                            ? QStringLiteral("announcements") : QStringLiteral("presentation")},
        {QStringLiteral("slide_show"), document.value(QStringLiteral("slideShow"))},
        {QStringLiteral("arrangement"), document.value(QStringLiteral("selectedArrangement")).toString()},
        {QStringLiteral("key"), document.value(QStringLiteral("originalKey"))},
        {QStringLiteral("shown_key"), document.value(QStringLiteral("userKey"))},
        {QStringLiteral("copyright"), copyright},
        {QStringLiteral("groups"), groups},
        {QStringLiteral("arrangements"), arrangements},
        {QStringLiteral("slide_count"), slides.size()},
        {QStringLiteral("timeline"), timelineOf(document.value(QStringLiteral("timeline")).toMap())},
    };
    if (detail != Summary)
        made.insert(QStringLiteral("slides"), slides);
    return made;
}

// ---- playlists

QVariantMap playlistItem(const QVariantMap &row, int index, const QString &uuid, const QString &library)
{
    const QString kind = row.value(QStringLiteral("kind")).toString();
    QVariantMap made {
        {QStringLiteral("id"), row.value(QStringLiteral("path"))},
        {QStringLiteral("index"), index},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("name"), row.value(QStringLiteral("name"))},
    };
    if (kind == QLatin1String("header")) {
        made.insert(QStringLiteral("color"), row.value(QStringLiteral("color")).toString());
    } else if (kind == QLatin1String("presentation")) {
        made.insert(QStringLiteral("presentation"), uuid);
        made.insert(QStringLiteral("library"), library);
        made.insert(QStringLiteral("arrangement"), row.value(QStringLiteral("arrangement")).toString());
        made.insert(QStringLiteral("missing"), row.value(QStringLiteral("missing")).toBool());
        if (row.value(QStringLiteral("destination")).toString() == QLatin1String("announcements"))
            made.insert(QStringLiteral("destination"), QStringLiteral("announcements"));
    }
    return made;
}

QVariantList playlistTree(const QVariantList &nodes, const QHash<QString, int> &counts)
{
    QVariantList made;
    for (const QVariant &each : nodes) {
        const QVariantMap node = each.toMap();
        const QString id = node.value(QStringLiteral("path")).toString();
        QVariantMap shaped {
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), node.value(QStringLiteral("name"))},
            {QStringLiteral("folder"), node.value(QStringLiteral("folder")).toBool()},
            {QStringLiteral("depth"), node.value(QStringLiteral("depth"))},
            {QStringLiteral("parent"), node.value(QStringLiteral("parent"))},
        };
        if (!node.value(QStringLiteral("folder")).toBool())
            shaped.insert(QStringLiteral("items"), counts.value(id));
        made.append(shaped);
    }
    return made;
}

QVariantMap error(const QString &why)
{
    return {{QStringLiteral("error"), why}};
}

}
