#include "mcpstdio.h"

#include "extendedapi.h"
#include "mcp.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QTcpSocket>
#include <QTextStream>
#include <QUrl>

#include <cstdio>

namespace mcpstdio {

bool asked(int argc, char *argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (qstrcmp(argv[i], "--mcp") == 0)
            return true;
    }
    return false;
}

bool parseResponse(const QByteArray &bytes, Response *response)
{
    const qsizetype end = bytes.indexOf("\r\n\r\n");
    if (end < 0)
        return false;
    const QList<QByteArray> lines = bytes.left(end).split('\n');
    const QList<QByteArray> first = lines.value(0).trimmed().split(' ');
    if (first.size() < 2)
        return false;
    qint64 length = -1;
    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const qsizetype colon = line.indexOf(':');
        if (colon > 0 && line.left(colon).trimmed().toLower() == "content-length")
            length = line.mid(colon + 1).trimmed().toLongLong();
    }
    // (A 204 and its like say nothing of a length, and have no body.)
    if (length < 0)
        length = 0;
    if (bytes.size() < end + 4 + length)
        return false;
    response->status = first.at(1).toInt();
    response->body = bytes.mid(end + 4, length);
    return true;
}

Response request(const QString &host, int port, const QByteArray &method, const QString &path, const QByteArray &body, int timeoutMs)
{
    Response response;
    QTcpSocket socket;
    socket.connectToHost(host, quint16(port));
    if (!socket.waitForConnected(3000))
        return response;
    // (An IPv6 address goes in brackets where a port follows it)
    QByteArray name = host.toUtf8();
    if (host.contains(u':'))
        name = '[' + name + ']';
    QByteArray head = method + ' ' + path.toUtf8() + " HTTP/1.1\r\n"
                      + "host: " + name + ':' + QByteArray::number(port) + "\r\n"
                      + "connection: close\r\n";
    if (!body.isEmpty())
        head += "content-type: application/json\r\ncontent-length: " + QByteArray::number(body.size()) + "\r\n";
    socket.write(head + "\r\n" + body);
    QByteArray received;
    while (socket.waitForReadyRead(timeoutMs)) {
        received += socket.readAll();
        if (parseResponse(received, &response))
            return response;
    }
    received += socket.readAll();
    parseResponse(received, &response);
    return response;
}

int run(int argc, char *argv[], const QString &version)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("SimplePresenter"));
    QCoreApplication::setApplicationName(QStringLiteral("SimplePresenter"));
    QCoreApplication::setApplicationVersion(version);

    // Where the app that runs the show is asked: this computer, at the port its
    // settings name, unless the command line says another
    QString host = QStringLiteral("127.0.0.1");
    int port = QSettings(QStringLiteral("SimplePresenter"), QStringLiteral("SimplePresenter")).value(QStringLiteral("networkPort"), 50001).toInt();
    for (int i = 1; i + 1 < argc; ++i) {
        if (qstrcmp(argv[i], "--mcp-api") != 0)
            continue;
        const QUrl address = QUrl::fromUserInput(QString::fromLocal8Bit(argv[i + 1]));
        if (address.isValid() && !address.host().isEmpty()) {
            host = address.host();
            port = address.port(50001);
        }
    }
    QTextStream(stderr) << "Simple Presenter " << version << " as an MCP server: its tools ask the app at " << host << ':' << port << Qt::endl;

    const mcp::Server server {QStringLiteral("simplepresenter"), QStringLiteral("Simple Presenter"), version};
    const mcp::Caller call = [&](const extendedapi::Operation &operation, bool theme, const QVariantMap &params, const QVariantMap &query,
                                 const QVariantMap &body) -> mcp::Called {
        QString path = extendedapi::pathOf(operation, params, theme);
        if (path.isEmpty())
            return {400, QVariantMap {{QStringLiteral("error"), QStringLiteral("A part of what the tool needs was not given")}}};
        QStringList asked;
        for (auto it = query.constBegin(); it != query.constEnd(); ++it)
            asked.append(it.key() + u'=' + QString::fromLatin1(QUrl::toPercentEncoding(it.value().toString())));
        if (!asked.isEmpty())
            path += u'?' + asked.join(u'&');
        const bool sends = operation.method != "GET" && operation.method != "DELETE";
        const Response response = request(host, port, operation.method, path,
                                          sends ? QJsonDocument::fromVariant(body).toJson(QJsonDocument::Compact) : QByteArray());
        if (response.status == 0) {
            return {503, QVariantMap {{QStringLiteral("error"),
                                       QStringLiteral("Simple Presenter could not be reached at %1:%2. It has to be running, with Enable Network and "
                                                      "Enable Extended API switched on in its Settings, under Network.").arg(host).arg(port)}}};
        }
        const QVariant data = QJsonDocument::fromJson(response.body).toVariant();
        if (response.status >= 400 && data.toMap().value(QStringLiteral("error")).toString().isEmpty()) {
            return {response.status, QVariantMap {{QStringLiteral("error"),
                                                   QStringLiteral("Simple Presenter answered %1 with no reason. Its extended API may be switched off: "
                                                                  "it is switched on in its Settings, under Network.").arg(response.status)}}};
        }
        return {response.status, data};
    };

    // A message a line, in and out, until there is no more to read
    QFile in;
    QFile out;
    if (!in.open(stdin, QIODevice::ReadOnly) || !out.open(stdout, QIODevice::WriteOnly | QIODevice::Unbuffered))
        return 1;
    while (true) {
        const QByteArray line = in.readLine();
        if (line.isEmpty())
            break;
        if (line.trimmed().isEmpty())
            continue;
        const mcp::Reply reply = mcp::handle(line.trimmed(), {}, server, call);
        if (reply.body.isEmpty())
            continue;
        out.write(reply.body + '\n');
        out.flush();
    }
    return 0;
}

}
