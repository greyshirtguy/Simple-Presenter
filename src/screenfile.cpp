#include "screenfile.h"

#include "proworkspace.pb.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QUuid>

namespace screenfile {

namespace {

QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/Workspace");
}

// (A new id and, below, the writing of the file are as workspacefiles.h has them. They
// are not taken from there so that this file stands by itself, with nothing of the app
// behind it.)
std::string newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

QString idOf(const rv::data::ProPresenterScreen &screen)
{
    return QString::fromStdString(screen.uuid().string());
}

Screen described(const rv::data::ProPresenterScreen &screen)
{
    Screen made;
    made.id = idOf(screen);
    made.name = QString::fromStdString(screen.name());
    made.stage = screen.screen_type() == rv::data::ProPresenterScreen::SCREEN_TYPE_STAGE;
    // Its size is that of the first of its parts, which is the whole of it unless
    // several outputs are combined.
    const rv::data::Screen *part = screen.has_arrangement_single() && screen.arrangement_single().screens_size() > 0
                                       ? &screen.arrangement_single().screens(0)
                                 : screen.has_arrangement_combined() && screen.arrangement_combined().screens_size() > 0
                                       ? &screen.arrangement_combined().screens(0)
                                 : screen.has_arrangement_edge_blend() && screen.arrangement_edge_blend().screens_size() > 0
                                       ? &screen.arrangement_edge_blend().screens(0)
                                       : nullptr;
    if (part && part->bounds().size().width() >= 1 && part->bounds().size().height() >= 1) {
        made.width = int(part->bounds().size().width());
        made.height = int(part->bounds().size().height());
    }
    return made;
}

// A screen as ProPresenter writes one that it has connected to nothing (which it calls
// a placeholder): what it is connected to on this computer is not this file's business.
void fill(rv::data::ProPresenterScreen *screen, const Screen &wanted)
{
    const std::string name = wanted.name.toStdString();
    screen->set_name(name);
    screen->set_screen_type(wanted.stage ? rv::data::ProPresenterScreen::SCREEN_TYPE_STAGE : rv::data::ProPresenterScreen::SCREEN_TYPE_AUDIENCE);
    screen->mutable_uuid()->set_string(wanted.id.toStdString());
    screen->mutable_background_color()->set_alpha(1);
    screen->set_rendering_enabled(true);
    rv::data::Screen *part = screen->mutable_arrangement_single()->add_screens();
    part->mutable_uuid()->set_string(newUuid());
    part->set_name(name);
    part->mutable_color()->set_red(0.6667f);
    part->mutable_color()->set_green(0.6667f);
    part->mutable_color()->set_blue(0.6667f);
    part->mutable_color()->set_alpha(1);
    part->mutable_bounds()->mutable_origin();
    part->mutable_bounds()->mutable_size()->set_width(wanted.width);
    part->mutable_bounds()->mutable_size()->set_height(wanted.height);
    part->mutable_subscreen_unit_rect()->mutable_origin();
    part->mutable_subscreen_unit_rect()->mutable_size()->set_width(1);
    part->mutable_subscreen_unit_rect()->mutable_size()->set_height(1);
    part->mutable_corner_values()->mutable_top_left();
    part->mutable_corner_values()->mutable_top_right();
    part->mutable_corner_values()->mutable_bottom_left();
    part->mutable_corner_values()->mutable_bottom_right();
    rv::data::OutputDisplay *display = part->mutable_output_display();
    display->set_name(name);
    display->set_devicename(name);
    display->set_type(rv::data::OutputDisplay::TYPE_CUSTOM);
    display->mutable_mode()->set_name(QStringLiteral("%1x%2p0").arg(wanted.width).arg(wanted.height).toStdString());
    display->mutable_mode()->set_width(uint32_t(wanted.width));
    display->mutable_mode()->set_height(uint32_t(wanted.height));
    display->set_render_id("placeholder:" + newUuid());
    display->set_hdr_max_nits_offset(1000);
    part->set_color_enabled(true);
    part->mutable_color_adjustment();
    part->mutable_alpha_settings()->set_mode(rv::data::Screen::AlphaSettings::MODE_DISABLED);
}

// The file, read whole: an empty one for a workspace that has none.
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
    // Written beside the file and then put in its place, so that the file is never
    // half of one.
    QSaveFile file(pathOf(workspace));
    const std::string bytes = document.SerializeAsString();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("The workspace's set-up file could not be written: %1").arg(file.errorString());
    return {};
}

// The file with its screens in it, the two every workspace has had put there if it
// says nothing of screens: what a change starts from.
QString loadForChange(const QString &workspace, rv::data::ProPresenterWorkspace *document)
{
    const QString error = load(workspace, document);
    if (!error.isEmpty())
        return error;
    if (document->pro_screens_size() == 0) {
        const QList<Screen> first = defaults();
        for (const Screen &screen : first)
            fill(document->add_pro_screens(), screen);
    }
    return {};
}

int indexOf(const rv::data::ProPresenterWorkspace &document, const QString &id)
{
    for (int i = 0; i < document.pro_screens_size(); ++i) {
        if (idOf(document.pro_screens(i)) == id)
            return i;
    }
    return -1;
}

// A look says what each screen gets; one that is gone has no say in it.
void forget(rv::data::ProAudienceLook *look, const std::string &id)
{
    auto *looks = look->mutable_screen_looks();
    for (int i = looks->size() - 1; i >= 0; --i) {
        if (looks->Get(i).pro_screen_uuid().string() == id)
            looks->DeleteSubrange(i, 1);
    }
}

}

QList<Screen> defaults()
{
    Screen audience;
    audience.id = QStringLiteral("5D1A0001-51DE-4A0D-8000-A0D1E9CE0001");
    audience.name = QStringLiteral("Audience");
    Screen stage;
    stage.id = QStringLiteral("5D1A0002-51DE-4A0D-8000-57A9E0000002");
    stage.name = QStringLiteral("Stage");
    stage.stage = true;
    return {audience, stage};
}

Screens read(const QString &workspace)
{
    Screens answer;
    rv::data::ProPresenterWorkspace document;
    answer.error = load(workspace, &document);
    for (const rv::data::ProPresenterScreen &screen : document.pro_screens()) {
        if (!idOf(screen).isEmpty())
            answer.screens << described(screen);
    }
    answer.fromFile = !answer.screens.isEmpty();
    if (!answer.fromFile)
        answer.screens = defaults();
    for (const rv::data::Stage::ScreenAssignment &mapping : document.stage_layout_mappings()) {
        const QString screen = QString::fromStdString(mapping.screen().parameter_uuid().string());
        const QString layout = QString::fromStdString(mapping.layout().parameter_uuid().string());
        if (!screen.isEmpty() && !layout.isEmpty())
            answer.layouts.insert(screen, layout);
    }
    return answer;
}

QString add(const QString &workspace, bool stage, const QString &name, QString *id)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = loadForChange(workspace, &document);
    if (!error.isEmpty())
        return error;
    if (document.pro_screens_size() >= limit)
        return QStringLiteral("A workspace can have %1 screens, and this one has them").arg(limit);
    Screen wanted;
    wanted.id = QString::fromStdString(newUuid());
    wanted.name = name;
    wanted.stage = stage;
    fill(document.add_pro_screens(), wanted);
    if (id)
        *id = wanted.id;
    return save(workspace, document);
}

QString remove(const QString &workspace, const QString &id)
{
    rv::data::ProPresenterWorkspace document;
    const QString error = loadForChange(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int at = indexOf(document, id);
    if (at < 0)
        return QStringLiteral("That screen is not in the workspace any more");
    const bool stage = document.pro_screens(at).screen_type() == rv::data::ProPresenterScreen::SCREEN_TYPE_STAGE;
    if (!stage) {
        int audience = 0;
        for (const rv::data::ProPresenterScreen &screen : document.pro_screens())
            audience += screen.screen_type() == rv::data::ProPresenterScreen::SCREEN_TYPE_STAGE ? 0 : 1;
        if (audience <= 1)
            return QStringLiteral("The last audience screen cannot be removed: there would be nothing to show on");
    }
    document.mutable_pro_screens()->DeleteSubrange(at, 1);
    // Nor is it anything's business any more what layout it had, or what a look gave it.
    const std::string gone = id.toStdString();
    auto *mappings = document.mutable_stage_layout_mappings();
    for (int i = mappings->size() - 1; i >= 0; --i) {
        if (mappings->Get(i).screen().parameter_uuid().string() == gone)
            mappings->DeleteSubrange(i, 1);
    }
    for (rv::data::ProAudienceLook &look : *document.mutable_audience_looks())
        forget(&look, gone);
    if (document.has_live_audience_look())
        forget(document.mutable_live_audience_look(), gone);
    return save(workspace, document);
}

QString rename(const QString &workspace, const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("A screen needs a name");
    rv::data::ProPresenterWorkspace document;
    const QString error = loadForChange(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int at = indexOf(document, id);
    if (at < 0)
        return QStringLiteral("That screen is not in the workspace any more");
    document.mutable_pro_screens(at)->set_name(name.trimmed().toStdString());
    return save(workspace, document);
}

}
