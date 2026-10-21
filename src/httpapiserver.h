#pragma once

#include "httpapi.h"

#include <QColor>
#include <QElapsedTimer>
#include <QJSValue>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QTcpServer;
class QTcpSocket;

// The HTTP API: ProPresenter's own network API (its rules, and what was seen of
// ProPresenter, are in httpapi.h), served to clients on the same network so that they
// can follow and operate the show as they would ProPresenter's: Bitfocus Companion's
// ProPresenter module, a phone, a script.
//
// It is off until it is switched on in the settings (Network), as ProPresenter's is, and
// costs nothing while it is: no port is open and nothing is made. Switched on, it
// listens on every address of the computer at the port set, and /version gives the
// Network Name set (by default the computer's name), with this app's name and version
// rather than ProPresenter's: it is not ProPresenter, and Companion only shows the words.
//
// Who answers. Most of what the API reports is the operator window's (which
// presentation is open there, the live one's slides, the audience and stage buttons, the
// video playing), and what it operates is done by the window's own functions, the same
// ones its buttons and keys use: so the window answers. While it listens, the window
// gives `handler` a function (qml/HttpApiControl.qml) that is called with a route's
// pattern (with "#PUT", "#POST" or "#DELETE" after it for those methods), the parts of
// the path in braces, the query and the body, and returns
// {status, data}: 200 with data, 204 for something done, 400 or 404; or nothing, for a
// route it has no answer for, which is 404. Only /version and timer/system_time are
// answered here. Everything is on the app's own thread, as the window is.
//
// Streams. A GET that streams, asked for with `chunked`, and POST /v1/status/updates,
// keep the connection and are sent again whatever they report when it would be
// different. When that is: the window calls changed() whenever anything the API reports
// may have changed (the show, the workspace, what is open); every stream is then looked
// at once, soon, however many changes came at once, and a chunk sent for what is now
// different from what was last sent. What changes with time alone (timers, the clock, a
// video's time: a route's `clocked`) is looked at four times a second, while a stream of
// it is open. With no stream open, changed() does nothing at all.
class HttpApi : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // From the settings: whether it listens, on which port, and the name it gives
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int port READ port WRITE setPort NOTIFY settingsChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY settingsChanged)
    // Whether it is listening; if it should be and is not, why; and the computer's
    // addresses a client on the network reaches it at
    Q_PROPERTY(bool listening READ listening NOTIFY statusChanged)
    Q_PROPERTY(QString problem READ problem NOTIFY statusChanged)
    Q_PROPERTY(QStringList addresses READ addresses NOTIFY statusChanged)
    // How many streams are open now
    Q_PROPERTY(int streams READ streams NOTIFY streamsChanged)
    // The operator window's answerer (see above); undefined while there is none
    Q_PROPERTY(QJSValue handler READ handler WRITE setHandler NOTIFY handlerChanged)
    // The port it listens on until another is set (ProPresenter's documentation's)
    Q_PROPERTY(int defaultPort READ defaultPortNumber CONSTANT)

public:
    static constexpr int defaultPort = 50001;

    explicit HttpApi(QObject *parent = nullptr);
    ~HttpApi() override;

    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    int port() const { return m_port; }
    void setPort(int port);
    QString name() const { return m_name; }
    void setName(const QString &name);
    bool listening() const;
    QString problem() const { return m_problem; }
    QStringList addresses() const { return m_addresses; }
    int streams() const;
    QJSValue handler() const { return m_handler; }
    int defaultPortNumber() const { return defaultPort; }
    void setHandler(const QJSValue &handler);

    // Something the API reports may have changed: every open stream is looked at again,
    // once, soon.
    Q_INVOKABLE void changed();
    // The computer's name, which the Network Name is until one is set
    Q_INVOKABLE QString computerName() const;

    // The answers' shapes, for the window (see httpapijson.h and httpapifiles.h)
    Q_INVOKABLE QVariantMap id(const QString &uuid, const QString &name, double index) const;
    Q_INVOKABLE QVariantMap color(const QColor &color) const;
    Q_INVOKABLE QVariantMap slideStatus(const QVariantMap &current, const QVariantMap &next) const;
    Q_INVOKABLE QVariantMap slideIndex(const QString &key, const QString &uuid, const QString &name, double listIndex, int index, int total) const;
    Q_INVOKABLE QString videoCountdown(double seconds) const;
    Q_INVOKABLE QVariantMap presentation(const QString &path, double listIndex) const;
    // Which of `items` (maps, each with its UUID under `uuidKey` and its name under
    // `nameKey`) an id in a path is, by UUID, name or index; -1 for none
    Q_INVOKABLE int resolve(const QVariantList &items, const QString &id, const QString &uuidKey, const QString &nameKey) const;
    // ProPresenter's index for "not in a list"
    Q_INVOKABLE double noIndex() const;
    Q_INVOKABLE QVariantList libraries(const QStringList &names) const;
    Q_INVOKABLE QString libraryUuid(const QString &name) const;
    Q_INVOKABLE QVariantMap library(const QVariantList &rows) const;
    Q_INVOKABLE QVariantList playlistTree(const QVariantList &nodes) const;
    Q_INVOKABLE QVariantList playlistItems(const QVariantList &rows) const;
    Q_INVOKABLE QVariantMap playlistItem(const QVariantMap &row, int index) const;
    Q_INVOKABLE QString presentationUuid(const QString &path) const;
    Q_INVOKABLE QString findPresentation(const QString &uuid, const QStringList &files) const;
    Q_INVOKABLE QVariantMap timer(const QVariantMap &timer, int index) const;
    Q_INVOKABLE QVariantMap timerCurrent(const QVariantMap &timer, const QVariantMap &state, int index) const;
    Q_INVOKABLE QVariantMap look(const QVariantMap &look, const QStringList &screenIds, const QString &name, int index) const;
    Q_INVOKABLE QVariantMap prop(const QVariantMap &prop, int index, bool active) const;
    Q_INVOKABLE QVariantMap message(const QVariantMap &message, int index, bool active) const;
    Q_INVOKABLE QVariantMap clearGroup(const QVariantMap &group, int index) const;
    Q_INVOKABLE QVariantMap macro(const QVariantMap &macro, int index) const;
    Q_INVOKABLE QVariantMap videoInput(const QVariantMap &input, int index) const;
    Q_INVOKABLE QVariantList binTree(const QVariantList &nodes) const;
    Q_INVOKABLE QVariantMap binItem(const QVariantMap &row, int index, bool audio) const;

signals:
    void settingsChanged();
    void statusChanged();
    void streamsChanged();
    void handlerChanged();

private:
    // What a stream follows: a path asked for, as its route, and what was last sent of it
    struct Watched
    {
        QString path;
        const httpapi::Route *route = nullptr;
        QVariantMap params;
        QVariantMap query;
        QByteArray last;
    };
    struct Client
    {
        QTcpSocket *socket = nullptr;
        httpapi::Parser parser;
        // A stream: a chunked GET (one, sent bare) or status/updates ({url, data} each)
        bool streaming = false;
        bool updates = false;
        QList<Watched> watched;
    };
    struct Answer
    {
        int status = 404;
        QVariant data;
    };

    void settle();
    void listen();
    void stopListening();
    void accept();
    void readFrom(Client *client);
    void respond(Client *client, const httpapi::Request &request);
    void send(Client *client, const httpapi::Request &request, int status, const QByteArray &body = {},
              const QByteArray &type = "application/json");
    void startStream(Client *client, const httpapi::Request &request, const QList<Watched> &watched, bool updates);
    // Sends whatever of a stream is now different: everything that is, or only what is clocked
    void refresh(Client *client, bool clockedOnly);
    void refreshAll(bool clockedOnly);
    void drop(Client *client);
    Answer evaluate(const httpapi::Route &route, const QVariantMap &params, const QVariantMap &query, const QByteArray &body);
    void fail(const QString &why);
    void updateTicking();

    bool m_enabled = false;
    int m_port = defaultPort;
    QString m_name;
    QString m_problem;
    QStringList m_addresses;
    QJSValue m_handler;
    QTcpServer *m_server = nullptr;
    QList<Client *> m_clients;
    bool m_refreshDue = false;
    QTimer m_tick;
};
