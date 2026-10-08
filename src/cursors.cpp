#include "cursors.h"

#include <QCursor>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QQuickWindow>

#include <cmath>

Cursors::Cursors(QObject *parent)
    : QObject(parent)
{
    // Every event of the app passes here, which is how the key is seen whatever has
    // the keyboard. Only the few kinds below are looked at.
    qGuiApp->installEventFilter(this);
}

void Cursors::setCtrlHeld(bool held)
{
    if (held == m_ctrlHeld)
        return;
    m_ctrlHeld = held;
    emit ctrlHeldChanged();
}

bool Cursors::eventFilter(QObject *, QEvent *event)
{
    switch (event->type()) {
    case QEvent::KeyPress:
    case QEvent::KeyRelease: {
        // The key itself going down or coming up: what is held besides it is said as
        // it was before the key, so the key is asked for by name.
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Control)
            setCtrlHeld(event->type() == QEvent::KeyPress);
        break;
    }
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
        // The pointer says what is held as it moves, which puts right a key that went
        // down or came up while another window had the keyboard.
        setCtrlHeld(static_cast<QMouseEvent *>(event)->modifiers() & Qt::ControlModifier);
        break;
    case QEvent::ApplicationDeactivate:
        setCtrlHeld(false);
        break;
    default:
        break;
    }
    return false;
}

void Cursors::turn(QQuickItem *item, qreal degrees)
{
    if (!item)
        return;
    const qreal ratio = item->window() ? item->window()->effectiveDevicePixelRatio() : 1.0;
    const int side = 26;
    QPixmap picture(qRound(side * ratio), qRound(side * ratio));
    picture.setDevicePixelRatio(ratio);
    picture.fill(Qt::transparent);

    // Rather more than a third of a circle with an arrowhead at each end. It is drawn with
    // the circle's middle at the origin and the arc on its right, and then put so that
    // the middle of the arc is the middle of the picture and the circle's middle lies
    // back the way asked: towards the middle of what is turned.
    const qreal radius = 9;
    const QRectF circle(-radius, -radius, 2 * radius, 2 * radius);
    QPainterPath arc;
    arc.arcMoveTo(circle, -70);
    arc.arcTo(circle, -70, 140);
    const auto head = [&arc, radius](qreal at, qreal sign) {
        // The end of the arc at this angle, and two barbs back from it along the arc
        const qreal radians = at * M_PI / 180;
        const QPointF tip(radius * std::cos(radians), -radius * std::sin(radians));
        const QPointF along(-sign * std::sin(radians), -sign * std::cos(radians));
        const QPointF across(std::cos(radians), -std::sin(radians));
        arc.moveTo(tip - along * 4 + across * 2.8);
        arc.lineTo(tip);
        arc.lineTo(tip - along * 4 - across * 2.8);
    };
    head(70, 1);
    head(-70, -1);

    QPainter painter(&picture);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(side / 2.0, side / 2.0);
    painter.rotate(degrees);
    painter.translate(-radius, 0);
    // Dark under light, so that it shows against anything
    painter.setPen(QPen(QColor(0, 0, 0, 220), 3.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(arc);
    painter.setPen(QPen(Qt::white, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.drawPath(arc);
    painter.end();

    item->setCursor(QCursor(picture, side / 2, side / 2));
}

void Cursors::shape(QQuickItem *item, int cursorShape)
{
    if (item)
        item->setCursor(QCursor(Qt::CursorShape(cursorShape)));
}
