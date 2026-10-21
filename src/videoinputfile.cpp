#include "videoinputfile.h"

#include "action.pb.h"
#include "proworkspace.pb.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include <functional>

namespace videoinputfile {

namespace {

using Document = rv::data::ProPresenterWorkspace;
using FileInput = rv::data::VideoInput;

QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/Workspace");
}

QString text(const std::string &value)
{
    return QString::fromStdString(value);
}

// (A new id, and the reading and writing of the file, are as screenfile.cpp has them,
// and kept here so that this stands by itself.)
std::string newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

QString load(const QString &workspace, Document *document)
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

QString save(const QString &workspace, const Document &document)
{
    if (!QDir().mkpath(workspace + QStringLiteral("/Configuration")))
        return QStringLiteral("The workspace's Configuration folder could not be made");
    QSaveFile file(pathOf(workspace));
    const std::string bytes = document.SerializeAsString();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("The workspace's set-up file could not be written: %1").arg(file.errorString());
    return {};
}

// Reads the file, makes one change and writes it back, if the change changed anything
QString change(const QString &workspace, const std::function<QString(Document *, bool *)> &what)
{
    Document document;
    QString error = load(workspace, &document);
    if (!error.isEmpty())
        return error;
    bool changed = false;
    error = what(&document, &changed);
    if (!error.isEmpty() || !changed)
        return error;
    return save(workspace, document);
}

int indexOf(const Document &document, const QString &id)
{
    for (int i = 0; i < document.videoinputs_size(); ++i) {
        if (text(document.videoinputs(i).uuid().string()) == id)
            return i;
    }
    return -1;
}

QString colorOf(const rv::data::Color &color)
{
    return QColor::fromRgbF(float(color.red()), float(color.green()), float(color.blue())).name();
}

} // namespace

QList<Input> inputs(const QString &workspace, QString *error)
{
    Document document;
    const QString failed = load(workspace, &document);
    if (error)
        *error = failed;
    QList<Input> list;
    for (const FileInput &input : document.videoinputs()) {
        Input read;
        read.id = text(input.uuid().string());
        read.name = text(input.user_description());
        read.color = input.has_display_color() ? colorOf(input.display_color()) : QString();
        read.audioType = int(input.audio_type());
        read.proDevice = text(input.video_input_device().name());
        list.append(read);
    }
    return list;
}

QString add(const QString &workspace, const QString &name, QString *id)
{
    return change(workspace, [&](Document *document, bool *changed) {
        FileInput *made = document->add_videoinputs();
        made->mutable_uuid()->set_string(newUuid());
        made->set_user_description(name.toStdString());
        // The dark blue ProPresenter tagged its own first input with
        rv::data::Color *color = made->mutable_display_color();
        color->set_red(0.0471f);
        color->set_green(0.1725f);
        color->set_blue(0.3529f);
        color->set_alpha(1);
        made->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
        if (id)
            *id = text(made->uuid().string());
        *changed = true;
        return QString();
    });
}

QString rename(const QString &workspace, const QString &id, const QString &name)
{
    return change(workspace, [&](Document *document, bool *changed) {
        const int at = indexOf(*document, id);
        if (at < 0)
            return QStringLiteral("There is no such video input in the workspace");
        if (text(document->videoinputs(at).user_description()) == name)
            return QString();
        document->mutable_videoinputs(at)->set_user_description(name.toStdString());
        *changed = true;
        return QString();
    });
}

QString remove(const QString &workspace, const QString &id)
{
    return change(workspace, [&](Document *document, bool *changed) {
        const int at = indexOf(*document, id);
        if (at < 0)
            return QStringLiteral("There is no such video input in the workspace");
        document->mutable_videoinputs()->DeleteSubrange(at, 1);
        *changed = true;
        return QString();
    });
}

bool isAction(const rv::data::Action &action)
{
    return action.has_media() && action.media().element().has_live_video();
}

int actionInput(const rv::data::Action &action)
{
    return inputOf(action.media().element());
}

void buildAction(int input, rv::data::Action *action)
{
    if (action->uuid().string().empty())
        action->mutable_uuid()->set_string(newUuid());
    action->set_isenabled(true);
    action->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
    rv::data::Action::MediaType *media = action->mutable_media();
    rv::data::Media *element = media->mutable_element();
    if (element->uuid().string().empty())
        element->mutable_uuid()->set_string(newUuid());
    element->mutable_metadata();
    if (!element->has_live_video()) {
        element->mutable_live_video()->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
        element->mutable_live_video()->mutable_audio()->set_volume(1);
    }
    setInput(input, element);
    // (ProPresenter's own has its media's sound said, with nothing in it, and the input
    // layer: as it has, so here.)
    if (!media->has_audio() && !media->has_video() && !media->has_image() && !media->has_live_video())
        media->mutable_audio();
    media->set_layer_type(rv::data::Action::LAYER_TYPE_INPUT);
}

bool isInput(const rv::data::Media &media)
{
    return media.has_live_video();
}

int inputOf(const rv::data::Media &media)
{
    return qMax(0, media.live_video().live_video().live_video_index());
}

void setInput(int input, rv::data::Media *media)
{
    media->mutable_live_video()->mutable_live_video()->set_live_video_index(qMax(0, input));
}

QString Mode::key() const
{
    return QStringLiteral("%1x%2@%3 %4").arg(width).arg(height).arg(fps, 0, 'g', 4).arg(format);
}

QString Mode::label() const
{
    return QStringLiteral("%1 × %2, %3 a second%4").arg(width).arg(height).arg(fps, 0, 'g', 4)
        .arg(format.compare(QLatin1String("Jpeg"), Qt::CaseInsensitive) == 0 ? QStringLiteral(", compressed") : QString());
}

int chooseMode(const QList<Mode> &modes, const QString &wanted)
{
    if (!wanted.isEmpty()) {
        for (int i = 0; i < modes.size(); ++i) {
            if (modes.at(i).key() == wanted)
                return i;
        }
    }
    const auto better = [](const Mode &a, const Mode &b) {
        const bool aFits = a.width <= 1920 && a.height <= 1080;
        const bool bFits = b.width <= 1920 && b.height <= 1080;
        if (aFits != bFits)
            return aFits;
        const qint64 aArea = qint64(a.width) * a.height;
        const qint64 bArea = qint64(b.width) * b.height;
        if (aArea != bArea)
            return aFits ? aArea > bArea : aArea < bArea;
        const double aRate = qMin(a.fps, 30.0);
        const double bRate = qMin(b.fps, 30.0);
        if (aRate != bRate)
            return aRate > bRate;
        const bool aRaw = a.format.compare(QLatin1String("Jpeg"), Qt::CaseInsensitive) != 0;
        const bool bRaw = b.format.compare(QLatin1String("Jpeg"), Qt::CaseInsensitive) != 0;
        return aRaw && !bRaw;
    };
    int best = -1;
    for (int i = 0; i < modes.size(); ++i) {
        if (best < 0 || better(modes.at(i), modes.at(best)))
            best = i;
    }
    return best;
}

int ownAudio(const QString &device, const QStringList &audioInputs)
{
    const QString name = device.simplified();
    if (name.isEmpty())
        return -1;
    for (int i = 0; i < audioInputs.size(); ++i) {
        if (audioInputs.at(i).contains(name, Qt::CaseInsensitive))
            return i;
    }
    // (Its first word worth the name: "Integrated_Webcam_HD: Integrate" is its first)
    QString word;
    for (const QString &part : name.split(QRegularExpression(QStringLiteral("[^A-Za-z0-9]+")), Qt::SkipEmptyParts)) {
        if (part.size() > 3) {
            word = part;
            break;
        }
    }
    if (word.isEmpty())
        return -1;
    for (int i = 0; i < audioInputs.size(); ++i) {
        if (audioInputs.at(i).contains(word, Qt::CaseInsensitive))
            return i;
    }
    return -1;
}

} // namespace videoinputfile
