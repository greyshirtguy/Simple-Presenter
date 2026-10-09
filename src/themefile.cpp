#include "themefile.h"

#include "proconvert.h"
#include "richtext.h"
#include "thememath.h"

#include "template.pb.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>

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

}

void dress(rv::data::Slide *slide, const rv::data::Slide &theme, const QSet<QString> &themeElements)
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
    QList<int> themeBoxes;
    QList<thememath::Box> themeSizes;
    for (int i = 0; i < theme.elements_size(); ++i) {
        if (isTextBox(theme.elements(i))) {
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
    const auto fitted = [across, down](rv::data::Graphics::Element *graphic) {
        if (across == 1 && down == 1)
            return;
        rv::data::Graphics::Rect *bounds = graphic->mutable_bounds();
        bounds->mutable_origin()->set_x(bounds->origin().x() * across);
        bounds->mutable_origin()->set_y(bounds->origin().y() * down);
        bounds->mutable_size()->set_width(bounds->size().width() * across);
        bounds->mutable_size()->set_height(bounds->size().height() * down);
    };

    QList<Element> made = own;
    QList<bool> used(worded.size(), false);
    for (int i = 0; i < theme.elements_size(); ++i) {
        const Element &themed = theme.elements(i);
        const int box = themeBoxes.indexOf(i);
        const int from = box < 0 ? -1 : into.at(box);
        Element element = from < 0 ? themed : worded.at(from);
        *element.mutable_element() = themed.element();
        fitted(element.mutable_element());
        if (box >= 0) {
            // The words that go in it, or none
            const QString words = from < 0 ? QString() : wordsOf(worded.at(from));
            proconvert::writeText(element.mutable_element()->mutable_text(), poured(words, themed.element().text()));
        }
        if (from >= 0) {
            used[from] = true;
            // It is still the slide's own text box, by its id
            *element.mutable_element()->mutable_uuid() = worded.at(from).element().uuid();
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
    slide->set_draws_background_color(theme.draws_background_color());
    if (theme.has_background_color())
        *slide->mutable_background_color() = theme.background_color();
    else
        slide->clear_background_color();
}

}
