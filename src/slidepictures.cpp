#include "slidepictures.h"

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace slidepictures {

int qualityOf(const QString &asked)
{
    bool ok = false;
    const int quality = asked.trimmed().toInt(&ok);
    if (!ok || quality <= 0)
        return 400;
    return std::clamp(quality, 16, 1920);
}

bool pngAsked(const QString &thumbnailType, const QByteArray &accept)
{
    const QString type = thumbnailType.trimmed().toLower();
    if (type == QLatin1String("png"))
        return true;
    if (type == QLatin1String("jpeg") || type == QLatin1String("jpg"))
        return false;
    const QByteArray asked = accept.toLower();
    const qsizetype png = asked.indexOf("image/png");
    const qsizetype jpeg = asked.indexOf("image/jpeg");
    return png >= 0 && (jpeg < 0 || png < jpeg);
}

QSize sizeFor(double slideWidth, double slideHeight, int quality)
{
    if (slideWidth <= 0 || slideHeight <= 0)
        return {quality, int(std::lround(quality * 9.0 / 16.0))};
    if (slideWidth >= slideHeight)
        return {quality, std::max(1, int(std::lround(quality * slideHeight / slideWidth)))};
    return {std::max(1, int(std::lround(quality * slideWidth / slideHeight))), quality};
}

int squareFor(int width)
{
    return std::max(2, int(std::lround(width / 50.0)));
}

QByteArray askedLine(qint64 id, const Asked &asked)
{
    QJsonObject object {{"id", double(id)}, {"file", asked.file}, {"index", asked.index}, {"quality", asked.quality}, {"png", asked.png}};
    if (asked.arrangement)
        object.insert(QStringLiteral("arrangement"), *asked.arrangement);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

bool readAsked(const QByteArray &line, qint64 *id, Asked *asked)
{
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed());
    if (!document.isObject())
        return false;
    const QJsonObject object = document.object();
    if (!object.value(QStringLiteral("file")).isString() || !object.value(QStringLiteral("index")).isDouble())
        return false;
    Asked made;
    made.file = object.value(QStringLiteral("file")).toString();
    if (object.value(QStringLiteral("arrangement")).isString())
        made.arrangement = object.value(QStringLiteral("arrangement")).toString();
    made.index = object.value(QStringLiteral("index")).toInt();
    made.quality = std::clamp(object.value(QStringLiteral("quality")).toInt(400), 16, 1920);
    made.png = object.value(QStringLiteral("png")).toBool();
    *id = qint64(object.value(QStringLiteral("id")).toDouble());
    *asked = made;
    return true;
}

QByteArray answerLine(const Answer &answer)
{
    QJsonObject object {{"id", double(answer.id)}, {"status", answer.status}, {"length", double(answer.length)}};
    if (!answer.type.isEmpty())
        object.insert(QStringLiteral("type"), QString::fromLatin1(answer.type));
    if (!answer.error.isEmpty())
        object.insert(QStringLiteral("error"), answer.error);
    return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

bool readAnswer(const QByteArray &line, Answer *answer)
{
    const QJsonDocument document = QJsonDocument::fromJson(line.trimmed());
    if (!document.isObject())
        return false;
    const QJsonObject object = document.object();
    Answer made;
    made.id = qint64(object.value(QStringLiteral("id")).toDouble());
    made.status = object.value(QStringLiteral("status")).toInt(500);
    made.length = qint64(object.value(QStringLiteral("length")).toDouble());
    made.type = object.value(QStringLiteral("type")).toString().toLatin1();
    made.error = object.value(QStringLiteral("error")).toString();
    if (made.length < 0 || made.length > 64 * 1024 * 1024)
        return false;
    *answer = made;
    return true;
}

QString cacheName(const Asked &asked, qint64 fileChanged, const QString &mediaPath, qint64 mediaChanged, const QString &version)
{
    const QByteArray key = QJsonDocument(QJsonObject {{"file", asked.file},
                                                      {"arrangement", asked.arrangement ? QJsonValue(*asked.arrangement) : QJsonValue()},
                                                      {"index", asked.index},
                                                      {"quality", asked.quality},
                                                      {"fileChanged", double(fileChanged)},
                                                      {"media", mediaPath},
                                                      {"mediaChanged", double(mediaChanged)},
                                                      {"version", version}})
                               .toJson(QJsonDocument::Compact);
    return QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex()) + (asked.png ? QStringLiteral(".png") : QStringLiteral(".jpg"));
}

}
