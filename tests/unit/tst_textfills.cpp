// ProPresenter's fills of a text's letters other than a colour (src/textfills.h): read from
// a text's attributes, as ProPresenter writes them beside the RTF, onto the runs the slides
// are drawn from. Each run covered by a fill is given that fill's number; runs are cut where
// a range begins or ends inside one, counting a paragraph break as one character, as
// capitalization's ranges are counted.
//
// What is written back (write): the fills as the runs have them, each a copy of what the
// file had, with everything in it, where it came from the file, at its new place where the
// words changed; set by the editor, as ProPresenter writes it; and a text not changed as
// it was, byte for byte. And a blur's amount shared over a slide (shareBlur).

#include "graphicsData.pb.h"
#include "slide.pb.h"
#include "textfills.h"

#include <google/protobuf/unknown_field_set.h>

#include <QTest>
#include <QUrl>

namespace {

using Attributes = rv::data::Graphics::Text::Attributes;

// A text of two paragraphs, a run each: "Hello world" and "Next"
RichText twoParagraphs()
{
    RichText text;
    TextParagraph first;
    first.runs = {TextRun()};
    first.runs[0].text = QStringLiteral("Hello world");
    first.runs[0].fill = Qt::white;
    TextParagraph second;
    second.runs = {TextRun()};
    second.runs[0].text = QStringLiteral("Next");
    second.runs[0].fill = Qt::white;
    text.paragraphs = {first, second};
    return text;
}

void setRange(Attributes::CustomAttribute *custom, int start, int end)
{
    custom->mutable_range()->set_start(start);
    custom->mutable_range()->set_end(end);
}

// A gradient as ProPresenter saves one nobody moved the stops of: red to cyan, both stops
// at 0, at 245 degrees, as in the maintainer's demo
void setGradient(rv::data::Graphics::Text::GradientFill *fill)
{
    rv::data::Graphics::Gradient *gradient = fill->mutable_gradient();
    gradient->set_angle(245);
    rv::data::Graphics::Gradient::ColorStop *red = gradient->add_stops();
    red->mutable_color()->set_red(1);
    red->mutable_color()->set_alpha(1);
    rv::data::Graphics::Gradient::ColorStop *cyan = gradient->add_stops();
    cyan->mutable_color()->set_green(1);
    cyan->mutable_color()->set_blue(1);
    cyan->mutable_color()->set_alpha(1);
}

// A picture as a media fill names one, scaled to fill
void setPicture(rv::data::Graphics::Text::MediaFill *fill, const char *url)
{
    rv::data::Media *media = fill->mutable_media();
    media->mutable_url()->set_absolute_string(url);
    media->mutable_image()->mutable_drawing()->set_scale_behavior(rv::data::Media::SCALE_BEHAVIOR_FILL);
    media->mutable_image()->mutable_drawing()->mutable_natural_size()->set_width(1920);
    media->mutable_image()->mutable_drawing()->mutable_natural_size()->set_height(1080);
}

// Finds a media fill's file as the app would: here, by the name at the end of its address
QString found(const rv::data::Media &media)
{
    return QStringLiteral("/pictures/") + QUrl(QString::fromStdString(media.url().absolute_string())).fileName();
}

// The attributes as proconvert's writeText leaves them: with the file's ranges that are
// not fills kept, and the fills written from the runs (textfills::write)
Attributes written(const Attributes &file, const RichText &text)
{
    Attributes after = file;
    after.clear_custom_attributes();
    for (const Attributes::CustomAttribute &custom : file.custom_attributes()) {
        if (!custom.has_text_gradient_fill() && !custom.has_media_fill() && !custom.has_cut_out_fill()
            && !custom.has_background_effect())
            *after.add_custom_attributes() = custom;
    }
    textfills::write(file, &after, text, nullptr);
    return after;
}

// The ranges of fills among the attributes' custom attributes
QList<Attributes::CustomAttribute> fillRanges(const Attributes &attributes)
{
    QList<Attributes::CustomAttribute> list;
    for (const Attributes::CustomAttribute &custom : attributes.custom_attributes()) {
        if (custom.has_text_gradient_fill() || custom.has_media_fill() || custom.has_cut_out_fill() || custom.has_background_effect())
            list << custom;
    }
    return list;
}

QStringList pieces(const TextParagraph &paragraph)
{
    QStringList list;
    for (const TextRun &run : paragraph.runs)
        list << run.text + QLatin1Char('=') + QString::number(run.fillEffect);
    return list;
}

} // namespace

class TestTextFills : public QObject
{
    Q_OBJECT

private slots:
    // A text with no such fill is left as it was, with no fills and its runs whole
    void noFillLeavesTheText()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        attributes.mutable_text_solid_fill()->set_red(1);
        textfills::apply(&text, attributes, found);
        QVERIFY(text.fillEffects.isEmpty());
        QCOMPARE(pieces(text.paragraphs[0]), QStringList({"Hello world=-1"}));
        QVERIFY(text == twoParagraphs());
    }

    // A fill of the whole text, with no ranges: every run has it
    void wholeTextFill()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        setPicture(attributes.mutable_media_fill(), "file:///Users/someone/Media/Gold.png");
        textfills::apply(&text, attributes, found);
        QCOMPARE(text.fillEffects.size(), 1);
        const QVariantMap media = text.fillEffects[0].toMap();
        QCOMPARE(media.value("kind").toString(), QStringLiteral("media"));
        QCOMPARE(media.value("source").toUrl(), QUrl::fromLocalFile("/pictures/Gold.png"));
        QCOMPARE(media.value("scale").toInt(), 1);
        QVERIFY(!media.contains("settings"));
        QCOMPARE(pieces(text.paragraphs[0]), QStringList({"Hello world=0"}));
        QCOMPARE(pieces(text.paragraphs[1]), QStringList({"Next=0"}));
        // Drawn over the letters by the graphics chip, with its number
        const QVariantList layered = textfills::layered(text);
        QCOMPARE(layered.size(), 1);
        QCOMPARE(layered[0].toMap().value("index").toInt(), 0);
        // And no longer the same text as before
        QVERIFY(!(text == twoParagraphs()));
    }

    // Ranges cut runs where they begin and end, counting a paragraph break as one
    // character; a range of something else (capitalization) is no fill
    void rangesCutRuns()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        Attributes::CustomAttribute *caps = attributes.add_custom_attributes();
        setRange(caps, 0, 5);
        caps->set_capitalization(Attributes::CAPITALIZATION_ALL_CAPS);
        Attributes::CustomAttribute *gradient = attributes.add_custom_attributes();
        setRange(gradient, 6, 11);
        setGradient(gradient->mutable_text_gradient_fill());
        Attributes::CustomAttribute *blur = attributes.add_custom_attributes();
        setRange(blur, 12, 16);
        blur->mutable_background_effect()->mutable_backgroundblur()->set_blur_amount(0.2648);
        textfills::apply(&text, attributes, found);

        QCOMPARE(pieces(text.paragraphs[0]), QStringList({"Hello =-1", "world=0"}));
        QCOMPARE(pieces(text.paragraphs[1]), QStringList({"Next=1"}));
        QCOMPARE(text.paragraphs[0].runs[1].fill, QColor(Qt::white));
        QCOMPARE(text.fillEffects.size(), 2);

        // The gradient's stops, all at one place in the file, spread evenly, as ProPresenter
        // draws them; not stretched over the box
        const QVariantMap ramp = text.fillEffects[0].toMap();
        QCOMPARE(ramp.value("kind").toString(), QStringLiteral("gradient"));
        QCOMPARE(ramp.value("angle").toDouble(), 245.0);
        QCOMPARE(ramp.value("radial").toBool(), false);
        QCOMPARE(ramp.value("stretch").toBool(), false);
        const QVariantList stops = ramp.value("stops").toList();
        QCOMPARE(stops.size(), 2);
        QCOMPARE(stops[0].toMap().value("position").toDouble(), 0.0);
        QCOMPARE(stops[1].toMap().value("position").toDouble(), 1.0);
        QCOMPARE(stops[0].toMap().value("color").value<QColor>(), QColor(Qt::red));
        QCOMPARE(stops[1].toMap().value("color").value<QColor>(), QColor(Qt::cyan));

        // The blur: its size as measured against ProPresenter's picture of it, and its
        // saturation, not written, as the colours are
        const QVariantMap blurred = text.fillEffects[1].toMap();
        QCOMPARE(blurred.value("kind").toString(), QStringLiteral("blur"));
        QVERIFY(qAbs(blurred.value("sigma").toDouble() - 6.9) < 0.1);
        QCOMPARE(blurred.value("saturation").toDouble(), 1.0);

        // Only the blur is the graphics chip's: the gradient is drawn with the letters
        const QVariantList layered = textfills::layered(text);
        QCOMPARE(layered.size(), 1);
        QCOMPARE(layered[0].toMap().value("kind").toString(), QStringLiteral("blur"));
        QCOMPARE(layered[0].toMap().value("index").toInt(), 1);
    }

    // A range that ends inside a run, and one that runs past the end of the text
    void rangesInsideARun()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        Attributes::CustomAttribute *invert = attributes.add_custom_attributes();
        setRange(invert, 2, 4);
        invert->mutable_background_effect()->mutable_backgroundinvert();
        Attributes::CustomAttribute *cut = attributes.add_custom_attributes();
        setRange(cut, 14, 400);
        cut->mutable_cut_out_fill();
        textfills::apply(&text, attributes, found);
        QCOMPARE(pieces(text.paragraphs[0]), QStringList({"He=-1", "ll=0", "o world=-1"}));
        QCOMPARE(pieces(text.paragraphs[1]), QStringList({"Ne=-1", "xt=1"}));
        QCOMPARE(text.fillEffects[0].toMap().value("kind").toString(), QStringLiteral("invert"));
        QCOMPARE(text.fillEffects[1].toMap().value("kind").toString(), QStringLiteral("cutOut"));
    }

    // Two ranges with the same fill share its number, and runs next to each other with the
    // same fill are one run
    void sameFillsShareANumber()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        for (const auto &[start, end] : {std::pair(0, 3), std::pair(3, 5), std::pair(12, 14)}) {
            Attributes::CustomAttribute *cut = attributes.add_custom_attributes();
            setRange(cut, start, end);
            cut->mutable_cut_out_fill();
        }
        textfills::apply(&text, attributes, found);
        QCOMPARE(text.fillEffects.size(), 1);
        QCOMPARE(pieces(text.paragraphs[0]), QStringList({"Hello=0", " world=-1"}));
        QCOMPARE(pieces(text.paragraphs[1]), QStringList({"Ne=0", "xt=-1"}));
    }

    // Where the file has ranges of fills, they are what is read, and not the whole text's
    // fill (which ProPresenter writes over the whole text and over a range of all of it
    // alike)
    void rangesOverTheWhole()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        attributes.mutable_background_effect()->mutable_backgroundinvert();
        Attributes::CustomAttribute *cut = attributes.add_custom_attributes();
        setRange(cut, 0, 5);
        cut->mutable_cut_out_fill();
        textfills::apply(&text, attributes, found);
        QCOMPARE(text.fillEffects.size(), 1);
        QCOMPARE(text.fillEffects[0].toMap().value("kind").toString(), QStringLiteral("cutOut"));
        QCOMPARE(pieces(text.paragraphs[1]), QStringList({"Next=-1"}));
    }

    // A blur's saturation, where it is written, is kept; a video fills nothing (as a shape's
    // video fill is not drawn); a gradient stretched over the box says so
    void otherSettings()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        Attributes::CustomAttribute *blur = attributes.add_custom_attributes();
        setRange(blur, 0, 5);
        blur->mutable_background_effect()->mutable_backgroundblur()->set_blur_amount(0.5);
        blur->mutable_background_effect()->mutable_backgroundblur()->set_saturation(0.25);
        Attributes::CustomAttribute *video = attributes.add_custom_attributes();
        setRange(video, 6, 11);
        video->mutable_media_fill()->mutable_media()->mutable_url()->set_absolute_string("file:///Users/someone/Media/Loop.mov");
        video->mutable_media_fill()->mutable_media()->mutable_video();
        Attributes::CustomAttribute *ramp = attributes.add_custom_attributes();
        setRange(ramp, 12, 16);
        setGradient(ramp->mutable_text_gradient_fill());
        ramp->mutable_text_gradient_fill()->set_stretch_to_document_bounds(true);
        textfills::apply(&text, attributes, found);
        QCOMPARE(text.fillEffects[0].toMap().value("saturation").toDouble(), 0.25);
        QCOMPARE(text.fillEffects[0].toMap().value("sigma").toDouble(), 13.0);
        QVERIFY(text.fillEffects[1].toMap().value("source").toUrl().isEmpty());
        QCOMPARE(text.fillEffects[2].toMap().value("stretch").toBool(), true);
        QCOMPARE(textfills::blurSigma(-1), 0.0);
    }

    // Words changed: the ranges go with the runs, each a copy of the file's own, with all
    // that is in it (here something this app knows nothing of), at its new place
    void rangesCarriedToNewWords()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        Attributes::CustomAttribute *caps = attributes.add_custom_attributes();
        setRange(caps, 0, 5);
        caps->set_capitalization(Attributes::CAPITALIZATION_ALL_CAPS);
        Attributes::CustomAttribute *gradient = attributes.add_custom_attributes();
        setRange(gradient, 6, 11);
        setGradient(gradient->mutable_text_gradient_fill());
        gradient->mutable_text_gradient_fill()->set_stretch_to_document_bounds(true);
        gradient->GetReflection()->MutableUnknownFields(gradient)->AddVarint(900, 7);
        Attributes::CustomAttribute *cut = attributes.add_custom_attributes();
        setRange(cut, 12, 16);
        cut->mutable_cut_out_fill();
        textfills::apply(&text, attributes, found);
        QCOMPARE(text.fillEffects[0].toMap().value("from").toInt(), 1);
        QCOMPARE(text.fillEffects[1].toMap().value("from").toInt(), 2);

        // "Hello world" becomes "Hi world!", the "!" typed at the end of the gradient's run
        text.paragraphs[0].runs[0].text = QStringLiteral("Hi ");
        text.paragraphs[0].runs[1].text = QStringLiteral("world!");
        const QList<Attributes::CustomAttribute> made = fillRanges(written(attributes, text));
        QCOMPARE(made.size(), 2);
        QVERIFY(made[0].has_text_gradient_fill());
        QCOMPARE(made[0].range().start(), 3);
        QCOMPARE(made[0].range().end(), 9);
        QCOMPARE(made[0].text_gradient_fill().gradient().stops_size(), 2);
        QCOMPARE(made[0].text_gradient_fill().gradient().angle(), 245.0);
        QCOMPARE(made[0].text_gradient_fill().stretch_to_document_bounds(), true);
        const google::protobuf::UnknownFieldSet &unknown = made[0].GetReflection()->GetUnknownFields(made[0]);
        QCOMPARE(unknown.field_count(), 1);
        QCOMPARE(unknown.field(0).number(), 900);
        QVERIFY(made[1].has_cut_out_fill());
        QCOMPARE(made[1].range().start(), 10);
        QCOMPARE(made[1].range().end(), 14);
        // (The file's own are as they were)
        QCOMPARE(attributes.custom_attributes(1).range().start(), 6);
    }

    // A fill over a paragraph break goes on over it; a fill of the whole text that the
    // file had as the text's own alone stays that, with no range
    void rangesOverABreak()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        Attributes::CustomAttribute *blur = attributes.add_custom_attributes();
        setRange(blur, 6, 16);
        blur->mutable_background_effect()->mutable_backgroundinvert();
        textfills::apply(&text, attributes, found);
        text.paragraphs[1].runs[0].text = QStringLiteral("Then");
        text.paragraphs[0].runs[0].text = QStringLiteral("Hey ");
        const QList<Attributes::CustomAttribute> made = fillRanges(written(attributes, text));
        QCOMPARE(made.size(), 1);
        QCOMPARE(made[0].range().start(), 4);
        QCOMPARE(made[0].range().end(), 14);

        RichText whole = twoParagraphs();
        Attributes all;
        all.mutable_cut_out_fill();
        textfills::apply(&whole, all, found);
        whole.paragraphs[0].runs[0].text = QStringLiteral("Other words");
        const Attributes after = written(all, whole);
        QVERIFY(fillRanges(after).isEmpty());
        QVERIFY(after.has_cut_out_fill());
    }

    // Texts as ProPresenter writes them, read and written again with nothing changed, are
    // as they were, byte for byte: a fill of the whole text, as the text's own and as a
    // range one past its end (its demo's), with something in the range this app knows
    // nothing of; two fills over two ranges and none of the text's own (its demo's); a
    // fill as the text's own alone; and a colour
    void unchangedIsAsItWas()
    {
        QList<Attributes> files;
        Attributes blurred;
        blurred.mutable_background_effect()->mutable_backgroundblur()->set_blur_amount(0.2647971854304636);
        Attributes::CustomAttribute *range = blurred.add_custom_attributes();
        setRange(range, 0, 17);
        range->mutable_background_effect()->mutable_backgroundblur()->set_blur_amount(0.2647971854304636);
        range->GetReflection()->MutableUnknownFields(range)->AddVarint(901, 3);
        files << blurred;
        Attributes two;
        Attributes::CustomAttribute *ramp = two.add_custom_attributes();
        setRange(ramp, 0, 11);
        setGradient(ramp->mutable_text_gradient_fill());
        ramp->mutable_text_gradient_fill()->mutable_gradient()->set_length(1);
        for (auto &stop : *ramp->mutable_text_gradient_fill()->mutable_gradient()->mutable_stops())
            stop.set_blend_point(0.5);
        Attributes::CustomAttribute *invert = two.add_custom_attributes();
        setRange(invert, 12, 16);
        invert->mutable_background_effect()->mutable_backgroundinvert();
        files << two;
        Attributes own;
        setPicture(own.mutable_media_fill(), "file:///Users/someone/Media/Gold.png");
        files << own;
        Attributes colour;
        colour.mutable_text_solid_fill()->set_red(1);
        colour.mutable_text_solid_fill()->set_green(1);
        colour.mutable_text_solid_fill()->set_blue(1);
        colour.mutable_text_solid_fill()->set_alpha(1);
        files << colour;
        for (const Attributes &file : std::as_const(files)) {
            RichText text = twoParagraphs();
            textfills::apply(&text, file, found);
            // (A colour has no fills to carry, and is written as it always was: see proconvert)
            if (text.fillEffects.isEmpty())
                continue;
            QCOMPARE(written(file, text).SerializeAsString(), file.SerializeAsString());
        }
    }

    // A fill set over all of the words, where there was a colour, is written as
    // ProPresenter writes it: as the text's own, and as a range over all of it
    void newFillOverAll()
    {
        Attributes file;
        file.mutable_text_solid_fill()->set_alpha(1);
        RichText text = twoParagraphs();
        textfills::apply(&text, file, found);
        const QVariantMap blur {{"kind", "blur"}, {"amount", 0.5}, {"saturation", 1.0}};
        const RichText changed = text.formatted(0, 1000, {{"fill", blur}});
        QCOMPARE(changed.formatAt(3, 3).value("fill").toMap(), blur);
        const Attributes after = written(file, changed);
        QVERIFY(after.has_background_effect() && after.background_effect().has_backgroundblur());
        QCOMPARE(after.background_effect().backgroundblur().blur_amount(), 0.5);
        const QList<Attributes::CustomAttribute> ranges = fillRanges(after);
        QCOMPARE(ranges.size(), 1);
        QCOMPARE(ranges[0].range().start(), 0);
        QCOMPARE(ranges[0].range().end(), 16);
        QCOMPARE(ranges[0].background_effect().backgroundblur().blur_amount(), 0.5);
        // (A saturation of 1 is the colours as they are, which the file leaves unsaid)
        QCOMPARE(ranges[0].background_effect().backgroundblur().saturation(), 0.0);
    }

    // A gradient set over part of the words: a range of it, made as ProPresenter makes one;
    // the text's own fill the colour of the words that have none
    void newFillOnPart()
    {
        Attributes file;
        file.mutable_text_solid_fill()->set_alpha(1);
        RichText text = twoParagraphs();
        textfills::apply(&text, file, found);
        const QVariantMap ramp {{"kind", "gradient"}, {"angle", 90.0}, {"radial", false}, {"stretch", false},
                                {"stops", QVariantList {QVariantMap {{"color", QColor(Qt::red)}, {"position", 0.0}},
                                                        QVariantMap {{"color", QColor(Qt::blue)}, {"position", 1.0}}}}};
        const RichText changed = text.formatted(6, 11, {{"fill", ramp}});
        QCOMPARE(pieces(changed.paragraphs[0]), QStringList({"Hello =-1", "world=0"}));
        QCOMPARE(changed.formatAt(0, 0).value("fill").toMap().value("kind").toString(), QStringLiteral("color"));
        const Attributes after = written(file, changed);
        const QList<Attributes::CustomAttribute> ranges = fillRanges(after);
        QCOMPARE(ranges.size(), 1);
        QCOMPARE(ranges[0].range().start(), 6);
        QCOMPARE(ranges[0].range().end(), 11);
        const rv::data::Graphics::Gradient &gradient = ranges[0].text_gradient_fill().gradient();
        QCOMPARE(gradient.angle(), 90.0);
        QCOMPARE(gradient.length(), 1.0);
        QCOMPARE(gradient.stops_size(), 2);
        QCOMPARE(gradient.stops(1).blend_point(), 0.5);
        QVERIFY(after.has_text_solid_fill());
        QCOMPARE(QColor::fromRgbF(after.text_solid_fill().red(), after.text_solid_fill().green(), after.text_solid_fill().blue()),
                 QColor(Qt::white));
        // The same fill given to more words is the same fill
        const RichText more = changed.formatted(12, 16, {{"fill", ramp}});
        QCOMPARE(more.fillEffects.size(), 1);
    }

    // Back to a colour: the fills go, and the text's own fill is the words' colour
    void backToColour()
    {
        RichText text = twoParagraphs();
        Attributes file;
        file.mutable_background_effect()->mutable_backgroundinvert();
        Attributes::CustomAttribute *range = file.add_custom_attributes();
        setRange(range, 0, 17);
        range->mutable_background_effect()->mutable_backgroundinvert();
        textfills::apply(&text, file, found);
        const Attributes after = written(file, text.formatted(0, 1000, {{"fill", QVariantMap {{"kind", "color"}}}}));
        QVERIFY(fillRanges(after).isEmpty());
        QVERIFY(after.has_text_solid_fill());
    }

    // A fill changed (a gradient's angle and first colour) is the file's own, changed:
    // what this app knows nothing of in it stays, and so do its stops' places
    void changedKeepsTheRest()
    {
        RichText text = twoParagraphs();
        Attributes file;
        Attributes::CustomAttribute *ramp = file.add_custom_attributes();
        setRange(ramp, 0, 17);
        setGradient(ramp->mutable_text_gradient_fill());
        ramp->GetReflection()->MutableUnknownFields(ramp)->AddVarint(902, 1);
        textfills::apply(&text, file, found);
        QVariantMap fill = text.formatAt(0, 0).value("fill").toMap();
        QCOMPARE(fill.value("from").toInt(), 0);
        fill.insert("angle", 30.0);
        QVariantList stops = fill.value("stops").toList();
        QVariantMap first = stops[0].toMap();
        first.insert("color", QColor(Qt::green));
        stops[0] = first;
        fill.insert("stops", stops);
        const Attributes after = written(file, text.formatted(0, 1000, {{"fill", fill}}));
        const QList<Attributes::CustomAttribute> ranges = fillRanges(after);
        QCOMPARE(ranges.size(), 1);
        QCOMPARE(ranges[0].range().end(), 17);
        QCOMPARE(ranges[0].text_gradient_fill().gradient().angle(), 30.0);
        QCOMPARE(ranges[0].text_gradient_fill().gradient().stops(0).color().green(), 1.0f);
        QCOMPARE(ranges[0].text_gradient_fill().gradient().stops(1).position(), 0.0);
        QCOMPARE(ranges[0].GetReflection()->GetUnknownFields(ranges[0]).field_count(), 1);
        // (Written as the text's own fill too, all of the words having it)
        QVERIFY(after.has_text_gradient_fill());
        QCOMPARE(after.text_gradient_fill().gradient().angle(), 30.0);
    }

    // A media fill given a new file: media made from it, the old one's settings kept
    void mediaGivenAFile()
    {
        RichText text = twoParagraphs();
        Attributes file;
        Attributes::CustomAttribute *picture = file.add_custom_attributes();
        setRange(picture, 0, 5);
        setPicture(picture->mutable_media_fill(), "file:///Users/someone/Media/Gold.png");
        textfills::apply(&text, file, found);
        QVariantMap fill = text.fillEffects[0].toMap();
        fill.insert("file", QStringLiteral("/pictures/Silver.png"));
        const textfills::MakeMedia make = [](const QString &path) {
            rv::data::Media made;
            made.mutable_url()->set_absolute_string(QUrl::fromLocalFile(path).toString().toStdString());
            made.mutable_image()->mutable_drawing()->mutable_natural_size()->set_width(640);
            return made;
        };
        Attributes after = file;
        after.clear_custom_attributes();
        textfills::write(file, &after, text.formatted(0, 5, {{"fill", fill}}), make);
        const QList<Attributes::CustomAttribute> ranges = fillRanges(after);
        QCOMPARE(ranges.size(), 1);
        QCOMPARE(QString::fromStdString(ranges[0].media_fill().media().url().absolute_string()),
                 QStringLiteral("file:///pictures/Silver.png"));
        // (Scale to Fill, as the old one was)
        QCOMPARE(ranges[0].media_fill().media().image().drawing().scale_behavior(), rv::data::Media::SCALE_BEHAVIOR_FILL);
    }

    // A blur's amount set for one is set for every blur on the slide, of texts and of
    // shapes; their saturations, and fills that are not blurs, are left alone
    void blurSharedOnASlide()
    {
        rv::data::Slide slide;
        rv::data::Graphics::Element *text = slide.add_elements()->mutable_element();
        Attributes *attributes = text->mutable_text()->mutable_attributes();
        Attributes::CustomAttribute *blur = attributes->add_custom_attributes();
        setRange(blur, 0, 5);
        blur->mutable_background_effect()->mutable_backgroundblur()->set_blur_amount(0.2);
        blur->mutable_background_effect()->mutable_backgroundblur()->set_saturation(0.4);
        attributes->mutable_background_effect()->mutable_backgroundblur()->set_blur_amount(0.2);
        rv::data::Graphics::Element *shape = slide.add_elements()->mutable_element();
        shape->mutable_fill()->mutable_backgroundeffect()->mutable_backgroundblur()->set_blur_amount(0.3);
        rv::data::Graphics::Element *other = slide.add_elements()->mutable_element();
        other->mutable_text()->mutable_attributes()->mutable_background_effect()->mutable_backgroundinvert();
        const std::string untouched = slide.elements(2).SerializeAsString();
        textfills::shareBlur(&slide, 0.7);
        QCOMPARE(slide.elements(0).element().text().attributes().custom_attributes(0).background_effect().backgroundblur().blur_amount(), 0.7);
        QCOMPARE(slide.elements(0).element().text().attributes().custom_attributes(0).background_effect().backgroundblur().saturation(), 0.4);
        QCOMPARE(slide.elements(0).element().text().attributes().background_effect().backgroundblur().blur_amount(), 0.7);
        QCOMPARE(slide.elements(1).element().fill().backgroundeffect().backgroundblur().blur_amount(), 0.7);
        QCOMPARE(slide.elements(2).SerializeAsString(), untouched);
    }

    // A fill no run is left with is not drawn
    void unusedFillsAreNotLayered()
    {
        RichText text = twoParagraphs();
        Attributes attributes;
        Attributes::CustomAttribute *blur = attributes.add_custom_attributes();
        setRange(blur, 40, 50);
        blur->mutable_background_effect()->mutable_backgroundinvert();
        textfills::apply(&text, attributes, found);
        QCOMPARE(text.fillEffects.size(), 1);
        QVERIFY(textfills::layered(text).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestTextFills)
#include "tst_textfills.moc"
