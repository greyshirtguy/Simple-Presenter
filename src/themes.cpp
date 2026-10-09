#include "themes.h"

#include "proconvert.h"
#include "prodocument.h"
#include "richtext.h"
#include "sessionlog.h"
#include "themefile.h"
#include "workspacefiles.h"

#include "template.pb.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

namespace {

QVariant orNull(const QVariantMap &map)
{
    return map.isEmpty() ? QVariant::fromValue(nullptr) : QVariant(map);
}

// A new theme slide: black with one text box, to be made into something.
void fillSlide(rv::data::Template::Slide *made, const QString &name)
{
    made->set_name(name.toStdString());
    rv::data::Slide *slide = made->mutable_base_slide();
    slide->set_draws_background_color(false);
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    slide->mutable_uuid()->set_string(workspace::newUuid());
    rv::data::Slide::Element element = proconvert::makeTextElement(*slide, nullptr);
    rv::data::Graphics::Text *text = element.mutable_element()->mutable_text();
    TextRun format = proconvert::readText(*text).firstRun();
    format.applyFormat({{"size", 90.0}, {"bold", true}, {"color", QColor(Qt::white)}});
    proconvert::writeText(text, RichText::plain(QStringLiteral("Text"), format, Qt::AlignHCenter));
    const QString id = QString::fromStdString(element.element().uuid().string());
    *slide->add_elements() = element;
    proconvert::applyChanges(slide, id, {
        {"name", QStringLiteral("Text")}, {"x", 110.0}, {"y", 190.0}, {"width", 1700.0}, {"height", 700.0},
        {"textScale", int(rv::data::Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_DOWN)},
    });
}

QString freeSlideName(const rv::data::Template::Document &theme)
{
    QSet<QString> taken;
    for (const rv::data::Template::Slide &slide : theme.slides())
        taken.insert(QString::fromStdString(slide.name()));
    QString name = QStringLiteral("Theme Slide");
    for (int n = 2; taken.contains(name); ++n)
        name = QStringLiteral("Theme Slide %1").arg(n);
    return name;
}

// The file as it is, kept beside the editor's backups before it is changed.
void keepCopy(const QString &path, const QString &workspace)
{
    const QFileInfo file(path);
    QString place = file.dir().dirName();
    const QString relative = QDir(workspace).relativeFilePath(file.absolutePath());
    if (!workspace.isEmpty() && !relative.startsWith(QLatin1String("..")))
        place = QFileInfo(workspace).fileName() + u'/' + relative;
    const QDir directory(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/Edit Backups/") + place);
    if (!directory.mkpath(QStringLiteral(".")))
        return;
    const QString target = directory.filePath(file.completeBaseName() + QDateTime::currentDateTime().toString(QStringLiteral(" yyyy-MM-dd HH.mm.ss"))
                                              + QStringLiteral(" before theme.") + file.suffix());
    if (!QFile::exists(target))
        QFile::copy(path, target);
}

}

void Themes::open(const QString &workspace)
{
    m_workspace = workspace;
    reload();
}

// A theme's slides, as entries for the windows.
QVariantList Themes::read(const QString &place)
{
    QVariantList slides;
    rv::data::Template::Document theme;
    if (!themefile::read(m_workspace, place, &theme, nullptr))
        return slides;
    for (const rv::data::Template::Slide &slide : theme.slides()) {
        const QString name = QString::fromStdString(slide.name());
        slides << QVariantMap {
            {"id", QString::fromStdString(slide.base_slide().uuid().string())},
            {"name", name},
            {"slide", proconvert::toSlideMap(slide.base_slide(), name)},
        };
        for (const rv::data::Slide::Element &element : slide.base_slide().elements())
            m_elementIds.insert(QString::fromStdString(element.element().uuid().string()));
    }
    return slides;
}

void Themes::reload()
{
    m_tree.clear();
    m_themes.clear();
    m_elementIds.clear();
    m_dressed.clear();
    const QStringList places = m_workspace.isEmpty() ? QStringList() : themefile::places(m_workspace);
    // Each theme is put in the folder its place says, the folders made as they are met.
    const std::function<void(QVariantList &, const QStringList &, int, const QVariantMap &)> put =
        [&put](QVariantList &items, const QStringList &parts, int depth, const QVariantMap &theme) {
            if (depth == parts.size() - 1) {
                items << theme;
                return;
            }
            for (QVariant &entry : items) {
                QVariantMap folder = entry.toMap();
                if (folder.value("kind") == QLatin1String("folder") && folder.value("name") == parts.at(depth)) {
                    QVariantList inside = folder.value("items").toList();
                    put(inside, parts, depth + 1, theme);
                    folder.insert("items", inside);
                    entry = folder;
                    return;
                }
            }
            QVariantList inside;
            put(inside, parts, depth + 1, theme);
            items << QVariantMap {{"kind", QStringLiteral("folder")}, {"name", parts.at(depth)},
                                  {"place", QStringList(parts.mid(0, depth + 1)).join(u'/')}, {"items", inside}};
        };
    for (const QString &place : places) {
        const QStringList parts = place.split(u'/');
        const QVariantMap theme {{"kind", QStringLiteral("theme")}, {"name", parts.last()}, {"place", place}, {"slides", read(place)}};
        m_themes << theme;
        put(m_tree, parts, 0, theme);
    }
    if (!places.isEmpty())
        SessionLog::write("themes", QStringLiteral("%1 themes in the workspace").arg(places.size()));
    emit changed();
}

QVariant Themes::theme(const QString &place) const
{
    for (const QVariant &entry : m_themes) {
        if (entry.toMap().value("place").toString() == place)
            return entry;
    }
    return QVariant::fromValue(nullptr);
}

QVariant Themes::slide(const QString &place, const QString &slideId) const
{
    const QVariantList slides = theme(place).toMap().value("slides").toList();
    for (const QVariant &entry : slides) {
        if (entry.toMap().value("id").toString() == slideId)
            return entry;
    }
    return QVariant::fromValue(nullptr);
}

QString Themes::apply(const QString &presentation, const QStringList &slideIds, const QString &place, const QString &slideId)
{
    rv::data::Template::Document theme;
    QString error;
    if (!themefile::read(m_workspace, place, &theme, &error))
        return error;
    const rv::data::Template::Slide *themed = themefile::slideOf(theme, slideId);
    if (!themed)
        return QStringLiteral("The theme \"%1\" no longer has that slide").arg(place);
    keepCopy(presentation, m_workspace);
    int count = 0;
    error = ProDocument::dressCues(presentation, slideIds, themed->base_slide(), m_elementIds, &count);
    if (error.isEmpty())
        SessionLog::write("themes", QStringLiteral("%1 of \"%2\" dressed in \"%3\" of the theme \"%4\"")
                                        .arg(count == 1 ? QStringLiteral("a slide") : QStringLiteral("%1 slides").arg(count),
                                             QFileInfo(presentation).completeBaseName(), QString::fromStdString(themed->name()), place));
    m_dressed.clear();
    return error;
}

QVariantMap Themes::dressed(const QString &presentation, const QString &slideId, const QString &place, const QString &themeSlideId)
{
    // Kept, by what it was made from, for as long as neither file changes: a slide
    // goes live many times in a show, and often on more than one screen.
    const QString themeFile = themefile::folder(m_workspace) + u'/' + place + QStringLiteral("/Theme");
    const QString key = QStringLiteral("%1|%2|%3|%4|%5|%6").arg(presentation).arg(QFileInfo(presentation).lastModified().toMSecsSinceEpoch())
                            .arg(slideId, place, themeSlideId).arg(QFileInfo(themeFile).lastModified().toMSecsSinceEpoch());
    const auto kept = m_dressed.constFind(key);
    if (kept != m_dressed.constEnd())
        return kept.value();
    rv::data::Template::Document theme;
    QVariantMap made;
    if (themefile::read(m_workspace, place, &theme, nullptr)) {
        if (const rv::data::Template::Slide *themed = themefile::slideOf(theme, themeSlideId))
            made = ProDocument::dressedSlide(presentation, slideId, themed->base_slide(), m_elementIds);
    }
    if (m_dressed.size() > 400)
        m_dressed.clear();
    m_dressed.insert(key, made);
    return made;
}

QVariantMap Themes::add(const QString &name)
{
    const QString wanted = name.trimmed();
    if (wanted.isEmpty() || wanted.contains(u'/'))
        return {{"error", QStringLiteral("A theme needs a name, without a slash in it")}};
    if (QFileInfo::exists(themefile::folder(m_workspace) + u'/' + wanted))
        return {{"error", QStringLiteral("There is a theme or a folder of themes called \"%1\" already").arg(wanted)}};
    rv::data::Template::Document theme;
    rv::data::Template::Slide *slide = theme.add_slides();
    fillSlide(slide, QStringLiteral("Theme Slide"));
    const QString slideId = QString::fromStdString(slide->base_slide().uuid().string());
    const QString error = themefile::write(m_workspace, wanted, theme);
    if (error.isEmpty()) {
        QDir().mkpath(themefile::folder(m_workspace) + u'/' + wanted + QStringLiteral("/Assets"));
        SessionLog::write("themes", QStringLiteral("made the theme \"%1\"").arg(wanted));
        reload();
    }
    return {{"place", wanted}, {"slideId", slideId}, {"error", error}};
}

QString Themes::remove(const QString &place)
{
    QDir directory(themefile::folder(m_workspace) + u'/' + place);
    if (place.isEmpty() || place.contains(QLatin1String("..")) || !QFileInfo::exists(directory.filePath(QStringLiteral("Theme"))))
        return QStringLiteral("That theme is not in the workspace any more");
    // The theme's own folder, with its pictures: nothing else is in it.
    if (!directory.removeRecursively())
        return QStringLiteral("The theme \"%1\" could not be removed").arg(place);
    SessionLog::write("themes", QStringLiteral("removed the theme \"%1\"").arg(place));
    reload();
    return {};
}

QVariantMap Themes::addSlide(const QString &place)
{
    rv::data::Template::Document theme;
    QString error;
    if (!themefile::read(m_workspace, place, &theme, &error))
        return {{"error", error}};
    rv::data::Template::Slide *slide = theme.add_slides();
    fillSlide(slide, freeSlideName(theme));
    const QString slideId = QString::fromStdString(slide->base_slide().uuid().string());
    error = themefile::write(m_workspace, place, theme);
    if (error.isEmpty())
        reload();
    return {{"place", place}, {"slideId", slideId}, {"id", slideId}, {"error", error}};
}

QString Themes::renameSlide(const QString &place, const QString &slideId, const QString &name)
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("A theme slide needs a name");
    rv::data::Template::Document theme;
    QString error;
    if (!themefile::read(m_workspace, place, &theme, &error))
        return error;
    for (rv::data::Template::Slide &slide : *theme.mutable_slides()) {
        if (QString::fromStdString(slide.base_slide().uuid().string()) != slideId)
            continue;
        slide.set_name(name.trimmed().toStdString());
        error = themefile::write(m_workspace, place, theme);
        if (error.isEmpty())
            reload();
        return error;
    }
    return QStringLiteral("The theme no longer has that slide");
}

QString Themes::removeSlide(const QString &place, const QString &slideId)
{
    rv::data::Template::Document theme;
    QString error;
    if (!themefile::read(m_workspace, place, &theme, &error))
        return error;
    if (theme.slides_size() <= 1)
        return QStringLiteral("A theme's last slide cannot be removed: remove the theme");
    for (int i = 0; i < theme.slides_size(); ++i) {
        if (QString::fromStdString(theme.slides(i).base_slide().uuid().string()) != slideId)
            continue;
        theme.mutable_slides()->DeleteSubrange(i, 1);
        error = themefile::write(m_workspace, place, theme);
        if (error.isEmpty())
            reload();
        return error;
    }
    return QStringLiteral("The theme no longer has that slide");
}
