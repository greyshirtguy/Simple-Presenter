#include "copyrightdisplay.h"

#include "ccli.h"
#include "proconvert.h"
#include "sessionlog.h"
#include "workspacefiles.h"

#include <QDir>
#include <QFile>

namespace {

// What a workspace with no file has (see copyrightdisplay.h): ProPresenter's layout, and
// a template of one text box along the foot of the slide, linked to the copyright, made
// as the editor makes a text box.
rv::data::CCLIDocument madeDocument()
{
    rv::data::CCLIDocument document = ccli::made(workspace::newUuid);
    rv::data::Slide *slide = document.mutable_template_()->mutable_base_slide();
    rv::data::Slide::Element box = proconvert::makeTextElement(*slide, nullptr);
    box.mutable_element()->set_name("Copyright");
    rv::data::Graphics::Rect *bounds = box.mutable_element()->mutable_bounds();
    bounds->mutable_origin()->set_x(60);
    bounds->mutable_origin()->set_y(860);
    bounds->mutable_size()->set_width(1800);
    bounds->mutable_size()->set_height(180);
    box.add_data_links()->mutable_ccli_text();
    *slide->add_elements() = box;
    return document;
}

} // namespace

QString CopyrightDisplay::open(const QString &workspace)
{
    m_workspace = workspace;
    m_path = workspace.isEmpty() ? QString() : QDir(workspace).filePath(QStringLiteral("Configuration/CCLI"));
    return reload();
}

QString CopyrightDisplay::reload()
{
    rv::data::CCLIDocument document;
    QString error;
    if (!m_path.isEmpty() && QFile::exists(m_path))
        error = workspace::readMessage(m_path, &document, QStringLiteral("the copyright display"));
    else
        document = madeDocument();
    if (!error.isEmpty())
        document = madeDocument();
    m_settings = ccli::describe(document);
    m_template = proconvert::toSlideMap(document.template_().base_slide(), QStringLiteral("Copyright"));
    emit changed();
    return error;
}

QString CopyrightDisplay::set(const QVariantMap &changes)
{
    if (m_path.isEmpty())
        return QStringLiteral("No workspace is open");
    rv::data::CCLIDocument document;
    if (QFile::exists(m_path)) {
        const QString error = workspace::readMessage(m_path, &document, QStringLiteral("the copyright display"));
        if (!error.isEmpty())
            return error;
    } else {
        document = madeDocument();
    }
    const QString error = ccli::change(&document, changes);
    if (!error.isEmpty())
        return error;
    const QString written = workspace::writeMessage(m_path, document, QStringLiteral("the copyright display"));
    if (written.isEmpty())
        SessionLog::write("edit", QStringLiteral("the copyright display: %1").arg(QStringList(changes.keys()).join(QStringLiteral(", "))));
    reload();
    return written;
}

QString CopyrightDisplay::example() const
{
    const QVariantMap song {{"title", QStringLiteral("Song Title")}, {"artist", QStringLiteral("Artist")}, {"author", QStringLiteral("Author")},
                            {"publisher", QStringLiteral("Publisher")}, {"year", 2026}, {"songNumber", 1234567}};
    const QString license = m_settings.value("license").toString();
    return ccli::compose(m_settings.value("tokens").toList(), song, license.isEmpty() ? QStringLiteral("123456") : license);
}

QString CopyrightDisplay::makeFile()
{
    if (m_path.isEmpty())
        return QStringLiteral("No workspace is open");
    if (QFile::exists(m_path))
        return {};
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    return workspace::writeMessage(m_path, madeDocument(), QStringLiteral("the copyright display"));
}
