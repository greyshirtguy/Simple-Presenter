#include "looks.h"

#include "sessionlog.h"

#include <QCoreApplication>

namespace {

QVariantMap described(const lookfile::ScreenLook &look)
{
    return {{"slide", look.slide}, {"media", look.media}, {"props", look.props}, {"theme", look.theme}, {"themeSlide", look.themeSlide},
            {"messages", look.messages}, {"announcements", look.announcements}, {"videoInput", look.videoInput}, {"mask", look.mask}};
}

QVariantMap described(const lookfile::Look &look)
{
    QVariantMap screens;
    for (auto screen = look.screens.constBegin(); screen != look.screens.constEnd(); ++screen)
        screens.insert(screen.key(), described(screen.value()));
    return {{"id", look.id}, {"name", look.name}, {"transition", look.transition}, {"screens", screens}};
}

// Whether two looks give every screen either of them names the same. (A screen a look
// does not name gets everything, which is what an empty line stands for.)
bool same(const lookfile::Look &one, const lookfile::Look &other)
{
    QStringList screens = one.screens.keys() + other.screens.keys();
    screens.removeDuplicates();
    for (const QString &screen : std::as_const(screens)) {
        if (one.screens.value(screen) != other.screens.value(screen))
            return false;
    }
    return true;
}

}

Looks::Looks(QObject *parent)
    : QObject(parent)
{
    m_write.setSingleShot(true);
    m_write.setInterval(300);
    connect(&m_write, &QTimer::timeout, this, &Looks::flush);
    // A look made live in the last moments of a run is written before the app goes:
    // when it says it is about to, and again here in case it never said so.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this] {
        QString error;
        writeMadeLive(&error);
    });
}

Looks::~Looks()
{
    QString error;
    writeMadeLive(&error);
}

QString Looks::open(const QString &workspace)
{
    // What was made live in the workspace being left is written there first.
    flush();
    m_workspace = workspace;
    const lookfile::Looks found = lookfile::read(workspace);
    m_list = found.looks;
    m_liveLook = found.live;
    describe();
    return found.error;
}

void Looks::reload()
{
    const lookfile::Looks found = lookfile::read(m_workspace);
    m_list = found.looks;
    m_liveLook = found.live;
    describe();
}

// The looks as QML has them, from the looks as they are held here.
void Looks::describe()
{
    m_looks.clear();
    for (const lookfile::Look &look : std::as_const(m_list))
        m_looks << described(look);
    // The saved look the live one was made from, if the workspace still has it, and
    // whether the live look is still what that look is.
    const auto origin = std::find_if(m_list.cbegin(), m_list.cend(), [this](const lookfile::Look &look) { return look.id == m_liveLook.origin; });
    m_live = described(m_liveLook);
    m_live.insert("origin", origin != m_list.cend() ? m_liveLook.origin : QString());
    m_live.insert("changed", origin != m_list.cend() && !same(*origin, m_liveLook));
    emit changed();
}

bool Looks::writeMadeLive(QString *error)
{
    m_write.stop();
    if (m_madeLive.isEmpty())
        return false;
    const QString id = m_madeLive;
    m_madeLive.clear();
    *error = lookfile::makeLive(m_workspace, id);
    return true;
}

void Looks::flush()
{
    QString error;
    if (!writeMadeLive(&error))
        return;
    if (!error.isEmpty()) {
        SessionLog::write("PROBLEM", QStringLiteral("The live look could not be written to the workspace's file: %1").arg(error));
        emit failed(error);
    }
    // The file is what is true: the live look as it is there (it has an id of its own
    // now, if it had none), or, had the writing failed, as it was.
    reload();
}

QVariantMap Looks::liveOf(const QString &screenId) const
{
    return described(m_liveLook.screens.value(screenId));
}

QVariantMap Looks::of(const QString &lookId, const QString &screenId) const
{
    if (!lookId.isEmpty() && lookId == m_liveLook.id)
        return liveOf(screenId);
    for (const lookfile::Look &look : m_list) {
        if (look.id == lookId)
            return described(look.screens.value(screenId));
    }
    return described(lookfile::ScreenLook());
}

QString Looks::makeLive(const QString &id)
{
    const auto saved = std::find_if(m_list.cbegin(), m_list.cend(), [&id](const lookfile::Look &look) { return look.id == id; });
    if (saved == m_list.cend())
        return QStringLiteral("That look is not in the workspace any more");
    // The look that is live already, and not changed since: nothing to do. In a
    // service this is the usual case, every song's first slide going over to the
    // same look.
    if (!m_liveLook.id.isEmpty() && m_liveLook.origin == id && m_liveLook.name == saved->name
        && m_liveLook.transition == saved->transition && same(*saved, m_liveLook))
        return {};
    // What the live look had been changed to goes with this, and that is easy to be
    // surprised by: a slide's action that goes over to a look undoes a change made to
    // the live look by hand. So the log says when it happens. (A guess as to
    // ProPresenter: that making a look live always copies it over the live look, even
    // when the live look came from that same look. It follows from what its file
    // holds, and has not been tried in ProPresenter itself.)
    if (m_live.value("changed").toBool()) {
        SessionLog::write("looks", m_liveLook.origin == id
                                       ? QStringLiteral("the live look had been changed: it is put back as the saved look \"%1\" has it").arg(saved->name)
                                       : QStringLiteral("the live look had been changed, and those changes go as the look \"%1\" is made live").arg(saved->name));
    }
    // The screens now, from what is in memory; the file in a moment (see looks.h).
    lookfile::Look live = *saved;
    live.id = m_liveLook.id.isEmpty() ? lookfile::liveLook() : m_liveLook.id;
    live.origin = id;
    m_liveLook = live;
    m_madeLive = id;
    m_write.start();
    describe();
    return {};
}

QString Looks::saveLive()
{
    flush();
    const auto origin = std::find_if(m_list.cbegin(), m_list.cend(), [this](const lookfile::Look &look) { return look.id == m_liveLook.origin; });
    const QString name = origin != m_list.cend() ? origin->name : QString();
    const QString error = lookfile::saveLive(m_workspace);
    if (error.isEmpty()) {
        SessionLog::write("looks", QStringLiteral("the live look, as it has been changed, saved as the look \"%1\" it was made from").arg(name));
        reload();
    }
    return error;
}

QVariantMap Looks::add(const QString &name, const QStringList &screenIds, const QString &copyOf)
{
    // (Whether it is the live look that is to be copied is asked before the look last
    // made live is written, which can give the live look its id.)
    const bool ofLive = !copyOf.isEmpty() && copyOf == m_liveLook.id;
    flush();
    QString id;
    const QString error = lookfile::add(m_workspace, name, screenIds, ofLive ? lookfile::liveLook() : copyOf, &id);
    if (error.isEmpty()) {
        SessionLog::write("looks", QStringLiteral("added the look \"%1\"%2").arg(name.trimmed(), ofLive ? QStringLiteral(", as the live look is")
                                                                                         : copyOf.isEmpty() ? QString() : QStringLiteral(", a copy of another")));
        reload();
    }
    return {{"id", id}, {"error", error}};
}

QString Looks::rename(const QString &id, const QString &name)
{
    flush();
    const QString error = lookfile::rename(m_workspace, id, name);
    if (error.isEmpty())
        reload();
    return error;
}

QString Looks::remove(const QString &id)
{
    flush();
    const QString error = lookfile::remove(m_workspace, id);
    if (error.isEmpty()) {
        SessionLog::write("looks", QStringLiteral("removed a look"));
        reload();
    }
    return error;
}

QString Looks::setScreen(const QString &id, const QString &screenId, const QVariantMap &changes)
{
    // The live look is named by its id, or by nothing where the workspace has none yet:
    // a change to it then makes it. (It is asked which this is before the look that
    // was last made live is written, since that can give the live look its id.)
    const bool live = id == m_liveLook.id;
    flush();
    lookfile::ScreenLook wanted = live ? m_liveLook.screens.value(screenId) : lookfile::ScreenLook();
    for (const lookfile::Look &look : std::as_const(m_list)) {
        if (!live && look.id == id)
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
    const QString error = live ? lookfile::setLiveScreen(m_workspace, screenId, wanted) : lookfile::setScreen(m_workspace, id, screenId, wanted);
    if (error.isEmpty())
        reload();
    return error;
}
