#include "pptxmake.h"

#include "mediasettings.h"
#include "pptxtext.h"
#include "proconvert.h"
#include "richtext.h"
#include "workspacefiles.h"

#include "presentation.pb.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>
#include <QVariantMap>

#include <cmath>

namespace pptx {

namespace {

// The slide every presentation this app makes has
const QSizeF slideSize(1920, 1080);
// What ProPresenter keeps clear beyond a text box's margins (KNOWN, ProToPPTX: measured;
// pptximport.cpp adds the same going the other way)
const double pad = 5;

void setColor(rv::data::Color *target, const QColor &color)
{
    target->set_red(float(color.redF()));
    target->set_green(float(color.greenF()));
    target->set_blue(float(color.blueF()));
    target->set_alpha(float(color.alphaF()));
}

// PowerPoint counts an angle clockwise from the right; ProPresenter's gradients and
// shadows count anticlockwise from the right (see proconvert's insertShadow)
double anticlockwise(double clockwise)
{
    return std::fmod(std::fmod(360 - clockwise, 360) + 360, 360);
}

QString markerOf(const Paragraph::Bullet &bullet, int number)
{
    if (bullet.numberScheme.isEmpty())
        return bullet.character.isEmpty() ? QStringLiteral("•") : bullet.character;
    const QString scheme = bullet.numberScheme;
    QString count;
    if (scheme.startsWith(QLatin1String("alpha"))) {
        count = QString(QChar((scheme.contains(QLatin1String("Uc")) ? 'A' : 'a') + std::clamp(number - 1, 0, 25)));
    } else if (scheme.startsWith(QLatin1String("roman"))) {
        static const char *const ones[] = {"", "i", "ii", "iii", "iv", "v", "vi", "vii", "viii", "ix"};
        static const char *const tens[] = {"", "x", "xx", "xxx", "xl", "l", "lx", "lxx", "lxxx", "xc"};
        const int n = std::clamp(number, 1, 99);
        count = QLatin1String(tens[n / 10]) + QLatin1String(ones[n % 10]);
        if (scheme.contains(QLatin1String("Uc")))
            count = count.toUpper();
    } else {
        count = QString::number(number);
    }
    if (scheme.endsWith(QLatin1String("ParenR")))
        return count + u')';
    if (scheme.endsWith(QLatin1String("ParenBoth")))
        return u'(' + count + u')';
    if (scheme.endsWith(QLatin1String("Plain")))
        return count;
    return count + u'.';
}

class Maker
{
public:
    Maker(const Deck &deck, const QString &name, const QString &workspace, const QString &mediaFolder, const MakeOptions &options,
          NoteCollector *notes)
        : m_deck(deck), m_name(name), m_workspace(workspace), m_mediaFolder(mediaFolder), m_options(options), m_notes(notes)
    {
        // The deck fitted into the slide and centred: whole, at one scale
        const double dw = std::max(1.0, deck.size.width());
        const double dh = std::max(1.0, deck.size.height());
        m_scale = std::min(slideSize.width() / dw, slideSize.height() / dh);
        m_offset = QPointF((slideSize.width() - dw * m_scale) / 2, (slideSize.height() - dh * m_scale) / 2);
    }

    rv::data::Presentation build(QString *error)
    {
        rv::data::Presentation presentation = proconvert::newPresentation(m_name);
        if (!QDir().mkpath(m_mediaFolder)) {
            *error = QStringLiteral("The folder for the media, %1, could not be made").arg(m_mediaFolder);
            return presentation;
        }
        for (int k = 0; k < m_deck.slides.size(); ++k) {
            const int number = k + 1;
            rv::data::Cue *cue = proconvert::addBlankCue(&presentation, std::string(), slideSize);
            cue->set_isenabled(!m_deck.slides.at(k).hidden);
            m_cueIds.append(QString::fromStdString(cue->uuid().string()));
            rv::data::PresentationSlide *presentationSlide = cue->mutable_actions(0)->mutable_slide()->mutable_presentation();
            fillSlide(m_deck.slides.at(k), number, cue, presentationSlide->mutable_base_slide());
            if (!m_deck.slides.at(k).notes.isEmpty())
                proconvert::writeNotes(presentationSlide, RichText::plain(m_deck.slides.at(k).notes, proconvert::notesFormat(), Qt::AlignLeft));
            if (m_progress && !m_progress(number, int(m_deck.slides.size()))) {
                *error = QStringLiteral("The import was stopped");
                return presentation;
            }
        }
        addGroups(&presentation);
        if (m_listsSimplified)
            m_notes->add(Note::Simplified, QStringLiteral("Lists are written as their markers and their words: a line that wraps does not hang"));
        return presentation;
    }

    void setProgress(const std::function<bool(int, int)> &progress) { m_progress = progress; }

private:
    QRectF placed(const QRectF &frame) const
    {
        return QRectF(m_offset.x() + frame.x() * m_scale, m_offset.y() + frame.y() * m_scale, frame.width() * m_scale, frame.height() * m_scale);
    }

    // ---- Groups: the deck's sections, or one group with no name

    void addGroups(rv::data::Presentation *presentation)
    {
        QList<Section> sections = m_deck.sections;
        std::sort(sections.begin(), sections.end(), [](const Section &a, const Section &b) { return a.firstSlide < b.firstSlide; });
        if (sections.isEmpty() || sections.first().firstSlide > 0)
            sections.prepend({QString(), 0});
        for (int k = 0; k < sections.size(); ++k) {
            const int from = std::clamp(sections.at(k).firstSlide, 0, int(m_cueIds.size()));
            const int to = k + 1 < sections.size() ? std::clamp(sections.at(k + 1).firstSlide, from, int(m_cueIds.size())) : int(m_cueIds.size());
            if (to <= from && !(k == 0 && sections.size() == 1))
                continue;
            auto *group = presentation->add_cue_groups();
            group->mutable_group()->mutable_uuid()->set_string(workspace::newUuid());
            group->mutable_group()->set_name(sections.at(k).name.toStdString());
            group->mutable_group()->mutable_hotkey();
            for (int c = from; c < to; ++c)
                group->add_cue_identifiers()->set_string(m_cueIds.at(c).toStdString());
        }
    }

    // ---- Media files: each written into the folder once

    QString fileFor(const Media &media, int slide)
    {
        const QString key = media.key();
        if (m_written.contains(key))
            return m_written.value(key);
        if (media.isMissing()) {
            m_notes->add(Note::MissingMedia, QStringLiteral("A picture could not be taken out of the file"), slide);
            return {};
        }
        QString name = media.fileName.isEmpty() ? QStringLiteral("picture") : media.fileName;
        const QString base = QFileInfo(name).completeBaseName();
        const QString suffix = QFileInfo(name).suffix();
        QString destination = QDir(m_mediaFolder).filePath(name);
        for (int n = 2; QFile::exists(destination) && n < 1000; ++n)
            destination = QDir(m_mediaFolder).filePath(base + QStringLiteral("-%1").arg(n) + (suffix.isEmpty() ? QString() : u'.' + suffix));
        bool ok = false;
        if (!media.path.isEmpty()) {
            ok = QFile::copy(media.path, destination);
        } else {
            QFile out(destination);
            ok = out.open(QIODevice::WriteOnly) && out.write(media.data) == media.data.size();
        }
        if (!ok) {
            m_notes->add(Note::MissingMedia, QStringLiteral("A picture could not be written into the workspace (%1)").arg(QFileInfo(destination).fileName()), slide);
            return {};
        }
        m_written.insert(key, destination);
        return destination;
    }

    // ---- A slide

    void fillSlide(const Slide &slide, int number, rv::data::Cue *cue, rv::data::Slide *base)
    {
        if (!m_options.leaveOutBackgrounds) {
            if (slide.backgroundColor) {
                base->set_draws_background_color(true);
                setColor(base->mutable_background_color(), *slide.backgroundColor);
            }
            if (slide.backgroundGradient)
                addGradientBackdrop(*slide.backgroundGradient, base);
            if (slide.background)
                addBackgroundMedia(slide.background->media, number, cue);
        }
        // The deck's items are back to front; ProPresenter lists elements front to back
        for (int k = int(slide.items.size()) - 1; k >= 0; --k) {
            const Item &item = slide.items.at(k);
            if (const Shape *shape = std::get_if<Shape>(&item))
                addShape(*shape, number, base);
            else if (const Picture *picture = std::get_if<Picture>(&item))
                addPicture(*picture, number, base);
            else if (const Video *video = std::get_if<Video>(&item))
                addVideo(*video, number, base);
        }
    }

    void addBackgroundMedia(const Media &media, int number, rv::data::Cue *cue)
    {
        const QString file = fileFor(media, number);
        if (file.isEmpty())
            return;
        // (As ProPresenter writes a background: see ProDocument::setCueMedia)
        rv::data::Action *action = cue->add_actions();
        action->mutable_uuid()->set_string(workspace::newUuid());
        action->set_isenabled(true);
        action->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        action->mutable_media()->set_layer_type(rv::data::Action::LAYER_TYPE_BACKGROUND);
        action->mutable_media()->mutable_audio();
        *action->mutable_media()->mutable_element() = workspace::mediaElement(file, m_workspace);
        workspace::setMediaForeground(action, false);
        // Stretched over the slide, as PowerPoint stretches a background, where the deck is
        // the slide's shape; fitted, with the sides left black, where it is not
        const bool sameShape = std::abs(m_deck.size.width() / std::max(1.0, m_deck.size.height()) - slideSize.width() / slideSize.height()) < 0.01;
        mediasettings::apply(action->mutable_media()->mutable_element(), {{"scale", sameShape ? 2 : 0}});
    }

    void addGradientBackdrop(const Gradient &gradient, rv::data::Slide *base)
    {
        rv::data::Slide::Element made = proconvert::makeShapeElement(*base, QStringLiteral("rectangle"));
        made.mutable_element()->set_name(uniqueName(QStringLiteral("Background"), base).toStdString());
        *base->add_elements() = made;
        const QString id = QString::fromStdString(made.element().uuid().string());
        QVariantMap changes = frameChanges(QRectF(QPointF(0, 0), m_deck.size));
        changes.insert("fillOn", true);
        gradientChanges(gradient, &changes);
        proconvert::applyChanges(base, id, changes);
    }

    QVariantMap frameChanges(const QRectF &deckFrame) const
    {
        const QRectF frame = placed(deckFrame);
        return {{"x", frame.x()}, {"y", frame.y()}, {"width", std::max(1.0, frame.width())}, {"height", std::max(1.0, frame.height())}};
    }

    void gradientChanges(const Gradient &gradient, QVariantMap *changes) const
    {
        changes->insert("fillKind", QStringLiteral("gradient"));
        changes->insert("fillGradientFrom", gradient.stops.first().color);
        changes->insert("fillGradientTo", gradient.stops.last().color);
        changes->insert("fillGradientAngle", anticlockwise(gradient.radial ? 0 : gradient.angle));
    }

    QString uniqueName(const QString &wanted, const rv::data::Slide *slide) const
    {
        QSet<QString> taken;
        for (const rv::data::Slide::Element &element : slide->elements())
            taken.insert(QString::fromStdString(element.element().name()));
        QString name = wanted.isEmpty() ? QStringLiteral("Shape") : wanted;
        for (int n = 2; taken.contains(name) && n < 1000; ++n)
            name = wanted + QStringLiteral(" %1").arg(n);
        return name;
    }

    // ---- Shapes

    void addShape(const Shape &shape, int number, rv::data::Slide *base)
    {
        // A text box, with nothing drawn but its words, is a text element; anything with a
        // fill, a line or an outline of its own is a shape, with words if it has them (in
        // ProPresenter's files the two are one kind of thing, and only the mark that says
        // "text" differs: A GUESS that this is where ProPresenter draws the line)
        const bool plainBox = shape.text && !shape.fill && !shape.line && shape.geometry == Shape::Rectangle;
        const QString kind = shape.geometry == Shape::Ellipse ? QStringLiteral("ellipse")
                           : shape.geometry == Shape::RoundedRectangle ? QStringLiteral("roundedRectangle")
                                                                         : QStringLiteral("rectangle");
        rv::data::Slide::Element made = plainBox ? proconvert::makeTextElement(*base, nullptr) : proconvert::makeShapeElement(*base, kind);
        made.mutable_element()->set_name(uniqueName(shape.name, base).toStdString());
        made.mutable_element()->set_rotation(std::fmod(std::fmod(shape.rotation, 360) + 360, 360));
        if (shape.geometry == Shape::Polygon && shape.points.size() >= 3)
            writePolygon(shape.points, made.mutable_element()->mutable_path());
        *base->add_elements() = made;
        const QString id = QString::fromStdString(made.element().uuid().string());

        QRectF frame = shape.frame;
        QVariantMap changes;
        changes.insert("opacity", std::clamp(shape.opacity, 0.0, 1.0));
        if (shape.fill) {
            changes.insert("fillOn", true);
            if (const QColor *color = std::get_if<QColor>(&*shape.fill)) {
                changes.insert("fillColor", *color);
            } else if (const Gradient *gradient = std::get_if<Gradient>(&*shape.fill)) {
                gradientChanges(*gradient, &changes);
            } else if (const PictureFill *picture = std::get_if<PictureFill>(&*shape.fill)) {
                const QString file = fileFor(picture->media, number);
                if (!file.isEmpty())
                    m_pendingMedia.insert(id, file);
                else
                    changes.insert("fillOn", false);
            }
        } else {
            changes.insert("fillOn", false);
        }
        if (shape.line && shape.line->width > 0) {
            changes.insert("strokeOn", true);
            changes.insert("strokeColor", shape.line->color);
            changes.insert("strokeWidth", shape.line->width * m_scale);
        } else {
            changes.insert("strokeOn", false);
        }
        if (shape.shadow) {
            changes.insert("shadowEnabled", true);
            changes.insert("shadowColor", shape.shadow->color);
            changes.insert("shadowAngle", anticlockwise(shape.shadow->direction));
            changes.insert("shadowOffset", shape.shadow->distance * m_scale);
            changes.insert("shadowRadius", shape.shadow->blurRadius * m_scale);
        } else {
            changes.insert("shadowEnabled", false);
        }
        if (shape.softEdge > 0) {
            changes.insert("featherOn", true);
            changes.insert("featherRadius", std::clamp(shape.softEdge / std::max(1.0, std::min(frame.width(), frame.height())), 0.0, 0.5));
        }
        if (shape.geometry == Shape::RoundedRectangle)
            changes.insert("roundness", std::clamp(shape.cornerFraction, 0.0, 0.5));

        if (shape.text) {
            textChanges(*shape.text, &frame, number, &changes);
        }
        changes.insert(frameChanges(frame));
        proconvert::applyChanges(base, id, changes);

        rv::data::Slide::Element *element = &*(base->mutable_elements()->end() - 1);
        if (m_pendingMedia.contains(id)) {
            rv::data::Graphics::Fill *fill = element->mutable_element()->mutable_fill();
            *fill->mutable_media() = workspace::mediaElement(m_pendingMedia.take(id), m_workspace);
            fill->set_enable(true);
            mediasettings::apply(fill->mutable_media(), {{"scale", 2}});
        }
        if (shape.text) {
            proconvert::writeText(element->mutable_element()->mutable_text(), richTextOf(*shape.text, number));
            if (shape.text->autofit == TextBody::Shrink)
                element->mutable_element()->mutable_text()->set_scale_behavior(rv::data::Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_DOWN);
            if (!shape.text->wraps)
                m_notes->add(Note::Simplified, QStringLiteral("Words set not to wrap may wrap here, in a box of the same size"), number);
        }
    }

    // A closed outline of straight sides, on the unit square, as ProPresenter writes a
    // drawn shape's (see proconvert's setShapePath for the form of a point)
    static void writePolygon(const QList<QPointF> &points, rv::data::Graphics::Path *path)
    {
        path->clear_points();
        path->set_closed(true);
        path->mutable_shape()->set_type(rv::data::Graphics::Path::Shape::TYPE_CUSTOM);
        for (const QPointF &p : points) {
            auto *made = path->add_points();
            const double x = std::clamp(p.x(), -1.0, 2.0), y = std::clamp(p.y(), -1.0, 2.0);
            made->mutable_point()->set_x(x);
            made->mutable_point()->set_y(y);
            made->mutable_q0()->set_x(x);
            made->mutable_q0()->set_y(y);
            made->mutable_q1()->set_x(x);
            made->mutable_q1()->set_y(y);
        }
    }

    // ---- Text

    // The margins, the vertical alignment and the fit of a box's words, and the move of
    // its frame that puts its first line where PowerPoint draws it
    void textChanges(const TextBody &body, QRectF *frame, int number, QVariantMap *changes)
    {
        double top = body.insets.top() - pad;
        double bottom = body.insets.bottom() - pad;
        const double shift = firstLineShift(body, *frame);
        top += shift;
        bottom -= shift;
        // ProPresenter's margins cannot be less than nothing: the frame moves instead, and
        // grows by as much, so that the words keep their place
        if (top < 0) {
            frame->setTop(frame->top() + top);
            top = 0;
        }
        if (bottom < 0) {
            frame->setBottom(frame->bottom() - bottom);
            bottom = 0;
        }
        changes->insert("marginLeft", std::max(0.0, body.insets.left() - pad) * m_scale);
        changes->insert("marginRight", std::max(0.0, body.insets.right() - pad) * m_scale);
        changes->insert("marginTop", top * m_scale);
        changes->insert("marginBottom", bottom * m_scale);
        changes->insert("verticalAlignment", int(body.verticalAlignment == TextBody::Middle ? Qt::AlignVCenter
                                                 : body.verticalAlignment == TextBody::Bottom ? Qt::AlignBottom : Qt::AlignTop));
        changes->insert("textScale", body.autofit == TextBody::Shrink ? 2 : 0);
        Q_UNUSED(number)
    }

    // How far down from where ProPresenter would put the first baseline PowerPoint puts
    // it, in deck units: the reverse of the export's TextFit (pptxwriter.cpp), from the
    // same measurements (KNOWN, ProToPPTX, PowerPoint for the Mac: lines 1.2 x the size x
    // the percentage apart, the first baseline 0.9 of that down, a centred block shorter
    // by 0.4 x the size x (the percentage - 1)). Only where PowerPoint was given a line
    // spacing: given none, it goes by the font's metrics, as ProPresenter does.
    double firstLineShift(const TextBody &body, const QRectF &frame) const
    {
        if (body.paragraphs.isEmpty() || !body.paragraphs.first().lineSpacing)
            return 0;
        QList<double> spacing;
        for (const Paragraph &p : body.paragraphs) {
            const RunStyle *largest = &p.endStyle;
            for (const Run &run : p.runs) {
                if (largest == &p.endStyle || run.style.size > largest->size)
                    largest = &run.style;
            }
            const double size = std::max(largest->size, 0.1);
            const double natural = lineHeightRatio(*largest) * size;
            double pitch = natural;
            if (p.lineSpacing) {
                switch (p.lineSpacing->kind) {
                case Paragraph::LineSpacing::Multiple:
                    pitch = p.lineSpacing->value * natural;
                    break;
                case Paragraph::LineSpacing::Exactly:
                    pitch = p.lineSpacing->value;
                    break;
                case Paragraph::LineSpacing::AtLeast:
                    pitch = std::max(p.lineSpacing->value, natural);
                    break;
                }
            }
            spacing << pitch / (1.2 * size);
        }
        const double width = frame.width() - body.insets.left() - body.insets.right();
        const double available = frame.height() - body.insets.top() - body.insets.bottom();
        const LaidText laid = layoutLines(body, width, 1);
        if (laid.lines.isEmpty())
            return 0;
        const LaidLine &first = laid.lines.first();
        const LaidLine &last = laid.lines.last();
        double pro = first.baseline;
        if (body.verticalAlignment == TextBody::Middle)
            pro = (available - laid.height) / 2 + first.baseline;
        else if (body.verticalAlignment == TextBody::Bottom)
            pro = available - laid.height + first.baseline;
        const auto m = [&spacing](const LaidLine &line) { return spacing.value(line.paragraph, 1); };
        const double top = 0.9 * m(first) * first.largestSize;
        double height = 0;
        for (const LaidLine &line : laid.lines)
            height += 1.2 * m(line) * line.largestSize;
        height -= 0.4 * std::max(0.0, m(last) - 1) * last.largestSize;
        for (int k = 0; k < body.paragraphs.size(); ++k) {
            if (k > 0)
                height += body.paragraphs.at(k).spaceBefore;
            if (k < body.paragraphs.size() - 1)
                height += body.paragraphs.at(k).spaceAfter;
        }
        double powerPoint = top;
        if (body.verticalAlignment == TextBody::Middle)
            powerPoint = (available - height) / 2 + top;
        else if (body.verticalAlignment == TextBody::Bottom)
            powerPoint = available - height + top;
        return powerPoint - pro;
    }

    RichText richTextOf(const TextBody &body, int number)
    {
        RichText rich;
        int numbering = 0;
        for (int k = 0; k < body.paragraphs.size(); ++k) {
            const Paragraph &p = body.paragraphs.at(k);
            TextParagraph paragraph;
            paragraph.alignment = p.alignment == Paragraph::Center ? Qt::AlignHCenter
                                : p.alignment == Paragraph::Right ? Qt::AlignRight
                                : p.alignment == Paragraph::Justified ? Qt::AlignJustify : Qt::AlignLeft;
            if (p.lineSpacing) {
                switch (p.lineSpacing->kind) {
                case Paragraph::LineSpacing::Multiple:
                    // (RTF's \slmult: 240 is single)
                    paragraph.lineHeight = qRound(p.lineSpacing->value * 240);
                    paragraph.lineHeightIsMultiple = true;
                    break;
                case Paragraph::LineSpacing::Exactly:
                    paragraph.lineHeight = -qRound(p.lineSpacing->value * m_scale * 20);
                    break;
                case Paragraph::LineSpacing::AtLeast:
                    paragraph.lineHeight = qRound(p.lineSpacing->value * m_scale * 20);
                    break;
                }
            }
            if (p.bullet && !p.runs.isEmpty()) {
                // The marker written into the words, in the first run's style
                m_listsSimplified = true;
                if (p.bullet->numberScheme.isEmpty()) {
                    numbering = 0;
                } else {
                    const bool continues = k > 0 && body.paragraphs.at(k - 1).bullet && !body.paragraphs.at(k - 1).bullet->numberScheme.isEmpty();
                    numbering = continues ? numbering + 1 : p.bullet->startAt;
                }
                TextRun marker = runOf(p.runs.first().style, number);
                marker.text = markerOf(*p.bullet, numbering) + u' ';
                paragraph.runs.append(marker);
            } else {
                numbering = 0;
            }
            for (const Run &run : p.runs) {
                TextRun made = runOf(run.style, number);
                made.text = run.text;
                made.text.replace(QChar(0x2028), QChar(QChar::LineSeparator));
                paragraph.runs.append(made);
            }
            if (paragraph.runs.isEmpty()) {
                TextRun empty = runOf(p.endStyle, number);
                paragraph.runs.append(empty);
            }
            rich.paragraphs.append(paragraph);
            if ((p.spaceBefore > 0 && k > 0) || (p.spaceAfter > 0 && k + 1 < body.paragraphs.size()))
                m_spacingDropped = true;
        }
        if (m_spacingDropped) {
            m_notes->add(Note::Simplified, QStringLiteral("The space PowerPoint puts before and after a paragraph is not kept"), number);
            m_spacingDropped = false;
        }
        return rich;
    }

    TextRun runOf(const RunStyle &style, int number)
    {
        TextRun run;
        run.fontName = style.fontName;
        run.family = style.fontFamily;
        run.size = std::max(1.0, style.size * m_scale);
        run.bold = style.bold;
        run.italic = style.italic;
        run.underline = style.underline;
        run.strikethrough = style.strikethrough;
        run.kerning = style.characterSpacing * m_scale;
        run.superscript = style.superscript;
        run.capitalization = style.capitalization == RunStyle::AllCaps ? TextRun::AllCaps
                           : style.capitalization == RunStyle::SmallCaps ? TextRun::SmallCaps
                           : style.capitalization == RunStyle::TitleCase ? TextRun::TitleCase : TextRun::NoCapitalization;
        if (const QColor *color = std::get_if<QColor>(&style.fill)) {
            run.fill = *color;
            run.fillVisible = color->alpha() > 0;
        } else if (const Gradient *gradient = std::get_if<Gradient>(&style.fill)) {
            run.fill = gradient->stops.first().color;
            m_notes->add(Note::Simplified, QStringLiteral("Words filled with a gradient are shown in its first colour"), number);
        } else {
            run.fill = Qt::white;
            m_notes->add(Note::Simplified, QStringLiteral("Words filled with a picture are shown in white"), number);
        }
        if (style.outline && style.outline->width > 0) {
            run.stroke = style.outline->color;
            run.strokeWidth = style.outline->width * m_scale;
        }
        if (style.highlight)
            m_notes->add(Note::Simplified, QStringLiteral("Highlights behind words are left out"), number);
        if (style.shadow)
            m_shadowed = true;
        return run;
    }

    // ---- Pictures and videos

    void addPicture(const Picture &picture, int number, rv::data::Slide *base)
    {
        const QString file = fileFor(picture.media, number);
        if (file.isEmpty())
            return;
        rv::data::Slide::Element made = proconvert::makeShapeElement(*base, QStringLiteral("rectangle"));
        made.mutable_element()->set_name(uniqueName(picture.name.isEmpty() ? QFileInfo(file).completeBaseName() : picture.name, base).toStdString());
        made.mutable_element()->set_rotation(std::fmod(std::fmod(picture.rotation, 360) + 360, 360));
        *base->add_elements() = made;
        const QString id = QString::fromStdString(made.element().uuid().string());
        QVariantMap changes = frameChanges(picture.frame);
        changes.insert("opacity", std::clamp(picture.opacity, 0.0, 1.0));
        changes.insert("strokeOn", false);
        proconvert::applyChanges(base, id, changes);
        rv::data::Slide::Element *element = &*(base->mutable_elements()->end() - 1);
        rv::data::Graphics::Fill *fill = element->mutable_element()->mutable_fill();
        *fill->mutable_media() = workspace::mediaElement(file, m_workspace);
        fill->set_enable(true);
        // Stretched into its box after the crop, as PowerPoint draws it; the crop in the
        // picture's own pixels, as ProPresenter keeps one
        QVariantMap settings {{"scale", 2}, {"flipH", picture.flipHorizontal}, {"flipV", picture.flipVertical}};
        const QSize natural = QImageReader(file).size();
        if (!picture.crop.isNull() && natural.isValid()) {
            settings.insert("cropOn", true);
            settings.insert("cropLeft", picture.crop.left() * natural.width());
            settings.insert("cropRight", picture.crop.right() * natural.width());
            settings.insert("cropTop", picture.crop.top() * natural.height());
            settings.insert("cropBottom", picture.crop.bottom() * natural.height());
        }
        mediasettings::apply(fill->mutable_media(), settings);
    }

    void addVideo(const Video &video, int number, rv::data::Slide *base)
    {
        const QString file = fileFor(video.media, number);
        if (file.isEmpty())
            return;
        rv::data::Slide::Element made = proconvert::makeShapeElement(*base, QStringLiteral("rectangle"));
        made.mutable_element()->set_name(uniqueName(video.name.isEmpty() ? QFileInfo(file).completeBaseName() : video.name, base).toStdString());
        *base->add_elements() = made;
        const QString id = QString::fromStdString(made.element().uuid().string());
        QVariantMap changes = frameChanges(video.frame);
        changes.insert("strokeOn", false);
        proconvert::applyChanges(base, id, changes);
        rv::data::Slide::Element *element = &*(base->mutable_elements()->end() - 1);
        rv::data::Graphics::Fill *fill = element->mutable_element()->mutable_fill();
        *fill->mutable_media() = workspace::mediaElement(file, m_workspace);
        fill->set_enable(true);
        mediasettings::apply(fill->mutable_media(), {{"scale", video.scale == ScaleMode::Fit ? 0 : video.scale == ScaleMode::Fill ? 1 : 2},
                                                     {"playback", video.loops ? 1 : 0}});
        if (!video.playsAutomatically)
            m_notes->add(Note::Simplified, QStringLiteral("A video PowerPoint plays on a click plays when its slide comes up"), number);
    }

    const Deck &m_deck;
    QString m_name;
    QString m_workspace;
    QString m_mediaFolder;
    MakeOptions m_options;
    NoteCollector *m_notes;
    std::function<bool(int, int)> m_progress;
    double m_scale = 1;
    QPointF m_offset;
    QStringList m_cueIds;
    QHash<QString, QString> m_written;
    QHash<QString, QString> m_pendingMedia;
    bool m_listsSimplified = false;
    bool m_spacingDropped = false;
    bool m_shadowed = false;
};

} // namespace

rv::data::Presentation buildPresentation(const Deck &deck, const QString &name, const QString &workspace, const QString &mediaFolder,
                                         const MakeOptions &options, NoteCollector *notes, QString *error)
{
    NoteCollector own;
    Maker maker(deck, name, workspace, mediaFolder, options, notes ? notes : &own);
    error->clear();
    return maker.build(error);
}

Made makePresentation(const Deck &deck, const QString &name, const QString &library, const QString &workspace, const MakeOptions &options,
                      NoteCollector *notes, const std::function<bool(int done, int total)> &progress)
{
    Made made;
    if (!QFileInfo(library).isDir()) {
        made.error = QStringLiteral("There is no library to put it in");
        return made;
    }
    QString unique;
    made.path = proconvert::presentationFileFor(library, name, QStringLiteral("Presentation"), &unique);
    made.name = unique;
    const QString mediaFolder = QDir(workspace).filePath(QStringLiteral("Media/Imported/") + unique);
    NoteCollector own;
    Maker maker(deck, unique, workspace, mediaFolder, options, notes ? notes : &own);
    maker.setProgress(progress);
    QString error;
    const rv::data::Presentation presentation = maker.build(&error);
    if (!error.isEmpty()) {
        // (Nothing of a stopped or failed import is left: the media folder made for it
        // goes, by the path made here and no other)
        QDir folder(mediaFolder);
        if (folder.exists() && mediaFolder.contains(QLatin1String("/Media/Imported/")))
            folder.removeRecursively();
        made.error = error;
        return made;
    }
    made.error = workspace::writeMessage(made.path, presentation, QStringLiteral("the presentation"));
    made.slides = presentation.cues_size();
    return made;
}

} // namespace pptx
