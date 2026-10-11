#include "labelfile.h"

#include "labels.pb.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace labelfile {

namespace {

QString pathOf(const QString &workspace)
{
    return workspace + QStringLiteral("/Configuration/Labels");
}

// (The reading and the writing of the file are as workspacefiles.h has them. They are
// not taken from there so that this file stands by itself, with nothing of the app
// behind it.)
QString readFile(const QString &workspace, rv::data::ProLabelsDocument *document)
{
    QFile file(pathOf(workspace));
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read the labels: %1").arg(file.errorString());
    const QByteArray bytes = file.readAll();
    if (!document->ParseFromArray(bytes.constData(), int(bytes.size())))
        return QStringLiteral("The workspace's labels are not in a form that can be read");
    return {};
}

QString writeFile(const QString &workspace, const rv::data::ProLabelsDocument &document)
{
    std::string bytes;
    if (!document.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode the labels");
    QDir().mkpath(QFileInfo(pathOf(workspace)).absolutePath());
    QSaveFile file(pathOf(workspace));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("Cannot write the labels: %1").arg(file.errorString());
    return {};
}

// The action of a cue that shows its slide
int slideAction(const rv::data::Cue &cue)
{
    for (int i = 0; i < cue.actions_size(); ++i) {
        if (cue.actions(i).has_slide())
            return i;
    }
    return -1;
}

} // namespace

Label read(const rv::data::Action::Label &label)
{
    Label made;
    made.text = QString::fromStdString(label.text());
    const rv::data::Color &color = label.color();
    if (label.has_color() && color.alpha() > 0) {
        const auto part = [](float v) { return qBound(0, qRound(v * 255), 255); };
        made.color = QStringLiteral("#%1%2%3").arg(part(color.red()), 2, 16, QLatin1Char('0'))
                         .arg(part(color.green()), 2, 16, QLatin1Char('0')).arg(part(color.blue()), 2, 16, QLatin1Char('0'));
    }
    return made;
}

void write(rv::data::Action::Label *target, const Label &label)
{
    target->set_text(label.text.toStdString());
    bool isColor = label.color.size() == 7 && label.color.startsWith(u'#');
    int parts[3] = {0, 0, 0};
    for (int i = 0; isColor && i < 3; ++i)
        parts[i] = label.color.mid(1 + 2 * i, 2).toInt(&isColor, 16);
    if (!isColor) {
        target->clear_color();
        return;
    }
    rv::data::Color *color = target->mutable_color();
    color->set_red(float(parts[0] / 255.0));
    color->set_green(float(parts[1] / 255.0));
    color->set_blue(float(parts[2] / 255.0));
    color->set_alpha(1);
}

Label of(const rv::data::Cue &cue)
{
    const int action = slideAction(cue);
    return action >= 0 && cue.actions(action).has_label() ? read(cue.actions(action).label()) : Label();
}

bool set(rv::data::Cue *cue, const Label &label)
{
    const int action = slideAction(*cue);
    if (action < 0)
        return false;
    if (label.text.isEmpty() && label.color.isEmpty())
        cue->mutable_actions(action)->clear_label();
    else
        write(cue->mutable_actions(action)->mutable_label(), label);
    return true;
}

QList<Label> presets(const QString &workspace, QString *error)
{
    rv::data::ProLabelsDocument document;
    *error = readFile(workspace, &document);
    QList<Label> labels;
    for (const rv::data::Action::Label &label : document.labels())
        labels.append(read(label));
    return labels;
}

QString add(const QString &workspace, const Label &label)
{
    if (label.text.trimmed().isEmpty())
        return QStringLiteral("A label that is kept ready has to have words");
    rv::data::ProLabelsDocument document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    for (const rv::data::Action::Label &has : document.labels()) {
        if (read(has) == label)
            return {};
    }
    write(document.add_labels(), label);
    return writeFile(workspace, document);
}

QString change(const QString &workspace, int index, const Label &label)
{
    if (label.text.trimmed().isEmpty())
        return QStringLiteral("A label that is kept ready has to have words");
    rv::data::ProLabelsDocument document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    if (index < 0 || index >= document.labels_size())
        return QStringLiteral("That label is no longer in the list");
    write(document.mutable_labels(index), label);
    return writeFile(workspace, document);
}

QString remove(const QString &workspace, int index)
{
    rv::data::ProLabelsDocument document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    if (index < 0 || index >= document.labels_size())
        return QStringLiteral("That label is no longer in the list");
    document.mutable_labels()->DeleteSubrange(index, 1);
    return writeFile(workspace, document);
}

} // namespace labelfile
