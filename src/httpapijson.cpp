#include "httpapijson.h"

#include <QHash>
#include <QUuid>

namespace httpapijson {

QVariantMap id(const QString &uuid, const QString &name, qint64 index)
{
    return {{"uuid", uuid}, {"name", name}, {"index", double(index)}};
}

QVariantMap color(const QColor &color)
{
    return {{"red", color.redF()}, {"green", color.greenF()}, {"blue", color.blueF()}, {"alpha", color.alphaF()}};
}

namespace {

QVariantMap slideOf(const QVariantMap &slide)
{
    return {{"text", slide.value("plainText").toString()},
            {"notes", slide.value("notesText").toString()},
            {"uuid", slide.value("id").toString()}};
}

}

QVariantMap slideStatus(const QVariantMap &current, const QVariantMap &next)
{
    return {{"current", slideOf(current)}, {"next", slideOf(next)}};
}

QVariantMap slideIndex(const QString &key, const QString &uuid, const QString &name, qint64 listIndex, int index, int total)
{
    if (index < 0)
        return {{key, QVariant::fromValue(nullptr)}};
    return {{key, QVariantMap {{"index", index},
                               {"presentation_id", id(uuid, name, listIndex)},
                               {"total_cues", total},
                               {"remaining_cues", qMax(0, total - index - 1)}}}};
}

QString libraryUuid(const QString &name)
{
    // (A namespace of this app's own, for UUIDs made from names)
    static const QUuid space = QUuid::fromString(QStringLiteral("5D1A0E00-51DE-4A0D-8000-0000000000A1"));
    return QUuid::createUuidV5(space, name).toString(QUuid::WithoutBraces).toUpper();
}

QVariantList libraries(const QStringList &names)
{
    QVariantList made;
    for (const QString &name : names)
        made << id(libraryUuid(name), name, made.size());
    return made;
}

namespace {

QVariantList childrenOf(const QVariantList &nodes, const QString &parent)
{
    QVariantList made;
    for (const QVariant &entry : nodes) {
        const QVariantMap node = entry.toMap();
        if (node.value("parent").toString() != parent)
            continue;
        const bool folder = node.value("folder").toBool();
        made << QVariantMap {{"id", id(node.value("path").toString(), node.value("name").toString(), made.size())},
                             {"field_type", folder ? QStringLiteral("group") : QStringLiteral("playlist")},
                             {"children", folder ? childrenOf(nodes, node.value("path").toString()) : QVariantList()}};
    }
    return made;
}

}

QVariantList playlistTree(const QVariantList &nodes)
{
    return childrenOf(nodes, QString());
}

QString timerTime(double seconds)
{
    const bool past = seconds < -0.0001;
    const qint64 whole = qint64(qAbs(seconds));
    return (past ? QStringLiteral("-") : QString())
        + QStringLiteral("%1:%2:%3").arg(whole / 3600, 2, 10, QLatin1Char('0')).arg(whole / 60 % 60, 2, 10, QLatin1Char('0'))
              .arg(whole % 60, 2, 10, QLatin1Char('0'));
}

QString timerState(const QVariantMap &timer, const QVariantMap &state)
{
    const bool running = state.value("running").toBool();
    const double seconds = state.value("seconds").toDouble();
    const QString kind = timer.value("kind").toString();
    if (kind == QLatin1String("elapsed")) {
        const bool ended = timer.value("hasEndTime").toBool() && seconds >= timer.value("endTime").toDouble();
        return running ? QStringLiteral("running") : ended ? QStringLiteral("complete") : QStringLiteral("stopped");
    }
    // A countdown, to nothing or to a time of day: what it shows is the time left
    if (running)
        return seconds < 0 ? QStringLiteral("overrunning") : QStringLiteral("running");
    if (seconds < 0)
        return QStringLiteral("overran");
    return seconds < 0.5 ? QStringLiteral("complete") : QStringLiteral("stopped");
}

QVariantMap timer(const QVariantMap &timer, int index)
{
    QVariantMap made {{"id", id(timer.value("id").toString(), timer.value("name").toString(), index)},
                      {"allows_overrun", timer.value("overrun").toBool()}};
    const QString kind = timer.value("kind").toString();
    if (kind == QLatin1String("countdownTo")) {
        made.insert("count_down_to_time", QVariantMap {{"time_of_day", qRound64(timer.value("timeOfDay").toDouble())},
                                                       {"period", QStringLiteral("24_hour")}});
    } else if (kind == QLatin1String("elapsed")) {
        QVariantMap elapsed {{"start_time", qRound64(timer.value("startTime").toDouble())}};
        if (timer.value("hasEndTime").toBool())
            elapsed.insert("end_time", qRound64(timer.value("endTime").toDouble()));
        made.insert("elapsed", elapsed);
    } else {
        made.insert("countdown", QVariantMap {{"duration", qRound64(timer.value("duration").toDouble())}});
    }
    return made;
}

QVariantMap timerCurrent(const QVariantMap &timer, const QVariantMap &state, int index)
{
    return {{"id", id(timer.value("id").toString(), timer.value("name").toString(), index)},
            {"time", timerTime(state.value("seconds").toDouble())},
            {"state", timerState(timer, state)}};
}

QVariantMap look(const QVariantMap &look, const QStringList &screenIds, const QString &name, int index)
{
    const QVariantMap screens = look.value("screens").toMap();
    QVariantList made;
    for (const QString &screenId : screenIds) {
        // (A screen the look has nothing for gets everything, with no theme and no mask.)
        const QVariantMap line = screens.value(screenId).toMap();
        const auto on = [&line](const char *key) { return !line.contains(key) || line.value(key).toBool(); };
        made << QVariantMap {{"video_input", on("videoInput")}, {"media", on("media")}, {"slide", on("slide")},
                             {"announcements", on("announcements")}, {"props", on("props")}, {"messages", on("messages")},
                             {"presentation", line.value("theme").toString()}, {"mask", line.value("mask").toString()}};
    }
    return {{"id", id(look.value("id").toString(), name, index)}, {"screens", made}};
}

QVariantMap prop(const QVariantMap &prop, int index, bool active)
{
    return {{"id", id(prop.value("id").toString(), prop.value("name").toString(), index)},
            {"is_active", active},
            {"auto_clear_enabled", false},
            {"auto_clear_duration", 0},
            {"transition", QVariant::fromValue(nullptr)}};
}

QVariantMap message(const QVariantMap &message, int index, bool active)
{
    // Its words, each token standing in them by its name, as ProPresenter writes them
    const QVariantList tokens = message.value("tokens").toList();
    QHash<QString, QString> names;
    QVariantList madeTokens;
    for (const QVariant &entry : tokens) {
        const QVariantMap token = entry.toMap();
        const QString name = token.value("name").toString();
        names.insert(token.value("id").toString(), name);
        const QString kind = token.value("kind").toString();
        if (kind == QLatin1String("timer"))
            madeTokens << QVariantMap {{"name", name}, {"timer", QVariantMap {{"id", id(token.value("timerId").toString(), name, 0)}}}};
        else if (kind == QLatin1String("clock"))
            madeTokens << QVariantMap {{"name", name}, {"clock", QVariantMap {{"format", QVariantMap {{"is_24_hours", token.value("twentyFour").toBool()}}}}}};
        else
            madeTokens << QVariantMap {{"name", name}, {"text", QVariantMap {{"text", token.value("value").toString()}}}};
    }
    QString words;
    for (const QVariant &entry : message.value("pieces").toList()) {
        const QVariantMap piece = entry.toMap();
        words += piece.value("token").toBool() ? QStringLiteral("${%1}").arg(names.value(piece.value("text").toString()))
                                               : piece.value("text").toString();
    }
    return {{"id", id(message.value("id").toString(), message.value("name").toString(), index)},
            {"message", words},
            {"tokens", madeTokens},
            {"theme", id(QString(), message.value("theme").toString(), 0)},
            {"visible_on_network", false},
            {"is_active", active}};
}

namespace {

// clearfile's names for the layers, and ProPresenter's in its API
QString layerName(const QString &ours)
{
    static const QHash<QString, QString> names {{"audio", "music"}, {"audioEffects", "audio_effects"}, {"messages", "messages"},
                                                {"props", "props"}, {"announcements", "announcements"}, {"slide", "presentation"},
                                                {"media", "presentation_media"}, {"videoInput", "video_input"}};
    return names.value(ours, ours);
}

// clearfile's names for the pictures, and ProPresenter's
QString iconName(const QVariantMap &group)
{
    if (!group.value("image").toString().isEmpty())
        return QStringLiteral("Custom");
    static const QHash<QString, QString> words {{"0", "Zero"}, {"1", "One"}, {"2", "Two"}, {"3", "Three"}, {"4", "Four"}, {"5", "Five"},
                                                {"6", "Six"}, {"7", "Seven"}, {"8", "Eight"}, {"9", "Nine"}, {"Megaphone", "Megahorn"}};
    const QString picture = group.value("picture").toString();
    return words.value(picture, picture);
}

}

QVariantMap clearGroup(const QVariantMap &group, int index)
{
    QStringList layers;
    for (const QVariant &layer : group.value("layers").toList())
        layers << layerName(layer.toString());
    const QColor tint(group.value("tint").toString());
    return {{"id", id(group.value("id").toString(), group.value("name").toString(), index)},
            {"icon", iconName(group)},
            {"tint", group.value("tinted").toBool() && tint.isValid() ? QVariant(color(tint)) : QVariant::fromValue(nullptr)},
            {"layers", layers},
            {"stop_timeline_announcements", false},
            {"stop_timeline_presentation", false},
            {"clear_next_presentation", false}};
}

QVariantMap macro(const QVariantMap &macro, int index)
{
    static const QHash<QString, QString> types {{"timer", "timer"}, {"clear", "clear"}, {"clearGroup", "clear_group"},
                                                {"stage", "stage_layout"}, {"prop", "prop"}, {"look", "audience_look"},
                                                {"macro", "macro"}, {"audio", "audio_bin_playlist"}};
    QVariantList actions;
    for (const QVariant &entry : macro.value("actions").toList())
        actions << QVariantMap {{"type", types.value(entry.toMap().value("kind").toString(), QStringLiteral("unknown"))}};
    const QColor tint(macro.value("color").toString());
    return {{"id", id(macro.value("id").toString(), macro.value("name").toString(), index)},
            {"color", color(tint.isValid() ? tint : QColor(Qt::white))},
            {"image_type", QStringLiteral("Default")},
            {"actions", actions}};
}

namespace {

QVariantList binChildrenOf(const QVariantList &nodes, const QString &parent)
{
    QVariantList made;
    for (const QVariant &entry : nodes) {
        const QVariantMap node = entry.toMap();
        if (node.value("parent").toString() != parent)
            continue;
        const bool folder = node.value("folder").toBool();
        made << QVariantMap {{"id", id(node.value("path").toString(), node.value("name").toString(), made.size())},
                             {"type", folder ? QStringLiteral("group") : QStringLiteral("playlist")},
                             {"children", folder ? binChildrenOf(nodes, node.value("path").toString()) : QVariantList()}};
    }
    return made;
}

}

QVariantList binTree(const QVariantList &nodes)
{
    return binChildrenOf(nodes, QString());
}

QVariantMap binItem(const QVariantMap &row, int index, bool audio)
{
    const QString type = audio ? QStringLiteral("audio") : row.value("video").toBool() ? QStringLiteral("video") : QStringLiteral("image");
    return {{"id", id(row.value("id").toString(), row.value("name").toString(), index)},
            {"type", type},
            {"artist", QString()},
            {"duration", qRound64(row.value("duration").toDouble())}};
}

QString videoCountdown(double seconds)
{
    const qint64 whole = seconds > 0 ? qint64(seconds) : 0;
    return QStringLiteral("%1:%2:%3").arg(whole / 3600).arg(whole / 60 % 60, 2, 10, QLatin1Char('0')).arg(whole % 60, 2, 10, QLatin1Char('0'));
}

}
