// MCP on top of the extended API (src/mcp.h): the tools made of the table of
// operations; a conversation as the earlier revisions of the protocol begin one
// (`initialize`), and requests as its current one has them, each saying its revision;
// what is wrong with a request, with the HTTP status it goes with; a tool's arguments
// sorted into its operation's path, query and body, and its answer made a tool's
// result; and the reading of an HTTP response, for the app started with --mcp
// (src/mcpstdio.h).

#include "mcp.h"
#include "mcpstdio.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTest>

namespace {

const QString kVersion = QStringLiteral("io.modelcontextprotocol/protocolVersion");
const QString kCapabilities = QStringLiteral("io.modelcontextprotocol/clientCapabilities");
const QString kServerInfo = QStringLiteral("io.modelcontextprotocol/serverInfo");

QJsonObject read(const QByteArray &body)
{
    return QJsonDocument::fromJson(body).object();
}

QByteArray written(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

// A request as the current revision has one: with what it speaks in its _meta
QJsonObject modern(const QJsonValue &id, const QString &method, QJsonObject params = {}, const QString &version = QStringLiteral("2026-07-28"))
{
    params.insert(QStringLiteral("_meta"), QJsonObject {{kVersion, version}, {kCapabilities, QJsonObject()}});
    return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
}

QJsonObject legacy(const QJsonValue &id, const QString &method, const QJsonObject &params = {})
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"method", method}, {"params", params}};
}

}

class TestMcp : public QObject
{
    Q_OBJECT

    // What a tool was called with, the last time one was
    struct Seen
    {
        QByteArray operation;
        bool theme = false;
        QVariantMap params;
        QVariantMap query;
        QVariantMap body;
        int calls = 0;
    };
    Seen m_seen;
    mcp::Called m_answer {200, QVariantMap {{"ok", true}}};
    const mcp::Server m_server {QStringLiteral("simplepresenter"), QStringLiteral("Simple Presenter"), QStringLiteral("9.9")};

    mcp::Reply ask(const QJsonObject &message, const mcp::Headers &headers = {})
    {
        return mcp::handle(written(message), headers, m_server,
                           [this](const extendedapi::Operation &operation, bool theme, const QVariantMap &params, const QVariantMap &query,
                                  const QVariantMap &body) {
            m_seen = {operation.name, theme, params, query, body, m_seen.calls + 1};
            return m_answer;
        });
    }

private slots:
    void init()
    {
        m_seen = {};
        m_answer = {200, QVariantMap {{"ok", true}}};
    }

    // ---- the tools

    void theToolsAreTheOperationsThatAreOffered()
    {
        const QVariantList tools = mcp::tools();
        QStringList names;
        for (const QVariant &each : tools) {
            const QVariantMap tool = each.toMap();
            names.append(tool.value("name").toString());
            QVERIFY(!tool.value("description").toString().isEmpty());
            QCOMPARE(tool.value("inputSchema").toMap().value("type").toString(), QStringLiteral("object"));
            // (A name as MCP asks one to be: letters, digits, _ - . and no more than 128)
            QVERIFY(QRegularExpression(QStringLiteral("^[A-Za-z0-9_.-]{1,128}$")).match(names.last()).hasMatch());
        }
        // In the table's order, which is the same every time; those for programs alone left out
        int offered = 0;
        for (const extendedapi::Operation &operation : extendedapi::operations()) {
            if (operation.tool)
                QCOMPARE(names.value(offered++), QString::fromLatin1(operation.name));
        }
        QCOMPARE(names.size(), offered);
        QVERIFY(names.contains("create_presentation"));
        QVERIFY(!names.contains("get_openapi"));
        QCOMPARE(mcp::tools(), tools);
    }

    void aToolSaysWhetherItReadsAddsOrChanges()
    {
        const auto hints = [](const QString &name) {
            for (const QVariant &each : mcp::tools()) {
                if (each.toMap().value("name").toString() == name)
                    return each.toMap().value("annotations").toMap();
            }
            return QVariantMap();
        };
        QCOMPARE(hints("get_presentation").value("readOnlyHint").toBool(), true);
        QCOMPARE(hints("get_presentation").value("destructiveHint").toBool(), false);
        // What only adds is not destructive, and does more done twice
        QCOMPARE(hints("add_slides").value("readOnlyHint").toBool(), false);
        QCOMPARE(hints("add_slides").value("destructiveHint").toBool(), false);
        QCOMPARE(hints("add_slides").value("idempotentHint").toBool(), false);
        // What changes or removes writes over what was there, and done twice is done once
        QCOMPARE(hints("update_slide").value("destructiveHint").toBool(), true);
        QCOMPARE(hints("update_slide").value("idempotentHint").toBool(), true);
        QCOMPARE(hints("delete_presentation").value("destructiveHint").toBool(), true);
        // (A format can step a size by so much, which adds up)
        QCOMPARE(hints("format_text").value("idempotentHint").toBool(), false);
        QCOMPARE(hints("update_slide").value("openWorldHint").toBool(), false);
    }

    // ---- as the earlier revisions begin a conversation

    void aLegacyClientBeginsWithInitialize()
    {
        const mcp::Reply reply = ask(legacy(1, "initialize", {{"protocolVersion", "2025-06-18"}, {"capabilities", QJsonObject()},
                                                               {"clientInfo", QJsonObject {{"name", "t"}, {"version", "1"}}}}));
        QCOMPARE(reply.status, 200);
        const QJsonObject result = read(reply.body).value("result").toObject();
        QCOMPARE(read(reply.body).value("id").toInt(), 1);
        // The revision it asked for, being one of these
        QCOMPARE(result.value("protocolVersion").toString(), QStringLiteral("2025-06-18"));
        QVERIFY(result.value("capabilities").toObject().contains("tools"));
        QCOMPARE(result.value("serverInfo").toObject().value("name").toString(), QStringLiteral("simplepresenter"));
        QCOMPARE(result.value("serverInfo").toObject().value("version").toString(), QStringLiteral("9.9"));
        QVERIFY(!result.value("instructions").toString().isEmpty());
        // One line, with no line break in it, as standard output must have it
        QVERIFY(!reply.body.contains('\n'));
    }

    void aRevisionNotSpokenIsAnsweredWithTheNewestThatIs()
    {
        const QJsonObject result = read(ask(legacy(1, "initialize", {{"protocolVersion", "1999-01-01"}})).body).value("result").toObject();
        QCOMPARE(result.value("protocolVersion").toString(), mcp::legacyVersions().first());
    }

    void aNotificationIsTakenAndNotAnswered()
    {
        const mcp::Reply reply = ask({{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}});
        QCOMPARE(reply.status, 202);
        QVERIFY(reply.body.isEmpty());
        QCOMPARE(ask({{"jsonrpc", "2.0"}, {"method", "notifications/cancelled"}, {"params", QJsonObject {{"requestId", 4}}}}).status, 202);
    }

    void aLegacyClientListsAndCallsTools()
    {
        QCOMPARE(read(ask(legacy("a", "ping")).body).value("result").toObject(), QJsonObject());
        const QJsonObject listed = read(ask(legacy(2, "tools/list")).body).value("result").toObject();
        QCOMPARE(listed.value("tools").toArray().size(), mcp::tools().size());
        // (Nothing of the current revision's in an answer to an earlier one)
        QVERIFY(!listed.contains("resultType"));

        const mcp::Reply reply = ask(legacy(3, "tools/call", {{"name", "get_presentation"},
                                                             {"arguments", QJsonObject {{"presentation", "Amazing Grace"}, {"detail", "full"}}}}));
        QCOMPARE(reply.status, 200);
        QCOMPARE(m_seen.operation, QByteArray("get_presentation"));
        QCOMPARE(m_seen.params.value("presentation").toString(), QStringLiteral("Amazing Grace"));
        QCOMPARE(m_seen.query.value("detail").toString(), QStringLiteral("full"));
        QVERIFY(m_seen.body.isEmpty());
        const QJsonObject result = read(reply.body).value("result").toObject();
        QCOMPARE(result.value("isError").toBool(), false);
        QCOMPARE(result.value("content").toArray().first().toObject().value("type").toString(), QStringLiteral("text"));
        QCOMPARE(result.value("content").toArray().first().toObject().value("text").toString(), QStringLiteral("{\"ok\":true}"));
    }

    void whatIsNotServedIsNotFound()
    {
        const QJsonObject error = read(ask(legacy(5, "resources/list")).body).value("error").toObject();
        QCOMPARE(error.value("code").toInt(), -32601);
        const QJsonObject unknown = read(ask(legacy(6, "tools/call", {{"name", "no_such_tool"}})).body).value("error").toObject();
        QCOMPARE(unknown.value("code").toInt(), -32602);
        QCOMPARE(m_seen.calls, 0);
        // A tool that is for programs alone is no tool
        QCOMPARE(read(ask(legacy(7, "tools/call", {{"name", "get_openapi"}})).body).value("error").toObject().value("code").toInt(), -32602);
    }

    void severalAtOnceAreAnsweredTogether()
    {
        const QByteArray batch = QJsonDocument(QJsonArray {legacy(1, "ping"), QJsonObject {{"jsonrpc", "2.0"}, {"method", "notifications/initialized"}},
                                                           legacy(2, "tools/list")}).toJson(QJsonDocument::Compact);
        const mcp::Reply reply = mcp::handle(batch, {}, m_server, {});
        QCOMPARE(reply.status, 200);
        const QJsonArray replies = QJsonDocument::fromJson(reply.body).array();
        QCOMPARE(replies.size(), 2);
        QCOMPARE(replies.at(0).toObject().value("id").toInt(), 1);
        QCOMPARE(replies.at(1).toObject().value("id").toInt(), 2);
    }

    // ---- as the current revision has it

    void aModernClientDiscoversTheServer()
    {
        const mcp::Reply reply = ask(modern("d1", "server/discover"));
        QCOMPARE(reply.status, 200);
        const QJsonObject result = read(reply.body).value("result").toObject();
        QCOMPARE(result.value("resultType").toString(), QStringLiteral("complete"));
        QVERIFY(result.value("supportedVersions").toArray().contains(QStringLiteral("2026-07-28")));
        QVERIFY(result.value("supportedVersions").toArray().contains(QStringLiteral("2025-06-18")));
        QVERIFY(result.value("capabilities").toObject().contains("tools"));
        QCOMPARE(result.value("_meta").toObject().value(kServerInfo).toObject().value("name").toString(), QStringLiteral("simplepresenter"));
        QVERIFY(!result.value("instructions").toString().isEmpty());
        QVERIFY(result.value("ttlMs").toInt() > 0);
        QCOMPARE(result.value("cacheScope").toString(), QStringLiteral("public"));
    }

    void aModernRequestIsAnsweredByItself()
    {
        // No `initialize` first: each says what it speaks
        const QJsonObject listed = read(ask(modern(1, "tools/list")).body).value("result").toObject();
        QCOMPARE(listed.value("resultType").toString(), QStringLiteral("complete"));
        QCOMPARE(listed.value("tools").toArray().size(), mcp::tools().size());
        QVERIFY(listed.value("ttlMs").toInt() > 0);
        QCOMPARE(listed.value("cacheScope").toString(), QStringLiteral("public"));

        const QJsonObject called = read(ask(modern(2, "tools/call", {{"name", "get_workspace"}, {"arguments", QJsonObject()}})).body).value("result").toObject();
        QCOMPARE(called.value("resultType").toString(), QStringLiteral("complete"));
        QCOMPARE(called.value("isError").toBool(), false);
        QCOMPARE(called.value("_meta").toObject().value(kServerInfo).toObject().value("version").toString(), QStringLiteral("9.9"));
        QCOMPARE(m_seen.operation, QByteArray("get_workspace"));
    }

    void aRevisionThatIsNotSpokenIsRefusedWithThoseThatAre()
    {
        const mcp::Reply reply = ask(modern(1, "tools/list", {}, QStringLiteral("1900-01-01")));
        QCOMPARE(reply.status, 400);
        const QJsonObject error = read(reply.body).value("error").toObject();
        QCOMPARE(error.value("code").toInt(), -32022);
        QCOMPARE(error.value("data").toObject().value("requested").toString(), QStringLiteral("1900-01-01"));
        QVERIFY(error.value("data").toObject().value("supported").toArray().contains(QStringLiteral("2026-07-28")));
        // (An earlier revision named the modern way: it is begun with initialize, not so)
        QCOMPARE(ask(modern(2, "tools/list", {}, QStringLiteral("2025-06-18"))).status, 400);
    }

    void aModernRequestMustSayWhatTheClientCanDo()
    {
        QJsonObject request = modern(1, "tools/list");
        QJsonObject params = request.value("params").toObject();
        params.insert("_meta", QJsonObject {{kVersion, "2026-07-28"}});
        request.insert("params", params);
        const mcp::Reply reply = ask(request);
        QCOMPARE(reply.status, 400);
        QCOMPARE(read(reply.body).value("error").toObject().value("code").toInt(), -32602);
    }

    void aMethodThisRevisionHasNotIsNotFound()
    {
        const mcp::Reply reply = ask(modern(1, "initialize"));
        QCOMPARE(reply.status, 404);
        const QJsonObject error = read(reply.body).value("error").toObject();
        QCOMPARE(error.value("code").toInt(), -32601);
        // (The revisions spoken are named, a client having nothing else to go by)
        QVERIFY(error.value("data").toObject().value("supported").toArray().size() > 1);
        QCOMPARE(ask(modern(2, "resources/list")).status, 404);
        QCOMPARE(ask(modern(3, "tools/call", {{"name", "no_such_tool"}})).status, 400);
    }

    void overHttpTheHeadersMustAgreeWithTheBody()
    {
        const QJsonObject call = modern(1, "tools/call", {{"name", "get_workspace"}});
        // As they should be
        QCOMPARE(ask(call, {true, "2026-07-28", "tools/call", "get_workspace"}).status, 200);
        QCOMPARE(ask(modern(2, "tools/list"), {true, "2026-07-28", "tools/list", {}}).status, 200);
        // One missing, or saying other than the body
        for (const mcp::Headers &wrong : {mcp::Headers {true, {}, "tools/call", "get_workspace"}, mcp::Headers {true, "2026-07-28", {}, "get_workspace"},
                                          mcp::Headers {true, "2025-06-18", "tools/call", "get_workspace"},
                                          mcp::Headers {true, "2026-07-28", "tools/list", "get_workspace"},
                                          mcp::Headers {true, "2026-07-28", "tools/call", "delete_presentation"},
                                          mcp::Headers {true, "2026-07-28", "tools/call", {}}}) {
            const mcp::Reply reply = ask(call, wrong);
            QCOMPARE(reply.status, 400);
            QCOMPARE(read(reply.body).value("error").toObject().value("code").toInt(), -32020);
        }
        QCOMPARE(m_seen.calls, 1);
        // A name sent in MCP's base 64, for one that could not be sent as it is
        QCOMPARE(mcp::headerValue("=?base64?Z2V0X3dvcmtzcGFjZQ==?="), QByteArray("get_workspace"));
        QCOMPARE(mcp::headerValue("get_workspace"), QByteArray("get_workspace"));
        QCOMPARE(ask(call, {true, "2026-07-28", "tools/call", "=?base64?Z2V0X3dvcmtzcGFjZQ==?="}).status, 200);
        // By standard input there are no headers to agree
        QCOMPARE(ask(call).status, 200);
        // And a legacy client's header that names no revision spoken is refused
        QCOMPARE(ask(legacy(1, "tools/list"), {true, "1900-01-01", {}, {}}).status, 400);
        QCOMPARE(ask(legacy(1, "tools/list"), {true, "2025-06-18", {}, {}}).status, 200);
        // And a header that names the revision in which a request says its revision
        // itself, on one that does not, is at odds with it: not answered as a legacy one
        for (const QJsonObject &bare : {legacy(1, "tools/list"), legacy(1, "initialize", {{"protocolVersion", "2026-07-28"}}),
                                        legacy(1, "tools/call", {{"name", "delete_presentation"}})}) {
            const mcp::Reply reply = ask(bare, {true, "2026-07-28", {}, {}});
            QCOMPARE(reply.status, 400);
            QCOMPARE(read(reply.body).value("error").toObject().value("code").toInt(), -32020);
        }
        QCOMPARE(m_seen.calls, 3);
    }

    void whatIsNotARequestIsSaidToBeNone()
    {
        mcp::Reply reply = mcp::handle("this is not JSON", {}, m_server, {});
        QCOMPARE(reply.status, 400);
        QCOMPARE(read(reply.body).value("error").toObject().value("code").toInt(), -32700);
        QVERIFY(!read(reply.body).contains("id"));
        reply = mcp::handle("{\"id\":1,\"method\":\"tools/list\"}", {}, m_server, {});
        QCOMPARE(read(reply.body).value("error").toObject().value("code").toInt(), -32600);
        // An answer to something this never asked is let go
        QCOMPARE(mcp::handle("{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{}}", {}, m_server, {}).status, 202);
    }

    // ---- a tool's arguments, and its result

    void argumentsAreSortedIntoPathQueryAndBody()
    {
        ask(legacy(1, "tools/call", {{"name", "update_slide"},
                                    {"arguments", QJsonObject {{"presentation", "Song"}, {"slide", 3}, {"library", "Songs"}, {"text", "New words"},
                                                               {"notes", ""}, {"media", QJsonValue()}}}}));
        QCOMPARE(m_seen.operation, QByteArray("update_slide"));
        QCOMPARE(m_seen.params.value("presentation").toString(), QStringLiteral("Song"));
        // (A slide given as a number is its number)
        QCOMPARE(m_seen.params.value("slide").toString(), QStringLiteral("3"));
        QCOMPARE(m_seen.query.value("library").toString(), QStringLiteral("Songs"));
        QCOMPARE(m_seen.body.value("text").toString(), QStringLiteral("New words"));
        // What is given as nothing is still given: null takes a slide's media away
        QVERIFY(m_seen.body.contains("notes"));
        QVERIFY(m_seen.body.contains("media"));
        QVERIFY(!m_seen.body.contains("presentation"));
        QVERIFY(!m_seen.theme);
    }

    void aPartOfThePathNotGivenIsAToolsErrorAndNothingIsCalled()
    {
        const QJsonObject result = read(ask(legacy(1, "tools/call", {{"name", "update_slide"}, {"arguments", QJsonObject {{"presentation", "Song"}}}})).body)
                                       .value("result").toObject();
        QCOMPARE(result.value("isError").toBool(), true);
        QVERIFY(result.value("content").toArray().first().toObject().value("text").toString().contains(QStringLiteral("`slide`")));
        QCOMPARE(m_seen.calls, 0);
        // And so is a query's that is needed
        QCOMPARE(read(ask(legacy(2, "tools/call", {{"name", "search_presentations"}})).body).value("result").toObject().value("isError").toBool(), true);
        QCOMPARE(m_seen.calls, 0);
    }

    void aThemesSlideIsAskedForWithThemeInPlaceOfPresentation()
    {
        ask(legacy(1, "tools/call", {{"name", "update_element"},
                                    {"arguments", QJsonObject {{"theme", "Samples/Black Box"}, {"slide", "0"}, {"element", "Lyrics"}, {"x", 100}}}}));
        QCOMPARE(m_seen.operation, QByteArray("update_element"));
        QVERIFY(m_seen.theme);
        QCOMPARE(m_seen.params.value("theme").toString(), QStringLiteral("Samples/Black Box"));
        QCOMPARE(m_seen.params.value("element").toString(), QStringLiteral("Lyrics"));
        QVERIFY(!m_seen.params.contains("presentation"));
        QCOMPARE(m_seen.body.value("x").toInt(), 100);
        // Its path, as the app started with --mcp asks the API: the slash in the theme's place escaped
        QCOMPARE(extendedapi::pathOf(*extendedapi::operation("update_element"), m_seen.params, m_seen.theme),
                 QStringLiteral("/sp/v1/theme/Samples%2FBlack%20Box/slide/0/element/Lyrics"));

        // Neither, or both, is an error that says so
        for (const QJsonObject &arguments : {QJsonObject {{"slide", "0"}, {"element", "e"}},
                                             QJsonObject {{"theme", "T"}, {"presentation", "P"}, {"slide", "0"}, {"element", "e"}}}) {
            const int before = m_seen.calls;
            const QJsonObject result = read(ask(legacy(2, "tools/call", {{"name", "update_element"}, {"arguments", arguments}})).body).value("result").toObject();
            QCOMPARE(result.value("isError").toBool(), true);
            QCOMPARE(m_seen.calls, before);
        }
    }

    void anOperationThatDidNotGoThroughIsAToolsErrorWithItsReason()
    {
        m_answer = {409, QVariantMap {{"error", "It is open in the editor"}}};
        const QJsonObject result = read(ask(legacy(1, "tools/call", {{"name", "delete_presentation"}, {"arguments", QJsonObject {{"presentation", "x"}}}})).body)
                                       .value("result").toObject();
        QCOMPARE(result.value("isError").toBool(), true);
        QCOMPARE(result.value("content").toArray().first().toObject().value("text").toString(), QStringLiteral("It is open in the editor"));

        // The reference is given as the text it is; a list as JSON
        QCOMPARE(mcp::toolResult({200, QVariantMap {{"reference", "# Elements\nwords"}}}).value("content").toList().first().toMap().value("text").toString(),
                 QStringLiteral("# Elements\nwords"));
        QCOMPARE(mcp::toolResult({200, QVariantList {1, 2}}).value("content").toList().first().toMap().value("text").toString(), QStringLiteral("[1,2]"));
        QCOMPARE(mcp::toolResult({201, QVariantMap {{"id", "x"}}}).value("isError").toBool(), false);
        QVERIFY(!mcp::toolResult({500, QVariant()}).value("content").toList().first().toMap().value("text").toString().isEmpty());
    }

    // ---- the app started with --mcp

    void itIsAskedForByItsFlag()
    {
        char program[] = "SimplePresenter";
        char flag[] = "--mcp";
        char other[] = "--selftest";
        char *with[] = {program, flag};
        char *without[] = {program, other};
        QVERIFY(mcpstdio::asked(2, with));
        QVERIFY(!mcpstdio::asked(2, without));
        QVERIFY(!mcpstdio::asked(1, with));
    }

    void anHttpResponseIsReadWhenAllOfItHasCome()
    {
        mcpstdio::Response response;
        const QByteArray whole = "HTTP/1.1 201 Created\r\ncontent-type: application/json\r\nContent-Length: 11\r\n\r\n{\"ok\":true}";
        // In pieces, as a network brings one
        QVERIFY(!mcpstdio::parseResponse(whole.left(20), &response));
        QVERIFY(!mcpstdio::parseResponse(whole.left(whole.size() - 3), &response));
        QVERIFY(mcpstdio::parseResponse(whole, &response));
        QCOMPARE(response.status, 201);
        QCOMPARE(response.body, QByteArray("{\"ok\":true}"));
        // One with no body says nothing of a length
        QVERIFY(mcpstdio::parseResponse("HTTP/1.1 404 Not Found\r\ndate: now\r\n\r\n", &response));
        QCOMPARE(response.status, 404);
        QVERIFY(response.body.isEmpty());
    }

    void anAppThatIsNotThereIsNotReached()
    {
        // (A port nothing listens on, on this computer)
        const mcpstdio::Response response = mcpstdio::request(QStringLiteral("127.0.0.1"), 9, "GET", QStringLiteral("/sp/v1/workspace"), {}, 500);
        QCOMPARE(response.status, 0);
    }
};

QTEST_GUILESS_MAIN(TestMcp)
#include "tst_mcp.moc"
