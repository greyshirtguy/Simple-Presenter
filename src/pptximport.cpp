#include "pptximport.h"

#include "fontnames.h"
#include "pptxfonts.h"
#include "pptxrtf.h"
#include "pptxtext.h"

#include "presentation.pb.h"

#include <QBuffer>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace pptx {

namespace {

using rv::data::Action;
using rv::data::Cue;
using rv::data::Graphics;

QColor toColor(const rv::data::Color &c)
{
    return QColor::fromRgbF(std::clamp(c.red(), 0.0f, 1.0f), std::clamp(c.green(), 0.0f, 1.0f),
                            std::clamp(c.blue(), 0.0f, 1.0f), std::clamp(c.alpha(), 0.0f, 1.0f));
}

double wrapped(double degrees)
{
    double d = std::fmod(degrees, 360);
    return d < 0 ? d + 360 : d;
}

// ---------------------------------------------------------------------------------------
// The media a presentation names.
//
// Where a media item says its file is: its own URL, or else its file's. KNOWN (ProToPPTX,
// 2026-10): a presentation from a database workspace has no URL on the media itself, only
// on the media's file.
const rv::data::URL &urlOf(const rv::data::Media &media)
{
    if (media.has_url() && (!media.url().absolute_string().empty() || !media.url().local().path().empty()))
        return media.url();
    if (media.has_image())
        return media.image().file().local_url();
    if (media.has_video())
        return media.video().file().local_url();
    if (media.has_audio())
        return media.audio().file().local_url();
    return media.url();
}

// The file name a media item goes by
QString fileNameOf(const rv::data::Media &media)
{
    const rv::data::URL &url = urlOf(media);
    const QString whole = QUrl(QString::fromStdString(url.absolute_string())).path();
    const QString name = QFileInfo(whole).fileName();
    return name.isEmpty() ? QFileInfo(QString::fromStdString(url.local().path())).fileName() : name;
}

// What kind of media an item is: what it says, or, if it says nothing, what its file's
// ending says. None for what is not a file to show (live video, a web page), or a file
// whose ending says nothing known. KNOWN (ProToPPTX, 2026-10): a media action in one of
// the maintainer's presentations has a video with no type properties at all;
// ProPresenter 22, copying it into a database workspace, made it a video. That
// ProPresenter plays such a file as its ending says is A GUESS from that.
std::optional<Media::Kind> kindOf(const rv::data::Media &media)
{
    if (media.has_image())
        return Media::Image;
    if (media.has_video())
        return Media::Video;
    if (media.has_audio())
        return Media::Audio;
    if (media.has_live_video() || media.has_web_content())
        return std::nullopt;
    const QString ending = QFileInfo(fileNameOf(media)).suffix().toLower();
    static const QStringList images = {"png", "jpg", "jpeg", "gif", "tif", "tiff", "bmp", "heic", "heif", "webp", "psd", "pdf"};
    static const QStringList videos = {"mp4", "mov", "m4v", "avi", "mkv", "webm", "mpg", "mpeg", "mts", "m2ts", "wmv", "flv", "3gp", "mxf"};
    static const QStringList sounds = {"mp3", "m4a", "aac", "wav", "aif", "aiff", "caf", "flac", "ogg", "wma"};
    if (images.contains(ending))
        return Media::Image;
    if (videos.contains(ending))
        return Media::Video;
    if (sounds.contains(ending))
        return Media::Audio;
    return std::nullopt;
}

const rv::data::Media::DrawingProperties *drawingOf(const rv::data::Media &media)
{
    if (media.has_image())
        return &media.image().drawing();
    if (media.has_video())
        return &media.video().drawing();
    return nullptr;
}

ScaleMode scaleMode(rv::data::Media::ScaleBehavior behaviour)
{
    switch (behaviour) {
    case rv::data::Media::SCALE_BEHAVIOR_FILL:
        return ScaleMode::Fill;
    case rv::data::Media::SCALE_BEHAVIOR_STRETCH:
        return ScaleMode::Stretch;
    default:
        return ScaleMode::Fit;
    }
}

ScaleMode scaleOf(const rv::data::Media &media)
{
    const rv::data::Media::DrawingProperties *drawing = drawingOf(media);
    return drawing ? scaleMode(drawing->scale_behavior()) : ScaleMode::Fit;
}

// A GUESS (ProToPPTX): crop insets are in pixels of the picture's own size.
QMarginsF cropOf(const rv::data::Media::DrawingProperties &d)
{
    if (!d.crop_enable() || d.natural_size().width() <= 0 || d.natural_size().height() <= 0)
        return {};
    const double w = d.natural_size().width();
    const double h = d.natural_size().height();
    return QMarginsF(d.crop_insets().left() / w, d.crop_insets().top() / h, d.crop_insets().right() / w, d.crop_insets().bottom() / h);
}

// ---------------------------------------------------------------------------------------
// PlayOrder: a presentation's slides in the order ProPresenter shows them.
//
// KNOWN (the protos, and every ProPresenter 7 file looked at): a presentation keeps its
// slides ("cues") in one list, in no particular order, and says the order in its groups:
// each group lists its cues by UUID, and the groups are listed in order. An arrangement,
// if one is chosen, is a list of groups (a group may come more than once, as a chorus
// does); without one, every group comes once, in the order listed. A cue that no group
// lists is not shown by ProPresenter, and is left out here too. (This app orders a
// presentation's slides the same way: prodocument.cpp.)

struct PlayedCue
{
    const Cue *cue = nullptr;
    QString groupName;
    // Counts the groups as they come in the order, from 0; the same group coming twice in
    // an arrangement counts twice
    int groupOccurrence = 0;
};

QList<PlayedCue> playOrder(const rv::data::Presentation &presentation, bool followArrangement, const QString &arrangement,
                           QString *followed)
{
    QHash<QString, const Cue *> cuesById;
    for (const Cue &cue : presentation.cues())
        cuesById.insert(QString::fromStdString(cue.uuid().string()).toUpper(), &cue);
    QHash<QString, const rv::data::Presentation::CueGroup *> groupsById;
    for (const auto &group : presentation.cue_groups())
        groupsById.insert(QString::fromStdString(group.group().uuid().string()).toUpper(), &group);

    QList<const rv::data::Presentation::CueGroup *> groups;
    for (const auto &group : presentation.cue_groups())
        groups << &group;
    if (followArrangement && !arrangement.isEmpty()) {
        for (const auto &candidate : presentation.arrangements()) {
            if (QString::fromStdString(candidate.name()) != arrangement)
                continue;
            QList<const rv::data::Presentation::CueGroup *> arranged;
            for (const rv::data::UUID &id : candidate.group_identifiers()) {
                if (const auto *group = groupsById.value(QString::fromStdString(id.string()).toUpper()))
                    arranged << group;
            }
            if (!arranged.isEmpty()) {
                groups = arranged;
                *followed = arrangement;
            }
            break;
        }
    }

    QList<PlayedCue> played;
    for (int occurrence = 0; occurrence < groups.size(); ++occurrence) {
        const auto *group = groups.at(occurrence);
        for (const rv::data::UUID &id : group->cue_identifiers()) {
            if (const Cue *cue = cuesById.value(QString::fromStdString(id.string()).toUpper()))
                played << PlayedCue {cue, QString::fromStdString(group->group().name()), occurrence};
        }
    }
    return played;
}

// ---------------------------------------------------------------------------------------
// MediaLayers: what media ProPresenter has on screen at each slide, when the slides are
// clicked through in order.
//
// This is the reason for the main option. In ProPresenter a slide's media cue puts a
// picture or video on the media layer, and it stays there while later slides with no
// media cue of their own are clicked: their words go up over the same background.
// PowerPoint has no such layer: each slide shows only what is on it. With "keep
// background media" on, each slide is given the background that would be on screen, as
// if the slides were clicked through in order. The rules, as ProToPPTX understood them:
//
// - KNOWN: a media action whose layer is Background (the default, and what the
//   maintainer's presentations nearly all use) replaces what is on the background layer
//   and stays until something else replaces or clears it.
// - KNOWN: a Clear action whose target is All or Background (Media) takes it off.
// - A GUESS: a media action on the Foreground layer is shown with its own slide only, and
//   leaves the background that was up before it to come back on the next slide. Its
//   picture is drawn over the background and under the slide's own words.
// - A GUESS: Fill and Input layers are taken as Background.
// - NOT KNOWN: what a macro does. Macros are kept in the workspace, not the presentation,
//   so one that clears the background, or puts one up, cannot be followed. They are
//   listed in the report so the person can check those slides.
// - Audio cues are not pictures and are not carried on; they are noted in the report.

struct MediaCue
{
    const rv::data::Media *media = nullptr;
    bool foreground = false;
    const Action::MediaType *action = nullptr;
};

struct Shown
{
    // The background media of the slide's own cue, if it has one
    std::optional<MediaCue> own;
    // What is on the background layer while the slide is up: its own, or one left there
    // by an earlier slide
    std::optional<MediaCue> live;
    // Foreground media of the slide's own cue
    std::optional<MediaCue> foreground;
    // Names of the cue's macros, and file names of its audio
    QStringList macros;
    QStringList audio;
};

class MediaLayers
{
public:
    Shown click(const Cue &cue)
    {
        Shown shown;
        for (const Action &action : cue.actions()) {
            if (!action.isenabled())
                continue;
            if (action.has_media()) {
                const rv::data::Media &media = action.media().element();
                const std::optional<Media::Kind> kind = kindOf(media);
                if (kind == Media::Audio) {
                    shown.audio << fileNameOf(media);
                    continue;
                }
                // Live video and web pages: nothing to put on a slide
                if (!kind)
                    continue;
                const MediaCue found {&media, action.media().layer_type() == Action::LAYER_TYPE_FOREGROUND, &action.media()};
                if (!found.foreground) {
                    m_background = found;
                    shown.own = found;
                } else {
                    shown.foreground = found;
                }
            } else if (action.has_clear()) {
                const auto target = action.clear().target_layer();
                if (target == Action::ClearType::CLEAR_TARGET_LAYER_ALL || target == Action::ClearType::CLEAR_TARGET_LAYER_BACKGROUND) {
                    m_background.reset();
                    shown.own.reset();
                    shown.foreground.reset();
                }
            } else if (action.has_macro()) {
                const QString name = QString::fromStdString(action.macro().identification().parameter_name());
                shown.macros << (name.isEmpty() ? QString::fromStdString(action.name()) : name);
            }
        }
        shown.live = m_background;
        return shown;
    }

private:
    std::optional<MediaCue> m_background;
};

// ---------------------------------------------------------------------------------------
// The picture behind an element: what the slide has drawn so far, at the slide's own size
// (no more than 1920 across), for the stand-ins for a background effect and a cut-out:
// the colour under everything, the background media (a video's still), and the shapes
// and pictures behind the element; not words.

class Backdrop
{
public:
    explicit Backdrop(const QSizeF &slideSize) : m_scale(std::min(1.0, 1920.0 / std::max(1.0, slideSize.width()))), m_size(slideSize) {}

    double scale() const { return m_scale; }

    QImage draw(const Slide &slide) const
    {
        QImage image(QSize(std::max(1, int(std::ceil(m_size.width() * m_scale))), std::max(1, int(std::ceil(m_size.height() * m_scale)))),
                     QImage::Format_ARGB32_Premultiplied);
        image.fill(slide.backgroundColor.value_or(QColor(Qt::black)));
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.scale(m_scale, m_scale);
        const QRectF whole(QPointF(0, 0), m_size);
        if (slide.background) {
            const Media &media = slide.background->media;
            drawPicture(&painter, media.kind == Media::Video ? media.stillPicture().value_or(Media()) : media, whole,
                        slide.background->scale, {}, 1);
        }
        for (const Item &item : slide.items) {
            if (const Shape *shape = std::get_if<Shape>(&item)) {
                drawShape(&painter, *shape);
            } else if (const Picture *picture = std::get_if<Picture>(&item)) {
                painter.save();
                rotateAbout(&painter, picture->frame, picture->rotation);
                drawPicture(&painter, picture->media, picture->frame, picture->scale, picture->crop, picture->opacity);
                painter.restore();
            } else if (const Video *video = std::get_if<Video>(&item)) {
                if (const std::optional<Media> still = video->media.stillPicture())
                    drawPicture(&painter, *still, video->frame, video->scale, {}, 1);
            }
        }
        return image;
    }

private:
    static void rotateAbout(QPainter *painter, const QRectF &frame, double degrees)
    {
        if (degrees == 0)
            return;
        painter->translate(frame.center());
        painter->rotate(degrees);
        painter->translate(-frame.center());
    }

    static QImage load(const Media &media, const QSize &wanted)
    {
        QBuffer buffer;
        QImageReader reader;
        if (!media.data.isEmpty()) {
            buffer.setData(media.data);
            buffer.open(QIODevice::ReadOnly);
            reader.setDevice(&buffer);
        } else if (!media.path.isEmpty()) {
            reader.setFileName(media.path);
        } else {
            return {};
        }
        reader.setAutoTransform(true);
        // (No larger than it is drawn, give or take)
        const QSize size = reader.size();
        if (size.isValid() && wanted.isValid() && size.width() > 2 * wanted.width())
            reader.setScaledSize(size.scaled(wanted * 2, Qt::KeepAspectRatio));
        return reader.read();
    }

    void drawPicture(QPainter *painter, const Media &media, const QRectF &frame, ScaleMode mode, const QMarginsF &crop, double opacity) const
    {
        const QImage image = load(media, (frame.size() * m_scale).toSize());
        if (image.isNull())
            return;
        const QRectF source(image.width() * crop.left(), image.height() * crop.top(),
                            image.width() * (1 - crop.left() - crop.right()), image.height() * (1 - crop.top() - crop.bottom()));
        painter->save();
        painter->setOpacity(opacity);
        painter->setClipRect(frame);
        painter->drawImage(placing(frame, source.size(), mode), image, source);
        painter->restore();
    }

    void drawShape(QPainter *painter, const Shape &shape) const
    {
        QPainterPath path;
        const QRectF &f = shape.frame;
        switch (shape.geometry) {
        case Shape::Ellipse:
            path.addEllipse(f);
            break;
        case Shape::RoundedRectangle: {
            const double radius = shape.cornerFraction * std::min(f.width(), f.height());
            path.addRoundedRect(f, radius, radius);
            break;
        }
        case Shape::Polygon:
            for (int k = 0; k < shape.points.size(); ++k) {
                const QPointF p(f.x() + shape.points.at(k).x() * f.width(), f.y() + shape.points.at(k).y() * f.height());
                k == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            path.closeSubpath();
            break;
        case Shape::Rectangle:
            path.addRect(f);
            break;
        }
        painter->save();
        painter->setOpacity(shape.opacity);
        rotateAbout(painter, f, shape.rotation);
        if (shape.fill) {
            if (const QColor *c = std::get_if<QColor>(&*shape.fill)) {
                painter->fillPath(path, *c);
            } else if (const Gradient *g = std::get_if<Gradient>(&*shape.fill)) {
                // Along its angle through the middle of the box, as PowerPoint draws it
                const double radians = qDegreesToRadians(g->angle);
                const QPointF half(std::cos(radians) * f.width() / 2, std::sin(radians) * f.height() / 2);
                QLinearGradient linear(f.center() - half, f.center() + half);
                for (const Gradient::Stop &stop : g->stops)
                    linear.setColorAt(std::clamp(stop.position, 0.0, 1.0), stop.color);
                painter->fillPath(path, linear);
            } else if (const PictureFill *picture = std::get_if<PictureFill>(&*shape.fill)) {
                painter->setClipPath(path);
                drawPicture(painter, picture->media, f, ScaleMode::Stretch, {}, 1);
            }
        }
        if (shape.line)
            painter->strokePath(path, QPen(shape.line->color, shape.line->width));
        painter->restore();
    }

    const double m_scale;
    const QSizeF m_size;
};

// A picture made here as a medium of the deck's, by a name that says what it was made of
Media madePicture(const QImage &image, const QString &id, const QString &fileName)
{
    Media picture;
    picture.kind = Media::Image;
    QBuffer buffer(&picture.data);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    picture.dataId = id;
    picture.fileName = fileName;
    picture.naturalSize = image.size();
    return picture;
}

// KNOWN (measured: see pptximport.h): the standard deviation of ProPresenter's blur, in
// slide units, for each whole of its blur amount
constexpr double blurPerAmount = 26;

// What is behind an element, cut from a picture of the slide behind it to the element's
// box: blurred or inverted as a background effect says, or as it is (a cut-out). As a
// picture of its own, to fill the letters or the shape (see pptximport.h).
std::optional<Media> behindPicture(const Graphics::BackgroundEffect *effect, const QImage &behind, const Backdrop &backdrop,
                                   const QRectF &frame, const QString &id)
{
    if (effect && !effect->has_backgroundblur() && !effect->has_backgroundinvert())
        return std::nullopt;
    const QRect box = QRectF(frame.x() * backdrop.scale(), frame.y() * backdrop.scale(), frame.width() * backdrop.scale(),
                             frame.height() * backdrop.scale()).toAlignedRect().intersected(behind.rect());
    if (box.isEmpty())
        return std::nullopt;
    if (!effect)
        return madePicture(behind.copy(box), id, QStringLiteral("Behind the words.png"));
    double sigma = 0;
    double saturation = 1;
    if (effect->has_backgroundblur()) {
        sigma = effect->backgroundblur().blur_amount() * blurPerAmount * backdrop.scale();
        if (effect->backgroundblur().saturation() > 0)
            saturation = effect->backgroundblur().saturation();
    }
    // Blurred from a margin round the box, so that its edges are blurred with what is
    // beside them, as a blur behind a pane would be
    const int margin = int(std::ceil(sigma * 3));
    const QRect wider = box.adjusted(-margin, -margin, margin, margin).intersected(behind.rect());
    QImage made = backgroundEffect(behind.copy(wider), sigma, saturation, effect->has_backgroundinvert())
                      .copy(box.translated(-wider.topLeft()));
    // A blurred picture at half its size, which its blur hides, and the file is the smaller
    if (sigma >= 2)
        made = made.scaled(made.size() / 2, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return madePicture(made, id, effect->has_backgroundinvert() ? QStringLiteral("Inverted.png") : QStringLiteral("Blurred.png"));
}

// ---------------------------------------------------------------------------------------
// ElementMapper: one thing on a ProPresenter slide (a text box, a shape, a picture) as the
// deck's item.
//
// What ProPresenter keeps of an element and where it goes (KNOWN from the protos; the
// conventions marked A GUESS were judged by ProToPPTX against the slide pictures
// ProPresenter exported of the maintainer's presentations):
//
// - bounds, in slide units from the top left: the item's frame. rotation, in degrees: A
//   GUESS that it is clockwise, as everything on a screen with y down.
// - opacity (0...1), flip, hidden (a hidden element is left out). KNOWN: elements
//   ProPresenter made from a PowerPoint slide say 100 for fully opaque; its own say 1.
// - path: the shape. Rectangles, ellipses and rounded rectangles become the same in
//   PowerPoint; any other shape becomes a polygon of its points (curves are not followed:
//   A GUESS that nobody's slides depend on them).
// - fill, stroke, shadow and feather each have an `enable`; one that is not enabled is
//   kept in the file but not drawn, and is left out here. (KNOWN: the maintainer's files
//   have blue fills and white strokes, not enabled, under every text box.)
// - A fill that is a picture or a video: a picture or video item when the element has no
//   words, a shape filled with the picture when it has.
// - text: the words, from the RTF (pptxrtf.h), with what ProPresenter keeps beside the
//   RTF laid on top: capitalization and gradient fills over ranges of the text, an
//   outline (stroke_width, as macOS's text system counts it: percent of the font size,
//   negative for filled and outlined), the text's shadow, margins, vertical alignment,
//   and how the text is fitted (scale_behavior).
//
// Shadows: ProPresenter's angle is A GUESS to be counted as on paper (anticlockwise from
// the right, so 315 is down and to the right); PowerPoint counts clockwise, so the
// direction is 360 less the angle. Opacity is the colour's alpha.

class ElementMapper
{
public:
    ElementMapper(const ImportContext &context, NoteCollector *notes, const QHash<QString, QString> &familiesFromFile, const Backdrop &backdrop)
        : m_context(context), m_notes(notes), m_familiesFromFile(familiesFromFile), m_backdrop(backdrop)
    {
    }

    // The items one element becomes (none, if nothing of it would be seen). `soFar` is
    // the slide as far as it has been made, behind this element.
    QList<Item> items(const rv::data::Slide::Element &slideElement, const Slide &soFar, int slide)
    {
        const Graphics::Element &g = slideElement.element();
        if (g.hidden())
            return {};
        const QRectF frame(g.bounds().origin().x(), g.bounds().origin().y(), g.bounds().size().width(), g.bounds().size().height());
        const QString name = g.name().empty() ? QStringLiteral("Element") : QString::fromStdString(g.name());
        const bool flipH = g.flipmode() == Graphics::Element::FLIP_MODE_HORIZONTAL || g.flipmode() == Graphics::Element::FLIP_MODE_BOTH;
        const bool flipV = g.flipmode() == Graphics::Element::FLIP_MODE_VERTICAL || g.flipmode() == Graphics::Element::FLIP_MODE_BOTH;
        const double opacity = std::clamp(g.opacity(), 0.0, 1.0);
        const QString id = QString::fromStdString(g.uuid().string()) + u':' + QString::number(slide);

        // (What is behind it, drawn only if a background effect asks for it)
        std::optional<QImage> behind;
        const auto behindIt = [&]() -> const QImage & {
            if (!behind)
                behind = m_backdrop.draw(soFar);
            return *behind;
        };

        const std::optional<TextBody> text = g.has_text() ? textBody(g.text(), frame, slide, behindIt, id) : std::nullopt;
        const bool hasWords = text && !text->plainText().trimmed().isEmpty();

        // A picture or a video with no words on it
        if (g.fill().enable() && g.fill().has_media() && !hasWords) {
            const rv::data::Media &media = g.fill().media();
            const Media found = deckMedia(media, slide);
            if (found.kind == Media::Image) {
                Picture picture;
                picture.name = name;
                picture.frame = frame;
                picture.rotation = g.rotation();
                picture.media = found;
                if (const rv::data::Media::DrawingProperties *drawing = drawingOf(media)) {
                    picture.scale = scaleMode(drawing->scale_behavior());
                    picture.crop = cropOf(*drawing);
                    picture.flipHorizontal = flipH != drawing->flipped_horizontally();
                    picture.flipVertical = flipV != drawing->flipped_vertically();
                }
                picture.opacity = opacity;
                return {picture};
            }
            if (found.kind == Media::Video) {
                Video video;
                video.name = name;
                video.frame = frame;
                video.media = found;
                video.scale = scaleOf(media);
                video.loops = media.video().transport().playback_behavior() == rv::data::Media::TransportProperties::PLAYBACK_BEHAVIOR_LOOP;
                video.muted = media.video().has_audio() && media.video().audio().volume() == 0;
                return {video};
            }
            return {};
        }

        std::optional<Fill> fill;
        if (g.fill().enable() && g.fill().has_backgroundeffect()) {
            if (const std::optional<Media> picture = behindPicture(&g.fill().backgroundeffect(), behindIt(), m_backdrop, frame, id))
                fill = PictureFill {*picture, ScaleMode::Stretch};
            m_notes->add(Note::Simplified, QStringLiteral("A blur or invert behind an element is put in as a picture of what is behind it, blurred or inverted"), slide);
        } else {
            fill = this->fill(g.fill(), slide);
        }
        std::optional<Line> line;
        if (g.stroke().enable() && g.stroke().width() > 0)
            line = Line {g.stroke().width(), toColor(g.stroke().color()), dashes(g.stroke())};
        if (!hasWords && !fill && !line)
            return {};

        Shape shape;
        shape.name = name;
        shape.frame = frame;
        shape.rotation = g.rotation();
        shape.flipHorizontal = flipH;
        shape.flipVertical = flipV;
        geometry(g.path(), &shape);
        shape.fill = fill;
        shape.line = line;
        if (g.shadow().enable())
            shape.shadow = shadow(g.shadow());
        if (g.feather().enable())
            shape.softEdge = g.feather().radius() * std::min(frame.width(), frame.height());
        shape.opacity = opacity;
        if (hasWords)
            shape.text = text;
        return {shape};
    }

    // The deck's medium for ProPresenter's, found on this computer or noted as missing
    Media deckMedia(const rv::data::Media &m, int slide)
    {
        Media media;
        media.kind = kindOf(m).value_or(Media::Image);
        if (const rv::data::Media::DrawingProperties *drawing = drawingOf(m)) {
            if (drawing->natural_size().width() > 0 && drawing->natural_size().height() > 0)
                media.naturalSize = QSizeF(drawing->natural_size().width(), drawing->natural_size().height());
        }
        media.fileName = fileNameOf(m);
        media.path = m_context.findMedia ? m_context.findMedia(urlOf(m)) : QString();
        if (media.path.isEmpty())
            m_notes->add(Note::MissingMedia, QStringLiteral("The media file %1 could not be found").arg(media.fileName), slide);
        return media;
    }

private:
    // ----- Text

    std::optional<TextBody> textBody(const Graphics::Text &t, const QRectF &frame, int slide,
                                     const std::function<const QImage &()> &behindIt, const QString &id)
    {
        if (t.rtf_data().empty())
            return std::nullopt;
        RtfDocument document = readRtf(QByteArray::fromStdString(t.rtf_data()));
        const Graphics::Text::Attributes &a = t.attributes();

        // Over the whole text: capitalization, an outline, the text's shadow, and a
        // gradient fill or a background effect given with no range
        const int wholeCaps = a.capitalization();
        const double outlineWidth = std::abs(a.stroke_width());
        const QColor outlineColor = toColor(a.stroke_color());
        const std::optional<Shadow> textShadow = t.shadow().enable() ? std::optional<Shadow>(shadow(t.shadow())) : std::nullopt;
        bool rangedGradient = false;
        bool rangedPicture = false;
        bool chords = false;
        for (const auto &custom : a.custom_attributes()) {
            rangedGradient = rangedGradient || custom.has_text_gradient_fill();
            rangedPicture = rangedPicture || custom.has_background_effect() || custom.has_cut_out_fill() || custom.has_media_fill();
            chords = chords || custom.has_chord();
        }
        std::optional<Gradient> wholeGradient;
        if (a.has_text_gradient_fill() && !rangedGradient)
            wholeGradient = gradient(a.text_gradient_fill().gradient());
        // (KNOWN, the demo: ProPresenter writes such a fill over the whole text and over a
        // range of all of it alike; the ranges, where there are any, are what is read.)
        std::optional<Media> wholePicture;
        if (!rangedPicture) {
            if (a.has_background_effect())
                wholePicture = effectPicture(&a.background_effect(), behindIt, frame, id + QStringLiteral(":text"), slide);
            else if (a.has_cut_out_fill())
                wholePicture = effectPicture(nullptr, behindIt, frame, id + QStringLiteral(":text"), slide);
            else if (a.has_media_fill())
                wholePicture = mediaFillPicture(a.media_fill().media(), frame, id + QStringLiteral(":text"), slide);
        }

        const auto modify = [&](RunStyle &style) {
            applyCapitalization(style, wholeCaps);
            if (outlineWidth > 0)
                style.outline = Line {outlineWidth / 100 * style.size, outlineColor, {}};
            if (textShadow)
                style.shadow = textShadow;
            if (wholeGradient)
                style.fill = *wholeGradient;
            if (wholePicture)
                style.fill = *wholePicture;
            resolveFont(style, slide);
        };
        for (Paragraph &paragraph : document.paragraphs) {
            for (Run &run : paragraph.runs)
                modify(run.style);
            modify(paragraph.endStyle);
        }

        // Over ranges of it
        for (const auto &custom : a.custom_attributes()) {
            const int start = int(custom.range().start());
            const int end = int(custom.range().end());
            if (end <= start)
                continue;
            if (custom.has_capitalization()) {
                const int caps = custom.capitalization();
                apply(&document, start, end, [caps](RunStyle &style) { applyCapitalization(style, caps); });
            } else if (custom.has_text_gradient_fill()) {
                const Gradient g = gradient(custom.text_gradient_fill().gradient());
                apply(&document, start, end, [&g](RunStyle &style) { style.fill = g; });
            } else if (custom.has_background_effect() || custom.has_cut_out_fill() || custom.has_media_fill()) {
                const QString rangeId = id + QStringLiteral(":%1-%2").arg(start).arg(end);
                const std::optional<Media> picture
                    = custom.has_media_fill()     ? mediaFillPicture(custom.media_fill().media(), frame, rangeId, slide)
                    : custom.has_cut_out_fill() ? effectPicture(nullptr, behindIt, frame, rangeId, slide)
                                                  : effectPicture(&custom.background_effect(), behindIt, frame, rangeId, slide);
                if (picture)
                    apply(&document, start, end, [&picture](RunStyle &style) { style.fill = *picture; });
            }
        }
        // Title and start case are ProPresenter's way of showing the words, which
        // PowerPoint has no setting for: put into the words themselves, each word's
        // first letter made capital, as this app draws them (textlayout.cpp)
        for (Paragraph &paragraph : document.paragraphs) {
            for (Run &run : paragraph.runs) {
                if (run.style.capitalization == RunStyle::TitleCase) {
                    run.text = capitalizedWords(run.text);
                    run.style.capitalization = RunStyle::NoCapitalization;
                }
            }
            if (paragraph.endStyle.capitalization == RunStyle::TitleCase)
                paragraph.endStyle.capitalization = RunStyle::NoCapitalization;
        }

        // Text transforms (one line, one word per line, …) change only text with more than
        // one line in it; NOT KNOWN exactly how, so such text is shown as written, and said.
        if (t.transform() != Graphics::Text::TRANSFORM_NONE && document.paragraphs.size() > 1)
            m_notes->add(Note::Simplified, QStringLiteral("Text set in ProPresenter to be shown on one line, or a word at a time, is shown as written"), slide);
        if (chords)
            m_notes->add(Note::Simplified, QStringLiteral("Chords are left out: only the words go over"), slide);

        // KNOWN (ProToPPTX, measured against ProPresenter's exported slides, 2026-10-18):
        // the words start 5 units in from each edge of the box beyond its margins: macOS's
        // text container keeps 5 points clear at either end of every line, and
        // ProPresenter keeps the same at the top.
        const double pad = 5;
        TextBody body;
        body.paragraphs = document.paragraphs;
        body.insets = QMarginsF(t.margins().left() + pad, t.margins().top() + pad, t.margins().right() + pad, t.margins().bottom() + pad);
        body.verticalAlignment = t.vertical_alignment() == Graphics::Text::VERTICAL_ALIGNMENT_MIDDLE   ? TextBody::Middle
                               : t.vertical_alignment() == Graphics::Text::VERTICAL_ALIGNMENT_BOTTOM ? TextBody::Bottom
                                                                                                      : TextBody::Top;
        switch (t.scale_behavior()) {
        case Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_DOWN:
        case Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_UP_DOWN:
            body.autofit = TextBody::Shrink;
            body.fontScale = shrinkFactor(body, frame);
            break;
        case Graphics::Text::SCALE_BEHAVIOR_ADJUST_CONTAINER_HEIGHT:
            body.autofit = TextBody::ResizeShape;
            break;
        case Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_UP:
            m_notes->add(Note::Simplified, QStringLiteral("Text set to grow to fill its box is shown at its own size"), slide);
            break;
        default:
            break;
        }
        return body;
    }

    // The letters' fill for a background effect, or for a cut-out (no effect): a picture
    // of what is behind them (see pptximport.h)
    std::optional<Media> effectPicture(const Graphics::BackgroundEffect *effect, const std::function<const QImage &()> &behindIt,
                                       const QRectF &frame, const QString &id, int slide)
    {
        m_notes->add(Note::Simplified, effect ? QStringLiteral("Words filled with a blur or invert of what is behind them are filled with a picture of it, blurred or inverted")
                                              : QStringLiteral("Words cut out of their box are filled with a picture of what is behind the box"),
                     slide);
        return behindPicture(effect, behindIt(), m_backdrop, frame, id);
    }

    // The letters' fill for a media fill: the picture fitted to the box as it says, at no
    // more than the size of a 1080p slide (KNOWN, the demo: over the whole box, each
    // letter showing its part). A video's still, for a video.
    std::optional<Media> mediaFillPicture(const rv::data::Media &media, const QRectF &frame, const QString &id, int slide)
    {
        Media found = deckMedia(media, slide);
        if (found.isMissing())
            return std::nullopt;
        QImage image;
        if (found.kind == Media::Video) {
            if (m_context.videoStill)
                image = m_context.videoStill(found.path, nullptr);
        } else {
            QImageReader reader(found.path);
            reader.setAutoTransform(true);
            image = reader.read();
        }
        if (image.isNull() || frame.width() <= 0 || frame.height() <= 0)
            return std::nullopt;
        const double scale = std::min(1.0, 1920.0 / std::max(frame.width(), frame.height()));
        QImage made(QSize(std::max(1, int(std::round(frame.width() * scale))), std::max(1, int(std::round(frame.height() * scale)))),
                    QImage::Format_ARGB32_Premultiplied);
        made.fill(Qt::transparent);
        QPainter painter(&made);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QRectF box(QPointF(0, 0), made.size());
        const QMarginsF crop = drawingOf(media) ? cropOf(*drawingOf(media)) : QMarginsF();
        const QRectF source(image.width() * crop.left(), image.height() * crop.top(),
                            image.width() * (1 - crop.left() - crop.right()), image.height() * (1 - crop.top() - crop.bottom()));
        painter.drawImage(placing(box, source.size(), scaleOf(media)), image, source);
        painter.end();
        return madePicture(made, id + u':' + found.key(), found.fileName);
    }

    // ProPresenter's capitalization as PowerPoint's, the two it has not got (title and
    // start case) put into the words (see textBody)
    static void applyCapitalization(RunStyle &style, int caps)
    {
        switch (caps) {
        case Graphics::Text::Attributes::CAPITALIZATION_ALL_CAPS:
            style.capitalization = RunStyle::AllCaps;
            break;
        case Graphics::Text::Attributes::CAPITALIZATION_SMALL_CAPS:
            style.capitalization = RunStyle::SmallCaps;
            break;
        case Graphics::Text::Attributes::CAPITALIZATION_TITLE_CASE:
        case Graphics::Text::Attributes::CAPITALIZATION_START_CASE:
            style.capitalization = RunStyle::TitleCase;
            break;
        default:
            break;
        }
    }

    static QString capitalizedWords(const QString &text)
    {
        QString out = text;
        bool start = true;
        for (QChar &c : out) {
            if (c.isLetter()) {
                if (start)
                    c = c.toUpper();
                start = false;
            } else if (c.isSpace()) {
                start = true;
            }
        }
        return out;
    }

    // Fills in the family and the bold and italic switches PowerPoint will name the font
    // by (pptxfonts.h says why they are not simply the family this computer shows).
    void resolveFont(RunStyle &style, int slide)
    {
        // KNOWN (ProToPPTX): macOS reads RTF's \b and \i as "the bold (italic) face of
        // this font's family", and so does ProPresenter: "CMGSans-Regular" with \b is
        // drawn in CMG Sans Bold. So the face is changed first, and named after.
        if (const std::optional<FontFace> face = installedFace(style.fontName, style.bold, style.italic)) {
            style.fontName = face->postScriptName;
            style.fontFamily = face->officeTypeface();
            style.bold = face->officeBold();
            style.italic = face->officeItalic();
            return;
        }
        // Not installed here: the family the presentation gives, else a guess from the
        // name ("Montserrat-ExtraBold" is of the family "Montserrat"). A face of one of
        // Office's four (Regular, Bold, Italic, Bold Italic) goes by its family and RTF's
        // own bold and italic, as ProToPPTX has it; any other by its full name as Office
        // knows such a face (ProToPPTX's notes: "Helvetica Neue Condensed Black", found by
        // its full name), made of the family and the name's style part spelt out
        // ("CondensedBlack", "Condensed Black"): A GUESS at the full name, which only the
        // font itself can say, and which matters on a computer without the Mac's fonts,
        // where a family alone would be drawn in its Regular or Bold. With RTF's \b over a
        // face that is not bold, the family's bold, as macOS draws it; over one that is
        // heavy already (its name says Bold, Black, Heavy or Fat: KNOWN, ProPresenter
        // writes \b with "HelveticaNeue-CondensedBlack"), nothing more.
        const QString family = m_familiesFromFile.value(style.fontName, style.fontName.section(u'-', 0, 0));
        const QString face = fontnames::postScriptStyle(style.fontName);
        static const QStringList office = {QString(), QStringLiteral("regular"), QStringLiteral("roman"), QStringLiteral("normal"),
                                           QStringLiteral("plain"), QStringLiteral("book"), QStringLiteral("bold"), QStringLiteral("italic"),
                                           QStringLiteral("oblique"), QStringLiteral("bolditalic"), QStringLiteral("boldoblique")};
        const QString key = face.toLower();
        const bool heavy = key.contains(QLatin1String("bold")) || key.contains(QLatin1String("black"))
                           || key.contains(QLatin1String("heavy")) || key.contains(QLatin1String("fat"));
        if (office.contains(key) || (style.bold && !heavy)) {
            style.fontFamily = family;
            style.bold = style.bold || key.startsWith(QLatin1String("bold"));
            style.italic = style.italic || key.endsWith(QLatin1String("italic")) || key.endsWith(QLatin1String("oblique"));
        } else {
            QString spelt;
            for (int k = 0; k < face.size(); ++k) {
                if (k > 0 && face.at(k).isUpper() && !face.at(k - 1).isUpper())
                    spelt += u' ';
                spelt += face.at(k);
            }
            style.fontFamily = family + u' ' + spelt;
            style.bold = false;
            style.italic = false;
        }
        m_notes->add(Note::MissingFont, QStringLiteral("The font %1 is not installed on this computer: it is named in the file all the same, and PowerPoint will show it where it is installed").arg(style.fontName), slide);
    }

    // Lays `change` on the runs between two places in ProPresenter's count of the text,
    // splitting runs where a range starts or ends inside one.
    static void apply(RtfDocument *document, int start, int end, const std::function<void(RunStyle &)> &change)
    {
        const QList<int> starts = document->paragraphStarts();
        for (int p = 0; p < document->paragraphs.size(); ++p) {
            int at = starts.at(p);
            QList<Run> runs;
            for (const Run &run : std::as_const(document->paragraphs[p].runs)) {
                const int units = int(run.text.size());
                const int runStart = at;
                const int runEnd = at + units;
                at = runEnd;
                const int from = std::max(start, runStart);
                const int to = std::min(end, runEnd);
                if (from >= to) {
                    runs << run;
                    continue;
                }
                const int cuts[4] = {0, from - runStart, to - runStart, units};
                for (int k = 0; k < 3; ++k) {
                    if (cuts[k + 1] <= cuts[k])
                        continue;
                    Run piece {run.text.mid(cuts[k], cuts[k + 1] - cuts[k]), run.style};
                    if (k == 1)
                        change(piece.style);
                    runs << piece;
                }
            }
            document->paragraphs[p].runs = runs;
        }
    }

    // ----- Fills, lines, shadows

    std::optional<Fill> fill(const Graphics::Fill &f, int slide)
    {
        if (!f.enable())
            return std::nullopt;
        if (f.has_color()) {
            const QColor colour = toColor(f.color());
            return colour.alpha() > 0 ? std::optional<Fill>(colour) : std::nullopt;
        }
        if (f.has_gradient())
            return gradient(f.gradient());
        if (f.has_media()) {
            const Media found = deckMedia(f.media(), slide);
            if (found.kind == Media::Image)
                return PictureFill {found, scaleOf(f.media())};
        }
        return std::nullopt;
    }

public:
    static Shadow shadow(const Graphics::Shadow &s)
    {
        QColor colour = toColor(s.color());
        colour.setAlphaF(float(colour.alphaF() * std::clamp(s.opacity(), 0.0, 1.0)));
        return Shadow {colour, wrapped(360 - s.angle()), s.offset(), s.radius()};
    }

    // ProPresenter's gradient. KNOWN (ProToPPTX, from the title of one of the
    // maintainer's slides, against ProPresenter's picture of it): the angle is counted as
    // for shadows, anticlockwise from the right, with the first stop at the tail; and
    // stops whose positions are all the same (all nought, as ProPresenter saves a
    // gradient nobody moved the stops of) are spread evenly. A stop's blend point (where
    // between it and the next the colour is half way) other than the middle is made by a
    // stop of the halfway colour there, which is as near as PowerPoint comes.
    static Gradient gradient(const Graphics::Gradient &g)
    {
        Gradient made;
        made.radial = g.type() == Graphics::Gradient::TYPE_RADIAL;
        made.angle = wrapped(360 - g.angle());
        QList<double> positions;
        for (const auto &stop : g.stops())
            positions << std::clamp(stop.position(), 0.0, 1.0);
        if (positions.size() > 1 && std::all_of(positions.begin(), positions.end(), [&](double p) { return p == positions.first(); })) {
            for (int k = 0; k < positions.size(); ++k)
                positions[k] = double(k) / (positions.size() - 1);
        }
        for (int k = 0; k < g.stops_size(); ++k) {
            const QColor c = toColor(g.stops(k).color());
            made.stops << Gradient::Stop {c, positions.at(k)};
            const double blend = g.stops(k).blend_point();
            if (k + 1 < g.stops_size() && blend > 0.01 && blend < 0.99 && std::abs(blend - 0.5) > 0.01) {
                const QColor next = toColor(g.stops(k + 1).color());
                const QColor half = QColor::fromRgbF((c.redF() + next.redF()) / 2, (c.greenF() + next.greenF()) / 2,
                                                     (c.blueF() + next.blueF()) / 2, (c.alphaF() + next.alphaF()) / 2);
                made.stops << Gradient::Stop {half, positions.at(k) + (positions.at(k + 1) - positions.at(k)) * blend};
            }
        }
        return made;
    }

private:
    static QList<double> dashes(const Graphics::Stroke &s)
    {
        switch (s.style()) {
        case Graphics::Stroke::STYLE_SQUARE_DASH:
            return {1, 1};
        case Graphics::Stroke::STYLE_SHORT_DASH:
            return {3, 1};
        case Graphics::Stroke::STYLE_LONG_DASH:
            return {8, 3};
        default:
            return QList<double>(s.pattern().begin(), s.pattern().end());
        }
    }

    static void geometry(const Graphics::Path &path, Shape *shape)
    {
        switch (path.shape().type()) {
        case Graphics::Path::Shape::TYPE_RECTANGLE:
        case Graphics::Path::Shape::TYPE_UNKNOWN:
            shape->geometry = Shape::Rectangle;
            return;
        case Graphics::Path::Shape::TYPE_ELLIPSE:
            shape->geometry = Shape::Ellipse;
            return;
        case Graphics::Path::Shape::TYPE_ROUNDED_RECTANGLE:
            // A GUESS: roundness 1 is fully round ends.
            shape->geometry = Shape::RoundedRectangle;
            shape->cornerFraction = std::clamp(path.shape().rounded_rectangle().roundness(), 0.0, 1.0) * 0.5;
            return;
        default:
            break;
        }
        for (const auto &point : path.points())
            shape->points << QPointF(point.point().x(), point.point().y());
        shape->geometry = shape->points.size() >= 3 ? Shape::Polygon : Shape::Rectangle;
    }

    const ImportContext &m_context;
    NoteCollector *m_notes;
    const QHash<QString, QString> &m_familiesFromFile;
    const Backdrop &m_backdrop;
};

// ---------------------------------------------------------------------------------------

const rv::data::PresentationSlide *presentationSlideOf(const Cue &cue)
{
    for (const Action &action : cue.actions()) {
        if (action.has_slide() && action.slide().has_presentation())
            return &action.slide().presentation();
    }
    return nullptr;
}

// Every font the presentation's own descriptions name, with its family
QHash<QString, QString> familyNames(const rv::data::Presentation &presentation)
{
    QHash<QString, QString> result;
    const auto note = [&result](const rv::data::Font &font) {
        if (!font.name().empty() && !font.family().empty())
            result.insert(QString::fromStdString(font.name()), QString::fromStdString(font.family()));
    };
    for (const Cue &cue : presentation.cues()) {
        const rv::data::PresentationSlide *slide = presentationSlideOf(cue);
        if (!slide)
            continue;
        for (const auto &element : slide->base_slide().elements()) {
            if (!element.element().has_text())
                continue;
            note(element.element().text().attributes().font());
            for (const auto &custom : element.element().text().attributes().custom_attributes()) {
                if (custom.has_original_font())
                    note(custom.original_font());
            }
        }
    }
    return result;
}

// A box filling a run of blur (three box blurs one after another, which come close to a
// Gaussian), across and then down
void boxBlur(QImage *image, int radius)
{
    if (radius < 1)
        return;
    const int w = image->width();
    const int h = image->height();
    QList<quint32> line(std::max(w, h));
    const auto pass = [&](bool across) {
        const int length = across ? w : h;
        const int lines = across ? h : w;
        for (int l = 0; l < lines; ++l) {
            const auto pixel = [&](int k) -> QRgb & {
                return across ? reinterpret_cast<QRgb *>(image->scanLine(l))[k] : reinterpret_cast<QRgb *>(image->scanLine(k))[l];
            };
            for (int k = 0; k < length; ++k)
                line[k] = pixel(k);
            int sum[4] = {0, 0, 0, 0};
            const auto take = [&](int k, int sign) {
                const quint32 p = line[std::clamp(k, 0, length - 1)];
                sum[0] += sign * int(qAlpha(p));
                sum[1] += sign * int(qRed(p));
                sum[2] += sign * int(qGreen(p));
                sum[3] += sign * int(qBlue(p));
            };
            for (int k = -radius; k <= radius; ++k)
                take(k, 1);
            const int span = 2 * radius + 1;
            for (int k = 0; k < length; ++k) {
                pixel(k) = qRgba(sum[1] / span, sum[2] / span, sum[3] / span, sum[0] / span);
                take(k + radius + 1, 1);
                take(k - radius, -1);
            }
        }
    };
    pass(true);
    pass(false);
}

} // namespace

QImage backgroundEffect(const QImage &behind, double sigma, double saturation, bool invert)
{
    QImage image = behind.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (invert) {
        image.invertPixels();
        return image;
    }
    // Three box blurs of width w one after another are a Gaussian of a standard deviation
    // whose square is 3 (w^2 - 1) / 12; each box is as wide as makes the one wanted.
    const int box = int(std::round((std::sqrt(4 * sigma * sigma + 1) - 1) / 2));
    for (int k = 0; k < 3; ++k)
        boxBlur(&image, box);
    if (std::abs(saturation - 1) > 0.001) {
        const double s = std::clamp(saturation, 0.0, 3.0);
        for (int y = 0; y < image.height(); ++y) {
            QRgb *row = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const QRgb p = row[x];
                const double grey = 0.2126 * qRed(p) + 0.7152 * qGreen(p) + 0.0722 * qBlue(p);
                const auto channel = [&](int c) { return std::clamp(int(std::round(grey + (c - grey) * s)), 0, qAlpha(p)); };
                row[x] = qRgba(channel(qRed(p)), channel(qGreen(p)), channel(qBlue(p)), qAlpha(p));
            }
        }
    }
    return image;
}

ImportResult importPresentation(const rv::data::Presentation &presentation, const QString &fallbackName,
                                const QString &arrangement, const Options &options, const ImportContext &context)
{
    ImportResult result;
    NoteCollector notes;
    QList<PlayedCue> played = playOrder(presentation, options.followArrangement, arrangement, &result.arrangement);
    QList<PlayedCue> shownCues;
    for (const PlayedCue &item : played) {
        if (!options.skipDisabledSlides || item.cue->isenabled())
            shownCues << item;
    }

    // The size of the slides: the first one's that says, else 1920 x 1080
    Deck &deck = result.deck;
    for (const Cue &cue : presentation.cues()) {
        const rv::data::PresentationSlide *slide = presentationSlideOf(cue);
        if (slide && slide->base_slide().size().width() > 0 && slide->base_slide().size().height() > 0) {
            deck.size = QSizeF(slide->base_slide().size().width(), slide->base_slide().size().height());
            break;
        }
    }
    deck.title = presentation.name().empty() ? fallbackName : QString::fromStdString(presentation.name());

    QColor under(Qt::black);
    if (presentation.background().is_enabled() && presentation.background().has_color())
        under = toColor(presentation.background().color());

    const QHash<QString, QString> families = familyNames(presentation);
    const Backdrop backdrop(deck.size);
    ElementMapper mapper(context, &notes, families, backdrop);
    MediaLayers layers;
    QHash<QString, std::pair<QByteArray, double>> stills; // a video's still and length, by file
    const auto prepareVideo = [&](Media *media) {
        if (media->kind != Media::Video || media->path.isEmpty() || !context.videoStill)
            return;
        if (!stills.contains(media->path)) {
            // PowerPoint shows a video as a picture until it plays, and will not open a
            // deck whose video has none; and to start a video by itself it wants to know
            // how long it runs. ProPresenter keeps neither in the presentation.
            double length = 0;
            const QImage still = context.videoStill(media->path, &length);
            QByteArray png;
            if (!still.isNull()) {
                QBuffer buffer(&png);
                buffer.open(QIODevice::WriteOnly);
                still.save(&buffer, "PNG");
                if (media->naturalSize.isEmpty())
                    media->naturalSize = still.size();
            }
            stills.insert(media->path, std::make_pair(png, length));
        }
        media->still = stills.value(media->path).first;
        media->duration = stills.value(media->path).second;
        if (media->naturalSize.isEmpty() && !media->still.isEmpty())
            media->naturalSize = QImage::fromData(media->still).size();
    };

    for (int k = 0; k < shownCues.size(); ++k) {
        const int number = k + 1;
        const Cue &cue = *shownCues.at(k).cue;
        const Shown shown = layers.click(cue);
        Slide slide;
        slide.backgroundColor = under;
        const rv::data::PresentationSlide *base = presentationSlideOf(cue);

        // The background media
        const std::optional<MediaCue> background = shown.own ? shown.own : options.carryBackgroundForward ? shown.live : std::nullopt;
        if (background) {
            Media media = mapper.deckMedia(*background->media, number);
            if (media.kind != Media::Audio) {
                prepareVideo(&media);
                slide.background = BackgroundMedia {media, scaleOf(*background->media), !shown.own};
            }
        }

        // The slide's own colour
        if (base && base->base_slide().draws_background_color()) {
            const QColor colour = toColor(base->base_slide().background_color());
            if (!slide.background) {
                slide.backgroundColor = colour;
            } else if (colour.alpha() > 0) {
                Shape cover;
                cover.name = QStringLiteral("Slide colour");
                cover.frame = QRectF(QPointF(0, 0), deck.size);
                cover.fill = colour;
                slide.items << cover;
            }
        }

        // Foreground media
        if (shown.foreground) {
            Media media = mapper.deckMedia(*shown.foreground->media, number);
            const QRectF whole(QPointF(0, 0), deck.size);
            if (media.kind == Media::Image) {
                Picture picture;
                picture.name = QStringLiteral("Foreground media");
                picture.frame = whole;
                picture.media = media;
                picture.scale = scaleOf(*shown.foreground->media);
                slide.items << picture;
            } else if (media.kind == Media::Video) {
                prepareVideo(&media);
                Video video;
                video.name = QStringLiteral("Foreground media");
                video.frame = whole;
                video.media = media;
                video.scale = scaleOf(*shown.foreground->media);
                video.loops = (shown.foreground->action->has_video()
                               && shown.foreground->action->video().playback_behavior() == Action::MediaType::PLAYBACK_BEHAVIOR_LOOP)
                              || shown.foreground->media->video().transport().playback_behavior()
                                     == rv::data::Media::TransportProperties::PLAYBACK_BEHAVIOR_LOOP;
                slide.items << video;
            }
        }

        // The slide's own elements. KNOWN (ProToPPTX, from the slide pictures ProPresenter
        // exported, 2026-10-18): a slide lists its elements front to back, the first on
        // top. A deck lists them back to front.
        if (base) {
            const auto &elements = base->base_slide().elements();
            for (int e = elements.size() - 1; e >= 0; --e) {
                for (Item item : mapper.items(elements.at(e), slide, number)) {
                    if (Video *video = std::get_if<Video>(&item))
                        prepareVideo(&video->media);
                    slide.items << item;
                }
            }
            if (options.includeNotes && !base->notes().rtf_data().empty()) {
                const RtfDocument notesText = readRtf(QByteArray::fromStdString(base->notes().rtf_data()));
                QStringList lines;
                for (const Paragraph &paragraph : notesText.paragraphs) {
                    QString line;
                    for (const Run &run : paragraph.runs)
                        line += run.text;
                    lines << line.replace(QChar(0x2028), u'\n');
                }
                slide.notes = lines.join(u'\n').trimmed();
            }
        }

        for (const QString &macro : shown.macros)
            notes.add(Note::Macro, QStringLiteral("The macro \"%1\" is not followed: if it changes the background, check these slides").arg(macro), number);
        for (const QString &sound : shown.audio)
            notes.add(Note::Audio, QStringLiteral("The sound %1 is not put into the deck").arg(sound), number);
        if (!cue.isenabled())
            notes.add(Note::Simplified, QStringLiteral("Slides turned off in ProPresenter are included"), number);
        deck.slides << slide;
    }

    if (options.groupsAsSections) {
        QList<Section> sections;
        int lastOccurrence = -1;
        bool meaningful = false;
        for (int k = 0; k < shownCues.size(); ++k) {
            if (shownCues.at(k).groupOccurrence == lastOccurrence)
                continue;
            lastOccurrence = shownCues.at(k).groupOccurrence;
            const QString name = shownCues.at(k).groupName;
            meaningful = meaningful || (!name.isEmpty() && name.compare(QLatin1String("group"), Qt::CaseInsensitive) != 0);
            sections << Section {name.isEmpty() ? QStringLiteral("Untitled") : name, k};
        }
        if (meaningful)
            deck.sections = sections;
    }
    result.notes = notes.all();
    return result;
}

} // namespace pptx
