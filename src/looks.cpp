#include "looks.h"

#include "sessionlog.h"

namespace {

QVariantMap described(const lookfile::ScreenLook &look)
{
    return {{"slide", look.slide}, {"media", look.media}, {"props", look.props}, {"theme", look.theme}, {"themeSlide", look.themeSlide}};
}

}

QString Looks::open(const QString &workspace)
{
    m_workspace = workspace;
    const lookfile::Looks found = lookfile::read(workspace);
    m_list = found.looks;
    m_startsWith = found.live;
    m_looks.clear();
    for (const lookfile::Look &look : std::as_const(m_list)) {
        QVariantMap screens;
        for (auto screen = look.screens.constBegin(); screen != look.screens.constEnd(); ++screen)
            screens.insert(screen.key(), described(screen.value()));
        m_looks << QVariantMap {{"id", look.id}, {"name", look.name}, {"transition", look.transition}, {"screens", screens}};
    }
    emit changed();
    return found.error;
}

void Looks::reload()
{
    open(m_workspace);
}

QVariantMap Looks::of(const QString &lookId, const QString &screenId) const
{
    for (const lookfile::Look &look : m_list) {
        if (look.id == lookId)
            return described(look.screens.value(screenId));
    }
    return described(lookfile::ScreenLook());
}

QVariantMap Looks::add(const QString &name, const QStringList &screenIds)
{
    QString id;
    const QString error = lookfile::add(m_workspace, name, screenIds, &id);
    if (error.isEmpty()) {
        SessionLog::write("looks", QStringLiteral("added the look \"%1\"").arg(name.trimmed()));
        reload();
    }
    return {{"id", id}, {"error", error}};
}

QString Looks::rename(const QString &id, const QString &name)
{
    const QString error = lookfile::rename(m_workspace, id, name);
    if (error.isEmpty())
        reload();
    return error;
}

QString Looks::remove(const QString &id)
{
    const QString error = lookfile::remove(m_workspace, id);
    if (error.isEmpty()) {
        SessionLog::write("looks", QStringLiteral("removed a look"));
        reload();
    }
    return error;
}

QString Looks::setScreen(const QString &id, const QString &screenId, const QVariantMap &changes)
{
    lookfile::ScreenLook wanted;
    for (const lookfile::Look &look : std::as_const(m_list)) {
        if (look.id == id)
            wanted = look.screens.value(screenId);
    }
    if (changes.contains("slide"))
        wanted.slide = changes.value("slide").toBool();
    if (changes.contains("media"))
        wanted.media = changes.value("media").toBool();
    if (changes.contains("props"))
        wanted.props = changes.value("props").toBool();
    if (changes.contains("theme")) {
        wanted.theme = changes.value("theme").toString();
        wanted.themeSlide = wanted.theme.isEmpty() ? QString() : changes.value("themeSlide").toString();
    }
    const QString error = lookfile::setScreen(m_workspace, id, screenId, wanted);
    if (error.isEmpty())
        reload();
    return error;
}
