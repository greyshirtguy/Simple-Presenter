#include "prodocument.h"

#include "richtext.h"
#include "rtf.h"

#include "presentation.pb.h"

#include <QColor>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QSaveFile>
#include <QUrl>
#include <QUuid>
#include <QtMath>

namespace {

QColor toColor(const rv::data::Color &color)
{
    const auto unit = [](float v) { return qBound(0.0f, v, 1.0f); };
    return QColor::fromRgbF(unit(color.red()), unit(color.green()), unit(color.blue()), unit(color.alpha()));
}

int toQtVerticalAlignment(rv::data::Graphics::Text::VerticalAlignment alignment)
{
    switch (alignment) {
    case rv::data::Graphics::Text::VERTICAL_ALIGNMENT_TOP:
        return Qt::AlignTop;
    case rv::data::Graphics::Text::VERTICAL_ALIGNMENT_BOTTOM:
        return Qt::AlignBottom;
    default:
        return Qt::AlignVCenter;
    }
}

void addFontHint(RtfDefaults *defaults, const rv::data::Font &font)
{
    if (!font.name().empty() && !font.family().empty())
        defaults->familyForPostScriptName.insert(QString::fromStdString(font.name()), QString::fromStdString(font.family()));
}

QVariantMap textProperties(const RichText &text, int verticalAlignment)
{
    return {
        {"hasText", !text.paragraphs.isEmpty()},
        {"text", QVariant::fromValue(text)},
        {"verticalAlignment", verticalAlignment},
    };
}

QVariantMap toElement(const rv::data::Graphics::Element &element)
{
    QVariantMap map {
        {"x", element.bounds().origin().x()},
        {"y", element.bounds().origin().y()},
        {"width", element.bounds().size().width()},
        {"height", element.bounds().size().height()},
        {"rotation", element.rotation()},
        {"opacity", element.opacity()},
        {"fillEnabled", element.fill().enable() && element.fill().has_color()},
        {"fillColor", toColor(element.fill().color())},
        {"strokeEnabled", element.stroke().enable() && element.stroke().width() > 0},
        {"strokeColor", toColor(element.stroke().color())},
        {"strokeWidth", element.stroke().width()},
    };

    // Elements carry a shadow of their own and another on their text; honour whichever is on.
    const rv::data::Graphics::Shadow &shadow =
        element.text().shadow().enable() ? element.text().shadow() : element.shadow();
    QColor shadowColor = toColor(shadow.color());
    shadowColor.setAlphaF(shadowColor.alphaF() * qBound(0.0, shadow.opacity(), 1.0));
    // The angle is measured anticlockwise with y pointing up; slides have y pointing down.
    const double angle = qDegreesToRadians(shadow.angle());
    map.insert("shadowEnabled", shadow.enable());
    map.insert("shadowColor", shadowColor);
    map.insert("shadowOffsetX", shadow.offset() * qCos(angle));
    map.insert("shadowOffsetY", -shadow.offset() * qSin(angle));
    map.insert("shadowRadius", shadow.radius());

    RichText text;
    if (element.has_text() && !element.text().rtf_data().empty()) {
        const rv::data::Graphics::Text::Attributes &attributes = element.text().attributes();
        RtfDefaults defaults;
        if (attributes.has_text_solid_fill())
            defaults.textColor = toColor(attributes.text_solid_fill());
        addFontHint(&defaults, attributes.font());
        for (const auto &custom : attributes.custom_attributes()) {
            if (custom.has_original_font())
                addFontHint(&defaults, custom.original_font());
        }
        text = parseRtf(QByteArray::fromStdString(element.text().rtf_data()), defaults);
    }
    map.insert(textProperties(text, toQtVerticalAlignment(element.text().vertical_alignment())));
    return map;
}

QVariantMap toSlide(const rv::data::Slide &slide, const QString &label)
{
    QVariantList elements;
    QStringList texts;
    for (const rv::data::Slide::Element &element : slide.elements()) {
        if (element.element().hidden())
            continue;
        const QVariantMap map = toElement(element.element());
        const QString text = map.value("text").value<RichText>().plainText().trimmed();
        if (!text.isEmpty())
            texts << text;
        elements.append(map);
    }
    const bool hasSize = slide.size().width() > 0 && slide.size().height() > 0;
    return {
        {"width", hasSize ? slide.size().width() : 1920.0},
        {"height", hasSize ? slide.size().height() : 1080.0},
        {"drawsBackground", slide.draws_background_color()},
        {"backgroundColor", toColor(slide.background_color())},
        {"label", label},
        {"group", QString()},
        {"groupColor", QString()},
        {"groupStart", false},
        {"mediaName", QString()},
        {"plainText", texts.join(u'\n')},
        {"elements", elements},
    };
}

// Documents record media by the path it had on the machine that wrote them. Use that if
// it exists here, otherwise the first file of the same name under mediaDirectory.
QString findMedia(const rv::data::URL &url, const QString &mediaDirectory)
{
    const QString recorded = QUrl(QString::fromStdString(url.absolute_string())).toLocalFile();
    if (!recorded.isEmpty() && QFile::exists(recorded))
        return recorded;

    QStringList names;
    if (!recorded.isEmpty())
        names << QFileInfo(recorded).fileName();
    if (url.has_local() && !url.local().path().empty())
        names << QFileInfo(QString::fromStdString(url.local().path())).fileName();
    if (names.isEmpty() || mediaDirectory.isEmpty())
        return {};

    QDirIterator it(mediaDirectory, names, QDir::Files, QDirIterator::Subdirectories);
    return it.hasNext() ? it.next() : QString();
}

// The file name a media element refers to, whether or not the file can be found here.
QString mediaName(const rv::data::Media &media)
{
    const QUrl url(QString::fromStdString(media.url().absolute_string()));
    const QString name = url.fileName();
    return name.isEmpty() ? QFileInfo(QString::fromStdString(media.url().local().path())).fileName() : name;
}

// Empty if the file cannot be found.
QVariantMap toMedia(const rv::data::Media &media, const QString &mediaDirectory)
{
    const QString path = findMedia(media.url(), mediaDirectory);
    if (path.isEmpty())
        return {};
    return {
        {"name", QFileInfo(path).fileName()},
        {"path", path},
        {"source", QUrl::fromLocalFile(path)},
        {"video", media.has_video()},
    };
}

bool isVisualMedia(const rv::data::Action &action)
{
    return action.has_media() && (action.media().element().has_video() || action.media().element().has_image());
}

QVariantMap emptySlide(const QString &label)
{
    return {
        {"width", 1920.0}, {"height", 1080.0},
        {"drawsBackground", false}, {"backgroundColor", QColor(Qt::transparent)},
        {"label", label},
        {"group", QString()},
        {"groupColor", QString()},
        {"groupStart", false},
        {"mediaName", QString()},
        {"plainText", QString()},
        {"elements", QVariantList()},
    };
}

bool readPresentation(const QString &path, rv::data::Presentation *presentation, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot open %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    if (!presentation->ParseFromArray(data.constData(), int(data.size()))) {
        *error = QStringLiteral("%1 is not a ProPresenter 7 presentation").arg(QFileInfo(path).fileName());
        return false;
    }
    return true;
}

// Everything in the message, including fields this app does not know about, is written
// back as it was read. The file is replaced in one step, so a failure part way through
// leaves the original untouched. Returns an error message, empty on success.
QString writePresentation(const QString &path, const rv::data::Presentation &presentation)
{
    std::string bytes;
    if (!presentation.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode %1").arg(QFileInfo(path).fileName());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size())
        || !file.commit())
        return QStringLiteral("Cannot write %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return {};
}

std::string newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper().toStdString();
}

// The arrangement the presentation has selected, or null.
const rv::data::Presentation::Arrangement *selectedArrangement(const rv::data::Presentation &presentation)
{
    if (!presentation.has_selected_arrangement())
        return nullptr;
    for (const auto &candidate : presentation.arrangements()) {
        if (candidate.uuid().string() == presentation.selected_arrangement().string())
            return &candidate;
    }
    return nullptr;
}

} // namespace

ProDocument ProDocument::load(const QString &path, const QString &mediaDirectory,
                              const std::optional<QString> &arrangement, QString *error)
{
    rv::data::Presentation presentation;
    if (!readPresentation(path, &presentation, error))
        return {};

    QHash<std::string, const rv::data::Cue *> cuesById;
    for (const rv::data::Cue &cue : presentation.cues())
        cuesById.insert(cue.uuid().string(), &cue);
    QHash<std::string, const rv::data::Presentation::CueGroup *> groupsById;
    for (const auto &group : presentation.cue_groups())
        groupsById.insert(group.group().uuid().string(), &group);

    ProDocument document;
    document.name = presentation.name().empty() ? QFileInfo(path).completeBaseName()
                                                 : QString::fromStdString(presentation.name());
    for (const auto &candidate : presentation.arrangements())
        document.arrangements << QString::fromStdString(candidate.name());

    // Which arrangement: the one asked for by name, or the one the document has selected.
    const rv::data::Presentation::Arrangement *chosen = nullptr;
    if (!arrangement) {
        chosen = selectedArrangement(presentation);
    } else {
        for (const auto &candidate : presentation.arrangements()) {
            if (QString::fromStdString(candidate.name()) == *arrangement) {
                chosen = &candidate;
                break;
            }
        }
    }

    // Display order: the arrangement's groups, which may repeat, or with no arrangement
    // every group as stored (ProPresenter's "Master"). Each group lists its cues.
    QList<const rv::data::Presentation::CueGroup *> groups;
    if (chosen) {
        document.arrangement = QString::fromStdString(chosen->name());
        for (const rv::data::UUID &id : chosen->group_identifiers()) {
            if (const auto *group = groupsById.value(id.string()))
                groups.append(group);
        }
    } else {
        for (const auto &group : presentation.cue_groups())
            groups.append(&group);
    }

    // A cue becomes one grid entry: its slide, plus the media it triggers alongside. A cue
    // with media and no slide still gets an entry, so the media can be triggered.
    const auto slidesForCue = [&mediaDirectory](const rv::data::Cue &cue) {
        QVariantList slides;
        if (!cue.isenabled())
            return slides;
        const QString id = QString::fromStdString(cue.uuid().string());
        // The cue's media action, if it has one: its file's name, and the file if found.
        QString name;
        QVariantMap media;
        for (const rv::data::Action &action : cue.actions()) {
            if (!action.isenabled())
                continue;
            if (action.has_slide() && action.slide().has_presentation()) {
                slides.append(toSlide(action.slide().presentation().base_slide(),
                                      QString::fromStdString(action.label().text())));
            } else if (isVisualMedia(action) && name.isEmpty()) {
                name = mediaName(action.media().element());
                media = toMedia(action.media().element(), mediaDirectory);
            }
        }
        if (slides.isEmpty() && !name.isEmpty())
            slides.append(emptySlide(name));
        if (!slides.isEmpty() && !name.isEmpty()) {
            QVariantMap first = slides.first().toMap();
            first.insert("mediaName", name);
            if (!media.isEmpty())
                first.insert("media", media);
            slides.first() = first;
        }
        for (QVariant &entry : slides) {
            QVariantMap slide = entry.toMap();
            slide.insert("id", id);
            entry = slide;
        }
        return slides;
    };

    if (groups.isEmpty()) {
        for (const rv::data::Cue &cue : presentation.cues())
            document.slides.append(slidesForCue(cue));
    } else {
        // Every slide knows its group; the first of each run of a group is marked, so
        // the group's name can be shown once.
        for (const auto *group : groups) {
            const rv::data::Color &color = group->group().color();
            bool first = true;
            for (const rv::data::UUID &id : group->cue_identifiers()) {
                const rv::data::Cue *cue = cuesById.value(id.string());
                if (!cue)
                    continue;
                for (const QVariant &entry : slidesForCue(*cue)) {
                    QVariantMap slide = entry.toMap();
                    slide.insert("group", QString::fromStdString(group->group().name()));
                    slide.insert("groupColor", group->group().has_color() && color.alpha() > 0
                                                   ? toColor(color).name() : QString());
                    slide.insert("groupStart", first);
                    first = false;
                    document.slides.append(slide);
                }
            }
        }
    }

    if (document.slides.isEmpty())
        *error = QStringLiteral("%1 contains no slides").arg(QFileInfo(path).fileName());
    return document;
}

bool ProDocument::arrangementsOf(const QString &path, QStringList *names, QString *selected)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return false;
    names->clear();
    for (const auto &candidate : presentation.arrangements())
        names->append(QString::fromStdString(candidate.name()));
    const auto *chosen = selectedArrangement(presentation);
    *selected = chosen ? QString::fromStdString(chosen->name()) : QString();
    return true;
}

QString ProDocument::setArrangement(const QString &path, const QString &name)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    if (name.isEmpty()) {
        presentation.clear_selected_arrangement();
    } else {
        const rv::data::Presentation::Arrangement *wanted = nullptr;
        for (const auto &candidate : presentation.arrangements()) {
            if (QString::fromStdString(candidate.name()) == name) {
                wanted = &candidate;
                break;
            }
        }
        if (!wanted)
            return QStringLiteral("%1 has no arrangement named %2").arg(QFileInfo(path).fileName(), name);
        *presentation.mutable_selected_arrangement() = wanted->uuid();
    }

    return writePresentation(path, presentation);
}

QString ProDocument::setCueMedia(const QString &path, const QString &cueId, const QString &mediaPath, bool video)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    rv::data::Cue *cue = nullptr;
    for (rv::data::Cue &candidate : *presentation.mutable_cues()) {
        if (QString::fromStdString(candidate.uuid().string()) == cueId) {
            cue = &candidate;
            break;
        }
    }
    if (!cue)
        return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());

    // Replace the media of the cue's existing media action, keeping that action's own
    // settings, or add an action the way ProPresenter writes a background.
    rv::data::Action *action = nullptr;
    for (rv::data::Action &candidate : *cue->mutable_actions()) {
        if (isVisualMedia(candidate)) {
            action = &candidate;
            break;
        }
    }
    if (!action) {
        action = cue->add_actions();
        action->mutable_uuid()->set_string(newUuid());
        action->set_isenabled(true);
        action->set_type(rv::data::Action::ACTION_TYPE_MEDIA);
        action->mutable_media()->set_layer_type(rv::data::Action::LAYER_TYPE_BACKGROUND);
        action->mutable_media()->mutable_audio();
    }
    // It described the old file's length.
    action->clear_duration();

    // The file is recorded by its path on this machine. ProPresenter's paths relative to
    // its own folders are left out: there is no such folder here to be relative to.
    const std::string url = QUrl::fromLocalFile(mediaPath).toString(QUrl::FullyEncoded).toStdString();
    rv::data::Media element;
    element.mutable_uuid()->set_string(newUuid());
    element.mutable_url()->set_absolute_string(url);
    element.mutable_metadata()->set_format(QFileInfo(mediaPath).suffix().toUpper().toStdString());
    if (video) {
        rv::data::Media::VideoTypeProperties *properties = element.mutable_video();
        properties->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
        properties->mutable_audio()->set_volume(1);
        properties->mutable_transport()->set_play_rate(1);
        properties->mutable_transport()->set_playback_behavior(rv::data::Media::TransportProperties::PLAYBACK_BEHAVIOR_LOOP);
        properties->mutable_file()->mutable_local_url()->set_absolute_string(url);
    } else {
        rv::data::Media::ImageTypeProperties *properties = element.mutable_image();
        const QSize size = QImageReader(mediaPath).size();
        if (size.isValid()) {
            properties->mutable_drawing()->mutable_natural_size()->set_width(size.width());
            properties->mutable_drawing()->mutable_natural_size()->set_height(size.height());
        }
        properties->mutable_drawing()->set_alpha_type(rv::data::ALPHA_TYPE_STRAIGHT);
        properties->mutable_file()->mutable_local_url()->set_absolute_string(url);
    }
    *action->mutable_media()->mutable_element() = element;

    return writePresentation(path, presentation);
}

QString ProDocument::removeCueMedia(const QString &path, const QString &cueId)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return error;

    for (rv::data::Cue &cue : *presentation.mutable_cues()) {
        if (QString::fromStdString(cue.uuid().string()) != cueId)
            continue;
        // The same action the slide's media is read from: the first image or video one.
        for (int i = 0; i < cue.actions_size(); ++i) {
            if (cue.actions(i).isenabled() && isVisualMedia(cue.actions(i))) {
                cue.mutable_actions()->DeleteSubrange(i, 1);
                return writePresentation(path, presentation);
            }
        }
        return {};
    }
    return QStringLiteral("%1 no longer has that slide").arg(QFileInfo(path).fileName());
}

QString ProDocument::arrangementId(const QString &path, const QString &name)
{
    rv::data::Presentation presentation;
    QString error;
    if (!readPresentation(path, &presentation, &error))
        return {};
    for (const auto &candidate : presentation.arrangements()) {
        if (QString::fromStdString(candidate.name()) == name)
            return QString::fromStdString(candidate.uuid().string());
    }
    return {};
}
