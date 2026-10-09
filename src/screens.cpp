#include "screens.h"

#include "sessionlog.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

namespace {

const QStringList outputKeys {QStringLiteral("output"), QStringLiteral("display"), QStringLiteral("ndiName"),
                              QStringLiteral("ndiWidth"), QStringLiteral("ndiHeight"), QStringLiteral("ndiRate")};
const QStringList outputKinds {QStringLiteral("window"), QStringLiteral("display"), QStringLiteral("ndi"), QStringLiteral("none")};

QString settingsKey(const QString &id, const QString &key)
{
    return QStringLiteral("screens/%1/%2").arg(id, key);
}

}

Screens::Screens(QObject *parent)
    : QObject(parent)
{
    readDisplays();
    const auto follow = [this] {
        readDisplays();
        rebuild();
        emit displaysChanged();
    };
    connect(qGuiApp, &QGuiApplication::screenAdded, this, follow);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, follow);
    m_list = screenfile::defaults();
    rebuild();
}

QStringList Screens::rates() const
{
    return {QStringLiteral("24"), QStringLiteral("25"), QStringLiteral("29.97"), QStringLiteral("30"), QStringLiteral("50"),
            QStringLiteral("59.94"), QStringLiteral("60")};
}

QVariantMap Screens::rateOf(const QString &rate) const
{
    const bool fractional = rate == QLatin1String("29.97") || rate == QLatin1String("59.94") || rate == QLatin1String("23.976");
    const int whole = qBound(1, qRound(rate.toDouble()), 120);
    return {{"numerator", fractional ? whole * 1000 : whole}, {"denominator", fractional ? 1001 : 1},
            {"perSecond", fractional ? whole * 1000.0 / 1001.0 : double(whole)}};
}

void Screens::readDisplays()
{
    m_displays.clear();
    const auto screens = QGuiApplication::screens();
    for (const QScreen *screen : screens) {
        const QSize size = screen->size() * screen->devicePixelRatio();
        const QString maker = (screen->manufacturer() + u' ' + screen->model()).trimmed();
        m_displays << QVariantMap {
            {"name", screen->name()},
            {"label", QStringLiteral("%1 (%2%3 x %4)").arg(screen->name(), maker.isEmpty() ? QString() : maker + QStringLiteral(", "))
                          .arg(size.width()).arg(size.height())},
            {"width", size.width()},
            {"height", size.height()},
        };
    }
}

QString Screens::open(const QString &workspace)
{
    m_workspace = workspace;
    const screenfile::Screens found = screenfile::read(workspace);
    m_list = found.screens;
    m_layouts.clear();
    for (auto layout = found.layouts.constBegin(); layout != found.layouts.constEnd(); ++layout)
        m_layouts.insert(layout.key(), layout.value());
    m_outputs.clear();
    if (m_remember) {
        QSettings settings;
        for (const screenfile::Screen &screen : std::as_const(m_list)) {
            QVariantMap kept;
            for (const QString &key : outputKeys) {
                const QVariant value = settings.value(settingsKey(screen.id, key));
                if (value.isValid())
                    kept.insert(key, value);
            }
            if (!kept.isEmpty())
                m_outputs.insert(screen.id, kept);
        }
    }
    rebuild();
    QStringList said;
    for (const QVariant &entry : std::as_const(m_screens)) {
        const QVariantMap screen = entry.toMap();
        const QString output = screen.value("output").toString();
        said << QStringLiteral("\"%1\" (%2) to %3").arg(screen.value("name").toString(), screen.value("kind").toString(),
                    output == QLatin1String("display") ? QStringLiteral("the display %1%2").arg(screen.value("display").toString(),
                                                             screen.value("displayThere").toBool() ? QString() : QStringLiteral(", which is not plugged in"))
                  : output == QLatin1String("ndi") ? QStringLiteral("NDI as \"%1\"").arg(screen.value("ndiName").toString())
                  : output == QLatin1String("none") ? QStringLiteral("nothing") : QStringLiteral("a window"));
    }
    SessionLog::write("screens", QStringLiteral("%1 %2: %3").arg(m_screens.size()).arg(found.fromFile ? QStringLiteral("in the workspace's set-up file")
                                                                                                        : QStringLiteral("(the workspace's file lists none, so the two every workspace has)"),
                                                                said.join(QStringLiteral("; "))));
    return found.error;
}

QVariantMap Screens::outputOf(const screenfile::Screen &screen, bool first) const
{
    const QVariantMap kept = m_outputs.value(screen.id);
    QString output = kept.value("output").toString();
    if (!outputKinds.contains(output))
        output = first ? QStringLiteral("window") : QStringLiteral("none");
    const QString display = kept.value("display").toString();
    bool there = false;
    for (const QVariant &entry : m_displays)
        there = there || entry.toMap().value("name").toString() == display;
    const QString rate = kept.value("ndiRate").toString();
    return {
        {"output", output},
        {"display", display},
        {"displayThere", there},
        {"ndiName", kept.value("ndiName", screen.name).toString()},
        {"ndiWidth", qBound(16, kept.value("ndiWidth", screen.width).toInt(), 7680)},
        {"ndiHeight", qBound(16, kept.value("ndiHeight", screen.height).toInt(), 4320)},
        {"ndiRate", rates().contains(rate) ? rate : QStringLiteral("30")},
    };
}

void Screens::rebuild()
{
    m_screens.clear();
    bool audienceSeen = false;
    bool stageSeen = false;
    for (const screenfile::Screen &screen : std::as_const(m_list)) {
        bool &seen = screen.stage ? stageSeen : audienceSeen;
        QVariantMap map = outputOf(screen, !seen);
        map.insert("id", screen.id);
        map.insert("name", screen.name);
        map.insert("kind", screen.stage ? QStringLiteral("stage") : QStringLiteral("audience"));
        map.insert("first", !seen);
        map.insert("width", screen.width);
        map.insert("height", screen.height);
        seen = true;
        m_screens << map;
    }
    emit changed();
}

QVariantList Screens::ofKind(bool stage) const
{
    QVariantList list;
    const QString kind = stage ? QStringLiteral("stage") : QStringLiteral("audience");
    for (const QVariant &screen : m_screens) {
        if (screen.toMap().value("kind").toString() == kind)
            list << screen;
    }
    return list;
}

QString Screens::add(const QString &kind)
{
    const bool stage = kind == QLatin1String("stage");
    // Named for its place among those of its kind, as far as that is not taken
    QStringList names;
    for (const screenfile::Screen &screen : std::as_const(m_list))
        names << screen.name;
    QString name;
    for (int number = 2; name.isEmpty() || names.contains(name); ++number)
        name = QStringLiteral("%1 %2").arg(stage ? QStringLiteral("Stage") : QStringLiteral("Audience")).arg(number);
    QString id;
    const QString error = screenfile::add(m_workspace, stage, name, &id);
    if (!error.isEmpty())
        return error;
    SessionLog::write("screens", QStringLiteral("added the %1 screen \"%2\"").arg(kind, name));
    m_list = screenfile::read(m_workspace).screens;
    rebuild();
    return {};
}

QString Screens::remove(const QString &id)
{
    const QString error = screenfile::remove(m_workspace, id);
    if (!error.isEmpty())
        return error;
    SessionLog::write("screens", QStringLiteral("removed a screen"));
    m_outputs.remove(id);
    if (m_remember)
        QSettings().remove(QStringLiteral("screens/%1").arg(id));
    const screenfile::Screens found = screenfile::read(m_workspace);
    m_list = found.screens;
    m_layouts.clear();
    for (auto layout = found.layouts.constBegin(); layout != found.layouts.constEnd(); ++layout)
        m_layouts.insert(layout.key(), layout.value());
    rebuild();
    return {};
}

QString Screens::rename(const QString &id, const QString &name)
{
    const QString error = screenfile::rename(m_workspace, id, name);
    if (!error.isEmpty())
        return error;
    m_list = screenfile::read(m_workspace).screens;
    rebuild();
    return {};
}

void Screens::setOutput(const QString &id, const QVariantMap &changes)
{
    QVariantMap kept = m_outputs.value(id);
    QSettings settings;
    for (auto change = changes.constBegin(); change != changes.constEnd(); ++change) {
        if (!outputKeys.contains(change.key()))
            continue;
        kept.insert(change.key(), change.value());
        if (m_remember)
            settings.setValue(settingsKey(id, change.key()), change.value());
    }
    m_outputs.insert(id, kept);
    rebuild();
}
