#include "textfills.h"

#include "mediasettings.h"

#include "graphicsData.pb.h"
#include "slide.pb.h"

#include <QSet>
#include <QUrl>

#include <algorithm>
#include <climits>

namespace textfills {

namespace {

using Attributes = rv::data::Graphics::Text::Attributes;
using MakeMedia = textfills::MakeMedia;

QColor toColor(const rv::data::Color &c)
{
    return QColor::fromRgbF(std::clamp(c.red(), 0.0f, 1.0f), std::clamp(c.green(), 0.0f, 1.0f),
                            std::clamp(c.blue(), 0.0f, 1.0f), std::clamp(c.alpha(), 0.0f, 1.0f));
}

// (As proconvert writes a colour)
void setColor(rv::data::Color *target, const QColor &color)
{
    target->set_red(color.redF());
    target->set_green(color.greenF());
    target->set_blue(color.blueF());
    target->set_alpha(color.alphaF());
}

// A gradient as the file has it: its angle as the file counts it (anticlockwise from
// pointing right, as a shape's gradient is drawn: SlideElement), and its stops, spread
// evenly where the file has them all at one place (KNOWN, ProToPPTX: as ProPresenter saves
// a gradient nobody moved the stops of, and draws it); and whether it is stretched over
// the text's whole box (`stretch_to_document_bounds`: A GUESS at what that is, the demo's
// gradient not having it, and running across its own letters)
QVariantMap gradientFill(const rv::data::Graphics::Text::GradientFill &fill)
{
    const rv::data::Graphics::Gradient &g = fill.gradient();
    QList<double> positions;
    for (const auto &stop : g.stops())
        positions << std::clamp(stop.position(), 0.0, 1.0);
    if (positions.size() > 1 && std::all_of(positions.begin(), positions.end(), [&](double p) { return p == positions.first(); })) {
        for (int k = 0; k < positions.size(); ++k)
            positions[k] = double(k) / (positions.size() - 1);
    }
    QVariantList stops;
    for (int k = 0; k < g.stops_size(); ++k)
        stops << QVariantMap {{"color", toColor(g.stops(k).color())}, {"position", positions.at(k)}};
    return {
        {"kind", QStringLiteral("gradient")},
        {"angle", g.angle()},
        {"radial", g.type() == rv::data::Graphics::Gradient::TYPE_RADIAL},
        {"stops", stops},
        {"stretch", fill.stretch_to_document_bounds()},
    };
}

QVariantMap mediaFill(const rv::data::Media &media, const std::function<QString(const rv::data::Media &)> &mediaFile)
{
    // (A picture: a video's fill is not drawn, as a shape's is not)
    const QString path = mediaFile && !media.has_video() ? mediaFile(media) : QString();
    const rv::data::Media::DrawingProperties &drawing = media.has_video() ? media.video().drawing() : media.image().drawing();
    QVariantMap made {
        {"kind", QStringLiteral("media")},
        {"source", path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path)},
        {"scale", std::min(2, int(drawing.scale_behavior())) % 3},
    };
    if (mediasettings::isSet(media, false))
        made.insert("settings", mediasettings::describe(media));
    return made;
}

QVariantMap backgroundEffect(const rv::data::Graphics::BackgroundEffect &effect)
{
    if (effect.has_backgroundblur()) {
        const double saturation = effect.backgroundblur().saturation();
        return {
            {"kind", QStringLiteral("blur")},
            {"amount", effect.backgroundblur().blur_amount()},
            {"sigma", blurSigma(effect.backgroundblur().blur_amount())},
            {"saturation", saturation > 0 ? saturation : 1.0},
        };
    }
    if (effect.has_backgroundinvert())
        return {{"kind", QStringLiteral("invert")}};
    return {};
}

struct Range
{
    int start;
    int end; // exclusive
    int fill;
};

// Gives each stretch of text the fill of the range covering it, cutting runs where a range
// begins or ends inside one (as capitalization's ranges are laid on: proconvert.cpp)
void applyRanges(RichText *text, const QList<Range> &ranges)
{
    const auto fillAt = [&ranges](int position) {
        for (const Range &range : ranges) {
            if (position >= range.start && position < range.end)
                return range.fill;
        }
        return -1;
    };
    int offset = 0;
    for (TextParagraph &paragraph : text->paragraphs) {
        QList<TextRun> pieces;
        for (const TextRun &run : std::as_const(paragraph.runs)) {
            const int runStart = offset;
            const int runEnd = offset + int(run.text.size());
            QList<int> cuts {runStart, runEnd};
            for (const Range &range : ranges) {
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
                piece.fillEffect = fillAt(from);
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
        // The paragraph's break
        offset += 1;
    }
}

// A fill as it would be read from what `holder` has (an attributes' own fill, or a range's):
// the kind and the settings, without the file a media fill's media is found at
template <typename Holder>
QVariantMap describedFill(const Holder &holder)
{
    if (holder.has_text_gradient_fill())
        return gradientFill(holder.text_gradient_fill());
    if (holder.has_media_fill())
        return mediaFill(holder.media_fill().media(), nullptr);
    if (holder.has_cut_out_fill())
        return {{"kind", QStringLiteral("cutOut")}};
    if (holder.has_background_effect())
        return backgroundEffect(holder.background_effect());
    return {};
}

// The fill `from` has put into `to` (between the attributes' own and a range's, which
// name their fills alike), the whole message, everything in it
template <typename From, typename To>
void copyFill(const From &from, To *to)
{
    if (from.has_text_gradient_fill())
        *to->mutable_text_gradient_fill() = from.text_gradient_fill();
    else if (from.has_media_fill())
        *to->mutable_media_fill() = from.media_fill();
    else if (from.has_cut_out_fill())
        *to->mutable_cut_out_fill() = from.cut_out_fill();
    else if (from.has_background_effect())
        *to->mutable_background_effect() = from.background_effect();
}

// Makes what `holder` has the fill `fill` describes, changing only what differs from it:
// a fill read from the file and not changed is left as it was, byte for byte, and one
// changed keeps everything in it that this app does not know of. A media fill's `file`
// is a new file for it (made into media by `makeMedia`, keeping the old one's settings,
// as a shape's media fill does), and its `mediaSettings` what the Media Inspector changed.
template <typename Holder>
void setFill(Holder *holder, const QVariantMap &fill, const MakeMedia &makeMedia)
{
    const QString kind = fill.value("kind").toString();
    if (kind == QLatin1String("gradient")) {
        const bool fresh = !holder->has_text_gradient_fill();
        auto *gradientFill = holder->mutable_text_gradient_fill();
        rv::data::Graphics::Gradient *gradient = gradientFill->mutable_gradient();
        const QVariantList stops = fill.value("stops").toList();
        if (fresh) {
            // (As ProPresenter writes a gradient: the demo's, and a shape's here)
            gradient->set_length(1);
        }
        if (gradient->angle() != fill.value("angle").toDouble())
            gradient->set_angle(fill.value("angle").toDouble());
        const auto type = fill.value("radial").toBool() ? rv::data::Graphics::Gradient::TYPE_RADIAL
                                                        : rv::data::Graphics::Gradient::TYPE_LINEAR;
        if (gradient->type() != type && (type == rv::data::Graphics::Gradient::TYPE_RADIAL
                                         || gradient->type() == rv::data::Graphics::Gradient::TYPE_RADIAL))
            gradient->set_type(type);
        if (gradient->stops_size() == stops.size()) {
            // The colours that changed; where the stops are, as the file has them
            for (int k = 0; k < stops.size(); ++k) {
                const QColor color = stops.at(k).toMap().value("color").value<QColor>();
                if (toColor(gradient->stops(k).color()) != color)
                    setColor(gradient->mutable_stops(k)->mutable_color(), color);
            }
        } else {
            gradient->clear_stops();
            for (const QVariant &stop : stops) {
                auto *made = gradient->add_stops();
                setColor(made->mutable_color(), stop.toMap().value("color").value<QColor>());
                made->set_position(stop.toMap().value("position").toDouble());
                made->set_blend_point(0.5);
            }
        }
        const bool stretch = fill.value("stretch").toBool();
        if (gradientFill->stretch_to_document_bounds() != stretch)
            gradientFill->set_stretch_to_document_bounds(stretch);
    } else if (kind == QLatin1String("media")) {
        const bool had = holder->has_media_fill();
        rv::data::Media *media = holder->mutable_media_fill()->mutable_media();
        if (fill.contains("file") && makeMedia) {
            rv::data::Media made = makeMedia(fill.value("file").toString());
            if (had)
                mediasettings::carryOver(*media, &made);
            *media = std::move(made);
        }
        if (fill.contains("mediaSettings"))
            mediasettings::apply(media, fill.value("mediaSettings").toMap());
        if (fill.contains("scale")) {
            rv::data::Media::DrawingProperties *drawing = media->has_video() ? media->mutable_video()->mutable_drawing()
                                                                             : media->mutable_image()->mutable_drawing();
            const int scale = std::clamp(fill.value("scale").toInt(), 0, 2);
            if (std::min(2, int(drawing->scale_behavior())) % 3 != scale)
                drawing->set_scale_behavior(rv::data::Media::ScaleBehavior(scale));
        }
    } else if (kind == QLatin1String("cutOut")) {
        if (!holder->has_cut_out_fill())
            holder->mutable_cut_out_fill();
    } else if (kind == QLatin1String("blur")) {
        if (!holder->has_background_effect() || !holder->background_effect().has_backgroundblur())
            holder->mutable_background_effect()->mutable_backgroundblur();
        auto *blur = holder->mutable_background_effect()->mutable_backgroundblur();
        const double amount = std::max(0.0, fill.value("amount").toDouble());
        if (blur->blur_amount() != amount)
            blur->set_blur_amount(amount);
        const double saturation = fill.value("saturation", 1.0).toDouble();
        if ((blur->saturation() > 0 ? blur->saturation() : 1.0) != saturation)
            blur->set_saturation(saturation);
    } else if (kind == QLatin1String("invert")) {
        if (!holder->has_background_effect() || !holder->background_effect().has_backgroundinvert())
            holder->mutable_background_effect()->mutable_backgroundinvert();
    }
}

// The settings of a fill that say what it looks like: what tells two fills apart
QVariantMap settingsOf(QVariantMap fill)
{
    for (const char *key : {"index", "from", "whole", "file", "mediaSettings", "source", "settings"})
        fill.remove(QLatin1String(key));
    return fill;
}

int indexOf(QVariantList *fills, const QVariantMap &fill)
{
    const qsizetype found = fills->indexOf(fill);
    if (found >= 0)
        return int(found);
    fills->append(fill);
    return int(fills->size() - 1);
}

} // namespace

void apply(RichText *text, const rv::data::Graphics_Text_Attributes &attributes,
           const std::function<QString(const rv::data::Media &)> &mediaFile)
{
    // The fills as they are told apart (`plain`), and as they are kept, each with the
    // number of the range it was first read from (`from`), for write()
    QVariantList plain;
    QVariantList fills;
    QList<Range> ranges;
    const auto described = [&mediaFile](const auto &holder) -> QVariantMap {
        if (holder.has_text_gradient_fill())
            return gradientFill(holder.text_gradient_fill());
        if (holder.has_media_fill())
            return mediaFill(holder.media_fill().media(), mediaFile);
        if (holder.has_cut_out_fill())
            return {{"kind", QStringLiteral("cutOut")}};
        if (holder.has_background_effect())
            return backgroundEffect(holder.background_effect());
        return {};
    };
    for (int i = 0; i < attributes.custom_attributes_size(); ++i) {
        const auto &custom = attributes.custom_attributes(i);
        const QVariantMap fill = described(custom);
        if (fill.isEmpty() || custom.range().end() <= custom.range().start())
            continue;
        const int number = indexOf(&plain, fill);
        if (number == fills.size()) {
            QVariantMap kept = fill;
            kept.insert("from", i);
            fills << kept;
        }
        ranges.append({int(custom.range().start()), int(custom.range().end()), number});
    }
    if (ranges.isEmpty()) {
        QVariantMap fill = described(attributes);
        fill.insert("whole", true);
        if (fill.size() > 1)
            ranges.append({0, INT_MAX, indexOf(&fills, fill)});
    }
    if (ranges.isEmpty())
        return;
    text->fillEffects = fills;
    applyRanges(text, ranges);
}

void write(const rv::data::Graphics_Text_Attributes &before, rv::data::Graphics_Text_Attributes *attributes,
           const RichText &text, const MakeMedia &makeMedia)
{
    const auto fillOf = [&text](const TextRun &run) {
        return run.fillEffect >= 0 && run.fillEffect < text.fillEffects.size()
                       && !text.fillEffects.at(run.fillEffect).toMap().value("kind").toString().isEmpty()
                   ? run.fillEffect : -1;
    };

    // Each stretch of runs with the same fill, its start and end and its fill; a paragraph
    // break between two runs with the same fill is in it, as ProPresenter's ranges count
    // it. And whether some of the words have no fill, and the colour of the first that
    // has none.
    struct Stretch
    {
        int start;
        int end;
        int fill;
    };
    QList<Stretch> stretches;
    QSet<int> used;
    bool someUnfilled = false;
    QColor unfilledColor = text.firstRun().fill;
    int offset = 0;
    int start = 0;
    int fill = -1;
    const auto close = [&](int end) {
        if (fill >= 0 && end > start)
            stretches.append({start, end, fill});
    };
    for (qsizetype p = 0; p < text.paragraphs.size(); ++p) {
        for (const TextRun &run : text.paragraphs.at(p).runs) {
            const int end = offset + int(run.text.size());
            if (end > offset) {
                const int own = fillOf(run);
                if (own < 0 && !someUnfilled) {
                    someUnfilled = true;
                    unfilledColor = run.fill;
                }
                if (own >= 0)
                    used.insert(own);
                if (own != fill) {
                    close(offset);
                    fill = own;
                    start = offset;
                }
            }
            offset = end;
        }
        if (p + 1 < text.paragraphs.size()) {
            const TextParagraph &next = text.paragraphs.at(p + 1);
            const bool goesOn = fill >= 0 && !next.runs.isEmpty() && fillOf(next.runs.first()) == fill;
            if (!goesOn) {
                close(offset);
                fill = -1;
            }
            ++offset;
        }
    }
    close(offset);
    const int length = offset;
    const bool allOne = !someUnfilled && used.size() == 1;

    // The ranges
    for (const Stretch &stretch : std::as_const(stretches)) {
        const QVariantMap map = text.fillEffects.at(stretch.fill).toMap();
        const int from = map.value("from", -1).toInt();
        const bool whole = map.value("whole").toBool();
        // A fill of the whole text that the file had as the text's own and no range, and
        // that still covers it all, stays that way
        if (whole && allOne)
            continue;
        Attributes::CustomAttribute range;
        if (from >= 0 && from < before.custom_attributes_size()) {
            range = before.custom_attributes(from);
        } else if (whole) {
            copyFill(before, &range);
        }
        setFill(&range, map, makeMedia);
        // Where the range the fill came from still covers the same words, its own numbers
        // stay (ProPresenter's range of a whole text runs one past its end)
        const bool same = range.has_range() && int(range.range().start()) == stretch.start
                          && std::min(int(range.range().end()), length) == stretch.end;
        if (!same) {
            range.mutable_range()->set_start(stretch.start);
            range.mutable_range()->set_end(stretch.end);
        }
        *attributes->add_custom_attributes() = range;
    }

    // The text's own fill. With no fill but colours, the colour of the words, as it has
    // always been written; with one fill over all of them, that fill as well as its range,
    // as ProPresenter writes it; with some words filled and some not, the colour of those
    // that are not (A GUESS: ProPresenter's own file was not seen for that); with words
    // filled with several fills and none without, as it was (ProPresenter's, for a text of
    // a gradient and an invert, had none).
    if (used.isEmpty() || someUnfilled) {
        if (!attributes->has_text_solid_fill() || toColor(attributes->text_solid_fill()) != unfilledColor)
            setColor(attributes->mutable_text_solid_fill(), unfilledColor);
    } else if (allOne) {
        const QVariantMap map = text.fillEffects.at(*used.begin()).toMap();
        if (!map.value("whole").toBool() && settingsOf(describedFill(*attributes)) != settingsOf(map)) {
            const int from = map.value("from", -1).toInt();
            if (from >= 0 && from < before.custom_attributes_size())
                copyFill(before.custom_attributes(from), attributes);
        }
        if (map.value("whole").toBool() || settingsOf(describedFill(*attributes)) != settingsOf(map) || map.contains("file")
            || map.contains("mediaSettings"))
            setFill(attributes, map, makeMedia);
    }
}

void shareBlur(rv::data::Slide *slide, double amount)
{
    const auto share = [amount](rv::data::Graphics::BackgroundEffect *effect) {
        if (effect->has_backgroundblur() && effect->backgroundblur().blur_amount() != amount)
            effect->mutable_backgroundblur()->set_blur_amount(amount);
    };
    for (rv::data::Slide::Element &element : *slide->mutable_elements()) {
        rv::data::Graphics::Element *graphics = element.mutable_element();
        if (graphics->fill().has_backgroundeffect() && graphics->fill().backgroundeffect().has_backgroundblur()
            && graphics->fill().backgroundeffect().backgroundblur().blur_amount() != amount)
            share(graphics->mutable_fill()->mutable_backgroundeffect());
        if (!graphics->has_text())
            continue;
        const Attributes &attributes = graphics->text().attributes();
        const auto blurred = [amount](const auto &holder) {
            return holder.has_background_effect() && holder.background_effect().has_backgroundblur()
                   && holder.background_effect().backgroundblur().blur_amount() != amount;
        };
        bool any = blurred(attributes);
        for (const auto &custom : attributes.custom_attributes())
            any = any || blurred(custom);
        if (!any)
            continue;
        Attributes *changing = graphics->mutable_text()->mutable_attributes();
        if (blurred(*changing))
            share(changing->mutable_background_effect());
        for (auto &custom : *changing->mutable_custom_attributes()) {
            if (blurred(custom))
                share(custom.mutable_background_effect());
        }
    }
}

double blurSigma(double amount)
{
    return 26 * std::max(0.0, amount);
}

QVariantList layered(const RichText &text)
{
    QVariantList list;
    for (qsizetype k = 0; k < text.fillEffects.size(); ++k) {
        QVariantMap fill = text.fillEffects.at(k).toMap();
        if (fill.value("kind") == QLatin1String("gradient"))
            continue;
        // (Only a fill some run has)
        bool used = false;
        for (const TextParagraph &paragraph : text.paragraphs) {
            for (const TextRun &run : paragraph.runs)
                used = used || run.fillEffect == k;
        }
        if (!used)
            continue;
        fill.insert("index", int(k));
        list << fill;
    }
    return list;
}

} // namespace textfills
