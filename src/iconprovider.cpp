#include "iconprovider.h"

#include <QColor>
#include <QPainter>

IconProvider::IconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    const QString name = id.section(u'/', 0, 0);
    const QString colour = id.section(u'/', 1, 1);
    QImage picture(QStringLiteral(":/icons/%1.png").arg(name));
    // No such picture: nothing is handed back, and QML says so where the picture was
    // asked for, which is where the mistake is.
    if (picture.isNull())
        return {};
    // With no size asked for, the size of the drawing as ProPresenter has it.
    const QSize wanted = requestedSize.width() > 0 && requestedSize.height() > 0 ? requestedSize : QSize(40, 40);
    picture = picture.convertToFormat(QImage::Format_ARGB32_Premultiplied).scaled(wanted, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QColor ink = colour.isEmpty() ? QColor() : QColor(u'#' + colour);
    if (ink.isValid()) {
        // The colour goes wherever the picture is, by as much as the picture is there.
        QPainter painter(&picture);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(picture.rect(), ink);
    }
    if (size)
        *size = picture.size();
    return picture;
}
