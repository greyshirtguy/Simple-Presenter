#include "slideedit.h"

#include "chords.h"
#include "proconvert.h"
#include "reflow.h"
#include "workspacefiles.h"

#include <QFileInfo>

#include <limits>

namespace slideedit {

namespace {

bool hasWords(const rv::data::Slide::Element &element)
{
    return element.element().has_text() && !proconvert::readText(element.element().text()).plainText().trimmed().isEmpty();
}

// Whether an element's text is its own to type: not another element's, a timer's or
// the live slide's, which a link puts there
bool ownText(const rv::data::Slide::Element &element)
{
    return proconvert::elementMap(element).value(QStringLiteral("linkKind")).toString() == QLatin1String("none");
}

// The text given to a text box, by whichever of the ways it was asked
QString writeTextOf(rv::data::Slide::Element *target, const extendedapijson::ElementChanges &changes)
{
    rv::data::Graphics::Text *text = target->mutable_element()->mutable_text();
    if (changes.hasParagraphs) {
        const RichText had = proconvert::readText(*text);
        RichText made;
        QString error;
        if (!extendedapijson::paragraphsOf(changes.paragraphs, had.firstRun(),
                                           had.paragraphs.isEmpty() ? Qt::AlignHCenter : had.paragraphs.first().alignment, &made, &error))
            return error;
        proconvert::writeText(text, made);
    }
    if (changes.hasText) {
        const RichText had = proconvert::readText(*text);
        if (had.plainText() != changes.text)
            proconvert::writeText(text, reflow::retyped(had, changes.text));
    }
    if (!changes.format.isEmpty()) {
        const RichText had = proconvert::readText(*text);
        const RichText formatted = had.formatted(0, std::numeric_limits<int>::max(), changes.format);
        if (!(formatted == had))
            proconvert::writeText(text, formatted);
    }
    if (changes.hasChords) {
        QList<chords::Chord> list;
        for (const QVariant &each : changes.chords) {
            const QVariantMap chord = each.toMap();
            const QString name = chord.value(QStringLiteral("name")).toString().trimmed();
            bool isPlace = false;
            const int at = chord.value(QStringLiteral("at")).toInt(&isPlace);
            if (name.isEmpty() || !isPlace || at < 0)
                return QStringLiteral("Each of `chords` is {at, name}: a place in the words from 0, and the chord's name");
            list.append({at, name});
        }
        proconvert::writeChords(text, list);
    }
    return {};
}

QString applied(rv::data::Slide *slide, int index, const extendedapijson::ElementChanges &changes)
{
    const QString id = QString::fromStdString(slide->elements(index).element().uuid().string());
    if (changes.properties.contains(QStringLiteral("fillMediaPath"))) {
        const QString file = changes.properties.value(QStringLiteral("fillMediaPath")).toString();
        if (!workspace::isMedia(file) || !QFileInfo(file).isFile())
            return QStringLiteral("%1 is not a picture or a video that is on this computer").arg(file);
    }
    if (!changes.properties.isEmpty() && !proconvert::applyChanges(slide, id, changes.properties))
        return QStringLiteral("That element is no longer on the slide");
    const QString error = writeTextOf(slide->mutable_elements(index), changes);
    if (!error.isEmpty())
        return error;
    if (changes.order >= 0) {
        int from = index;
        const int to = qBound(0, changes.order, slide->elements_size() - 1);
        // Step by step, each a swap with its neighbour, as the editor moves one
        for (; from < to; ++from)
            slide->mutable_elements()->SwapElements(from, from + 1);
        for (; from > to; --from)
            slide->mutable_elements()->SwapElements(from, from - 1);
    }
    return {};
}

}

int indexOf(const rv::data::Slide &slide, const QString &which)
{
    const std::string wanted = which.toStdString();
    for (int i = 0; i < slide.elements_size(); ++i) {
        if (slide.elements(i).element().uuid().string() == wanted)
            return i;
    }
    for (int i = 0; i < slide.elements_size(); ++i) {
        if (!wanted.empty() && slide.elements(i).element().name() == wanted)
            return i;
    }
    bool isNumber = false;
    const int number = which.toInt(&isNumber);
    return isNumber && number >= 0 && number < slide.elements_size() ? number : -1;
}

QString add(rv::data::Slide *slide, const extendedapijson::ElementChanges &wanted, QString *id)
{
    const rv::data::Slide before = *slide;
    rv::data::Slide::Element made;
    const QString kind = wanted.kind.isEmpty() ? QStringLiteral("text") : wanted.kind;
    if (kind == QLatin1String("text")) {
        // Set like the slide's first text, as the editor's Text button sets one
        const rv::data::Slide::Element *model = nullptr;
        for (int i = 0; !model && i < slide->elements_size(); ++i) {
            if (!slide->elements(i).element().text().rtf_data().empty())
                model = &slide->elements(i);
        }
        made = proconvert::makeTextElement(*slide, model);
    } else if (kind == QLatin1String("rectangle") || kind == QLatin1String("roundedRectangle") || kind == QLatin1String("ellipse")
               || kind == QLatin1String("arrow")) {
        made = proconvert::makeShapeElement(*slide, kind);
    } else if (kind == QLatin1String("media")) {
        if (!workspace::isMedia(wanted.file) || !QFileInfo(wanted.file).isFile())
            return wanted.file.isEmpty() ? QStringLiteral("An element of kind \"media\" needs `file`, a picture's or a video's path")
                                         : QStringLiteral("%1 is not a picture or a video that is on this computer").arg(wanted.file);
        made = proconvert::makeMediaElement(*slide, wanted.file);
    } else {
        return QStringLiteral("An element's `kind` is one of \"text\", \"rectangle\", \"rounded_rectangle\", \"ellipse\", \"arrow\" and \"media\"");
    }
    *id = QString::fromStdString(made.element().uuid().string());
    *slide->add_elements() = made;
    // (Where it is in the order is its own business from here: the last, in front.)
    extendedapijson::ElementChanges rest = wanted;
    const QString error = applied(slide, slide->elements_size() - 1, rest);
    if (!error.isEmpty())
        *slide = before;
    return error;
}

QString change(rv::data::Slide *slide, int index, const extendedapijson::ElementChanges &changes)
{
    if (index < 0 || index >= slide->elements_size())
        return QStringLiteral("That element is no longer on the slide");
    const rv::data::Slide before = *slide;
    const QString error = applied(slide, index, changes);
    if (!error.isEmpty())
        *slide = before;
    return error;
}

void remove(rv::data::Slide *slide, int index)
{
    if (index < 0 || index >= slide->elements_size())
        return;
    const std::string id = slide->elements(index).element().uuid().string();
    slide->mutable_elements()->DeleteSubrange(index, 1);
    for (int i = slide->element_build_order_size() - 1; i >= 0; --i) {
        if (slide->element_build_order(i).string() == id)
            slide->mutable_element_build_order()->DeleteSubrange(i, 1);
    }
}

int wordsBox(const rv::data::Slide &slide)
{
    int empty = -1;
    for (int i = 0; i < slide.elements_size(); ++i) {
        const rv::data::Slide::Element &element = slide.elements(i);
        if (!element.element().has_text() || !ownText(element))
            continue;
        if (hasWords(element))
            return i;
        if (empty < 0)
            empty = i;
    }
    return empty;
}

void setWords(rv::data::Slide *slide, const QString &words, const rv::data::Slide::Element *like)
{
    int box = wordsBox(*slide);
    if (box < 0) {
        rv::data::Slide::Element made;
        if (like) {
            // The other slide's box over again, as itself: an id of its own, and
            // without the builds and the chords that were the other's
            made = *like;
            made.mutable_element()->mutable_uuid()->set_string(workspace::newUuid());
            made.clear_build_in();
            made.clear_build_out();
            made.clear_childbuilds();
            proconvert::writeChords(made.mutable_element()->mutable_text(), {});
        } else {
            const bool hasSize = slide->size().width() > 0 && slide->size().height() > 0;
            const double width = hasSize ? slide->size().width() : 1920;
            const double height = hasSize ? slide->size().height() : 1080;
            made = proconvert::makeTextElement(*slide, nullptr);
            rv::data::Graphics::Element *element = made.mutable_element();
            // (As songimport makes a song's: see there for why it is called this)
            element->set_name("Lyrics");
            element->mutable_bounds()->mutable_origin()->set_x(width * 0.05);
            element->mutable_bounds()->mutable_origin()->set_y(height * 0.05);
            element->mutable_bounds()->mutable_size()->set_width(width * 0.9);
            element->mutable_bounds()->mutable_size()->set_height(height * 0.9);
            rv::data::Graphics::Text *text = element->mutable_text();
            TextRun format = proconvert::readText(*text).firstRun();
            format.size = qRound(height * 84 / 1080);
            proconvert::writeText(text, RichText::plain(QString(), format, Qt::AlignHCenter));
        }
        *slide->add_elements() = made;
        box = slide->elements_size() - 1;
    }
    rv::data::Graphics::Text *text = slide->mutable_elements(box)->mutable_element()->mutable_text();
    const RichText had = proconvert::readText(*text);
    if (had.plainText() != words)
        proconvert::writeText(text, reflow::retyped(had, words));
}

bool setBackground(rv::data::Slide *slide, const QString &color)
{
    if (color.trimmed().isEmpty()) {
        if (slide->draws_background_color())
            slide->set_draws_background_color(false);
        return true;
    }
    QColor wanted;
    if (!extendedapijson::colorOf(color, &wanted))
        return false;
    if (slide->draws_background_color() && proconvert::toColor(slide->background_color()).rgba() == wanted.rgba())
        return true;
    slide->set_draws_background_color(true);
    proconvert::setColor(slide->mutable_background_color(), wanted);
    return true;
}

}
