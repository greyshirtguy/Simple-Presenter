#include "clearfile.h"

#include "clearGroups.pb.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <functional>

namespace clearfile {

namespace {

using Document = rv::data::ClearGroupsDocument;
using FileGroup = rv::data::ClearGroupsDocument::ClearGroup;
using Clear = rv::data::Action::ClearType;
using rv::data::Action;

// The layers, in ProPresenter's order (see clearfile.h). `ours` is whether this app has
// the layer, and so whether a group's having it can be changed here.
//
// (The audio and the audio effects are the two halves of this app's audio layer: see
// State::clearGroup, src/showstate.cpp, for which half is which.)
//
// TO COME: when the app gains the one it has not (video inputs), its `ours` below
// becomes true, which is all the window the groups are set up in goes by
// (ClearGroupsPanel.qml). Two things go with that: the show is to clear the layer for
// a group that has it (State::clearGroup, src/showstate.cpp), and the layer wants a
// clear button of its own (PreviewPanel.qml).
struct Layer
{
    const char *key;
    Clear::ClearTargetLayer target;
    Action::ContentDestination destination;
    bool ours;
};
const Layer kLayers[] = {
    {"audio", Clear::CLEAR_TARGET_LAYER_AUDIO, Action::CONTENT_DESTINATION_GLOBAL, true},
    {"audioEffects", Clear::CLEAR_TARGET_LAYER_AUDIO_EFFECTS, Action::CONTENT_DESTINATION_GLOBAL, true},
    {"messages", Clear::CLEAR_TARGET_LAYER_MESSAGES, Action::CONTENT_DESTINATION_GLOBAL, true},
    {"props", Clear::CLEAR_TARGET_LAYER_PROP, Action::CONTENT_DESTINATION_GLOBAL, true},
    {"announcements", Clear::CLEAR_TARGET_LAYER_ALL, Action::CONTENT_DESTINATION_ANNOUNCEMENTS, true},
    {"slide", Clear::CLEAR_TARGET_LAYER_SLIDE, Action::CONTENT_DESTINATION_GLOBAL, true},
    {"media", Clear::CLEAR_TARGET_LAYER_BACKGROUND, Action::CONTENT_DESTINATION_GLOBAL, true},
    {"videoInput", Clear::CLEAR_TARGET_LAYER_LIVE_VIDEO, Action::CONTENT_DESTINATION_GLOBAL, false},
};
const int kLayerCount = int(sizeof kLayers / sizeof kLayers[0]);

// What each of ProPresenter's pictures is of, by its number in the file
const char *const kPictures[] = {
    "", "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "All", "Megaphone", "Play", "Bulb", "Sunglasses", "Arrow",
    "Target", "Star", "Sun", "Bell", "Paperclip", "Flask", "Eyeglasses", "Cupcake", "Slide", "Hat", "Flower", "Heart",
    "Message", "Audio", "Cloud", "Exclamation",
};
const int kPictureCount = int(sizeof kPictures / sizeof kPictures[0]) - 1;

// The id of the group a workspace with no file is taken to have: this app's own, the
// same everywhere (see the guesses in clearfile.h).
const char kStandardId[] = "936CE7CA-CDF7-4D72-A663-42F427823B0E";

QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/ClearGroups");
}

// Which of the layers a target of the file is: its place in kLayers, or -1 for one that
// is none of them.
int layerOf(const Clear &target)
{
    for (int i = 0; i < kLayerCount; ++i) {
        if (target.target_layer() == kLayers[i].target && target.content_destination() == kLayers[i].destination)
            return i;
    }
    return -1;
}

int layerOf(const QString &key)
{
    for (int i = 0; i < kLayerCount; ++i) {
        if (key == QLatin1String(kLayers[i].key))
            return i;
    }
    return -1;
}

QString hexOf(const rv::data::Color &color)
{
    const auto part = [](float v) { return qBound(0, qRound(v * 255), 255); };
    return QStringLiteral("#%1%2%3").arg(part(color.red()), 2, 16, QLatin1Char('0'))
        .arg(part(color.green()), 2, 16, QLatin1Char('0')).arg(part(color.blue()), 2, 16, QLatin1Char('0'));
}

// Writes "#rrggbb" as a colour that is wholly there. Answers whether it was one.
bool setColor(rv::data::Color *color, const QString &hex)
{
    bool isColor = hex.size() == 7 && hex.startsWith(u'#');
    int parts[3] = {0, 0, 0};
    for (int i = 0; isColor && i < 3; ++i)
        parts[i] = hex.mid(1 + 2 * i, 2).toInt(&isColor, 16);
    if (!isColor)
        return false;
    color->set_red(float(parts[0] / 255.0));
    color->set_green(float(parts[1] / 255.0));
    color->set_blue(float(parts[2] / 255.0));
    color->set_alpha(1);
    return true;
}

Group read(const FileGroup &from)
{
    Group group;
    group.id = QString::fromStdString(from.uuid().string());
    group.name = QString::fromStdString(from.name());
    for (const Clear &target : from.layer_targets()) {
        const int layer = layerOf(target);
        if (layer >= 0 && !group.layers.contains(QLatin1String(kLayers[layer].key)))
            group.layers.append(QLatin1String(kLayers[layer].key));
    }
    group.icon = int(from.image_type());
    group.image = QByteArray::fromStdString(from.image_data());
    group.tinted = from.is_icon_tinted();
    if (from.has_icon_tint_color() && from.icon_tint_color().alpha() > 0)
        group.tint = hexOf(from.icon_tint_color());
    group.hidden = from.is_hidden_in_preview();
    return group;
}

// ProPresenter's own group, as the file looked at has it, field for field
void writeStandard(FileGroup *group)
{
    group->mutable_uuid()->set_string(kStandardId);
    group->set_name("Clear All");
    for (const Layer &layer : kLayers) {
        Clear *target = group->add_layer_targets();
        // (What is nought is left unwritten, as the file leaves it: the announcements
        // have a destination and no layer, the others a layer and no destination.)
        if (layer.target != Clear::CLEAR_TARGET_LAYER_ALL)
            target->set_target_layer(layer.target);
        if (layer.destination != Action::CONTENT_DESTINATION_GLOBAL)
            target->set_content_destination(layer.destination);
    }
    group->set_image_type(FileGroup::ImageTypeAll);
    setColor(group->mutable_icon_tint_color(), QStringLiteral("#ffffff"));
    group->add_timeline_targets(Action::CONTENT_DESTINATION_GLOBAL);
    group->add_timeline_targets(Action::CONTENT_DESTINATION_ANNOUNCEMENTS);
    group->set_clear_presentation_next_slide(true);
}

// (The reading and the writing of the file are as workspacefiles.h has them. They are
// not taken from there so that this file stands by itself, with nothing of the app
// behind it.)
//
// Reads the workspace's groups. A workspace with no file is given the standard group,
// in the document only: nothing is written by reading.
QString readFile(const QString &workspace, Document *document)
{
    QFile file(pathOf(workspace));
    if (!file.exists()) {
        writeStandard(document->add_groups());
        return {};
    }
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read the clear groups: %1").arg(file.errorString());
    const QByteArray bytes = file.readAll();
    if (!document->ParseFromArray(bytes.constData(), int(bytes.size())))
        return QStringLiteral("The workspace's clear groups are not in a form that can be read");
    return {};
}

QString writeFile(const QString &workspace, const Document &document)
{
    std::string bytes;
    if (!document.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode the clear groups");
    QDir().mkpath(QFileInfo(pathOf(workspace)).absolutePath());
    QSaveFile file(pathOf(workspace));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("Cannot write the clear groups: %1").arg(file.errorString());
    return {};
}

int indexOf(const Document &document, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (int i = 0; i < document.groups_size(); ++i) {
        if (document.groups(i).uuid().string() == wanted)
            return i;
    }
    return -1;
}

// Changes one group of the file: `change` is given it, and says whether it changed
// anything, or what is wrong with what was asked. The file is written only for a change.
QString changeGroup(const QString &workspace, const QString &id, const std::function<QString(FileGroup *, bool *)> &change)
{
    Document document;
    QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int index = indexOf(document, id);
    if (index < 0)
        return QStringLiteral("That clear group is no longer there");
    bool changed = false;
    error = change(document.mutable_groups(index), &changed);
    if (!error.isEmpty() || !changed)
        return error;
    return writeFile(workspace, document);
}

} // namespace

QStringList layers()
{
    QStringList keys;
    for (const Layer &layer : kLayers)
        keys.append(QLatin1String(layer.key));
    return keys;
}

QStringList ourLayers()
{
    QStringList keys;
    for (const Layer &layer : kLayers) {
        if (layer.ours)
            keys.append(QLatin1String(layer.key));
    }
    return keys;
}

int pictures()
{
    return kPictureCount;
}

QString pictureName(int icon)
{
    return icon >= 1 && icon <= kPictureCount ? QLatin1String(kPictures[icon]) : QString();
}

Group standard()
{
    FileGroup group;
    writeStandard(&group);
    return read(group);
}

QList<Group> groups(const QString &workspace, QString *error)
{
    Document document;
    *error = readFile(workspace, &document);
    QList<Group> found;
    for (const FileGroup &group : document.groups())
        found.append(read(group));
    return found;
}

QString keep(const QString &workspace)
{
    if (QFile::exists(pathOf(workspace)))
        return {};
    Document document;
    const QString error = readFile(workspace, &document);
    return error.isEmpty() ? writeFile(workspace, document) : error;
}

QString add(const QString &workspace, const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("A clear group has to have a name");
    if (id.isEmpty())
        return QStringLiteral("A clear group has to have an id");
    Document document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    if (indexOf(document, id) >= 0)
        return QStringLiteral("There is a clear group with that id already");
    // Its picture: the first of the ten digits that no group has yet, so that one new
    // group can be told from the next at a glance. (This app's own choice.)
    int picture = int(FileGroup::ImageTypeOne);
    for (int digit = int(FileGroup::ImageTypeOne); digit <= int(FileGroup::ImageTypeZero); ++digit) {
        bool taken = false;
        for (const FileGroup &has : document.groups())
            taken = taken || int(has.image_type()) == digit;
        if (!taken) {
            picture = digit;
            break;
        }
    }
    FileGroup *group = document.add_groups();
    group->mutable_uuid()->set_string(id.toStdString());
    group->set_name(name.trimmed().toStdString());
    group->set_image_type(FileGroup::ImageType(picture));
    setColor(group->mutable_icon_tint_color(), QStringLiteral("#ffffff"));
    return writeFile(workspace, document);
}

QString rename(const QString &workspace, const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty())
        return QStringLiteral("A clear group has to have a name");
    return changeGroup(workspace, id, [&](FileGroup *group, bool *changed) {
        *changed = group->name() != name.trimmed().toStdString();
        group->set_name(name.trimmed().toStdString());
        return QString();
    });
}

QString setLayer(const QString &workspace, const QString &id, const QString &layer, bool on)
{
    const int wanted = layerOf(layer);
    if (wanted < 0 || !kLayers[wanted].ours)
        return QStringLiteral("That is not a layer that can be changed here");
    return changeGroup(workspace, id, [&](FileGroup *group, bool *changed) {
        // Every target that is this layer, should the file have it more than once
        QList<int> at;
        for (int i = 0; i < group->layer_targets_size(); ++i) {
            if (layerOf(group->layer_targets(i)) == wanted)
                at.append(i);
        }
        if (on == !at.isEmpty())
            return QString();
        *changed = true;
        if (!on) {
            for (int i = int(at.size()) - 1; i >= 0; --i)
                group->mutable_layer_targets()->DeleteSubrange(at.at(i), 1);
            return QString();
        }
        // Put where ProPresenter's own order has it: before the first target that comes
        // after it in that order. (Added at the end and then moved up, which leaves the
        // others in the order they were in. A target that is none of the eight stays
        // where it is and is passed over.)
        int before = group->layer_targets_size();
        for (int i = group->layer_targets_size() - 1; i >= 0; --i) {
            const int rank = layerOf(group->layer_targets(i));
            if (rank > wanted)
                before = i;
        }
        Clear *target = group->add_layer_targets();
        if (kLayers[wanted].target != Clear::CLEAR_TARGET_LAYER_ALL)
            target->set_target_layer(kLayers[wanted].target);
        if (kLayers[wanted].destination != Action::CONTENT_DESTINATION_GLOBAL)
            target->set_content_destination(kLayers[wanted].destination);
        for (int i = group->layer_targets_size() - 1; i > before; --i)
            group->mutable_layer_targets()->SwapElements(i, i - 1);
        return QString();
    });
}

QString setIcon(const QString &workspace, const QString &id, int icon)
{
    if (icon < 1 || icon > kPictureCount)
        return QStringLiteral("That is not one of the pictures there are");
    return changeGroup(workspace, id, [&](FileGroup *group, bool *changed) {
        *changed = int(group->image_type()) != icon;
        // (A picture of the user's own stays in the file: it is theirs, and ProPresenter
        // may be asked for it again.)
        group->set_image_type(FileGroup::ImageType(icon));
        return QString();
    });
}

QString setTint(const QString &workspace, const QString &id, bool tinted, const QString &color)
{
    return changeGroup(workspace, id, [&](FileGroup *group, bool *changed) {
        const QString had = group->has_icon_tint_color() && group->icon_tint_color().alpha() > 0 ? hexOf(group->icon_tint_color()) : QString();
        if (!color.isEmpty() && color.toLower() != had) {
            if (!setColor(group->mutable_icon_tint_color(), color))
                return QStringLiteral("That is not a colour");
            *changed = true;
        }
        if (group->is_icon_tinted() != tinted) {
            group->set_is_icon_tinted(tinted);
            *changed = true;
        }
        return QString();
    });
}

QString setHidden(const QString &workspace, const QString &id, bool hidden)
{
    return changeGroup(workspace, id, [&](FileGroup *group, bool *changed) {
        *changed = group->is_hidden_in_preview() != hidden;
        group->set_is_hidden_in_preview(hidden);
        return QString();
    });
}

QString move(const QString &workspace, const QString &id, int by)
{
    Document document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int index = indexOf(document, id);
    if (index < 0)
        return QStringLiteral("That clear group is no longer there");
    const int to = index + (by < 0 ? -1 : 1);
    if (by == 0 || to < 0 || to >= document.groups_size())
        return {};
    document.mutable_groups()->SwapElements(index, to);
    return writeFile(workspace, document);
}

QString remove(const QString &workspace, const QString &id)
{
    Document document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int index = indexOf(document, id);
    if (index < 0)
        return QStringLiteral("That clear group is no longer there");
    document.mutable_groups()->DeleteSubrange(index, 1);
    return writeFile(workspace, document);
}

} // namespace clearfile
