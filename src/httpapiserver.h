#pragma once

#include "httpapi.h"
#include "proremote.h"

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
// it is open. With no stream open, changed() does nothing at all, and the window does
// not call it (HttpApiControl's `following`).
//
// ProPresenter Remote (Renewed Vision's phone app; its rules are in proremote.h). With
// the API listening and the Remote switched on with a controller's or an observer's
// password, the app advertises itself on the network as ProPresenter does (dnssd.h), and
// a GET of /v1/ws that asks for a WebSocket is upgraded to one. Each message that comes
// on it is opened with the passwords' keys (the controller's first), and the first key
// that opens one is that client's from then on: it is answered by the same routes as an
// HTTP request, under that key, in the kind of frame it came in. An observer may ask
// only for what reports the show (httpapi::operates); the rest is answered "403
// Forbidden" (a GUESS: what ProPresenter answers an observer is not known). A request
// that streams is one of the client's streams, sent again as the HTTP ones are, with its
// own url; one for the same url again takes the place of the first, and a client keeps
// at most mostWatched of them, the oldest let go first (the app's own number: how the
// Remote ends a stream it no longer wants is not known). Changing the Remote's
// passwords, or switching it off, lets go of those it no longer lets in.
//
// Pictures of slides (a route's `picture`: slidepictures.h). The window's handler says of
// which presentation's file, in which arrangement, and which cue; the picture is drawn by
// a process of its own (slidepictureservice.h), so that nothing of it is done on this
// thread but the asking and the sending, and the request is answered when it comes. While
// it waits, the requests a client sends after it wait too (HTTP answers a connection's
// requests in turn); the Remote fetches its pictures over several connections at once. A
// Remote that asks for one over its WebSocket is answered 404, as a GUESS: it was seen
// to fetch them by HTTP only.
class DnsSd;
class SlidePictureService;

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
    // ProPresenter Remote, from the settings: switched on, and its controller's and
    // observer's passwords, each switched on or not
    Q_PROPERTY(bool remoteEnabled READ remoteEnabled WRITE setRemoteEnabled NOTIFY settingsChanged)
    Q_PROPERTY(bool remoteController READ remoteController WRITE setRemoteController NOTIFY settingsChanged)
    Q_PROPERTY(QString remoteControllerPassword READ remoteControllerPassword WRITE setRemoteControllerPassword NOTIFY settingsChanged)
    Q_PROPERTY(bool remoteObserver READ remoteObserver WRITE setRemoteObserver NOTIFY settingsChanged)
    Q_PROPERTY(QString remoteObserverPassword READ remoteObserverPassword WRITE setRemoteObserverPassword NOTIFY settingsChanged)
    // Whether it is advertised on the network now, under which name, or why it is not;
    // and how many Remotes are connected
    Q_PROPERTY(bool remoteAdvertised READ remoteAdvertised NOTIFY statusChanged)
    Q_PROPERTY(QString remoteName READ remoteName NOTIFY statusChanged)
    Q_PROPERTY(QString remoteProblem READ remoteProblem NOTIFY statusChanged)
    Q_PROPERTY(int remotes READ remotes NOTIFY streamsChanged)
    // Whether the process that draws pictures of slides is running (see
    // slidepictureservice.h), for the window to hold it still while the show changes
    Q_PROPERTY(bool drawingPictures READ drawingPictures NOTIFY picturesChanged)

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
    bool remoteEnabled() const { return m_remoteEnabled; }
    void setRemoteEnabled(bool on);
    bool remoteController() const { return m_remoteController; }
    void setRemoteController(bool on);
    QString remoteControllerPassword() const { return m_remoteControllerPassword; }
    void setRemoteControllerPassword(const QString &password);
    bool remoteObserver() const { return m_remoteObserver; }
    void setRemoteObserver(bool on);
    QString remoteObserverPassword() const { return m_remoteObserverPassword; }
    void setRemoteObserverPassword(const QString &password);
    bool remoteAdvertised() const;
    QString remoteName() const;
    QString remoteProblem() const;
    int remotes() const;
    bool drawingPictures() const;

    // Something the API reports may have changed: every open stream is looked at again,
    // once, soon.
    Q_INVOKABLE void changed();
    // The computer's name, which the Network Name is until one is set
    Q_INVOKABLE QString computerName() const;
    // The process drawing pictures of slides held still for the next `ms` milliseconds,
    // while the show changes (see slidepictureservice.h); nothing if there is none
    Q_INVOKABLE void holdPictures(int ms);

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
    Q_INVOKABLE QVariantMap timer(const QVariantMap &timer, int index) const;
    Q_INVOKABLE QVariantMap timerCurrent(const QVariantMap &timer, const QVariantMap &state, int index) const;
    // { changes, error }, from httpapijson::timerChanges
    Q_INVOKABLE QVariantMap timerChanges(const QVariantMap &body) const;
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
    void picturesChanged();

private:
    // What a stream follows: a path asked for, as its route, and what was last sent of it
    struct Watched
    {
        QString path;
        const httpapi::Route *route = nullptr;
        QVariantMap params;
        QVariantMap query;
        QByteArray last;
        // A Remote's: the url it was asked with, which each message of it gives back, and
        // whether it was asked in a binary frame (and so is sent in them)
        QString echo;
        bool binary = false;
    };
    struct Client
    {
        QTcpSocket *socket = nullptr;
        httpapi::Parser parser;
        // Which client it is, for an answer that comes later to find it by (a pointer may
        // be another client's by then); and whether it is waiting for a picture
        quint64 serial = 0;
        bool waiting = false;
        // A ProPresenter Remote's WebSocket (see above): its frames as they come, the key
        // its messages open with once one has, whether that is the observer's, and
        // whether the log has been told of a message that opened with none
        bool remote = false;
        proremote::FrameReader frames;
        QByteArray key;
        bool observer = false;
        bool toldUnopened = false;
        // A stream: a chunked GET (one, sent bare) or status/updates ({url, data} each)
        bool streaming = false;
        bool updates = false;
        QList<Watched> watched;
        // Runs while nothing has come from the client; a client that is quiet too long,
        // and streams nothing, is let go (see accept)
        QTimer *quiet = nullptr;
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
    // ProPresenter Remote's (see above)
    bool remoteOpen() const;
    void settleRemote();
    void upgrade(Client *client, const httpapi::Request &request);
    void readRemote(Client *client, const QByteArray &bytes);
    void heard(Client *client, const proremote::FrameReader::Message &message);
    void answerRemote(Client *client, const proremote::Request &request, bool binary);
    void sendRemote(Client *client, const QByteArray &key, const QByteArray &json, bool binary);
    void closeRemote(Client *client, quint16 code);
    // Pictures of slides (see above)
    void answerPicture(Client *client, const httpapi::Request &request, const httpapi::Route &route, const QVariantMap &params,
                       const QVariantMap &query);
    void respondQueued(Client *client);
    Client *clientOf(quint64 serial) const;

    bool m_enabled = false;
    int m_port = defaultPort;
    QString m_name;
    QString m_problem;
    QStringList m_addresses;
    QJSValue m_handler;
    QTcpServer *m_server = nullptr;
    QList<Client *> m_clients;
    // Whether as many clients as are kept are connected, and the log has said so
    bool m_full = false;
    bool m_refreshDue = false;
    QTimer m_tick;

    bool m_remoteEnabled = false;
    bool m_remoteController = false;
    QString m_remoteControllerPassword;
    bool m_remoteObserver = false;
    QString m_remoteObserverPassword;
    // The keys the passwords make, while their roles are on (empty otherwise)
    QByteArray m_controllerKey;
    QByteArray m_observerKey;
    // Made the first time the Remote is to be advertised
    DnsSd *m_dnssd = nullptr;
    // Made the first time a picture of a slide is asked for
    SlidePictureService *m_pictures = nullptr;
    quint64 m_lastSerial = 0;
};
