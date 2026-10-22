#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// The workspace's copyright display, for QML: ProPresenter's Configuration/CCLI (see
// ccli.h for what it holds and the rules), read when a workspace is opened and written
// back as it is changed, from Settings → Copyright and from the editor, which edits its
// template slide (PresentationEditor's "copyright" kind).
//
// A workspace with no such file is taken to have ProPresenter's layout, off (ccli::made),
// with a template of this app's own: one text box along the foot of the slide, linked to
// the copyright. Nothing is written until something is changed: then the file is made,
// with all of that in it. (A GUESS at ProPresenter's own template, which has not been
// seen being made.)
//
// The show is told of the settings (Show.copyright), which it lays the line over the
// slides by; the screens draw the template, its box showing the line (Show.copyrightText).
// There is one of these, which QML reaches by its name; the operator window points it at
// the workspace.
class CopyrightDisplay : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // { enabled, license, display, tokens } (see ccli::describe)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    // The template slide, as proconvert::toSlideMap describes a slide, for the screens
    Q_PROPERTY(QVariantMap templateSlide READ templateSlide NOTIFY changed)
    // The file, for the editor
    Q_PROPERTY(QString path READ path NOTIFY changed)

public:
    using QObject::QObject;

    QVariantMap settings() const { return m_settings; }
    QVariantMap templateSlide() const { return m_template; }
    QString path() const { return m_path; }

    // Reads a workspace folder's settings. Returns an error message, empty on success.
    Q_INVOKABLE QString open(const QString &workspace);
    // Reads them again (after the editor has saved the template)
    Q_INVOKABLE QString reload();
    // Changes them (as ccli::change takes changes) and writes the file. Returns an error
    // message, empty on success.
    Q_INVOKABLE QString set(const QVariantMap &changes);
    // Makes the file, as a workspace with none is taken to have it, if there is none: for
    // the editor, which edits the template in it. Returns an error message.
    Q_INVOKABLE QString makeFile();
    // The line the layout makes of made-up details, for the settings to show what it
    // looks like
    Q_INVOKABLE QString example() const;

signals:
    void changed();

private:
    QString m_workspace;
    QString m_path;
    QVariantMap m_settings;
    QVariantMap m_template;
};
