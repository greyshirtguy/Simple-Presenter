#include "screens.h"

#include "decklinkformats.h"
#include "delayline.h"
#include "framedelay.h"
#include "sessionlog.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSettings>

namespace {

const QStringList outputKeys {QStringLiteral("output"), QStringLiteral("display"), QStringLiteral("ndiName"),
                              QStringLiteral("ndiWidth"), QStringLiteral("ndiHeight"), QStringLiteral("ndiRate"),
                              QStringLiteral("decklinkOutput"), QStringLiteral("decklinkName"), QStringLiteral("decklinkFormat"),
                              QStringLiteral("decklinkAlpha"), QStringLiteral("decklinkKey"), QStringLiteral("decklinkBlend"),
                              QStringLiteral("videoDelay")};
const QStringList outputKinds {QStringLiteral("window"), QStringLiteral("display"), QStringLiteral("ndi"), QStringLiteral("decklink"),
                               QStringLiteral("none")};

QString settingsKey(const QString &id, const QString &key)
{
    return QStringLiteral("screens/%1/%2").arg(id, key);
}

// How a part of a screen of several outputs is known, here and in the operator window:
// the screen's id for its first, which is the screen's own
QString partKey(const QString &id, int part)
{
    return part == 0 ? id : QStringLiteral("%1/part%2").arg(id).arg(part);
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
            {"refresh", screen->refreshRate()},
        };
    }
}

void Screens::setShowStatistics(bool show)
{
    if (show == m_showStatistics)
        return;
    m_showStatistics = show;
    if (m_remember) {
        QSettings settings;
        if (show)
            settings.setValue("screens/showStatistics", true);
        else
            settings.remove("screens/showStatistics");
    }
    emit showStatisticsChanged();
}

int Screens::mostVideoDelay() const
{
    return delayline::kMostFrames;
}

bool Screens::videoDelayPossible() const
{
    return FrameDelay::possible();
}

double Screens::refreshIntervalOf(const QString &display) const
{
    for (const QScreen *screen : QGuiApplication::screens()) {
        if (screen->name() == display && screen->refreshRate() > 0)
            return 1000.0 / screen->refreshRate();
    }
    return 1000.0 / 60;
}

double Screens::frameIntervalOf(const QVariantMap &screen) const
{
    const QString output = screen.value("output").toString();
    if (output == QLatin1String("ndi")) {
        const QVariantMap rate = rateOf(screen.value("ndiRate").toString());
        return 1000.0 * rate.value("denominator").toDouble() / qMax(1.0, rate.value("numerator").toDouble());
    }
    if (output == QLatin1String("decklink")) {
        const decklinkformats::Format *format = decklinkformats::formatNamed(screen.value("decklinkFormat").toString());
        return format ? decklinkformats::period(*format) / 1e6 : 1000.0 / 30;
    }
    double refresh = 0;
    for (const QVariant &entry : m_displays) {
        const QVariantMap display = entry.toMap();
        if (refresh <= 0 || (output == QLatin1String("display") && display.value("name") == screen.value("display")))
            refresh = display.value("refresh").toDouble();
    }
    return 1000.0 / (refresh > 0 ? refresh : 60.0);
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
        const bool show = settings.value("screens/showStatistics", false).toBool();
        if (show != m_showStatistics) {
            m_showStatistics = show;
            emit showStatisticsChanged();
        }
        for (const screenfile::Screen &screen : std::as_const(m_list)) {
            for (int part = 0; part < screen.columns * screen.rows; ++part) {
                QVariantMap kept;
                for (const QString &key : outputKeys) {
                    const QVariant value = settings.value(settingsKey(partKey(screen.id, part), key));
                    if (value.isValid())
                        kept.insert(key, value);
                }
                if (!kept.isEmpty())
                    m_outputs.insert(partKey(screen.id, part), kept);
            }
        }
    }
    rebuild();
    QStringList said;
    for (const QVariant &entry : std::as_const(m_screens)) {
        for (const QVariant &partEntry : entry.toMap().value("parts").toList()) {
            // (A screen of several outputs is said a part at a time)
            QVariantMap screen = partEntry.toMap();
            const QVariantMap whole = entry.toMap();
            screen.insert("name", whole.value("parts").toList().size() > 1
                                      ? QStringLiteral("%1, part %2 of %3").arg(whole.value("name").toString()).arg(screen.value("part").toInt() + 1)
                                            .arg(whole.value("parts").toList().size())
                                      : whole.value("name").toString());
            screen.insert("kind", whole.value("kind"));
            const QString output = screen.value("output").toString();
            said << QStringLiteral("\"%1\" (%2) to %3").arg(screen.value("name").toString(), screen.value("kind").toString(),
                        output == QLatin1String("display") ? QStringLiteral("the display %1%2").arg(screen.value("display").toString(),
                                                                 screen.value("displayThere").toBool() ? QString() : QStringLiteral(", which is not plugged in"))
                      : output == QLatin1String("ndi") ? QStringLiteral("NDI as \"%1\"").arg(screen.value("ndiName").toString())
                      : output == QLatin1String("decklink") ? QStringLiteral("the Blackmagic output \"%1\" as %2%3").arg(screen.value("decklinkName").toString(),
                                                                  screen.value("decklinkFormat").toString(),
                                                                  screen.value("decklinkAlpha").toString() == QLatin1String("none") ? QString()
                                                                  : QStringLiteral(", keyed %1ly, %2").arg(screen.value("decklinkKey").toString(),
                                                                                                           screen.value("decklinkAlpha").toString()))
                      : output == QLatin1String("none") ? QStringLiteral("nothing") : QStringLiteral("a window"));
        }
    }
    SessionLog::write("screens", QStringLiteral("%1 %2: %3").arg(m_screens.size()).arg(found.fromFile ? QStringLiteral("in the workspace's set-up file")
                                                                                                        : QStringLiteral("(the workspace's file lists none, so the two every workspace has)"),
                                                                said.join(QStringLiteral("; "))));
    return found.error;
}

QVariantMap Screens::outputOf(const screenfile::Screen &screen, bool first, int part) const
{
    const QVariantMap kept = m_outputs.value(partKey(screen.id, part));
    QString output = kept.value("output").toString();
    if (!outputKinds.contains(output))
        output = first && part == 0 ? QStringLiteral("window") : QStringLiteral("none");
    // (A part is its share of the screen: an NDI one is that size, and named for its place)
    const int partWidth = screen.width / qMax(1, screen.columns);
    const int partHeight = screen.height / qMax(1, screen.rows);
    const QString partName = part == 0 ? screen.name : QStringLiteral("%1 %2").arg(screen.name).arg(part + 1);
    const QString display = kept.value("display").toString();
    bool there = false;
    for (const QVariant &entry : m_displays)
        there = there || entry.toMap().value("name").toString() == display;
    const QString rate = kept.value("ndiRate").toString();
    const QString format = kept.value("decklinkFormat").toString();
    const QString alpha = kept.value("decklinkAlpha").toString();
    static const QStringList alphas {QStringLiteral("none"), QStringLiteral("premultiplied"), QStringLiteral("straight")};
    return {
        {"output", output},
        {"display", display},
        {"displayThere", there},
        {"ndiName", kept.value("ndiName", partName).toString()},
        {"ndiWidth", qBound(16, kept.value("ndiWidth", partWidth).toInt(), 7680)},
        {"ndiHeight", qBound(16, kept.value("ndiHeight", partHeight).toInt(), 4320)},
        {"ndiRate", rates().contains(rate) ? rate : QStringLiteral("30")},
        {"decklinkOutput", kept.value("decklinkOutput").toString()},
        {"decklinkName", kept.value("decklinkName").toString()},
        {"decklinkFormat", decklinkformats::formatNamed(format) ? format : QStringLiteral("1080p29.97")},
        {"decklinkAlpha", alphas.contains(alpha) ? alpha : QStringLiteral("none")},
        {"decklinkKey", kept.value("decklinkKey").toString() == QLatin1String("internal") ? QStringLiteral("internal") : QStringLiteral("external")},
        {"decklinkBlend", qBound(0, kept.value("decklinkBlend", 100).toInt(), 100)},
        {"videoDelay", qBound(0, kept.value("videoDelay", 0).toInt(), delayline::kMostFrames)},
        {"part", part},
        {"key", partKey(screen.id, part)},
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
        map.insert("columns", screen.columns);
        map.insert("rows", screen.rows);
        QVariantList parts {outputOf(screen, !seen, 0)};
        for (int part = 1; part < screen.columns * screen.rows; ++part)
            parts << outputOf(screen, !seen, part);
        map.insert("parts", parts);
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
    for (int part = 0; part < screenfile::mostParts * screenfile::mostParts; ++part)
        m_outputs.remove(partKey(id, part));
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
    setPartOutput(id, 0, changes);
}

void Screens::setPartOutput(const QString &id, int part, const QVariantMap &changes)
{
    const QString key = partKey(id, qMax(0, part));
    QVariantMap kept = m_outputs.value(key);
    QSettings settings;
    for (auto change = changes.constBegin(); change != changes.constEnd(); ++change) {
        if (!outputKeys.contains(change.key()))
            continue;
        kept.insert(change.key(), change.value());
        if (m_remember)
            settings.setValue(settingsKey(key, change.key()), change.value());
    }
    m_outputs.insert(key, kept);
    rebuild();
}

QString Screens::setArrangement(const QString &id, int columns, int rows)
{
    const QString error = screenfile::arrange(m_workspace, id, columns, rows);
    if (!error.isEmpty())
        return error;
    m_list = screenfile::read(m_workspace).screens;
    // What was set here for parts the screen no longer has
    int count = 1;
    QString name;
    for (const screenfile::Screen &screen : std::as_const(m_list)) {
        if (screen.id == id) {
            count = screen.columns * screen.rows;
            name = screen.name;
        }
    }
    QSettings settings;
    for (int part = count; part < screenfile::mostParts * screenfile::mostParts; ++part) {
        m_outputs.remove(partKey(id, part));
        if (m_remember)
            settings.remove(QStringLiteral("screens/%1").arg(partKey(id, part)));
    }
    SessionLog::write("screens", count == 1 ? QStringLiteral("\"%1\" made a screen of one output").arg(name)
                                            : QStringLiteral("\"%1\" made a screen of %2 outputs, %3 across and %4 down")
                                                  .arg(name).arg(count).arg(qMax(1, columns)).arg(qMax(1, rows)));
    rebuild();
    return {};
}
