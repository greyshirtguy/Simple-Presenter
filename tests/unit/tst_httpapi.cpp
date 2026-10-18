// The rules of the HTTP API (src/httpapi.h): requests read as they come, responses and
// chunks written as ProPresenter writes them, paths matched to routes as it matches
// them, and an id found by UUID, name or index in its order.

#include "httpapi.h"

#include <QTest>

using namespace httpapi;

class TestHttpApi : public QObject
{
    Q_OBJECT

private slots:
    void aRequestIsReadWhenWhole()
    {
        Parser parser;
        QVERIFY(parser.feed("GET /v1/status/la"));
        QVERIFY(!parser.hasRequest());
        QVERIFY(parser.feed("yers?chunked=true&x=a%20b HTTP/1.1\r\nHost: sp\r\nOrigin: http://there\r\n"));
        QVERIFY(!parser.hasRequest());
        QVERIFY(parser.feed("\r\n"));
        QVERIFY(parser.hasRequest());
        const Request request = parser.take();
        QCOMPARE(request.method, QByteArray("GET"));
        QCOMPARE(request.path, QStringLiteral("/v1/status/layers"));
        QVERIFY(request.queryOn(QStringLiteral("chunked")));
        QCOMPARE(request.queryValue(QStringLiteral("x")), QStringLiteral("a b"));
        QCOMPARE(request.headers.value("origin"), QByteArray("http://there"));
        QVERIFY(request.keepAlive());
        QVERIFY(!parser.hasRequest());
    }

    void aBodyComesWithItsLength()
    {
        // Two requests in one go, the first with a body split from it
        Parser parser;
        QVERIFY(parser.feed("POST /v1/status/updates HTTP/1.1\r\nContent-Length: 16\r\nConnection: close\r\n\r\n[\"status/sl"));
        QVERIFY(!parser.hasRequest());
        QVERIFY(parser.feed("ide\"]GET /version HTTP/1.0\r\n\r\n"));
        QVERIFY(parser.hasRequest());
        const Request first = parser.take();
        QCOMPARE(first.body, QByteArray("[\"status/slide\"]"));
        QVERIFY(!first.keepAlive());
        QVERIFY(parser.hasRequest());
        const Request second = parser.take();
        QCOMPARE(second.path, QStringLiteral("/version"));
        QVERIFY(!second.keepAlive()); // HTTP/1.0 without keep-alive
    }

    void switchesInTheQuery()
    {
        Parser parser;
        QVERIFY(parser.feed("GET /a?chunked=false&b&c=0 HTTP/1.1\r\n\r\n"));
        const Request request = parser.take();
        QVERIFY(!request.queryOn(QStringLiteral("chunked")));
        QVERIFY(request.queryOn(QStringLiteral("b")));
        QVERIFY(!request.queryOn(QStringLiteral("c")));
        QVERIFY(!request.queryOn(QStringLiteral("d")));
    }

    void whatIsNotHttpIsRefused()
    {
        Parser bad;
        QVERIFY(!bad.feed("HELLO\r\n\r\n"));
        QCOMPARE(bad.error(), 400);
        QVERIFY(!bad.feed("GET / HTTP/1.1\r\n\r\n")); // nothing more is read
        Parser big;
        QVERIFY(!big.feed("GET / HTTP/1.1\r\nX: " + QByteArray(maxHead, 'a')));
        QCOMPARE(big.error(), 431);
        Parser huge;
        QVERIFY(!huge.feed("PUT / HTTP/1.1\r\nContent-Length: " + QByteArray::number(maxBody + 1) + "\r\n\r\n"));
        QCOMPARE(huge.error(), 413);
        Parser chunked;
        QVERIFY(!chunked.feed("PUT / HTTP/1.1\r\nTransfer-Encoding: chunked\r\n\r\n"));
        QCOMPARE(chunked.error(), 400);
    }

    void responsesAsProPresenterWritesThem()
    {
        Parser parser;
        parser.feed("GET /version HTTP/1.1\r\nOrigin: http://example.local\r\n\r\n");
        const Request request = parser.take();
        Headers headers = common(request);
        headers.append({QByteArray("content-length"), QByteArray("0")});
        QCOMPARE(head(404, headers), QByteArray("HTTP/1.1 404 Not Found\r\n"
                                                "vary: Origin, Access-Control-Request-Method, Access-Control-Request-Headers\r\n"
                                                "access-control-allow-origin: http://example.local\r\n"
                                                "content-length: 0\r\n\r\n"));
        QCOMPARE(httpDate(QDateTime(QDate(2026, 10, 17), QTime(12, 37, 56), QTimeZone::UTC)), QByteArray("Sat, 17 Oct 2026 12:37:56 GMT"));
    }

    void jsonWithItsSeparator()
    {
        QCOMPARE(json(QVariantMap {{"slide", true}, {"audio", false}}, true), QByteArray("{\"audio\":false,\"slide\":true}\r\n\r\n"));
        QCOMPARE(json(QVariantList {1, QStringLiteral("a")}, false), QByteArray("[1,\"a\"]"));
        // Bare values, as ProPresenter answers some things (stage/message "", audience_screens false)
        QCOMPARE(json(QString(), true), QByteArray("\"\"\r\n\r\n"));
        QCOMPARE(json(false, false), QByteArray("false"));
        QCOMPARE(json(QVariant::fromValue(nullptr), false), QByteArray("null"));
        QCOMPARE(json(1792240677.0, false), QByteArray("1792240677"));
        QCOMPARE(json(QStringLiteral("Show Mini 👍"), false), QStringLiteral("\"Show Mini 👍\"").toUtf8());
    }

    void chunks()
    {
        QCOMPARE(chunk("{\"a\":1}\r\n\r\n"), QByteArray("b\r\n{\"a\":1}\r\n\r\n\r\n"));
        QCOMPARE(lastChunk(), QByteArray("0\r\n\r\n"));
    }

    void pathsToRoutes()
    {
        Match found = match("GET", QStringLiteral("/v1/status/layers"));
        QVERIFY(found);
        QCOMPARE(found.route->pattern, QStringLiteral("status/layers"));
        QVERIFY(found.route->streams);
        QVERIFY(!found.route->clocked);
        QVERIFY(match("GET", QStringLiteral("/version")));
        QVERIFY(!match("GET", QStringLiteral("/v1/version")));
        QVERIFY(!match("GET", QStringLiteral("/v1/no/such/thing")));
        QVERIFY(!match("DELETE", QStringLiteral("/v1/status/layers")));
        QVERIFY(!match("GET", QStringLiteral("/status/layers")));
        // Its parts
        found = match("GET", QStringLiteral("/v1/presentation/8C6A1658-69C6-455A-A356-6D09D46CEA4E/12/trigger"));
        QCOMPARE(found.route->pattern, QStringLiteral("presentation/{uuid}/{index}/trigger"));
        QCOMPARE(found.params.value("uuid").toString(), QStringLiteral("8C6A1658-69C6-455A-A356-6D09D46CEA4E"));
        QCOMPARE(found.params.value("index").toString(), QStringLiteral("12"));
        // Words before parts, at the first place they differ
        QCOMPARE(match("GET", QStringLiteral("/v1/presentation/active/trigger")).route->pattern, QStringLiteral("presentation/active/trigger"));
        QCOMPARE(match("GET", QStringLiteral("/v1/presentation/active/next/trigger")).route->pattern, QStringLiteral("presentation/active/next/trigger"));
        QCOMPARE(match("GET", QStringLiteral("/v1/presentation/active/3/trigger")).route->pattern, QStringLiteral("presentation/active/{index}/trigger"));
        QCOMPARE(match("GET", QStringLiteral("/v1/timer/system_time")).route->pattern, QStringLiteral("timer/system_time"));
        QCOMPARE(match("GET", QStringLiteral("/v1/timer/Countdown/start")).route->pattern, QStringLiteral("timer/{id}/{operation}"));
        QCOMPARE(match("GET", QStringLiteral("/v1/playlist/focused/next")).route->pattern, QStringLiteral("playlist/focused/next"));
        QCOMPARE(match("GET", QStringLiteral("/v1/playlist/focused/2/trigger")).route->pattern, QStringLiteral("playlist/focused/{index}/trigger"));
        QCOMPARE(match("GET", QStringLiteral("/v1/media/playlist/Backgrounds/Sky/trigger")).route->pattern, QStringLiteral("media/playlist/{playlist_id}/{media_id}/trigger"));
        // %-escapes undone before matching: a name with a space
        Parser parser;
        parser.feed("GET /v1/look/Lyrics%20L3rd/trigger HTTP/1.1\r\n\r\n");
        found = match("GET", parser.take().path);
        QCOMPARE(found.params.value("id").toString(), QStringLiteral("Lyrics L3rd"));
    }

    void whatStatusUpdatesTakes()
    {
        QVERIFY(updatesPath(QStringLiteral("status/slide")));
        QCOMPARE(updatesPath(QStringLiteral("presentation/current")).route->pattern, QStringLiteral("presentation/active"));
        QCOMPARE(updatesPath(QStringLiteral("transport/audio/time")).params.value("layer").toString(), QStringLiteral("audio"));
        QVERIFY(updatesPath(QStringLiteral("timer/system_time")));
        QVERIFY(!updatesPath(QStringLiteral("/v1/status/slide")));
        QVERIFY(!updatesPath(QStringLiteral("v1/status/slide")));
        QVERIFY(!updatesPath(QStringLiteral("status/layers?chunked=true")));
        QVERIFY(!updatesPath(QStringLiteral("trigger/next"))); // does not stream
        QVERIFY(!updatesPath(QStringLiteral("no/such/url")));
        QStringList paths;
        QVERIFY(updatesPaths("[\"status/layers\",\"timers/current\"]", &paths));
        QCOMPARE(paths, QStringList({QStringLiteral("status/layers"), QStringLiteral("timers/current")}));
        QVERIFY(updatesPaths("[]", &paths));
        QVERIFY(paths.isEmpty());
        QVERIFY(!updatesPaths("not json", &paths));
        QVERIFY(!updatesPaths("{\"a\":1}", &paths));
        QVERIFY(!updatesPaths("[\"status/layers\",\"no/such/url\"]", &paths));
        QVERIFY(!updatesPaths("[\"status/layers\",3]", &paths));
    }

    void everyStreamOfCompanionsIsTaken()
    {
        // The paths Bitfocus Companion's ProPresenter module asks for, all in one
        // status/updates (its src/main.ts): ProPresenter refuses the whole request for any
        // one it does not stream, so every one must be taken here.
        QVERIFY(updatesPaths("[\"status/slide\",\"timers\",\"timers/current\",\"presentation/slide_index\",\"announcement/slide_index\","
                             "\"playlist/active\",\"playlist/focused\",\"presentation/focused\",\"presentation/active\",\"look/current\","
                             "\"looks\",\"macros\",\"props\",\"stage/layout_map\",\"stage/layouts\",\"stage/message\",\"messages\","
                             "\"status/audience_screens\",\"status/stage_screens\",\"status/layers\",\"timer/video_countdown\","
                             "\"transport/presentation/current\",\"transport/announcement/current\",\"transport/audio/current\","
                             "\"transport/audio/time\",\"timer/system_time\",\"capture/status\"]",
                             nullptr));
    }

    void anIdByUuidNameOrIndex()
    {
        const QList<std::pair<QString, QString>> items {{QStringLiteral("47C036A0-A9DF-47B4-BAB1-4171E2EAC249"), QStringLiteral("Main")},
                                                        {QStringLiteral("876F7CB3-3434-4EF9-BCE6-CEB14F66D865"), QStringLiteral("2")},
                                                        {QStringLiteral("E3A36D8C-E70D-45D8-A127-E7DAC6577CDC"), QStringLiteral("Rear")}};
        QCOMPARE(resolve(items, QStringLiteral("E3A36D8C-E70D-45D8-A127-E7DAC6577CDC")), 2);
        QCOMPARE(resolve(items, QStringLiteral("e3a36d8c-e70d-45d8-a127-e7dac6577cdc")), 2);
        QCOMPARE(resolve(items, QStringLiteral("Main")), 0);
        QCOMPARE(resolve(items, QStringLiteral("0")), 0);
        // A name before an index: the second is named "2"
        QCOMPARE(resolve(items, QStringLiteral("2")), 1);
        QCOMPARE(resolve(items, QStringLiteral("1")), 1);
        QCOMPARE(resolve(items, QStringLiteral("3")), -1);
        QCOMPARE(resolve(items, QStringLiteral("-1")), -1);
        QCOMPARE(resolve(items, QStringLiteral("Nothing")), -1);
        QCOMPARE(resolve(items, QString()), -1);
        QCOMPARE(resolve(items, QStringLiteral("00000000-0000-0000-0000-000000000000")), -1);
    }
};

QTEST_APPLESS_MAIN(TestHttpApi)
#include "tst_httpapi.moc"
