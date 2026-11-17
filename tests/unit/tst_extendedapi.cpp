// The extended API's table of operations and its rules for who may ask
// (src/extendedapi.h): that the table hangs together (each operation named once, each
// path its own), a request found by its method and path, what an MCP tool and the
// OpenAPI description are made of it, the reference page (docs/features/extended-api.md)
// having every operation, and which requests are turned away.

#include "extendedapi.h"

#include <QFile>
#include <QSet>
#include <QTest>

using namespace extendedapi;

class TestExtendedApi : public QObject
{
    Q_OBJECT

private slots:
    // ---- the table

    void everyOperationIsNamedOnceAndSaysWhatItIs()
    {
        QSet<QByteArray> names;
        QSet<QByteArray> routes;
        for (const Operation &each : operations()) {
            QVERIFY2(!names.contains(each.name), each.name);
            names.insert(each.name);
            QVERIFY2(!each.title.isEmpty() && !each.description.isEmpty(), each.name);
            QVERIFY2(QList<QByteArray>({"GET", "POST", "PUT", "PATCH", "DELETE"}).contains(each.method), each.name);
            // (A method and a path are one operation's only)
            for (const QByteArray &pattern : {each.pattern, each.themePattern}) {
                if (pattern.isEmpty())
                    continue;
                const QByteArray route = each.method + ' ' + pattern;
                QVERIFY2(!routes.contains(route), route);
                routes.insert(route);
            }
            // What reads is a GET, and nothing else is
            QCOMPARE(each.kind == Operation::Read, each.method == "GET");
            QCOMPARE(each.kind == Operation::Remove, each.method == "DELETE");
        }
        QVERIFY(operation("create_presentation"));
        QVERIFY(!operation("no_such_thing"));
    }

    void everyPartOfAPathIsAFieldAndEveryFieldSaysWhatItIs()
    {
        for (const Operation &each : operations()) {
            QStringList inPath;
            for (const QString &part : QString::fromLatin1(each.pattern).split(u'/')) {
                if (part.startsWith(u'{'))
                    inPath.append(part.mid(1, part.size() - 2));
            }
            QStringList fields;
            for (const Field &field : each.fields) {
                QVERIFY2(!field.description.isEmpty(), each.name + ' ' + field.name);
                QVERIFY2(!fields.contains(QString::fromLatin1(field.name)), each.name + ' ' + field.name);
                fields.append(QString::fromLatin1(field.name));
                if (field.where == Field::Path)
                    QVERIFY2(inPath.contains(QString::fromLatin1(field.name)), each.name + ' ' + field.name);
                // (A GET and a DELETE have no body to be given anything in)
                if (each.method == "GET" || each.method == "DELETE")
                    QVERIFY2(field.where != Field::Body, each.name + ' ' + field.name);
            }
            for (const QString &part : inPath)
                QVERIFY2(fields.contains(part), each.name + ' ' + part.toLatin1());
        }
    }

    // ---- finding the operation a request is for

    void aRequestIsFoundByItsMethodAndPath()
    {
        Match found = match("GET", QStringLiteral("/sp/v1/workspace"));
        QVERIFY(found);
        QCOMPARE(found.operation->name, QByteArray("get_workspace"));

        found = match("PATCH", QStringLiteral("/sp/v1/presentation/Amazing%20Grace/slide/3"));
        QVERIFY(found);
        QCOMPARE(found.operation->name, QByteArray("update_slide"));
        QCOMPARE(found.params.value("presentation").toString(), QStringLiteral("Amazing Grace"));
        QCOMPARE(found.params.value("slide").toString(), QStringLiteral("3"));
        QVERIFY(!found.theme);

        // The same path with another method is another operation, or none
        QCOMPARE(match("DELETE", QStringLiteral("/sp/v1/presentation/x/slide/3")).operation->name, QByteArray("delete_slide"));
        QVERIFY(!match("PUT", QStringLiteral("/sp/v1/presentation/x/slide/3")));
        // Not under the prefix: none, whatever it is
        QVERIFY(!match("GET", QStringLiteral("/v1/workspace")));
        QVERIFY(!match("GET", QStringLiteral("/sp/v1/")));
        QVERIFY(!match("GET", QStringLiteral("/sp/v1/workspace/more")));
    }

    void aThemesSlideIsTheSameOperationByItsOwnPath()
    {
        // (A theme in a folder: the slash in its place is written %2F, and is not one of the path's)
        const Match found = match("PATCH", QStringLiteral("/sp/v1/theme/Samples%2FBlack%20Box/slide/0/element/Lyrics"));
        QVERIFY(found);
        QCOMPARE(found.operation->name, QByteArray("update_element"));
        QVERIFY(found.theme);
        QCOMPARE(found.params.value("theme").toString(), QStringLiteral("Samples/Black Box"));
        QCOMPARE(found.params.value("element").toString(), QStringLiteral("Lyrics"));
        // And a theme's own slide, which is another operation than a presentation's
        QCOMPARE(match("PATCH", QStringLiteral("/sp/v1/theme/Black%20Box/slide/0")).operation->name, QByteArray("update_theme_slide"));
        QCOMPARE(match("GET", QStringLiteral("/sp/v1/theme/Black%20Box/slide/0")).operation->name, QByteArray("get_slide"));
    }

    void aPathIsMadeOfAnOperationAndItsPartsAndFoundAgain()
    {
        for (const Operation &each : operations()) {
            QVariantMap values;
            for (const Field &field : each.fields) {
                if (field.where == Field::Path)
                    values.insert(QString::fromLatin1(field.name), QStringLiteral("a b/c"));
            }
            const QString path = pathOf(each, values);
            QVERIFY2(path.startsWith(prefix()), each.name);
            // (A part with a space or a slash in it is escaped, and is one part still)
            const Match found = match(each.method, path);
            QVERIFY2(found, each.name + ' ' + path.toLatin1());
            QCOMPARE(found.operation->name, each.name);
            for (auto it = values.constBegin(); it != values.constEnd(); ++it)
                QCOMPARE(found.params.value(it.key()).toString(), QStringLiteral("a b/c"));
        }
        // A part with nothing given for it: no path
        QVERIFY(pathOf(*operation("get_presentation"), {}).isEmpty());
        QCOMPARE(pathOf(*operation("get_presentation"), {{"presentation", "Song 1"}}), QStringLiteral("/sp/v1/presentation/Song%201"));
        QCOMPARE(pathOf(*operation("delete_element"), {{"theme", "T"}, {"slide", "0"}, {"element", "e"}}, true),
                 QStringLiteral("/sp/v1/theme/T/slide/0/element/e"));
    }

    void thisAppsOwnPathsAreToldFromProPresenters()
    {
        QVERIFY(own(QStringLiteral("/sp/v1/workspace")));
        QVERIFY(own(QStringLiteral("/sp/v1/nothing/there")));
        QVERIFY(own(QStringLiteral("/mcp")));
        QVERIFY(!own(QStringLiteral("/v1/status/slide")));
        QVERIFY(!own(QStringLiteral("/version")));
        QVERIFY(!own(QStringLiteral("/mcp/more")));

        QCOMPARE(methodsOf(QStringLiteral("/sp/v1/playlist/x")), QList<QByteArray>({"GET", "PATCH", "DELETE"}));
        QVERIFY(methodsOf(QStringLiteral("/sp/v1/nothing")).isEmpty());
    }

    // ---- what is made of the table

    void aToolTakesTheOperationsFields()
    {
        const QVariantMap schema = inputSchema(*operation("update_slide"));
        QCOMPARE(schema.value("type").toString(), QStringLiteral("object"));
        const QVariantMap properties = schema.value("properties").toMap();
        QVERIFY(properties.contains("presentation"));
        QVERIFY(properties.contains("slide"));
        QVERIFY(properties.contains("library"));
        QVERIFY(properties.contains("text"));
        QCOMPARE(properties.value("enabled").toMap().value("type").toString(), QStringLiteral("boolean"));
        // (What may be one of several kinds says no type)
        QVERIFY(!properties.value("media").toMap().contains("type"));
        QCOMPARE(schema.value("required").toStringList(), QStringList({"presentation", "slide"}));

        // An operation that is for a theme's slide too takes `theme`, and asks for neither outright
        const QVariantMap element = inputSchema(*operation("add_element"));
        QVERIFY(element.value("properties").toMap().contains("theme"));
        QCOMPARE(element.value("required").toStringList(), QStringList({"slide"}));
        // One of a few words lists them
        QCOMPARE(element.value("properties").toMap().value("kind").toMap().value("enum").toStringList().first(), QStringLiteral("text"));
        // A list says what it is a list of
        QCOMPARE(inputSchema(*operation("apply_theme")).value("properties").toMap().value("slides").toMap().value("items").toMap().value("type").toString(),
                 QStringLiteral("string"));
    }

    void theOpenApiDescriptionHasEveryPath()
    {
        const QVariantMap document = openApi(QStringLiteral("0.1"));
        QCOMPARE(document.value("openapi").toString(), QStringLiteral("3.1.0"));
        QCOMPARE(document.value("info").toMap().value("version").toString(), QStringLiteral("0.1"));
        const QVariantMap paths = document.value("paths").toMap();
        QSet<QString> ids;
        for (const Operation &each : operations()) {
            for (const QByteArray &pattern : {each.pattern, each.themePattern}) {
                if (pattern.isEmpty())
                    continue;
                const QVariantMap made = paths.value(prefix() + QString::fromLatin1(pattern)).toMap().value(QString::fromLatin1(each.method).toLower()).toMap();
                QVERIFY2(!made.isEmpty(), each.name);
                const QString id = made.value("operationId").toString();
                QVERIFY2(!ids.contains(id), id.toLatin1());
                ids.insert(id);
            }
        }
        // A path's parts are parameters "in: path", and a body's fields its schema's
        const QVariantMap update = paths.value("/sp/v1/presentation/{presentation}/slide/{slide}").toMap().value("patch").toMap();
        QStringList inPath;
        for (const QVariant &each : update.value("parameters").toList()) {
            if (each.toMap().value("in").toString() == QLatin1String("path"))
                inPath.append(each.toMap().value("name").toString());
        }
        QCOMPARE(inPath, QStringList({"presentation", "slide"}));
        QVERIFY(update.value("requestBody").toMap().value("content").toMap().value("application/json").toMap().value("schema").toMap()
                    .value("properties").toMap().contains("notes"));
        // A theme's path has `theme` where a presentation's has `presentation`
        const QVariantMap themed = paths.value("/sp/v1/theme/{theme}/slide/{slide}/elements").toMap().value("post").toMap();
        QCOMPARE(themed.value("parameters").toList().first().toMap().value("name").toString(), QStringLiteral("theme"));
    }

    void theReferencePageHasEveryOperation()
    {
        // The page people and models read is kept in step with the table by this: an
        // operation it does not name by its tool's name and its path is one it leaves out
        QFile page(QStringLiteral(REFERENCE_PAGE));
        QVERIFY2(page.open(QIODevice::ReadOnly), REFERENCE_PAGE);
        const QString text = QString::fromUtf8(page.readAll());
        QStringList missing;
        for (const Operation &each : operations()) {
            if (!text.contains(u'`' + QString::fromLatin1(each.name) + u'`'))
                missing.append(QString::fromLatin1(each.name));
            if (!text.contains(QString::fromLatin1(each.method) + QStringLiteral(" /sp/v1/") + QString::fromLatin1(each.pattern)))
                missing.append(QString::fromLatin1(each.method + ' ' + each.pattern));
        }
        QVERIFY2(missing.isEmpty(), qPrintable(QStringLiteral("docs/features/extended-api.md does not have: ") + missing.join(QStringLiteral(", "))));
    }

    void whatMakesSomethingIsSaidToAnswer201()
    {
        // The description says 201 for what makes something, as that answers; the rest 200
        const QVariantMap paths = openApi(QStringLiteral("1.2")).value("paths").toMap();
        QVERIFY(paths.value("/sp/v1/playlists").toMap().value("post").toMap().value("responses").toMap().contains("201"));
        QVERIFY(paths.value("/sp/v1/presentation/{presentation}/slides").toMap().value("post").toMap().value("responses").toMap().contains("201"));
        QVERIFY(paths.value("/sp/v1/presentation/{presentation}/theme").toMap().value("post").toMap().value("responses").toMap().contains("200"));
        QVERIFY(paths.value("/sp/v1/playlists").toMap().value("get").toMap().value("responses").toMap().contains("200"));
    }

    // ---- who may ask

    void offItAnswersNobody()
    {
        const Access access {false, true, QStringLiteral("presenter"), {}};
        QVERIFY(off(access));
        QVERIFY(!refusal({true, {}, "127.0.0.1:50001"}, access).isEmpty());
    }

    void onItAnswersThisComputerAndOthersOnlyIfOpenedToThem()
    {
        const Access here {true, false, QStringLiteral("presenter"), {}};
        QVERIFY(!off(here));
        QVERIFY(refusal({true, {}, "127.0.0.1:50001"}, here).isEmpty());
        QVERIFY(refusal({true, {}, "localhost:50001"}, here).isEmpty());
        QVERIFY(!refusal({false, {}, "192.168.1.20:50001"}, here).isEmpty());

        const Access network {true, true, QStringLiteral("presenter"), {}};
        QVERIFY(refusal({false, {}, "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(refusal({false, {}, "presenter.local:50001"}, network).isEmpty());
    }

    void aWebPageFromElsewhereIsTurnedAway()
    {
        const Access here {true, false, QStringLiteral("presenter"), {QStringLiteral("192.168.1.20")}};
        // No Origin: a program. A page of this computer's own: answered, by whichever
        // of its addresses or names it was served at.
        QVERIFY(refusal({true, {}, "127.0.0.1:50001"}, here).isEmpty());
        QVERIFY(refusal({true, "http://localhost:3000", "127.0.0.1:50001"}, here).isEmpty());
        QVERIFY(refusal({true, "http://127.0.0.1:8080", "127.0.0.1:50001"}, here).isEmpty());
        QVERIFY(refusal({true, "http://[::1]:8080", "[::1]:50001"}, here).isEmpty());
        QVERIFY(refusal({true, "http://192.168.1.20:50001", "192.168.1.20:50001"}, here).isEmpty());
        QVERIFY(refusal({true, "http://presenter:50001", "presenter:50001"}, here).isEmpty());
        QVERIFY(refusal({true, "http://Presenter.local:50001", "presenter.local:50001"}, here).isEmpty());
        // A page from the internet, open in a browser on this computer: not
        QVERIFY(!refusal({true, "https://example.com", "127.0.0.1:50001"}, here).isEmpty());
        // Nor one that will not say where it is from (a page in a sandbox, a form sent
        // without its origin), which is from anywhere
        QVERIFY(!refusal({true, "null", "127.0.0.1:50001"}, here).isEmpty());
        // Nor a page served by another computer of the network, while only this one is let in
        QVERIFY(!refusal({true, "http://192.168.1.5:3000", "127.0.0.1:50001"}, here).isEmpty());
        QVERIFY(!refusal({true, "http://nas.local:3000", "127.0.0.1:50001"}, here).isEmpty());
        // Nor one whose name only begins as this computer's does
        QVERIFY(!refusal({true, "http://presenter.example.com", "127.0.0.1:50001"}, here).isEmpty());
        QVERIFY(!refusal({true, "http://presenter.local.example.com", "127.0.0.1:50001"}, here).isEmpty());

        const Access network {true, true, QStringLiteral("presenter"), {QStringLiteral("192.168.1.20")}};
        QVERIFY(refusal({false, "http://192.168.1.5:3000", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(refusal({false, "http://10.0.0.7", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(refusal({false, "http://nas.local:8000", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(refusal({false, "http://intranet", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(!refusal({false, "https://example.com", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(!refusal({false, "http://presenter.example.com", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(!refusal({false, "null", "192.168.1.20:50001"}, network).isEmpty());
        // (An address of the internet's is not one a local network gives out)
        QVERIFY(!refusal({false, "http://203.0.113.9", "192.168.1.20:50001"}, network).isEmpty());
        QVERIFY(!originAllowed("not an origin", network));
    }

    void itIsAskedForByANameALocalNetworkCouldGiveIt()
    {
        // A name of the internet's that only points here (a page that has had its own
        // name made to: DNS rebinding) is not this computer's, though it begin as its
        // name does
        const Access network {true, true, QStringLiteral("presenter"), {}};
        QVERIFY(!refusal({true, {}, "evil.example.com:50001"}, network).isEmpty());
        QVERIFY(!hostAllowed("presenter.example.com", QStringLiteral("presenter")));
        QVERIFY(!hostAllowed("presenter.local.example.com:50001", QStringLiteral("presenter")));
        QVERIFY(!hostAllowed("example.com", QString()));
        QVERIFY(hostAllowed("127.0.0.1:50001", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("192.168.1.20", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("[fe80::1]:50001", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("localhost:50001", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("Presenter:50001", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("presenter.lan:50001", QStringLiteral("presenter.local")));
        QVERIFY(hostAllowed("presenter.church.example:50001", QStringLiteral("presenter.church.example")));
        // (One word, or a local network's ending: what a network's own names are)
        QVERIFY(hostAllowed("show-laptop:50001", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("show.home.arpa", QStringLiteral("presenter")));
        QVERIFY(hostAllowed("", QStringLiteral("presenter")));
    }
};

QTEST_GUILESS_MAIN(TestExtendedApi)
#include "tst_extendedapi.moc"
