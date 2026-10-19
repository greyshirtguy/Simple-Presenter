#include "themefile.h"

#include "builds.h"
#include "proconvert.h"
#include "richtext.h"
#include "thememath.h"

#include "template.pb.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>

#include <algorithm>

namespace themefile {

QString folder(const QString &workspace)
{
    return workspace + QStringLiteral("/Themes");
}

QStringList places(const QString &workspace)
{
    QStringList found;
    const QDir root(folder(workspace));
    QDirIterator files(root.path(), {QStringLiteral("Theme")}, QDir::Files, QDirIterator::Subdirectories);
    while (files.hasNext()) {
        const QString place = root.relativeFilePath(QFileInfo(files.next()).path());
        if (place != QLatin1String("."))
            found << place;
    }
    found.sort(Qt::CaseInsensitive);
    return found;
}

bool read(const QString &workspace, const QString &place, rv::data::Template::Document *theme, QString *error)
{
    theme->Clear();
    QFile file(folder(workspace) + u'/' + place + QStringLiteral("/Theme"));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("The theme \"%1\" could not be read: %2").arg(place, file.errorString());
        return false;
    }
    const QByteArray bytes = file.readAll();
    if (!theme->ParseFromArray(bytes.constData(), int(bytes.size()))) {
        theme->Clear();
        if (error)
            *error = QStringLiteral("The theme \"%1\" is not a file this app can read").arg(place);
        return false;
    }
    return true;
}

QString write(const QString &workspace, const QString &place, const rv::data::Template::Document &theme)
{
    const QString directory = folder(workspace) + u'/' + place;
    if (!QDir().mkpath(directory))
        return QStringLiteral("The theme's folder could not be made");
    QSaveFile file(directory + QStringLiteral("/Theme"));
    const std::string bytes = theme.SerializeAsString();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("The theme could not be written: %1").arg(file.errorString());
    return {};
}

const rv::data::Template::Slide *slideOf(const rv::data::Template::Document &theme, const QString &id)
{
    for (const rv::data::Template::Slide &slide : theme.slides()) {
        if (QString::fromStdString(slide.base_slide().uuid().string()) == id)
            return &slide;
    }
    return nullptr;
}

QSet<QString> elementIds(const QString &workspace)
{
    QSet<QString> ids;
    const QStringList all = places(workspace);
    for (const QString &place : all) {
        rv::data::Template::Document theme;
        if (!read(workspace, place, &theme, nullptr))
            continue;
        for (const rv::data::Template::Slide &slide : theme.slides()) {
            for (const rv::data::Slide::Element &element : slide.base_slide().elements())
                ids.insert(QString::fromStdString(element.element().uuid().string()));
        }
    }
    return ids;
}

namespace {

using Element = rv::data::Slide::Element;

QString wordsOf(const Element &element)
{
    return element.element().text().rtf_data().empty() ? QString() : proconvert::readText(element.element().text()).plainText();
}

// A theme's text box is one with words in it, which stand for the words it is to hold
// ("Verse", "Lyrics"). Anything can carry text in ProPresenter's files, a picture
// included, and mostly carries none: that is not somewhere for words to go.
bool isTextBox(const Element &element)
{
    return !wordsOf(element).trimmed().isEmpty();
}

thememath::Box boxOf(const Element &element)
{
    thememath::Box box;
    box.name = QString::fromStdString(element.element().name());
    box.width = element.element().bounds().size().width();
    box.height = element.element().bounds().size().height();
    return box;
}

// Words in the one format a theme's text box has.
RichText poured(const QString &words, const rv::data::Graphics::Text &themed)
{
    const RichText was = proconvert::readText(themed);
    const TextParagraph first = was.paragraphs.isEmpty() ? TextParagraph() : was.paragraphs.first();
    RichText made = RichText::plain(words, was.firstRun(), first.alignment);
    for (TextParagraph &paragraph : made.paragraphs) {
        paragraph.lineHeight = first.lineHeight;
        paragraph.lineHeightIsMultiple = first.lineHeightIsMultiple;
    }
    return made;
}

// Where a thing of a slide of one size goes on a slide of another: as far across and as
// far down, and as large in the same proportion.
void fit(rv::data::Graphics::Element *graphic, double across, double down)
{
    if (across == 1 && down == 1)
        return;
    rv::data::Graphics::Rect *bounds = graphic->mutable_bounds();
    bounds->mutable_origin()->set_x(bounds->origin().x() * across);
    bounds->mutable_origin()->set_y(bounds->origin().y() * down);
    bounds->mutable_size()->set_width(bounds->size().width() * across);
    bounds->mutable_size()->set_height(bounds->size().height() * down);
}

// A text box of a slide made to look as another does: it takes everything of the
// other's (its name, where it is and how large, fitted to the slide, its fill, outline
// and shadow, and the one format its text is in), and keeps its own words, the chords
// over them, and what else is its own and not its look (its id, what its text is
// linked to, how it comes on, which are kept beside it and not in it).
//
// (The chords are put back over the words. They are kept in the text beside its
// format, and went with it once: a song dressed in a theme lost its chords. And a box
// with chords whose look is taken leaves its chords behind, which are its words', not
// its look's.)
void restyle(Element *element, const Element &styled, double across, double down)
{
    const QString words = wordsOf(*element);
    const QList<chords::Chord> chords = proconvert::readChords(element->element().text());
    const rv::data::UUID id = element->element().uuid();
    *element->mutable_element() = styled.element();
    fit(element->mutable_element(), across, down);
    proconvert::writeText(element->mutable_element()->mutable_text(), poured(words, styled.element().text()));
    proconvert::writeChords(element->mutable_element()->mutable_text(), chords);
    *element->mutable_element()->mutable_uuid() = id;
}

}

void dress(rv::data::Slide *slide, const rv::data::Slide &theme, const QSet<QString> &themeElements, bool withBuilds)
{
    // The slide's things: its words, which are matched with the theme's text boxes;
    // what an earlier dressing brought, which goes; and whatever else is its own.
    QList<Element> worded;
    QList<Element> own;
    for (const Element &element : slide->elements()) {
        if (!wordsOf(element).trimmed().isEmpty())
            worded << element;
        else if (!themeElements.contains(QString::fromStdString(element.element().uuid().string())))
            own << element;
    }
    // The theme's text boxes: those with words in them, which stand for the words to
    // come; and those with none that are named as one of the slide's text boxes is,
    // which are for that box's words. A theme made to bring a name in on one screen has
    // its boxes "Name" and "Role" empty, with builds, for the words of the slide's boxes
    // of those names (the maintainer's "L3rd Name - Animated" is one). (A GUESS at
    // ProPresenter, from such a theme: that it matches an empty box by its name.)
    QSet<QString> wordedNames;
    for (const Element &element : std::as_const(worded))
        wordedNames.insert(QString::fromStdString(element.element().name()).trimmed().toLower());
    const auto namedForWords = [&wordedNames](const Element &element) {
        const QString name = QString::fromStdString(element.element().name()).trimmed().toLower();
        return element.element().has_text() && !name.isEmpty() && wordedNames.contains(name);
    };
    QList<int> themeBoxes;
    QList<thememath::Box> themeSizes;
    for (int i = 0; i < theme.elements_size(); ++i) {
        if (isTextBox(theme.elements(i)) || namedForWords(theme.elements(i))) {
            themeBoxes << i;
            themeSizes << boxOf(theme.elements(i));
        }
    }
    QList<thememath::Box> slideSizes;
    for (const Element &element : std::as_const(worded))
        slideSizes << boxOf(element);
    const QList<int> into = thememath::match(slideSizes, themeSizes);

    // A theme made for a slide of another size is fitted to this one.
    const double across = theme.size().width() > 0 && slide->size().width() > 0 ? slide->size().width() / theme.size().width() : 1;
    const double down = theme.size().height() > 0 && slide->size().height() > 0 ? slide->size().height() / theme.size().height() : 1;

    QList<Element> made = own;
    QList<bool> used(worded.size(), false);
    // Which of the slide's text boxes took which theme text box's look, by their ids
    QHash<QString, QString> tookLook;
    for (int i = 0; i < theme.elements_size(); ++i) {
        const Element &themed = theme.elements(i);
        const int box = themeBoxes.indexOf(i);
        const int from = box < 0 ? -1 : into.at(box);
        Element element = from < 0 ? themed : worded.at(from);
        if (from >= 0) {
            // The slide's own text box, with its words, in the theme's look
            restyle(&element, themed, across, down);
            used[from] = true;
            tookLook.insert(QString::fromStdString(element.element().uuid().string()), QString::fromStdString(themed.element().uuid().string()));
        } else {
            fit(element.mutable_element(), across, down);
            // A text box of the theme's that gets no words is left empty
            if (box >= 0)
                proconvert::writeText(element.mutable_element()->mutable_text(), poured(QString(), themed.element().text()));
        }
        made << element;
    }
    for (int i = 0; i < worded.size(); ++i) {
        if (!used.at(i))
            made << worded.at(i);
    }

    slide->clear_elements();
    for (const Element &element : std::as_const(made))
        *slide->add_elements() = element;
    if (withBuilds)
        builds::takeThemeBuilds(slide, theme, tookLook);
    slide->set_draws_background_color(theme.draws_background_color());
    if (theme.has_background_color())
        *slide->mutable_background_color() = theme.background_color();
    else
        slide->clear_background_color();
}

bool hasWords(const rv::data::Slide &slide)
{
    return std::any_of(slide.elements().begin(), slide.elements().end(), [](const Element &element) { return isTextBox(element); });
}

int dressText(rv::data::Slide *slide, const rv::data::Slide &style)
{
    // The text boxes of each that have words, matched as a slide's are with a theme's
    QList<int> worded;
    QList<thememath::Box> slideSizes;
    for (int i = 0; i < slide->elements_size(); ++i) {
        if (isTextBox(slide->elements(i))) {
            worded << i;
            slideSizes << boxOf(slide->elements(i));
        }
    }
    QList<int> styled;
    QList<thememath::Box> styleSizes;
    for (int i = 0; i < style.elements_size(); ++i) {
        if (isTextBox(style.elements(i))) {
            styled << i;
            styleSizes << boxOf(style.elements(i));
        }
    }
    const QList<int> into = thememath::match(slideSizes, styleSizes);

    // A slide of another size has its boxes fitted to this one, as a theme's are.
    const double across = style.size().width() > 0 && slide->size().width() > 0 ? slide->size().width() / style.size().width() : 1;
    const double down = style.size().height() > 0 && slide->size().height() > 0 ? slide->size().height() / style.size().height() : 1;
    int count = 0;
    for (int box = 0; box < styled.size(); ++box) {
        if (into.at(box) < 0)
            continue;
        // In its own place among the slide's things: nothing else moves.
        restyle(slide->mutable_elements(worded.at(into.at(box))), style.elements(styled.at(box)), across, down);
        ++count;
    }
    return count;
}

}
