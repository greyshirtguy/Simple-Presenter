#include "pptxdeck.h"

#include <QCryptographicHash>
#include <QFileInfo>

#include <algorithm>

namespace pptx {

QRectF placing(const QRectF &box, const QSizeF &content, ScaleMode mode)
{
    if (content.width() <= 0 || content.height() <= 0 || mode == ScaleMode::Stretch)
        return box;
    const double sx = box.width() / content.width();
    const double sy = box.height() / content.height();
    const double s = mode == ScaleMode::Fit ? std::min(sx, sy) : std::max(sx, sy);
    const double w = content.width() * s;
    const double h = content.height() * s;
    return QRectF(box.x() + (box.width() - w) / 2, box.y() + (box.height() - h) / 2, w, h);
}

QString Media::fileExtension() const
{
    return QFileInfo(fileName).suffix().toLower();
}

QString Media::key() const
{
    // A file by where it is; bytes made here by their name, which says what they were
    // made from, and a hash of them, in case two were made of the same.
    if (!path.isEmpty())
        return QStringLiteral("file:") + path;
    return QStringLiteral("data:") + dataId + u':'
           + QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

std::optional<Media> Media::stillPicture() const
{
    if (still.isEmpty())
        return std::nullopt;
    Media picture;
    picture.kind = Image;
    picture.data = still;
    picture.dataId = QStringLiteral("still:") + key();
    picture.fileName = QFileInfo(fileName).completeBaseName() + QStringLiteral(" (still).png");
    picture.naturalSize = naturalSize;
    return picture;
}

QString TextBody::plainText() const
{
    QStringList lines;
    for (const Paragraph &paragraph : paragraphs) {
        QString line;
        for (const Run &run : paragraph.runs)
            line += run.text;
        lines << line;
    }
    return lines.join(u'\n');
}

QString Note::slidesText() const
{
    QList<int> sorted = slides;
    std::sort(sorted.begin(), sorted.end());
    if (sorted.isEmpty())
        return {};
    if (sorted.size() == 1)
        return QStringLiteral("slide %1").arg(sorted.first());
    QStringList first;
    for (int k = 0; k < sorted.size() - 1; ++k)
        first << QString::number(sorted.at(k));
    return QStringLiteral("slides %1 and %2").arg(first.join(QStringLiteral(", "))).arg(sorted.last());
}

void NoteCollector::add(Note::Kind kind, const QString &message, int slide)
{
    for (Note &note : m_notes) {
        if (note.kind == kind && note.message == message) {
            if (slide > 0 && !note.slides.contains(slide))
                note.slides << slide;
            return;
        }
    }
    Note note;
    note.kind = kind;
    note.message = message;
    if (slide > 0)
        note.slides << slide;
    m_notes << note;
}

} // namespace pptx
