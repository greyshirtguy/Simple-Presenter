// What the extended API sends and takes, shaped (src/extendedapijson.h): names and
// colours both ways; the app's maps as JSON has them; text as its words, its format and
// its runs; an element as the API shows it, and what is sent to change one sorted out,
// with what cannot be said so; a slide, a presentation and a playlist's rows. And that
// the properties it lets through to be changed are the ones proconvert::applyChanges
// reads, by reading its source.

#include "extendedapijson.h"

#include <QFile>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QTest>
#include <QUrl>

using namespace extendedapijson;

namespace {

TextRun run(const QString &text, double size = 84, const QColor &color = Qt::white)
{
    TextRun made;
    made.text = text;
    made.family = QStringLiteral("Helvetica Neue");
    made.fontName = QStringLiteral("HelveticaNeue");
    made.size = size;
    made.fill = color;
    return made;
}

RichText words(const QString &text, Qt::Alignment alignment = Qt::AlignHCenter)
{
    return RichText::plain(text, run(QString()), alignment);
}

// An element as proconvert::toSlideMap gives one: a text box of white words, no fill
// shown, no stroke, a text shadow, linked to nothing
QVariantMap textBox()
{
    return {
        {"id", "E1"}, {"name", "Lyrics"}, {"x", 96.0}, {"y", 54.0}, {"width", 1728.0}, {"height", 972.0}, {"rotation", 0.0},
        {"opacity", 1.0}, {"locked", false}, {"hidden", false},
        {"fillOn", false}, {"fillKind", "color"}, {"fillEnabled", false}, {"fillColor", QColor(0, 0, 0, 128)}, {"fillShown", false},
        {"fillLinesOnly", false}, {"lineMaskStyle", 0}, {"lineMaskWidthOffset", 0.0},
        {"fillGradientFrom", QColor(Qt::white)}, {"fillGradientTo", QColor(Qt::black)}, {"fillGradientAngle", 0.0},
        {"fillMediaName", ""}, {"fillMediaPath", ""}, {"fillMediaSource", QUrl()}, {"fillMediaVideo", false}, {"fillMediaScale", 0},
        {"strokeOn", false}, {"strokeEnabled", false}, {"strokeColor", QColor(Qt::white)}, {"strokeWidth", 3.0},
        {"shadowEnabled", false}, {"shadowColor", QColor(0, 0, 0, 191)}, {"shadowAngle", 315.0}, {"shadowOffset", 5.0}, {"shadowRadius", 5.0},
        {"shadowOffsetX", 3.5}, {"shadowOffsetY", 3.5},
        {"textShadowEnabled", true}, {"textShadowColor", QColor(0, 0, 0, 191)}, {"textShadowAngle", 315.0}, {"textShadowOffset", 5.0},
        {"textShadowRadius", 5.0}, {"textShadowOffsetX", 3.5}, {"textShadowOffsetY", 3.5},
        {"shape", "rectangle"}, {"roundness", 0.0}, {"outline", QVariantList()}, {"featherOn", false}, {"featherRadius", 0.0},
        {"text", QVariant::fromValue(words(QStringLiteral("Amazing grace\nhow sweet the sound")))}, {"textBox", true},
        {"words", "Amazing grace how sweet the sound"}, {"verticalAlignment", int(Qt::AlignVCenter)}, {"textScale", 0},
        {"marginLeft", 0.0}, {"marginTop", 0.0}, {"marginRight", 0.0}, {"marginBottom", 0.0}, {"textTransform", 0}, {"textTransformDelimiter", ""},
        {"chordsOn", false}, {"chordNotation", 0}, {"chordColor", QColor(Qt::white)},
        {"linkKind", "none"}, {"linkElementId", ""}, {"linkElementName", ""}, {"linkTransform", 0}, {"linkTimerId", ""},
        {"visibilityRules", false}, {"visibilityCriterion", 0}, {"visibilityConditions", QVariantList()}, {"visibilityTimed", false},
        {"displayText", QVariant::fromValue(words(QStringLiteral("Amazing grace\nhow sweet the sound")))}, {"hasText", true}, {"visible", true},
    };
}

}

class TestExtendedApiJson : public QObject
{
    Q_OBJECT

private slots:
    // ---- names and values

    void namesGoToSnakeCaseAndBack()
    {
        QCOMPARE(snake(QStringLiteral("fillColor")), QStringLiteral("fill_color"));
        QCOMPARE(snake(QStringLiteral("textShadowOffsetX")), QStringLiteral("text_shadow_offset_x"));
        QCOMPARE(snake(QStringLiteral("linkClock24")), QStringLiteral("link_clock24"));
        QCOMPARE(snake(QStringLiteral("x")), QStringLiteral("x"));
        QCOMPARE(camel(QStringLiteral("fill_color")), QStringLiteral("fillColor"));
        QCOMPARE(camel(QStringLiteral("link_clock24")), QStringLiteral("linkClock24"));
        QCOMPARE(camel(QStringLiteral("name")), QStringLiteral("name"));
        // Every property that can be changed is its own name there and back
        for (const QString &name : changeableProperties())
            QCOMPARE(snake(camel(name)), name);
    }

    void aColourIsWrittenAsAWebPageWritesOne()
    {
        QCOMPARE(colorText(QColor(255, 255, 255)), QStringLiteral("#ffffff"));
        QCOMPARE(colorText(QColor(1, 2, 3)), QStringLiteral("#010203"));
        // Partly see-through: that part last, not first as Qt has it
        QCOMPARE(colorText(QColor(255, 0, 0, 128)), QStringLiteral("#ff000080"));
        QCOMPARE(colorText(QColor(0, 0, 0, 0)), QStringLiteral("#00000000"));

        QColor color;
        QVERIFY(colorOf(QStringLiteral("#ff000080"), &color));
        QCOMPARE(color, QColor(255, 0, 0, 128));
        QVERIFY(colorOf(QStringLiteral("#0A0b0C"), &color));
        QCOMPARE(color, QColor(10, 11, 12));
        QVERIFY(colorOf(QStringLiteral("#f80"), &color));
        QCOMPARE(color, QColor(255, 136, 0));
        QVERIFY(colorOf(QStringLiteral("#f808"), &color));
        QCOMPARE(color, QColor(255, 136, 0, 136));
        QVERIFY(colorOf(QStringLiteral(" white "), &color));
        QCOMPARE(color, QColor(Qt::white));
        QVERIFY(colorOf(QColor(Qt::red), &color));
        QCOMPARE(color, QColor(Qt::red));
        for (const QVariant &wrong : {QVariant(QStringLiteral("#12345")), QVariant(QStringLiteral("#gggggg")), QVariant(QStringLiteral("no such colour")),
                                      QVariant(QStringLiteral("")), QVariant(12), QVariant()})
            QVERIFY2(!colorOf(wrong, &color), qPrintable(wrong.toString()));
        // There and back
        for (const QColor &each : {QColor(18, 52, 86), QColor(18, 52, 86, 120)}) {
            QVERIFY(colorOf(colorText(each), &color));
            QCOMPARE(color, each);
        }
    }

    void theAppsMapsAreSaidAsJsonHasThem()
    {
        const QVariantMap made = plain(QVariantMap {
            {"timerId", "T1"}, {"kind", "clearGroup"}, {"groupColor", QColor(0, 128, 255)},
            {"mediaSource", QUrl::fromLocalFile(QStringLiteral("/ws/Media/a b.png"))},
            {"notes", QVariant::fromValue(words(QStringLiteral("Say this")))},
            {"assignments", QVariantList {QVariantMap {{"screenId", "S1"}, {"layoutName", "Lyrics"}}}},
            {"inner", QVariantMap {{"loopCount", 3}}},
        }).toMap();
        QCOMPARE(made.value("timer_id").toString(), QStringLiteral("T1"));
        // (One of a few words goes as names go)
        QCOMPARE(made.value("kind").toString(), QStringLiteral("clear_group"));
        QCOMPARE(made.value("group_color").toString(), QStringLiteral("#0080ff"));
        QCOMPARE(made.value("media_source").toString(), QStringLiteral("/ws/Media/a b.png"));
        QCOMPARE(made.value("notes").toString(), QStringLiteral("Say this"));
        QCOMPARE(made.value("assignments").toList().first().toMap().value("screen_id").toString(), QStringLiteral("S1"));
        QCOMPARE(made.value("inner").toMap().value("loop_count").toInt(), 3);
        QVERIFY(!made.contains("timerId"));

        // And back to the app's names, the words with them, nothing else changed
        const QVariantMap back = named({{"kind", "clear_group"}, {"group_id", "G"}, {"assignments", QVariantList {QVariantMap {{"screen_id", "S1"}}}},
                                        {"values", QVariantList {QVariantMap {{"token_id", "t"}, {"kind", "text"}}}}});
        QCOMPARE(back.value("kind").toString(), QStringLiteral("clearGroup"));
        QCOMPARE(back.value("groupId").toString(), QStringLiteral("G"));
        QCOMPARE(back.value("assignments").toList().first().toMap().value("screenId").toString(), QStringLiteral("S1"));
        QCOMPARE(back.value("values").toList().first().toMap().value("tokenId").toString(), QStringLiteral("t"));
    }

    void numbersThatMeanWordsAreWords()
    {
        QCOMPARE(alignmentWord(Qt::AlignHCenter), QStringLiteral("center"));
        QCOMPARE(alignmentWord(Qt::AlignLeft | Qt::AlignVCenter), QStringLiteral("left"));
        QCOMPARE(alignmentOf(QStringLiteral("Right")), int(Qt::AlignRight));
        QCOMPARE(alignmentOf(QStringLiteral("centre")), int(Qt::AlignHCenter));
        QCOMPARE(alignmentOf(QStringLiteral("justify")), int(Qt::AlignJustify));
        QCOMPARE(alignmentOf(int(Qt::AlignRight)), int(Qt::AlignRight));
        QCOMPARE(alignmentOf(QStringLiteral("sideways")), -1);
        QCOMPARE(alignmentOf(3), -1);

        QCOMPARE(verticalWord(Qt::AlignVCenter), QStringLiteral("middle"));
        QCOMPARE(verticalOf(QStringLiteral("top")), int(Qt::AlignTop));
        QCOMPARE(verticalOf(QStringLiteral("center")), int(Qt::AlignVCenter));
        QCOMPARE(verticalOf(QStringLiteral("bottom")), int(Qt::AlignBottom));
        QCOMPARE(verticalOf(QStringLiteral("left")), -1);

        QCOMPARE(capitalsWord(TextRun::AllCaps), QStringLiteral("all_caps"));
        QCOMPARE(capitalsOf(QStringLiteral("title_case")), int(TextRun::TitleCase));
        QCOMPARE(capitalsOf(QStringLiteral("none")), 0);
        QCOMPARE(capitalsOf(QStringLiteral("shouting")), -1);

        QCOMPARE(playbackWord(1), QStringLiteral("loop"));
        QCOMPARE(playbackOf(QStringLiteral("loop_seconds")), 3);
        QCOMPARE(playbackOf(QStringLiteral("stop")), 0);
        QCOMPARE(playbackOf(QStringLiteral("backwards")), -1);
    }

    // ---- text

    void aFormatSaysWhatARunLooksLike()
    {
        TextRun bold = run(QStringLiteral("x"), 72, QColor(255, 255, 0));
        bold.bold = true;
        const QVariantMap made = format(bold, Qt::AlignHCenter);
        QCOMPARE(made.value("family").toString(), QStringLiteral("Helvetica Neue"));
        QCOMPARE(made.value("font_name").toString(), QStringLiteral("HelveticaNeue"));
        QCOMPARE(made.value("size").toDouble(), 72.0);
        QCOMPARE(made.value("bold").toBool(), true);
        QCOMPARE(made.value("italic").toBool(), false);
        QCOMPARE(made.value("color").toString(), QStringLiteral("#ffff00"));
        QCOMPARE(made.value("alignment").toString(), QStringLiteral("center"));
        // What is nothing is not said
        for (const char *unsaid : {"underline", "strikethrough", "kerning", "capitalization", "stroke_color", "stroke_width"})
            QVERIFY2(!made.contains(unsaid), unsaid);

        TextRun fancy = run(QStringLiteral("x"));
        fancy.underline = true;
        fancy.capitalization = TextRun::AllCaps;
        fancy.strokeWidth = 2;
        fancy.stroke = Qt::black;
        fancy.kerning = 1.5;
        const QVariantMap more = format(fancy, Qt::AlignLeft);
        QCOMPARE(more.value("underline").toBool(), true);
        QCOMPARE(more.value("capitalization").toString(), QStringLiteral("all_caps"));
        QCOMPARE(more.value("stroke_color").toString(), QStringLiteral("#000000"));
        QCOMPARE(more.value("stroke_width").toDouble(), 2.0);
        QCOMPARE(more.value("kerning").toDouble(), 1.5);
    }

    void aFormatSentIsTurnedToTheAppsOwn()
    {
        QString error;
        const QVariantMap made = formatChanges({{"family", "Arial"}, {"size", 90}, {"bold", true}, {"color", "#ff0000"}, {"alignment", "right"},
                                                {"capitalization", "all_caps"}, {"stroke_color", "black"}, {"stroke_width", 2.5}, {"font_name", "Arial-BoldMT"}},
                                               &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(made.value("family").toString(), QStringLiteral("Arial"));
        QCOMPARE(made.value("size").toDouble(), 90.0);
        QCOMPARE(made.value("bold").toBool(), true);
        QCOMPARE(made.value("color").value<QColor>(), QColor(Qt::red));
        QCOMPARE(made.value("alignment").toInt(), int(Qt::AlignRight));
        QCOMPARE(made.value("capitalization").toInt(), int(TextRun::AllCaps));
        QCOMPARE(made.value("strokeColor").value<QColor>(), QColor(Qt::black));
        QCOMPARE(made.value("strokeWidth").toDouble(), 2.5);
        QCOMPARE(made.value("fontName").toString(), QStringLiteral("Arial-BoldMT"));

        // What is wrong with one is said, and nothing is made of it
        for (const QVariantMap &wrong : {QVariantMap {{"color", "nearly red"}}, QVariantMap {{"alignment", "sideways"}}, QVariantMap {{"size", "big"}},
                                         QVariantMap {{"size", 0}}, QVariantMap {{"bold", "yes"}}, QVariantMap {{"family", ""}},
                                         QVariantMap {{"font_size", 12}}, QVariantMap {{"capitalization", "loud"}}}) {
            error.clear();
            QVERIFY(formatChanges(wrong, &error).isEmpty());
            QVERIFY2(!error.isEmpty(), qPrintable(wrong.firstKey()));
        }
        // (The name that was not known is named)
        formatChanges({{"font_size", 12}}, &error);
        QVERIFY(error.contains(QStringLiteral("font_size")));
    }

    void textIsSaidRunByRunAndReadBack()
    {
        RichText text = words(QStringLiteral("First line\nSecond"));
        TextRun red = run(QStringLiteral(" in red"), 84, Qt::red);
        text.paragraphs[1].runs.append(red);
        QVERIFY(!uniform(text));
        QVERIFY(uniform(words(QStringLiteral("One\nTwo"))));

        const QVariantList said = paragraphs(text);
        QCOMPARE(said.size(), 2);
        QCOMPARE(said.at(0).toMap().value("alignment").toString(), QStringLiteral("center"));
        QCOMPARE(said.at(0).toMap().value("runs").toList().first().toMap().value("text").toString(), QStringLiteral("First line"));
        const QVariantList runs = said.at(1).toMap().value("runs").toList();
        QCOMPARE(runs.size(), 2);
        QCOMPARE(runs.at(1).toMap().value("color").toString(), QStringLiteral("#ff0000"));
        // (An alignment is its paragraph's, not each run's)
        QVERIFY(!runs.at(1).toMap().contains("alignment"));

        // Read back, it is the text it was
        RichText back;
        QString error;
        QVERIFY2(paragraphsOf(said, run(QString(), 12, Qt::black), Qt::AlignLeft, &back, &error), qPrintable(error));
        QCOMPARE(back.plainText(), text.plainText());
        QCOMPARE(back.paragraphs.at(1).runs.at(1).fill, QColor(Qt::red));
        QCOMPARE(back.paragraphs.at(0).alignment, Qt::AlignHCenter);
        QCOMPARE(back.paragraphs.at(0).runs.at(0).size, 84.0);
    }

    void aRunTakesWhatItDoesNotSayFromTheLookThereWas()
    {
        RichText made;
        QString error;
        const TextRun like = run(QString(), 60, QColor(Qt::yellow));
        QVERIFY(paragraphsOf({QVariantMap {{"runs", QVariantList {QVariantMap {{"text", "Plain "}}, QVariantMap {{"text", "bold"}, {"bold", true}}}}},
                              QStringLiteral("Only words"), QVariantMap {{"text", "Words too"}, {"alignment", "left"}}},
                             like, Qt::AlignRight, &made, &error));
        QCOMPARE(made.paragraphs.size(), 3);
        QCOMPARE(made.paragraphs.at(0).runs.at(0).size, 60.0);
        QCOMPARE(made.paragraphs.at(0).runs.at(0).fill, QColor(Qt::yellow));
        QCOMPARE(made.paragraphs.at(0).runs.at(1).bold, true);
        QCOMPARE(made.paragraphs.at(0).alignment, Qt::AlignRight);
        QCOMPARE(made.paragraphs.at(1).runs.at(0).text, QStringLiteral("Only words"));
        QCOMPARE(made.paragraphs.at(2).alignment, Qt::AlignLeft);
        QCOMPARE(made.plainText(), QStringLiteral("Plain bold\nOnly words\nWords too"));

        // No paragraphs: no words, in the look there was
        QVERIFY(paragraphsOf({}, like, Qt::AlignRight, &made, &error));
        QVERIFY(made.isEmpty());
        QCOMPARE(made.firstRun().size, 60.0);
        // What is not one
        QVERIFY(!paragraphsOf({QVariantMap {{"runs", "words"}}}, like, Qt::AlignLeft, &made, &error));
        QVERIFY(!paragraphsOf({QVariantMap {{"runs", QVariantList {QVariantMap {{"text", "x"}, {"colour", "red"}}}}}}, like, Qt::AlignLeft, &made, &error));
        QVERIFY(!paragraphsOf({QVariantMap {{"alignment", "up"}, {"runs", QVariantList()}}}, like, Qt::AlignLeft, &made, &error));
    }

    // ---- an element

    void anElementReadsAsWhatIsOnIt()
    {
        const QVariantMap made = element(textBox(), 2);
        QCOMPARE(made.value("id").toString(), QStringLiteral("E1"));
        QCOMPARE(made.value("name").toString(), QStringLiteral("Lyrics"));
        QCOMPARE(made.value("x").toDouble(), 96.0);
        QCOMPARE(made.value("order").toInt(), 2);
        QCOMPARE(made.value("text").toString(), QStringLiteral("Amazing grace\nhow sweet the sound"));
        QCOMPARE(made.value("format").toMap().value("size").toDouble(), 84.0);
        QCOMPARE(made.value("format").toMap().value("color").toString(), QStringLiteral("#ffffff"));
        QCOMPARE(made.value("vertical_alignment").toString(), QStringLiteral("middle"));
        QCOMPARE(made.value("shape").toString(), QStringLiteral("rectangle"));
        QCOMPARE(made.value("visible").toBool(), true);
        // All in one format: its runs are not spelt out, unless asked for
        QVERIFY(!made.contains("paragraphs"));
        QVERIFY(element(textBox(), 0, true).contains("paragraphs"));

        // What says nothing while its switch is off is left out with it
        QCOMPARE(made.value("fill_on").toBool(), false);
        QCOMPARE(made.value("stroke_on").toBool(), false);
        QCOMPARE(made.value("shadow_enabled").toBool(), false);
        QCOMPARE(made.value("link_kind").toString(), QStringLiteral("none"));
        for (const char *unsaid : {"fill_color", "fill_kind", "fill_gradient_from", "fill_media_path", "stroke_color", "stroke_width", "shadow_color",
                                   "shadow_angle", "link_element_id", "link_timer_id", "visibility_criterion", "visibility_conditions",
                                   "feather_radius", "chord_color", "roundness", "line_mask_style", "text_transform"})
            QVERIFY2(!made.contains(unsaid), unsaid);
        // And what is on says all of itself (the shadow's colour with how see-through it is)
        QCOMPARE(made.value("text_shadow_enabled").toBool(), true);
        QCOMPARE(made.value("text_shadow_color").toString(), QStringLiteral("#000000bf"));
        QCOMPARE(made.value("text_shadow_offset").toDouble(), 5.0);
        // What is only worked out, or only for drawing, is not said at all
        for (const char *unsaid : {"display_text", "has_text", "fill_shown", "fill_enabled", "stroke_enabled", "shadow_offset_x", "text_shadow_offset_y",
                                   "words", "outline", "text_box", "fill_media_source", "visibility_timed"})
            QVERIFY2(!made.contains(unsaid), unsaid);
        // No name of the app's own form is left in it
        for (auto it = made.constBegin(); it != made.constEnd(); ++it)
            QVERIFY2(it.key() == it.key().toLower(), qPrintable(it.key()));
    }

    void anElementWithAFillAStrokeAndMixedTextSaysThem()
    {
        QVariantMap box = textBox();
        box.insert("fillOn", true);
        box.insert("fillColor", QColor(0, 0, 0, 128));
        box.insert("strokeOn", true);
        box.insert("shape", "roundedRectangle");
        box.insert("roundness", 0.2);
        RichText text = words(QStringLiteral("White"));
        text.paragraphs[0].runs.append(run(QStringLiteral(" red"), 84, Qt::red));
        box.insert("text", QVariant::fromValue(text));
        const QVariantMap made = element(box, 0);
        QCOMPARE(made.value("fill_kind").toString(), QStringLiteral("color"));
        QCOMPARE(made.value("fill_color").toString(), QStringLiteral("#00000080"));
        QCOMPARE(made.value("stroke_color").toString(), QStringLiteral("#ffffff"));
        QCOMPARE(made.value("stroke_width").toDouble(), 3.0);
        QCOMPARE(made.value("shape").toString(), QStringLiteral("rounded_rectangle"));
        QCOMPARE(made.value("roundness").toDouble(), 0.2);
        // (A gradient's colours are a gradient's, not a plain colour's)
        QVERIFY(!made.contains("fill_gradient_from"));
        // More than one format in it: its runs are spelt out
        QCOMPARE(made.value("text").toString(), QStringLiteral("White red"));
        QCOMPARE(made.value("paragraphs").toList().first().toMap().value("runs").toList().size(), 2);

        box.insert("fillKind", "gradient");
        const QVariantMap gradient = element(box, 0);
        QCOMPARE(gradient.value("fill_gradient_from").toString(), QStringLiteral("#ffffff"));
        QVERIFY(!gradient.contains("fill_color"));
    }

    void whatIsSentToChangeAnElementIsSortedOut()
    {
        const ElementChanges changes = elementChanges({{"x", 200}, {"width", 1728}, {"fill_on", true}, {"fill_color", "#00000080"},
                                                       {"vertical_alignment", "bottom"}, {"name", "Lyrics"}, {"text", "New words"},
                                                       {"format", QVariantMap {{"size", 100}}}, {"order", 0},
                                                       {"chords", QVariantList {QVariantMap {{"at", 0}, {"name", "G"}}}}},
                                                      textBox());
        QVERIFY2(changes.error.isEmpty(), qPrintable(changes.error));
        // In the app's own names and values
        QCOMPARE(changes.properties.value("x").toDouble(), 200.0);
        QCOMPARE(changes.properties.value("fillOn").toBool(), true);
        QCOMPARE(changes.properties.value("verticalAlignment").toInt(), int(Qt::AlignBottom));
        // What it has already is not to be written again: its width, its name, and a
        // colour that is the colour it has
        QVERIFY(!changes.properties.contains("width"));
        QVERIFY(!changes.properties.contains("name"));
        QVERIFY(!changes.properties.contains("fillColor"));
        QCOMPARE(changes.properties.size(), 3);
        QVERIFY(changes.hasText);
        QCOMPARE(changes.text, QStringLiteral("New words"));
        QCOMPARE(changes.format.value("size").toDouble(), 100.0);
        QCOMPARE(changes.order, 0);
        QVERIFY(changes.hasChords);
        QVERIFY(!changes.hasParagraphs);
    }

    void aColourAndAWordAreTurnedAsTheyGoIn()
    {
        const ElementChanges changes = elementChanges({{"stroke_color", "#ff000080"}, {"fill_kind", "slide_image"}, {"link_kind", "slide_text"},
                                                       {"link_color_triggers", QVariantList {QVariantMap {{"time", 10}, {"color", "red"}}}},
                                                       {"fill_media_settings", QVariantMap {{"flip_horizontal", true}}},
                                                       {"visibility_conditions", QVariantList {QVariantMap {{"kind", "element"}, {"element_id", "E2"}, {"has_text", true}}}}},
                                                      textBox());
        QVERIFY2(changes.error.isEmpty(), qPrintable(changes.error));
        QCOMPARE(changes.properties.value("strokeColor").value<QColor>(), QColor(255, 0, 0, 128));
        QCOMPARE(changes.properties.value("fillKind").toString(), QStringLiteral("slideImage"));
        QCOMPARE(changes.properties.value("linkKind").toString(), QStringLiteral("slideText"));
        QCOMPARE(changes.properties.value("linkColorTriggers").toList().first().toMap().value("color").value<QColor>(), QColor(Qt::red));
        QCOMPARE(changes.properties.value("fillMediaSettings").toMap().value("flipHorizontal").toBool(), true);
        QCOMPARE(changes.properties.value("visibilityConditions").toList().first().toMap().value("elementId").toString(), QStringLiteral("E2"));
    }

    void propertiesMayBeGivenInAMapOfTheirOwn()
    {
        const ElementChanges changes = elementChanges({{"text", "Words"}, {"properties", QVariantMap {{"x", 10}, {"opacity", 0.5}}}, {"x", 20}}, textBox());
        QVERIFY(changes.error.isEmpty());
        // (What is beside it wins over what is inside it)
        QCOMPARE(changes.properties.value("x").toDouble(), 20.0);
        QCOMPARE(changes.properties.value("opacity").toDouble(), 0.5);
        QVERIFY(!elementChanges({{"properties", "x"}}, textBox()).error.isEmpty());
    }

    void whatCannotBeIsSaidAndNothingIsToBeDone()
    {
        // A name that is none of an element's
        ElementChanges changes = elementChanges({{"x", 10}, {"font_size", 90}}, textBox());
        QVERIFY(changes.error.contains(QStringLiteral("font_size")));
        // What can be read and not changed: sent back as it is, passed over; changed, refused
        changes = elementChanges({{"id", "E1"}, {"visible", true}, {"shape", "rectangle"}, {"x", 10}}, textBox());
        QVERIFY2(changes.error.isEmpty(), qPrintable(changes.error));
        QCOMPARE(changes.properties.size(), 1);
        changes = elementChanges({{"shape", "ellipse"}}, textBox());
        QVERIFY(changes.error.contains(QStringLiteral("shape")));
        QVERIFY(!elementChanges({{"id", "E2"}}, textBox()).error.isEmpty());
        // Values that are not what they should be
        QVERIFY(!elementChanges({{"fill_color", "nearly black"}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"vertical_alignment", "left"}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"name", "  "}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"text", 12}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"format", "bold"}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"format", QVariantMap {{"colour", "red"}}}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"paragraphs", "words"}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"chords", "G"}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"order", -1}}, textBox()).error.isEmpty());
        QVERIFY(!elementChanges({{"link_color_triggers", QVariantList {QVariantMap {{"time", 1}}}}}, textBox()).error.isEmpty());
    }

    void anElementShownSentBackWholeChangesNothing()
    {
        // What a program that reads an element, changes one thing and sends it all back does
        QVariantMap shown = element(textBox(), 1);
        ElementChanges changes = elementChanges(shown, textBox());
        QVERIFY2(changes.error.isEmpty(), qPrintable(changes.error));
        QVERIFY2(changes.properties.isEmpty(), qPrintable(changes.properties.keys().join(u',')));
        // (Its words come back too, the same words to be typed over the same words,
        // which whoever does it writes nothing for; and its format, which being the
        // format it was shown with is no format to put on it)
        QCOMPARE(changes.text, QStringLiteral("Amazing grace\nhow sweet the sound"));
        QVERIFY(changes.format.isEmpty());
        QVERIFY(!changes.hasParagraphs);
        // But what of the format is other than it has is to be done, and that alone
        QVariantMap larger = shown;
        QVariantMap format = larger.value("format").toMap();
        format.insert("size", 100);
        larger.insert("format", format);
        QCOMPARE(elementChanges(larger, textBox()).format.keys(), QStringList({"size"}));
        // A format of a property or two is not one sent back but one asked for: it is
        // put on all the words, though it be what the first of them have already (the
        // words after may not)
        const QVariantMap had = shown.value("format").toMap();
        QCOMPARE(elementChanges({{"format", QVariantMap {{"color", had.value("color")}}}}, textBox()).format.keys(), QStringList({"color"}));
        QCOMPARE(elementChanges({{"format", QVariantMap {{"color", had.value("color")}, {"size", had.value("size")}}}}, textBox()).format.size(), 2);
        // And its runs sent back as they were shown are nothing to write
        const QVariantMap rich = element(textBox(), 1, true);
        QVERIFY(!elementChanges(rich, textBox()).hasParagraphs);

        shown.insert("opacity", 0.5);
        changes = elementChanges(shown, textBox());
        QVERIFY(changes.error.isEmpty());
        QCOMPARE(changes.properties.keys(), QStringList({"opacity"}));
    }

    void anElementAboutToBeMadeTakesWhatItIsGiven()
    {
        // With nothing to compare with, everything given is to be done; and one sent as
        // another was shown (to make a copy of it) has what can only be read passed over
        QVariantMap copy = element(textBox(), 1);
        copy.insert("kind", "rounded_rectangle");
        const ElementChanges changes = elementChanges(copy, {});
        QVERIFY2(changes.error.isEmpty(), qPrintable(changes.error));
        QCOMPARE(changes.kind, QStringLiteral("roundedRectangle"));
        QCOMPARE(changes.properties.value("x").toDouble(), 96.0);
        QCOMPARE(changes.properties.value("name").toString(), QStringLiteral("Lyrics"));
        QVERIFY(!changes.properties.contains("id"));
        QVERIFY(!changes.properties.contains("visible"));
        QCOMPARE(elementChanges({{"kind", "media"}, {"file", "/ws/Media/a.png"}}, {}).file, QStringLiteral("/ws/Media/a.png"));
        QVERIFY(!elementChanges({{"nonsense", 1}}, {}).error.isEmpty());
    }

    void thePropertiesLetThroughAreThoseTheEditorChanges()
    {
        // proconvert::applyChanges is what changes an element: every property it reads
        // of what it is given is one this lets through, or the API could not reach it
        QFile source(QStringLiteral(PROCONVERT_SOURCE));
        QVERIFY2(source.open(QIODevice::ReadOnly), PROCONVERT_SOURCE);
        const QString text = QString::fromUtf8(source.readAll());
        const qsizetype start = text.indexOf(QLatin1String("\nbool applyChanges("));
        QVERIFY(start > 0);
        const qsizetype end = text.indexOf(QLatin1String("\n}\n"), start);
        QVERIFY(end > start);
        const QString body = text.mid(start, end - start);
        QSet<QString> read;
        for (const char *pattern : {"changes\\.(?:contains|value)\\((?:QStringLiteral\\()?\"([A-Za-z0-9]+)\"", "number\\(\"([A-Za-z0-9]+)\"\\)"}) {
            auto found = QRegularExpression(QString::fromLatin1(pattern)).globalMatch(body);
            while (found.hasNext())
                read.insert(found.next().captured(1));
        }
        QVERIFY2(read.size() > 40, qPrintable(QString::number(read.size())));
        // (The two shadows are changed by one function given the first part of their names)
        for (const char *prefix : {"shadow", "textShadow"}) {
            QVERIFY(body.contains(QStringLiteral("\"%1\"").arg(QLatin1String(prefix))));
            for (const char *part : {"Enabled", "Color", "SolidColor", "Opacity", "Angle", "Offset", "Radius"})
                read.insert(QLatin1String(prefix) + QLatin1String(part));
        }
        QSet<QString> let;
        for (const QString &name : changeableProperties())
            let.insert(camel(name));
        QStringList missing;
        for (const QString &name : std::as_const(read)) {
            if (!let.contains(name))
                missing.append(name);
        }
        missing.sort();
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("applyChanges reads what the API does not let through: ") + missing.join(QStringLiteral(", "))));
        // And it lets nothing through that applyChanges does not read
        QStringList unread;
        for (const QString &name : std::as_const(let)) {
            if (!read.contains(name))
                unread.append(name);
        }
        unread.sort();
        QVERIFY2(unread.isEmpty(), qPrintable(QStringLiteral("the API lets through what applyChanges does not read: ") + unread.join(QStringLiteral(", "))));
    }

    // ---- a slide, a presentation, a playlist

    void aSlideIsSaidInOutlineOrInFull()
    {
        const QVariantMap internal {
            {"id", "CUE-1"}, {"width", 1920.0}, {"height", 1080.0}, {"drawsBackground", true}, {"backgroundColor", QColor(Qt::black)},
            {"label", "Intro"}, {"labelColor", "#ff0000"}, {"group", "Verse 1"}, {"groupColor", "#0000ff"}, {"groupStart", true},
            {"plainText", "Amazing grace"}, {"notesText", "Slowly"}, {"disabled", true},
            {"mediaName", "blue.mp4"}, {"mediaForeground", false}, {"mediaVideo", true}, {"mediaPlayback", 2}, {"mediaLoopCount", 3}, {"mediaLoopSeconds", 0.0},
            {"media", QVariantMap {{"name", "blue.mp4"}, {"path", "/ws/Media/blue.mp4"}}},
            {"goToNext", QVariantMap {{"seconds", 5.0}, {"loop", false}}},
            {"actions", QVariantList {QVariantMap {{"id", "A1"}, {"kind", "clear"}, {"layer", 2}, {"title", "Clear the media"}, {"label", "Clear"},
                                                   {"picture", "ClearMedia"}, {"done", true}}}},
            {"elements", QVariantList {textBox()}},
        };
        const QVariantMap outline = slide(internal, 4, false);
        QCOMPARE(outline.value("id").toString(), QStringLiteral("CUE-1"));
        QCOMPARE(outline.value("index").toInt(), 4);
        QCOMPARE(outline.value("group").toString(), QStringLiteral("Verse 1"));
        QCOMPARE(outline.value("label").toString(), QStringLiteral("Intro"));
        QCOMPARE(outline.value("label_color").toString(), QStringLiteral("#ff0000"));
        QCOMPARE(outline.value("enabled").toBool(), false);
        QCOMPARE(outline.value("text").toString(), QStringLiteral("Amazing grace"));
        QCOMPARE(outline.value("notes").toString(), QStringLiteral("Slowly"));
        QCOMPARE(outline.value("background").toString(), QStringLiteral("#000000"));
        const QVariantMap media = outline.value("media").toMap();
        QCOMPARE(media.value("file").toString(), QStringLiteral("/ws/Media/blue.mp4"));
        QCOMPARE(media.value("missing").toBool(), false);
        QCOMPARE(media.value("playback").toString(), QStringLiteral("loop_count"));
        QCOMPARE(media.value("loop_count").toInt(), 3);
        QVERIFY(!media.contains("loop_seconds"));
        QCOMPARE(outline.value("go_to_next").toMap().value("seconds").toDouble(), 5.0);
        // An action without what is only for a menu's row
        const QVariantMap action = outline.value("actions").toList().first().toMap();
        QCOMPARE(action.value("kind").toString(), QStringLiteral("clear"));
        QCOMPARE(action.value("layer").toInt(), 2);
        QVERIFY(!action.contains("label"));
        QVERIFY(!action.contains("picture"));
        QVERIFY(!outline.contains("elements"));

        const QVariantMap full = slide(internal, 4, true);
        QCOMPARE(full.value("elements").toList().size(), 1);
        QCOMPARE(full.value("elements").toList().first().toMap().value("order").toInt(), 0);

        // A slide with none of that says so plainly
        const QVariantMap bare = slide({{"id", "CUE-2"}, {"width", 1920.0}, {"height", 1080.0}, {"drawsBackground", false}, {"mediaName", ""}}, 0, false);
        QCOMPARE(bare.value("enabled").toBool(), true);
        QCOMPARE(bare.value("background").toString(), QString());
        QVERIFY(!bare.contains("media"));
        QVERIFY(!bare.contains("go_to_next"));
        QVERIFY(!bare.contains("label_color"));
        // And media whose file is not on this computer says that
        QVERIFY(slide({{"id", "CUE-3"}, {"mediaName", "gone.mp4"}}, 0, false).value("media").toMap().value("missing").toBool());
    }

    void aPresentationIsSaidWithItsGroupsArrangementsAndSlides()
    {
        const QVariantMap document {
            {"uuid", "P-1"}, {"name", "Amazing Grace"}, {"path", "/ws/Libraries/Songs/Amazing Grace.pro"}, {"destination", ""},
            {"slideShow", 0.0}, {"selectedArrangement", "Short"}, {"originalKey", "G"}, {"userKey", "A"},
            {"ccli", QVariantMap {{"title", "Amazing Grace"}, {"songNumber", 22025}, {"display", true}}},
            {"groups", QVariantList {QVariantMap {{"id", "G1"}, {"name", "Verse 1"}, {"color", "#0000ff"}, {"slides", 2}},
                                     QVariantMap {{"id", "G2"}, {"name", "Chorus"}, {"color", ""}, {"slides", 1}}}},
            {"arrangementList", QVariantList {QVariantMap {{"id", "A1"}, {"name", "Short"}, {"groups", QStringList {"G1", "G2", "G1"}}}}},
            {"slides", QVariantList {QVariantMap {{"id", "C1"}, {"plainText", "one"}}, QVariantMap {{"id", "C2"}, {"plainText", "two"}},
                                     // (A cue that shows two slides is one slide here: its first)
                                     QVariantMap {{"id", "C3"}, {"plainText", "three"}}, QVariantMap {{"id", "C3"}, {"plainText", "three b"}}}},
            {"timeline", QVariantMap {{"duration", 300.0}, {"loop", false}, {"slides", QVariantMap {{"C1", QVariantMap()}}},
                                      {"cues", QVariantList {QVariantMap {{"place", 0}, {"time", 2.5}, {"name", "Cue 1"}, {"track", "slide"}, {"slideId", "C1"}}}}}},
        };
        const QList<QStringList> groupSlides {{"C1", "C2"}, {"C3"}};
        const QVariantMap made = presentation(document, QStringLiteral("Songs"), groupSlides, Outline);
        QCOMPARE(made.value("uuid").toString(), QStringLiteral("P-1"));
        QCOMPARE(made.value("library").toString(), QStringLiteral("Songs"));
        QCOMPARE(made.value("destination").toString(), QStringLiteral("presentation"));
        QCOMPARE(made.value("arrangement").toString(), QStringLiteral("Short"));
        QCOMPARE(made.value("key").toString(), QStringLiteral("G"));
        QCOMPARE(made.value("shown_key").toString(), QStringLiteral("A"));
        QCOMPARE(made.value("copyright").toMap().value("song_number").toInt(), 22025);
        const QVariantList groups = made.value("groups").toList();
        QCOMPARE(groups.at(0).toMap().value("slides").toStringList(), QStringList({"C1", "C2"}));
        QCOMPARE(groups.at(1).toMap().value("name").toString(), QStringLiteral("Chorus"));
        const QVariantMap arrangement = made.value("arrangements").toList().first().toMap();
        QCOMPARE(arrangement.value("groups").toStringList(), QStringList({"G1", "G2", "G1"}));
        QCOMPARE(arrangement.value("group_names").toStringList(), QStringList({"Verse 1", "Chorus", "Verse 1"}));
        QCOMPARE(made.value("slide_count").toInt(), 3);
        const QVariantList slides = made.value("slides").toList();
        QCOMPARE(slides.size(), 3);
        QCOMPARE(slides.at(2).toMap().value("index").toInt(), 2);
        QCOMPARE(slides.at(2).toMap().value("text").toString(), QStringLiteral("three"));
        // Its timeline: the cues, each with the slide it is of, and not what is for drawing
        const QVariantMap timeline = made.value("timeline").toMap();
        QCOMPARE(timeline.value("duration").toDouble(), 300.0);
        QCOMPARE(timeline.value("cues").toList().first().toMap().value("slide").toString(), QStringLiteral("C1"));
        QVERIFY(!timeline.contains("slides"));

        // In a few words: everything but the slides, with how many there are
        const QVariantMap summary = presentation(document, QStringLiteral("Songs"), groupSlides, Summary);
        QVERIFY(!summary.contains("slides"));
        QCOMPARE(summary.value("slide_count").toInt(), 3);
        QCOMPARE(summary.value("groups").toList().size(), 2);
        // Sent to the announcements, it says so
        QVariantMap announcements = document;
        announcements.insert("destination", "announcements");
        QCOMPARE(presentation(announcements, QStringLiteral("Songs"), groupSlides, Summary).value("destination").toString(), QStringLiteral("announcements"));
    }

    void aThemesSlideIsSaidWithItsElements()
    {
        const QVariantMap made = themeSlide({{"id", "S1"}, {"width", 1920.0}, {"height", 1080.0}, {"drawsBackground", false}, {"elements", QVariantList {textBox()}}},
                                            QStringLiteral("Lower Third"), 1);
        QCOMPARE(made.value("id").toString(), QStringLiteral("S1"));
        QCOMPARE(made.value("name").toString(), QStringLiteral("Lower Third"));
        QCOMPARE(made.value("index").toInt(), 1);
        QCOMPARE(made.value("background").toString(), QString());
        QCOMPARE(made.value("elements").toList().first().toMap().value("name").toString(), QStringLiteral("Lyrics"));
    }

    void aPlaylistsRowsAndTheListOfPlaylistsAreSaid()
    {
        const QVariantMap heading = playlistItem({{"path", "R1"}, {"name", "Worship"}, {"kind", "header"}, {"color", "#ff9300"}}, 0, {}, {});
        QCOMPARE(heading.value("id").toString(), QStringLiteral("R1"));
        QCOMPARE(heading.value("kind").toString(), QStringLiteral("header"));
        QCOMPARE(heading.value("color").toString(), QStringLiteral("#ff9300"));
        QVERIFY(!heading.contains("presentation"));

        const QVariantMap song = playlistItem({{"path", "R2"}, {"name", "Amazing Grace"}, {"kind", "presentation"}, {"arrangement", "Short"}, {"missing", false},
                                               {"file", "/ws/Libraries/Songs/Amazing Grace.pro"}},
                                              1, QStringLiteral("P-1"), QStringLiteral("Songs"));
        QCOMPARE(song.value("index").toInt(), 1);
        QCOMPARE(song.value("presentation").toString(), QStringLiteral("P-1"));
        QCOMPARE(song.value("library").toString(), QStringLiteral("Songs"));
        QCOMPARE(song.value("arrangement").toString(), QStringLiteral("Short"));
        QCOMPARE(song.value("missing").toBool(), false);
        // A row of a kind this app does not open is a row all the same
        QCOMPARE(playlistItem({{"path", "R3"}, {"name", "Offering"}, {"kind", "other"}}, 2, {}, {}).value("kind").toString(), QStringLiteral("other"));

        const QVariantList tree = playlistTree({QVariantMap {{"path", "F1"}, {"name", "2026"}, {"folder", true}, {"depth", 0}, {"parent", ""}},
                                                QVariantMap {{"path", "L1"}, {"name", "Sunday"}, {"folder", false}, {"depth", 1}, {"parent", "F1"}}},
                                               {{"L1", 7}});
        QCOMPARE(tree.at(0).toMap().value("folder").toBool(), true);
        QVERIFY(!tree.at(0).toMap().contains("items"));
        QCOMPARE(tree.at(1).toMap().value("items").toInt(), 7);
        QCOMPARE(tree.at(1).toMap().value("parent").toString(), QStringLiteral("F1"));

        QCOMPARE(error(QStringLiteral("No")).value("error").toString(), QStringLiteral("No"));
    }
};

// Fonts are looked up for the text (a new text box is set in the system's own font), which
// Qt does only for an application that could draw: one with windows, on Qt's offscreen
// platform, so that the test runs where there is no desktop.
int main(int argc, char *argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    TestExtendedApiJson test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_extendedapijson.moc"
