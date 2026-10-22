#include "playbackmarkers.h"

#include <QSet>
#include <QStringList>

#include <algorithm>

namespace playbackmarkers {

using Marker = rv::data::Action::MediaType::PlaybackMarker;

namespace {

QColor toColor(const rv::data::Color &color)
{
    return QColor::fromRgbF(float(color.red()), float(color.green()), float(color.blue()), float(color.alpha()));
}

void setColor(rv::data::Color *to, const QColor &color)
{
    to->set_red(color.redF());
    to->set_green(color.greenF());
    to->set_blue(color.blueF());
    to->set_alpha(color.alphaF());
}

Marker *find(rv::data::Action *media, const QString &id)
{
    for (Marker &marker : *media->mutable_media()->mutable_markers()) {
        if (QString::fromStdString(marker.uuid().string()) == id)
            return &marker;
    }
    return nullptr;
}

// The markers kept in the order of their times, those at the same time as they were
void order(rv::data::Action *media)
{
    auto *markers = media->mutable_media()->mutable_markers();
    std::stable_sort(markers->begin(), markers->end(), [](const Marker &a, const Marker &b) { return a.time() < b.time(); });
}

const QString notThere = QStringLiteral("That media no longer has that marker");

} // namespace

bool takes(const rv::data::Action &media)
{
    return media.has_media() && (media.media().element().has_video() || media.media().element().has_audio());
}

QVariantList palette()
{
    QVariantList list;
    const std::pair<const char *, const char *> colours[] = {{"Red", "#e53935"}, {"Orange", "#fb8c00"}, {"Yellow", "#fdd835"},
                                                            {"Green", "#43a047"}, {"Blue", "#3d8be0"}, {"Purple", "#8e24aa"},
                                                            {"Pink", "#d81b60"}, {"White", "#ffffff"}};
    for (const auto &[name, colour] : colours)
        list.append(QVariantMap {{"name", QString::fromLatin1(name)}, {"color", QString::fromLatin1(colour)}});
    return list;
}

QColor newColor(const rv::data::Action &media, quint32 roll)
{
    QSet<QString> taken;
    if (media.has_media()) {
        for (const Marker &marker : media.media().markers())
            taken.insert(marker.has_color() ? toColor(marker.color()).name(QColor::HexRgb) : unsetColor().name(QColor::HexRgb));
    }
    QStringList all, free;
    for (const QVariant &entry : palette()) {
        const QString colour = entry.toMap().value("color").toString();
        // (White is for picking: on the bar's light grey it is hardly seen.)
        if (colour == QLatin1String("#ffffff"))
            continue;
        all.append(colour);
        if (!taken.contains(colour))
            free.append(colour);
    }
    const QStringList &from = free.isEmpty() ? all : free;
    return QColor(from.at(int(roll % quint32(from.size()))));
}

QColor unsetColor()
{
    return QColor(QStringLiteral("#3d8be0"));
}

int pick(const QVariantList &markers, double position, int which, const QString &name)
{
    const int count = int(markers.size());
    switch (which) {
    case 0:
        return count > 0 ? 0 : -1;
    case 1: {
        int found = -1;
        for (int i = 0; i < count; ++i) {
            if (markers.at(i).toMap().value("time").toDouble() <= position)
                found = i;
        }
        return found;
    }
    case 2:
        for (int i = 0; i < count; ++i) {
            if (markers.at(i).toMap().value("time").toDouble() > position)
                return i;
        }
        return -1;
    case 3:
        return count - 1;
    case 4:
        for (int i = 0; i < count; ++i) {
            if (markers.at(i).toMap().value("name").toString() == name)
                return i;
        }
        return -1;
    default:
        return -1;
    }
}

QVariantList describe(const rv::data::Action &media, const std::function<QVariantMap(const rv::data::Action &)> &describeAction)
{
    QVariantList list;
    if (!media.has_media())
        return list;
    for (const Marker &marker : media.media().markers()) {
        QVariantList actions;
        for (const rv::data::Action &action : marker.actions())
            actions.append(describeAction ? describeAction(action) : QVariantMap {});
        list.append(QVariantMap {
            {"id", QString::fromStdString(marker.uuid().string())},
            {"time", marker.time()},
            {"color", (marker.has_color() ? toColor(marker.color()) : unsetColor()).name(QColor::HexRgb)},
            {"name", QString::fromStdString(marker.name())},
            {"actions", actions},
        });
    }
    // (In the order of their times, whatever order the file has them in)
    std::stable_sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("time").toDouble() < b.toMap().value("time").toDouble();
    });
    return list;
}

QString add(rv::data::Action *media, double time, const std::function<std::string()> &newId, QString *made, quint32 roll)
{
    if (!takes(*media))
        return QStringLiteral("Only a video or a piece of audio can have markers");
    const QColor colour = newColor(*media, roll);
    Marker *marker = media->mutable_media()->add_markers();
    marker->mutable_uuid()->set_string(newId());
    marker->set_time(qMax(0.0, time));
    setColor(marker->mutable_color(), colour);
    if (made)
        *made = QString::fromStdString(marker->uuid().string());
    order(media);
    return {};
}

QString change(rv::data::Action *media, const QString &id, const QVariantMap &changes)
{
    Marker *marker = find(media, id);
    if (!marker)
        return notThere;
    if (changes.contains("name") && marker->name() != changes.value("name").toString().toStdString())
        marker->set_name(changes.value("name").toString().toStdString());
    if (changes.contains("color")) {
        const QColor wanted(changes.value("color").toString());
        if (!wanted.isValid())
            return QStringLiteral("That is not a colour");
        if (!marker->has_color() || toColor(marker->color()) != wanted)
            setColor(marker->mutable_color(), wanted);
    }
    if (changes.contains("time")) {
        const double time = qMax(0.0, changes.value("time").toDouble());
        if (marker->time() != time) {
            marker->set_time(time);
            order(media);
        }
    }
    return {};
}

QString remove(rv::data::Action *media, const QString &id)
{
    auto *markers = media->mutable_media()->mutable_markers();
    for (int i = 0; i < markers->size(); ++i) {
        if (QString::fromStdString(markers->Get(i).uuid().string()) == id) {
            markers->DeleteSubrange(i, 1);
            return {};
        }
    }
    return notThere;
}

QString duplicate(rv::data::Action *media, const QString &id, const std::function<std::string()> &newId, QString *made)
{
    const Marker *marker = find(media, id);
    if (!marker)
        return notThere;
    Marker copy = *marker;
    copy.mutable_uuid()->set_string(newId());
    for (rv::data::Action &action : *copy.mutable_actions())
        action.mutable_uuid()->set_string(newId());
    if (made)
        *made = QString::fromStdString(copy.uuid().string());
    *media->mutable_media()->add_markers() = std::move(copy);
    order(media);
    return {};
}

QString addAction(rv::data::Action *media, const QString &id, const rv::data::Action &action)
{
    Marker *marker = find(media, id);
    if (!marker)
        return notThere;
    *marker->add_actions() = action;
    return {};
}

QString changeAction(rv::data::Action *media, const QString &id, const QString &actionId,
                     const std::function<QString(rv::data::Action *)> &build)
{
    Marker *marker = find(media, id);
    if (!marker)
        return notThere;
    for (rv::data::Action &action : *marker->mutable_actions()) {
        if (QString::fromStdString(action.uuid().string()) == actionId)
            return build(&action);
    }
    return QStringLiteral("That marker no longer has that action");
}

QString removeAction(rv::data::Action *media, const QString &id, const QString &actionId)
{
    Marker *marker = find(media, id);
    if (!marker)
        return notThere;
    for (int i = 0; i < marker->actions_size(); ++i) {
        if (QString::fromStdString(marker->actions(i).uuid().string()) == actionId) {
            marker->mutable_actions()->DeleteSubrange(i, 1);
            return {};
        }
    }
    return QStringLiteral("That marker no longer has that action");
}

} // namespace playbackmarkers
