// ProPresenter Remote's connection (src/proremote.h): the key a password makes and the
// cipher, checked against the published test vectors (FIPS 180-4's SHA-512/256 of
// "abc", NIST SP 800-38A's AES-256 CTR); the WebSocket's handshake and frames, against
// RFC 6455's own examples; and how a message is read and answered, in the shapes the
// Remote was seen to send (2026-11-13).

#include "proremote.h"

#include <QtTest>

using namespace proremote;

class TestProRemote : public QObject
{
    Q_OBJECT

private slots:
    // ---- The key and the cipher

    void theKeyIsTheSha512_256OfThePassword()
    {
        QCOMPARE(key(QStringLiteral("abc")).toHex(), QByteArray("53048e2681941ef99b2e29b76b4c7dabe4c2d0c634fc6d46e0e2f13107e7af23"));
        QCOMPARE(key(QString()).toHex(), QByteArray("c672b8d1ef56ed28ab87c3622c5114069bdd3ad7b8f9737498d0c01ecef0967a"));
        QCOMPARE(key(QStringLiteral("control")).size(), 32);
    }

    void sealingIsAes256InCtrModeWithTheIvInFront()
    {
        // NIST SP 800-38A, F.5.5 CTR-AES256.Encrypt
        const QByteArray k = QByteArray::fromHex("603deb1015ca71be2b73aef0857d77811f352c073b6108d72d9810a30914dff4");
        const QByteArray iv = QByteArray::fromHex("f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff");
        const QByteArray plain = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
                                                     "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710");
        const QByteArray cipher = QByteArray::fromHex("601ec313775789a5b7a7f504bbf3d228f443e3ca4d62b59aca84e990cacaf5c5"
                                                      "2b0930daa23de94ce87017ba2d84988ddfc9c58db67aada613c2dd08457941a6");
        QCOMPARE(seal(k, plain, iv), iv + cipher);
        QCOMPARE(open(k, iv + cipher), plain);
        // (A length that is not whole blocks, as every message is)
        QCOMPARE(seal(k, plain.left(21), iv), iv + cipher.left(21));
    }

    void aSealedMessageOpensWithItsPasswordAndNoOther()
    {
        const QByteArray json = R"({"url":"v1/looks?id=1","method":"GET"})";
        const QByteArray sealed = seal(key(QStringLiteral("control")), json);
        QCOMPARE(sealed.size(), json.size() + 16);
        // A new IV each time, so the same message never looks the same twice
        QVERIFY(seal(key(QStringLiteral("control")), json).left(16) != sealed.left(16));
        QCOMPARE(open(key(QStringLiteral("control")), sealed), json);
        Request request;
        QVERIFY(read(open(key(QStringLiteral("control")), sealed), &request));
        // (CTR opens anything: under another password it opens to noise, which is no request)
        QVERIFY(!read(open(key(QStringLiteral("observe")), sealed), &request));
        QVERIFY(!read(open(key(QString()), sealed), &request));
        QVERIFY(open(key(QStringLiteral("control")), QByteArray(15, 'x')).isEmpty());
    }

    // ---- The WebSocket

    void theHandshakeIsRfc6455s()
    {
        QCOMPARE(acceptKey("dGhlIHNhbXBsZSBub25jZQ=="), QByteArray("s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
        QVERIFY(isUpgrade({"websocket", "Upgrade", "r6kERX9c94l0NsZlpr1QNQ==", "13"}));
        QVERIFY(isUpgrade({"WebSocket", "keep-alive, Upgrade", "x", "13"}));
        QVERIFY(!isUpgrade({"websocket", "Upgrade", "", "13"}));
        QVERIFY(!isUpgrade({"websocket", "keep-alive", "x", "13"}));
        QVERIFY(!isUpgrade({"h2c", "Upgrade", "x", "13"}));
        QVERIFY(!isUpgrade({"websocket", "Upgrade", "x", "8"}));
    }

    void framesAreWrittenAsRfc6455Has()
    {
        QCOMPARE(frame(Text, "Hello"), QByteArray::fromHex("810548656c6c6f"));
        QCOMPARE(frame(Text, "Hello", QByteArray::fromHex("37fa213d")), QByteArray::fromHex("818537fa213d7f9f4d5158"));
        QCOMPARE(frame(Binary, QByteArray(256, 'a')).left(4), QByteArray::fromHex("827e0100"));
        QCOMPARE(frame(Binary, QByteArray(65536, 'a')).left(10), QByteArray::fromHex("827f0000000000010000"));
        QCOMPARE(frame(Close, QByteArray::fromHex("03e8")), QByteArray::fromHex("880203e8"));
    }

    void framesAreReadAsTheyCome()
    {
        FrameReader reader;
        // A masked text, a ping between the two parts of a fragmented message, and a
        // binary of 256 bytes, fed a byte at a time
        const QByteArray all = QByteArray::fromHex("818537fa213d7f9f4d5158") + QByteArray::fromHex("010348656c")
                               + frame(Ping, "hi", QByteArray::fromHex("01020304")) + QByteArray::fromHex("80026c6f")
                               + frame(Binary, QByteArray(256, 'b'), QByteArray::fromHex("a1b2c3d4"));
        for (char byte : all)
            QVERIFY(reader.feed(QByteArray(1, byte)));
        QVERIFY(reader.hasMessage());
        FrameReader::Message first = reader.take();
        QCOMPARE(first.opcode, Text);
        QCOMPARE(first.payload, QByteArray("Hello"));
        FrameReader::Message ping = reader.take();
        QCOMPARE(ping.opcode, Ping);
        QCOMPARE(ping.payload, QByteArray("hi"));
        FrameReader::Message joined = reader.take();
        QCOMPARE(joined.opcode, Text);
        QCOMPARE(joined.payload, QByteArray("Hello"));
        FrameReader::Message binary = reader.take();
        QCOMPARE(binary.opcode, Binary);
        QCOMPARE(binary.payload, QByteArray(256, 'b'));
        QVERIFY(!reader.hasMessage());
    }

    void whatIsNotFramesIsRefused()
    {
        {
            // A reserved bit
            FrameReader reader;
            QVERIFY(!reader.feed(QByteArray::fromHex("c100")));
            QCOMPARE(reader.error(), 1002);
            QVERIFY(!reader.feed(QByteArray::fromHex("8100")));
        }
        {
            // A continuation with nothing to continue
            FrameReader reader;
            QVERIFY(!reader.feed(QByteArray::fromHex("8000")));
            QCOMPARE(reader.error(), 1002);
        }
        {
            // A control frame in parts, or longer than 125
            FrameReader reader;
            QVERIFY(!reader.feed(QByteArray::fromHex("0900")));
            QCOMPARE(reader.error(), 1002);
            FrameReader longer;
            QVERIFY(!longer.feed(QByteArray::fromHex("897e0080")));
            QCOMPARE(longer.error(), 1002);
        }
        {
            // A message larger than is kept
            FrameReader reader;
            QVERIFY(!reader.feed(QByteArray::fromHex("827f0000000010000000")));
            QCOMPARE(reader.error(), 1009);
        }
    }

    // ---- Messages

    void aMessageIsReadAsTheRemoteSendsIt()
    {
        Request request;
        QVERIFY(read(R"({"url":"v1/timers/current?id=b9dbc878-b62d-4e22-8775-99f3e096a413&chunked=true&queryKey=timers%2Ccurrent","method":"GET"})", &request));
        QCOMPARE(request.url, QStringLiteral("v1/timers/current?id=b9dbc878-b62d-4e22-8775-99f3e096a413&chunked=true&queryKey=timers%2Ccurrent"));
        QCOMPARE(request.method, QByteArray("GET"));
        QCOMPARE(request.path, QStringLiteral("/v1/timers/current"));
        QVERIFY(request.chunked);
        QCOMPARE(request.query.size(), 3);
        QCOMPARE(request.query[0], std::pair(QStringLiteral("id"), QStringLiteral("b9dbc878-b62d-4e22-8775-99f3e096a413")));
        QCOMPARE(request.query[2], std::pair(QStringLiteral("queryKey"), QStringLiteral("timers,current")));
        QVERIFY(request.body.isEmpty());

        QVERIFY(read(R"({"url":"v1/libraries?id=a1e0d252","method":"GET"})", &request));
        QVERIFY(!request.chunked);
        // (/version is bare, as ProPresenter answers it)
        QVERIFY(read(R"({"url":"version"})", &request));
        QCOMPARE(request.path, QStringLiteral("/version"));
        QCOMPARE(request.method, QByteArray("GET"));
    }

    void aMessageIsReadAsTheTcpIpApiHasIt()
    {
        Request request;
        // chunked as a member; a body (JSON, a string's included) makes it a POST unless it says
        QVERIFY(read(R"({"url":"v1/timer/system_time","chunked":true})", &request));
        QVERIFY(request.chunked);
        QVERIFY(read(R"({"url":"v1/stage/message","method":"put","body":"This is the new stage message"})", &request));
        QCOMPARE(request.method, QByteArray("PUT"));
        QCOMPARE(request.body, QByteArray("\"This is the new stage message\""));
        QVERIFY(read(R"({"url":"v1/message/1/trigger","body":[{"name":"Name","text":{"text":"Ann"}}]})", &request));
        QCOMPARE(request.method, QByteArray("POST"));
        QCOMPARE(request.body, QByteArray(R"([{"name":"Name","text":{"text":"Ann"}}])"));
        // A chunked=false in the query is not a stream; a url's escapes are undone
        QVERIFY(read(R"({"url":"/v1/library/My%20Songs?chunked=false"})", &request));
        QVERIFY(!request.chunked);
        QCOMPARE(request.path, QStringLiteral("/v1/library/My Songs"));
        // An object with no url opens (so the password was right) but asks for nothing
        QVERIFY(read(R"({"method":"GET"})", &request));
        QVERIFY(request.url.isEmpty());
        // Not an object: not a message
        QVERIFY(!read(R"(["v1/looks"])", &request));
        QVERIFY(!read("v1/looks", &request));
        QVERIFY(!read(QByteArray::fromHex("8f3a11"), &request));
    }

    void anAnswerGivesTheUrlBackAsItWasAsked()
    {
        QCOMPARE(answer(QStringLiteral("version"), R"({"api_version":"v1"})"), QByteArray(R"({"data":{"api_version":"v1"},"url":"version"})"));
        QCOMPARE(answer(QStringLiteral("v1/stage/message?id=x&chunked=true&queryKey=stage%2Cmessage"), "\"Hi\""),
                 QByteArray(R"({"data":"Hi","url":"v1/stage/message?id=x&chunked=true&queryKey=stage%2Cmessage"})"));
        QCOMPARE(failure(QStringLiteral("v1/macro/collections?id=y"), "404 Not Found"),
                 QByteArray(R"({"error":"404 Not Found","url":"v1/macro/collections?id=y"})"));
    }
};

QTEST_GUILESS_MAIN(TestProRemote)
#include "tst_proremote.moc"
