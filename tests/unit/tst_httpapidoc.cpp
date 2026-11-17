// The API's pages of documentation (src/httpapidoc.h): that ProPresenter's API is
// described as the app answers it and no further, every route with a line of its own;
// that the description is one the page can draw; and which files the page is made of.
// The description is held to the table of routes (httpapi::routes()) and to the
// window's answerer (qml/HttpApiControl.qml), whose source is read: a route with nothing
// to answer it must not be on the page, and one that is answered must be.

#include "httpapidoc.h"

#include "httpapi.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QTest>

namespace {

QString keyOf(const httpapi::Route &route)
{
    return QString::fromLatin1(route.method) + u' ' + route.pattern;
}

// The parts of a path that stand for something, by name
QStringList partsOf(const QString &path)
{
    QStringList names;
    for (const QString &part : path.split(u'/')) {
        if (part.startsWith(u'{') && part.endsWith(u'}'))
            names.append(part.mid(1, part.size() - 2));
    }
    return names;
}

}

class TestHttpApiDoc : public QObject
{
    Q_OBJECT

private slots:
    void everyRouteHasALineOrIsSaidToAnswerNothing()
    {
        const QStringList unanswered = httpapidoc::unanswered();
        QSet<QString> routes;
        for (const httpapi::Route &route : httpapi::routes()) {
            routes.insert(keyOf(route));
            const QString said = httpapidoc::summary(route);
            if (unanswered.contains(keyOf(route))) {
                QVERIFY2(said.isEmpty(), qPrintable(keyOf(route) + " answers nothing, and has a line"));
                continue;
            }
            QVERIFY2(!said.isEmpty(), qPrintable(keyOf(route) + " has no line in httpapidoc.cpp"));
            // A sentence, or sentences, and no more than fits a row of the page well
            QVERIFY2(said.endsWith(u'.') && said.at(0).isUpper() && said.size() < 330, qPrintable(keyOf(route) + ": " + said));
            QVERIFY2(httpapidoc::tags().contains(httpapidoc::tag(route)), qPrintable(keyOf(route) + " is under no heading"));
        }
        // And no line for a route there is not (one taken out of the table, or renamed)
        for (const QString &key : httpapidoc::summarised())
            QVERIFY2(routes.contains(key), qPrintable(key + " has a line, and is no route"));
        for (const QString &key : unanswered)
            QVERIFY2(routes.contains(key), qPrintable(key + " is said to answer nothing, and is no route"));
    }

    void whatIsOnThePageIsWhatTheAppAnswers()
    {
        // The window's answerer has a case for each route it answers, named by the
        // route's pattern (and its method, for one that is not a GET). The server
        // answers three itself.
        QFile source(QStringLiteral(HANDLER_SOURCE));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(source.readAll());
        QSet<QString> cases;
        static const QRegularExpression label(QStringLiteral("case \"([^\"]+)\":"));
        for (QRegularExpressionMatchIterator it = label.globalMatch(text); it.hasNext();)
            cases.insert(it.next().captured(1));
        QVERIFY(cases.size() > 150);
        const QStringList byServer {QStringLiteral("GET version"), QStringLiteral("POST status/updates"), QStringLiteral("GET timer/system_time")};
        for (const httpapi::Route &route : httpapi::routes()) {
            const QString name = route.method == "GET" ? route.pattern : route.pattern + u'#' + QString::fromLatin1(route.method);
            const bool answered = cases.contains(name) || byServer.contains(keyOf(route));
            QVERIFY2(answered == !httpapidoc::summary(route).isEmpty(),
                     qPrintable(keyOf(route) + (answered ? " is answered, and is not on the page" : " is on the page, and nothing answers it")));
        }
    }

    void theDescriptionIsOfThoseRoutesAndNoOthers()
    {
        const QVariantMap made = httpapidoc::openApi(QStringLiteral("9.9"), QStringLiteral("/sp/v1/doc/index.html"));
        QVERIFY(made.value("openapi").toString().startsWith(QStringLiteral("3.0.")));
        QCOMPARE(made.value("info").toMap().value("version").toString(), QStringLiteral("9.9"));
        // Asked of whatever served the page
        QCOMPARE(made.value("servers").toList().value(0).toMap().value("url").toString(), QStringLiteral("/"));
        const QVariantMap paths = made.value("paths").toMap();

        int operations = 0;
        QSet<QString> ids;
        QStringList headings;
        for (const QVariant &heading : made.value("tags").toList())
            headings.append(heading.toMap().value("name").toString());
        QCOMPARE(headings, httpapidoc::tags());
        QSet<QString> used;
        for (auto path = paths.constBegin(); path != paths.constEnd(); ++path) {
            QVERIFY2(path.key() == QLatin1String("/version") || path.key().startsWith(QLatin1String("/v1/")), qPrintable(path.key()));
            const QVariantMap methods = path.value().toMap();
            for (auto method = methods.constBegin(); method != methods.constEnd(); ++method) {
                ++operations;
                const QVariantMap operation = method.value().toMap();
                const QByteArray where = (method.key().toUpper() + u' ' + path.key()).toUtf8();
                // It is a route, by that method
                const QString pattern = path.key() == QLatin1String("/version") ? QStringLiteral("version") : path.key().mid(4);
                bool isRoute = false;
                for (const httpapi::Route &route : httpapi::routes())
                    isRoute = isRoute || (route.pattern == pattern && QString::fromLatin1(route.method).toLower() == method.key());
                QVERIFY2(isRoute, where.constData());
                QVERIFY2(!operation.value("summary").toString().isEmpty(), where.constData());
                // (It answers with something, or with nothing but that it was done: never both)
                QVERIFY2(operation.value("responses").toMap().contains("200") != operation.value("responses").toMap().contains("204"), where.constData());
                const QString id = operation.value("operationId").toString();
                QVERIFY2(!id.isEmpty() && !ids.contains(id), where.constData());
                ids.insert(id);
                const QStringList tags = operation.value("tags").toStringList();
                QVERIFY2(tags.size() == 1 && headings.contains(tags.first()), where.constData());
                used.insert(tags.first());
                // The parts of its path, each said once, and nothing said to be of the path that is not
                QStringList inPath;
                for (const QVariant &each : operation.value("parameters").toList()) {
                    const QVariantMap parameter = each.toMap();
                    QVERIFY2(!parameter.value("description").toString().isEmpty() && parameter.contains("schema"), where.constData());
                    if (parameter.value("in").toString() == QLatin1String("path")) {
                        QVERIFY2(parameter.value("required").toBool(), where.constData());
                        inPath.append(parameter.value("name").toString());
                    }
                }
                QVERIFY2(inPath == partsOf(path.key()), where.constData());
            }
        }
        // Every route but those that answer nothing; and no heading with nothing under it
        QCOMPARE(operations, int(httpapi::routes().size() - httpapidoc::unanswered().size()));
        QCOMPARE(int(used.size()), int(headings.size()));
        QVERIFY(!paths.contains("/v1/presentation/chord_chart/updates"));
        QVERIFY(paths.value("/v1/transport/{layer}/auto_advance").toMap().contains("get"));
        QVERIFY(!paths.value("/v1/transport/{layer}/auto_advance").toMap().contains("delete"));
    }

    void aRouteSaysWhatItTakes()
    {
        const QVariantMap paths = httpapidoc::openApi(QStringLiteral("9.9"), QString()).value("paths").toMap();
        const auto operation = [&paths](const char *path, const char *method) {
            return paths.value(QString::fromLatin1(path)).toMap().value(QString::fromLatin1(method)).toMap();
        };
        const auto parameter = [](const QVariantMap &of, const char *name) {
            for (const QVariant &each : of.value("parameters").toList()) {
                if (each.toMap().value("name").toString() == QLatin1String(name))
                    return each.toMap();
            }
            return QVariantMap();
        };
        // What can be followed says so; what cannot, does not
        QVERIFY(!parameter(operation("/v1/status/slide", "get"), "chunked").isEmpty());
        QVERIFY(parameter(operation("/v1/libraries", "get"), "chunked").isEmpty());
        // A picture's size and kind
        const QVariantMap picture = operation("/v1/presentation/{uuid}/thumbnail/{index}", "get");
        QCOMPARE(parameter(picture, "quality").value("schema").toMap().value("default").toInt(), 400);
        QCOMPARE(parameter(picture, "thumbnail_type").value("schema").toMap().value("enum").toStringList(), QStringList({"jpeg", "png"}));
        QVERIFY(picture.value("responses").toMap().value("200").toMap().value("content").toMap().contains("image/jpeg"));
        // A slide's index is a slide's, an item's an item's
        QVERIFY(parameter(picture, "index").value("description").toString().contains(QLatin1String("slide")));
        QVERIFY(parameter(operation("/v1/playlist/{playlist_id}/{index}/trigger", "get"), "index").value("description").toString().contains(QLatin1String("item")));
        // The words a part of a path is one of
        QCOMPARE(parameter(operation("/v1/clear/layer/{layer}", "get"), "layer").value("schema").toMap().value("enum").toStringList().size(), 7);
        QCOMPARE(parameter(operation("/v1/transport/{layer}/current", "get"), "layer").value("schema").toMap().value("enum").toStringList(),
                 QStringList({"presentation", "announcement", "audio"}));
        QCOMPARE(parameter(operation("/v1/timer/{id}/{operation}", "get"), "operation").value("schema").toMap().value("enum").toStringList(),
                 QStringList({"start", "stop", "reset"}));
        QCOMPARE(parameter(operation("/v1/presentation/active/timeline/{operation}", "get"), "operation").value("schema").toMap().value("enum").toStringList(),
                 QStringList({"play", "pause", "rewind"}));
        // What takes a body says what, with one to try; what takes none says nothing of one
        for (const char *path : {"/v1/status/audience_screens", "/v1/stage/layout_map", "/v1/stage/message", "/v1/timer/{id}", "/v1/transport/{layer}/time"}) {
            const QVariantMap body = operation(path, "put").value("requestBody").toMap();
            QVERIFY2(!body.value("description").toString().isEmpty() && body.value("content").toMap().contains("application/json"), path);
            QVERIFY2(!operation(path, "get").contains("requestBody"), path);
        }
        QCOMPARE(operation("/v1/status/audience_screens", "put").value("requestBody").toMap().value("content").toMap().value("application/json").toMap()
                     .value("schema").toMap().value("type").toString(), QStringLiteral("boolean"));
        QVERIFY(operation("/v1/status/updates", "post").contains("requestBody"));
        QCOMPARE(operation("/v1/message/{id}/trigger", "post").value("requestBody").toMap().value("required").toBool(), false);
        QVERIFY(!operation("/v1/stage/message", "delete").contains("requestBody"));
        // What reads answers JSON; what operates the show answers nothing
        QVERIFY(operation("/v1/looks", "get").value("responses").toMap().value("200").toMap().contains("content"));
        QVERIFY(operation("/v1/look/{id}/trigger", "get").value("responses").toMap().contains("204"));
        QVERIFY(!operation("/v1/look/{id}/trigger", "get").value("responses").toMap().contains("200"));
        QVERIFY(operation("/v1/stage/message", "put").value("responses").toMap().contains("204"));
    }

    void itsOpeningWordsSayWhatItIsAndWhereTheOtherPageIs()
    {
        const QString with = httpapidoc::openApi(QStringLiteral("1"), QStringLiteral("/sp/v1/doc/index.html")).value("info").toMap().value("description").toString();
        const QString without = httpapidoc::openApi(QStringLiteral("1"), QString()).value("info").toMap().value("description").toString();
        for (const QString &words : {with, without}) {
            QVERIFY(words.contains(QLatin1String("only what the app answers")));
            QVERIFY(words.contains(QLatin1String("not ProPresenter")));
            QVERIFY(words.contains(QLatin1String("not as a product")));
        }
        QVERIFY(with.contains(QLatin1String("(/sp/v1/doc/index.html)")));
        QVERIFY(!without.contains(QLatin1String("/sp/v1/doc")) && without.contains(QLatin1String("switched off")));
    }

    void thePageLoadsItAsAScript()
    {
        // As ProPresenter's page loads its own: a line of JavaScript that sets openapi_spec
        QVariantMap description = httpapidoc::openApi(QStringLiteral("9.9"), QString());
        // (with, in a string, what ends a line in JavaScript and not in JSON, and what would end a script element)
        description.insert(QStringLiteral("x-test"), QString(QChar(0x2028)) + QStringLiteral("</script>") + QChar(0x2029));
        const QByteArray script = httpapidoc::asScript(description);
        QVERIFY(script.startsWith("var openapi_spec = {"));
        QVERIFY(script.endsWith("};\n"));
        QVERIFY(!script.contains("\xe2\x80\xa8") && !script.contains("\xe2\x80\xa9") && !script.contains("</script"));
        // What is between is the description, as JSON reads it
        QJsonParseError error;
        const QJsonDocument read = QJsonDocument::fromJson(script.mid(int(qstrlen("var openapi_spec = ")), script.size() - int(qstrlen("var openapi_spec = ")) - 2), &error);
        QCOMPARE(error.error, QJsonParseError::NoError);
        QCOMPARE(read.object().value("paths").toObject().size(), description.value("paths").toMap().size());
        QCOMPARE(read.object().value("x-test").toString(), QString(QChar(0x2028)) + QStringLiteral("</script>") + QChar(0x2029));
    }

    void thePagesFilesAreTheAppsOwn()
    {
        const httpapidoc::File page = httpapidoc::file(QStringLiteral("index.html"));
        QVERIFY(bool(page));
        QCOMPARE(page.resource, QStringLiteral(":/apidoc/index.html"));
        QVERIFY(page.type.startsWith("text/html"));
        QCOMPARE(httpapidoc::file(QStringLiteral("swagger-ui.css")).type, QByteArray("text/css"));
        QCOMPARE(httpapidoc::file(QStringLiteral("swagger-ui-bundle.js")).type, QByteArray("text/javascript"));
        // Swagger UI's licence and its notice go with it
        QVERIFY(bool(httpapidoc::file(QStringLiteral("LICENSE"))));
        QVERIFY(bool(httpapidoc::file(QStringLiteral("NOTICE"))));
        // (The description is made, not kept; and nothing else is there)
        QVERIFY(!httpapidoc::file(QStringLiteral("swagger.json")));
        QVERIFY(!httpapidoc::file(QStringLiteral("swagger-ui-standalone-preset.js")));
        QVERIFY(!httpapidoc::file(QStringLiteral("../SimplePresenter.conf")));
        QVERIFY(!httpapidoc::file(QString()));
        // Each is one of the files the app carries (CMakeLists.txt, the apidoc resource)
        QFile cmake(QStringLiteral(CMAKE_LISTS));
        QVERIFY(cmake.open(QIODevice::ReadOnly));
        const QByteArray lists = cmake.readAll();
        for (const char *name : {"swagger-ui.css", "swagger-ui-bundle.js", "swagger-ui-bundle.js.LICENSE.txt", "LICENSE", "NOTICE"})
            QVERIFY2(lists.contains(QByteArray("third_party/swagger-ui/") + name), name);
        QVERIFY(lists.contains("src/apidoc.html PROPERTIES QT_RESOURCE_ALIAS index.html"));
    }

    void aPathIsOfThePagesFolderOrIsNot()
    {
        bool redirect = false;
        // The folder by its own name is sent on
        QVERIFY(!httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/doc"), &redirect).isNull());
        QVERIFY(redirect);
        // With a slash after it, it names nothing (which ProPresenter answers 404)
        QString name = httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/doc/"), &redirect);
        QVERIFY(!name.isNull() && name.isEmpty() && !redirect);
        QCOMPARE(httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/doc/index.html"), &redirect), QStringLiteral("index.html"));
        QVERIFY(!redirect);
        QCOMPARE(httpapidoc::nameIn(QStringLiteral("/sp/v1/doc"), QStringLiteral("/sp/v1/doc/swagger.json"), &redirect), QStringLiteral("swagger.json"));
        // Not of the folder: another path that begins the same, a folder within it, a way out of it
        QVERIFY(httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/docs"), &redirect).isNull());
        QVERIFY(httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/document/1"), &redirect).isNull());
        QVERIFY(httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/doc/a/b.js"), &redirect).isNull());
        QVERIFY(httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/doc/..%2Fx"), &redirect).isNull());
        QVERIFY(httpapidoc::nameIn(QStringLiteral("/v1/doc"), QStringLiteral("/v1/looks"), &redirect).isNull());
        QVERIFY(!redirect);
    }
};

QTEST_GUILESS_MAIN(TestHttpApiDoc)
#include "tst_httpapidoc.moc"
