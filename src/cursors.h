#pragma once

#include <QObject>
#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

// The pointer over the editor's handles: what QML cannot say about it.
//
// QML can give an item one of the desktop's own pointers, and none of those is for
// turning something. And it is told that Ctrl is held only when a key or a pointer event
// comes its way, where a handle has to change its pointer the moment the key goes down,
// with the pointer still. So this draws the pointer for turning, and watches the key.
//
// There is one of these, which QML reaches by its name.
class Cursors : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // Whether Ctrl is held, wherever in the app the keyboard is; and whether Alt is,
    // which the slides ask (a click with Alt shows a slide without its media)
    Q_PROPERTY(bool ctrlHeld READ ctrlHeld NOTIFY ctrlHeldChanged)
    Q_PROPERTY(bool altHeld READ altHeld NOTIFY altHeldChanged)

public:
    explicit Cursors(QObject *parent = nullptr);

    bool ctrlHeld() const { return m_ctrlHeld; }
    bool altHeld() const { return m_altHeld; }

    // Gives an item the pointer for turning: an arc with an arrowhead at each end.
    // `degrees` is the way from the middle of what is turned to the item, clockwise
    // from pointing right, so that the arc lies across that way, as a turn would go.
    Q_INVOKABLE void turn(QQuickItem *item, qreal degrees);
    // Gives an item one of the desktop's own pointers (a Qt.CursorShape).
    Q_INVOKABLE void shape(QQuickItem *item, int cursorShape);

signals:
    void ctrlHeldChanged();
    void altHeldChanged();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setCtrlHeld(bool held);
    void setAltHeld(bool held);

    bool m_ctrlHeld = false;
    bool m_altHeld = false;
};
