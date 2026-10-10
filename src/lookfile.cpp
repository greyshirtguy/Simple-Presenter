#include "lookfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>

namespace lookfile {

namespace {

QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/Workspace");
}

// (As in screenfile.cpp: this file stands by itself too.)
std::string newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

QString load(const QString &workspace, rv::data::ProPresenterWorkspace *document)
{
    document->Clear();
    QFile file(pathOf(workspace));
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("The workspace's set-up file could not be read: %1").arg(file.errorString());
    const QByteArray bytes = file.readAll();
    if (!document->ParseFromArray(bytes.constData(), int(bytes.size()))) {
        document->Clear();
        return QStringLiteral("The workspace's set-up file is not one this app can read, so it is left alone");
    }
    return {};
}

QString save(const QString &workspace, const rv::data::ProPresenterWorkspace &document)
{
    if (!QDir().mkpath(workspace + QStringLiteral("/Configuration")))
        return QStringLiteral("The workspace's Configuration folder could not be made");
    QSaveFile file(pathOf(workspace));
    const std::string bytes = document.SerializeAsString();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("The workspace's set-up file could not be written: %1").arg(file.errorString());
    return {};
}

QString named(const rv::data::URL &url)
{
    if (!url.absolute_string().empty())
        return QUrl::fromPercentEncoding(QByteArray::fromStdString(url.absolute_string()));
    if (!url.relative_path().empty())
        return QString::fromStdString(url.relative_path());
    if (url.has_local())
        return QString::fromStdString(url.local().path());
    return {};
}

Look described(const rv::data::ProAudienceLook &look)
{
    Look made;
    made.id = QString::fromStdString(look.uuid().string());
    made.name = QString::fromStdString(look.name());
    made.transition = look.transition_duration();
    for (const rv::data::ProAudienceLook::ProScreenLook &screen : look.screen_looks()) {
        ScreenLook given;
        given.slide = screen.presentation_foreground_enabled();
        given.media = screen.presentation_background_enabled();
        given.props = screen.props_layer_enabled();
        given.messages = screen.messages_layer_enabled();
        given.announcements = screen.announcements_enabled();
        given.videoInput = screen.live_video_enabled();
        given.mask = QString::fromStdString(screen.mask_uuid().string());
        if (screen.has_template_document_file_path()) {
            given.theme = themePlace(named(screen.template_document_file_path()));
            if (!given.theme.isEmpty())
                given.themeSlide = QString::fromStdString(screen.template_slide_uuid().string());
        }
        made.screens.insert(QString::fromStdString(screen.pro_screen_uuid().string()), given);
    }
    return made;
}

// A screen's line in a look as ProPresenter writes a new one: everything on.
void everything(rv::data::ProAudienceLook::ProScreenLook *screen, const QString &screenId)
{
    screen->mutable_pro_screen_uuid()->set_string(screenId.toStdString());
    screen->set_props_enabled(true);
    screen->set_live_video_enabled(true);
    screen->set_presentation_background_enabled(true);
    screen->set_presentation_foreground_enabled(true);
    screen->set_announcements_enabled(true);
    screen->set_props_layer_enabled(true);
    screen->set_messages_layer_enabled(true);
}

rv::data::ProAudienceLook *find(rv::data::ProPresenterWorkspace *document, const QString &id)
{
    for (rv::data::ProAudienceLook &look : *document->mutable_audience_looks()) {
        if (QString::fromStdString(look.uuid().string()) == id)
            return &look;
    }
    return nullptr;
}

}

QString themePlace(const QString &named)
{
    const QString marker = QStringLiteral("/Themes/");
    const qsizetype at = named.lastIndexOf(marker);
    if (at < 0)
        return {};
    QString place = named.mid(at + marker.size());
    // (The file named is the one called Theme in the theme's own folder.)
    if (place.endsWith(QStringLiteral("/Theme")))
        place.chop(6);
    while (place.endsWith(u'/'))
        place.chop(1);
    return place;
}

Looks read(const QString &workspace)
{
    Looks answer;
    rv::data::ProPresenterWorkspace document;
    answer.error = load(workspace, &document);
    for (const rv::data::ProAudienceLook &look : document.audience_looks()) {
        if (!look.uuid().string().empty())
            answer.looks << described(look);
    }
    if (document.has_live_audience_look()) {
        answer.live = described(document.live_audience_look());
        answer.live.origin = QString::fromStdString(document.live_audience_look().original_look_uuid().string());
        // (A live look written with no id of its own is still one: it is given an id
        // when it is next written.)
        if (answer.live.id.isEmpty())
            answer.live.id = liveLook();
    }
    return answer;
}

QString liveLook()
{
    return QStringLiteral("<live>");
}

QString add(const QString &workspace, const QString &name, const QStringList &screenIds, const QString &copyOf, QString *id)
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("A look needs a name");
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    // What it is a copy of is found before the list grows, which may move the list.
    rv::data::ProAudienceLook copied;
    bool copy = false;
    if (copyOf == liveLook()) {
        copy = document.has_live_audience_look();
        if (copy)
            copied = document.live_audience_look();
    } else if (!copyOf.isEmpty()) {
        const rv::data::ProAudienceLook *from = find(&document, copyOf);
        if (!from)
            return QStringLiteral("That look is not in the workspace any more");
        copied = *from;
        copy = true;
    }
    rv::data::ProAudienceLook *look = document.add_audience_looks();
    if (copy) {
        // Everything the look copied has, lines for screens and switches this app
        // does not know among them; but it is a saved look, with no note of another.
        *look = copied;
        look->clear_original_look_uuid();
    } else {
        look->set_transition_duration(1);
        for (const QString &screenId : screenIds)
            everything(look->add_screen_looks(), screenId);
    }
    const std::string made = newUuid();
    look->mutable_uuid()->set_string(made);
    look->set_name(name.trimmed().toStdString());
    if (id)
        *id = QString::fromStdString(made);
    return save(workspace, document);
}

QString rename(const QString &workspace, const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("A look needs a name");
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    rv::data::ProAudienceLook *look = find(&document, id);
    if (!look)
        return QStringLiteral("That look is not in the workspace any more");
    look->set_name(name.trimmed().toStdString());
    return save(workspace, document);
}

QString remove(const QString &workspace, const QString &id)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    auto *looks = document.mutable_audience_looks();
    for (int i = 0; i < looks->size(); ++i) {
        if (QString::fromStdString(looks->Get(i).uuid().string()) == id) {
            looks->DeleteSubrange(i, 1);
            return save(workspace, document);
        }
    }
    return QStringLiteral("That look is not in the workspace any more");
}

namespace {

// Gives a look's line for a screen what is wanted of the layers this app has; the rest
// of the line is left as it is.
void change(rv::data::ProAudienceLook *look, const QString &screenId, const ScreenLook &wanted, const QString &workspace)
{
    rv::data::ProAudienceLook::ProScreenLook *screen = nullptr;
    for (rv::data::ProAudienceLook::ProScreenLook &candidate : *look->mutable_screen_looks()) {
        if (QString::fromStdString(candidate.pro_screen_uuid().string()) == screenId)
            screen = &candidate;
    }
    if (!screen) {
        screen = look->add_screen_looks();
        everything(screen, screenId);
    }
    screen->set_presentation_foreground_enabled(wanted.slide);
    screen->set_presentation_background_enabled(wanted.media);
    screen->set_props_layer_enabled(wanted.props);
    // The theme, if it is another than the line has: one that is the same is left as
    // the file has it, in whatever way the computer that wrote it named it.
    const QString had = screen->has_template_document_file_path() ? themePlace(named(screen->template_document_file_path())) : QString();
    const QString hadSlide = QString::fromStdString(screen->template_slide_uuid().string());
    if (wanted.theme != had || wanted.themeSlide != hadSlide) {
        screen->clear_template_document_file_path();
        screen->clear_template_slide_uuid();
        if (!wanted.theme.isEmpty()) {
            const QString file = QDir(workspace).absoluteFilePath(QStringLiteral("Themes/") + wanted.theme + QStringLiteral("/Theme"));
            rv::data::URL *url = screen->mutable_template_document_file_path();
            url->set_absolute_string(QUrl::fromLocalFile(file).toString(QUrl::FullyEncoded).toStdString());
            url->mutable_local()->set_root(rv::data::URL::LocalRelativePath::ROOT_BOOT_VOLUME);
            url->mutable_local()->set_path(file.mid(1).toStdString());
            screen->mutable_template_slide_uuid()->set_string(wanted.themeSlide.toStdString());
        }
    }
}

}

QString setScreen(const QString &workspace, const QString &id, const QString &screenId, const ScreenLook &wanted)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    rv::data::ProAudienceLook *look = find(&document, id);
    if (!look)
        return QStringLiteral("That look is not in the workspace any more");
    change(look, screenId, wanted, workspace);
    return save(workspace, document);
}

QString setLiveScreen(const QString &workspace, const QString &screenId, const ScreenLook &wanted)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    rv::data::ProAudienceLook *live = document.mutable_live_audience_look();
    if (live->uuid().string().empty())
        live->mutable_uuid()->set_string(newUuid());
    change(live, screenId, wanted, workspace);
    return save(workspace, document);
}

QString makeLive(const QString &workspace, const QString &id)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    const rv::data::ProAudienceLook *look = find(&document, id);
    if (!look)
        return QStringLiteral("That look is not in the workspace any more");
    // A copy of all of it, what this app does not know of a look included, under the
    // live look's own id, which it keeps from one look to the next.
    const rv::data::ProAudienceLook copied = *look;
    rv::data::ProAudienceLook *live = document.mutable_live_audience_look();
    const std::string own = live->uuid().string().empty() ? newUuid() : live->uuid().string();
    *live = copied;
    live->mutable_uuid()->set_string(own);
    live->mutable_original_look_uuid()->set_string(copied.uuid().string());
    return save(workspace, document);
}

QString saveLive(const QString &workspace)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    if (!document.has_live_audience_look())
        return QStringLiteral("The workspace has no live look to save");
    rv::data::ProAudienceLook *saved = find(&document, QString::fromStdString(document.live_audience_look().original_look_uuid().string()));
    if (!saved)
        return QStringLiteral("The look the live look was made from is not in the workspace any more");
    // The lines as they stand, each whole: a line is copied and not rebuilt, so that
    // what ProPresenter keeps in one that this app knows nothing of goes with it.
    *saved->mutable_screen_looks() = document.live_audience_look().screen_looks();
    return save(workspace, document);
}

}
