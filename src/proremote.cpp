#include "proremote.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <memory>

namespace proremote {

namespace {

// A JSON value written compactly, a bare string or number included (Qt writes only an
// object or an array: the value is written in one, and that taken off again)
QByteArray jsonOf(const QJsonValue &value)
{
    const QByteArray inArray = QJsonDocument(QJsonArray {value}).toJson(QJsonDocument::Compact);
    return inArray.mid(1, inArray.size() - 2);
}

// AES-256 in CTR mode, which is the same both ways: the IV is the counter's first block
QByteArray ctr(const QByteArray &key, const QByteArray &iv, const QByteArray &in)
{
    if (key.size() != 32 || iv.size() != 16)
        return {};
    const std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> context(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
    if (!context)
        return {};
    QByteArray out(in.size() + 16, Qt::Uninitialized);
    int written = 0;
    int last = 0;
    if (EVP_EncryptInit_ex(context.get(), EVP_aes_256_ctr(), nullptr, reinterpret_cast<const unsigned char *>(key.constData()),
                           reinterpret_cast<const unsigned char *>(iv.constData())) != 1
        || EVP_EncryptUpdate(context.get(), reinterpret_cast<unsigned char *>(out.data()), &written,
                             reinterpret_cast<const unsigned char *>(in.constData()), int(in.size())) != 1
        || EVP_EncryptFinal_ex(context.get(), reinterpret_cast<unsigned char *>(out.data()) + written, &last) != 1)
        return {};
    out.truncate(written + last);
    return out;
}

// A query's name or value, its escapes undone (and a + as the space a form makes it)
QString unescaped(const QString &part)
{
    QString spaced = part;
    spaced.replace(u'+', u' ');
    return QUrl::fromPercentEncoding(spaced.toUtf8());
}

}

QByteArray key(const QString &password)
{
    const QByteArray bytes = password.toUtf8();
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(bytes.constData(), size_t(bytes.size()), digest, &length, EVP_sha512_256(), nullptr) != 1)
        return {};
    return QByteArray(reinterpret_cast<const char *>(digest), int(length));
}

QByteArray seal(const QByteArray &key, const QByteArray &plain, const QByteArray &iv)
{
    QByteArray start = iv;
    if (start.isEmpty()) {
        start.resize(16);
        if (RAND_bytes(reinterpret_cast<unsigned char *>(start.data()), 16) != 1)
            return {};
    }
    const QByteArray sealed = ctr(key, start, plain);
    if (sealed.isEmpty() && !plain.isEmpty())
        return {};
    return start + sealed;
}

QByteArray open(const QByteArray &key, const QByteArray &sealed)
{
    if (sealed.size() < 16)
        return {};
    return ctr(key, sealed.left(16), sealed.mid(16));
}

bool isUpgrade(const Upgrade &asked)
{
    return asked.upgrade.toLower().contains("websocket") && asked.connection.toLower().contains("upgrade")
           && !asked.key.isEmpty() && asked.version.trimmed() == "13";
}

QByteArray acceptKey(const QByteArray &clientKey)
{
    return QCryptographicHash::hash(clientKey.trimmed() + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", QCryptographicHash::Sha1).toBase64();
}

QByteArray frame(Opcode opcode, const QByteArray &payload, const QByteArray &mask)
{
    QByteArray made;
    made.append(char(0x80 | opcode));
    const char masked = mask.size() == 4 ? char(0x80) : char(0);
    const qsizetype length = payload.size();
    if (length < 126) {
        made.append(char(masked | char(length)));
    } else if (length < 65536) {
        made.append(char(masked | 126));
        made.append(char((length >> 8) & 0xff));
        made.append(char(length & 0xff));
    } else {
        made.append(char(masked | 127));
        for (int shift = 56; shift >= 0; shift -= 8)
            made.append(char((quint64(length) >> shift) & 0xff));
    }
    if (!masked)
        return made + payload;
    made += mask;
    QByteArray hidden = payload;
    for (qsizetype i = 0; i < hidden.size(); ++i)
        hidden[i] = char(hidden[i] ^ mask[i & 3]);
    return made + hidden;
}

bool FrameReader::feed(const QByteArray &bytes)
{
    if (m_error)
        return false;
    m_buffer += bytes;
    while (readOne()) {
    }
    return !m_error;
}

// One frame off the front of what has come, if it is all there
bool FrameReader::readOne()
{
    if (m_buffer.size() < 2)
        return false;
    const auto byte = [this](qsizetype at) { return quint8(m_buffer[at]); };
    const bool fin = byte(0) & 0x80;
    const int opcode = byte(0) & 0x0f;
    // (No extension is agreed to, so the three reserved bits are never set)
    if (byte(0) & 0x70) {
        m_error = 1002;
        return false;
    }
    const bool masked = byte(1) & 0x80;
    quint64 length = byte(1) & 0x7f;
    qsizetype at = 2;
    if (length == 126) {
        if (m_buffer.size() < 4)
            return false;
        length = (quint64(byte(2)) << 8) | byte(3);
        at = 4;
    } else if (length == 127) {
        if (m_buffer.size() < 10)
            return false;
        length = 0;
        for (int i = 0; i < 8; ++i)
            length = (length << 8) | byte(2 + i);
        at = 10;
    }
    const bool control = opcode >= 8;
    if ((control && (!fin || length > 125)) || (opcode > 2 && opcode < 8) || opcode > 10) {
        m_error = 1002;
        return false;
    }
    if (length > quint64(most) || qsizetype(length) + (m_open ? m_partial.payload.size() : 0) > most) {
        m_error = 1009;
        return false;
    }
    // (A client's frames are masked, as RFC 6455 has it; one that is not is read all the
    // same rather than refused: nothing here depends on the mask.)
    const qsizetype maskAt = at;
    if (masked)
        at += 4;
    if (m_buffer.size() < at + qsizetype(length))
        return false;
    QByteArray payload = m_buffer.mid(at, qsizetype(length));
    if (masked) {
        for (qsizetype i = 0; i < payload.size(); ++i)
            payload[i] = char(payload[i] ^ m_buffer[maskAt + (i & 3)]);
    }
    m_buffer.remove(0, at + qsizetype(length));

    if (control) {
        m_ready.append({Opcode(opcode), payload});
        return true;
    }
    if (opcode == Continuation) {
        if (!m_open) {
            m_error = 1002;
            return false;
        }
        m_partial.payload += payload;
    } else {
        if (m_open) {
            m_error = 1002;
            return false;
        }
        m_partial = {Opcode(opcode), payload};
        m_open = true;
    }
    if (fin) {
        m_ready.append(m_partial);
        m_partial = {};
        m_open = false;
    }
    return true;
}

bool read(const QByteArray &json, Request *request)
{
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(json, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    const QJsonObject object = document.object();
    Request made;
    made.url = object.value(QStringLiteral("url")).toString();
    const bool hasBody = object.contains(QStringLiteral("body")) && !object.value(QStringLiteral("body")).isNull();
    if (hasBody)
        made.body = jsonOf(object.value(QStringLiteral("body")));
    const QString method = object.value(QStringLiteral("method")).toString().trimmed().toUpper();
    made.method = !method.isEmpty() ? method.toLatin1() : hasBody ? QByteArray("POST") : QByteArray("GET");

    const qsizetype mark = made.url.indexOf(u'?');
    QString path = mark < 0 ? made.url : made.url.left(mark);
    while (path.startsWith(u'/'))
        path.remove(0, 1);
    made.path = u'/' + QUrl::fromPercentEncoding(path.toUtf8());
    if (mark >= 0) {
        for (const QString &pair : made.url.mid(mark + 1).split(u'&', Qt::SkipEmptyParts)) {
            const qsizetype equals = pair.indexOf(u'=');
            made.query.append({unescaped(equals < 0 ? pair : pair.left(equals)), equals < 0 ? QString() : unescaped(pair.mid(equals + 1))});
        }
    }
    const QJsonValue chunked = object.value(QStringLiteral("chunked"));
    made.chunked = chunked.isBool() ? chunked.toBool() : false;
    for (const auto &[name, value] : std::as_const(made.query)) {
        if (name == QLatin1String("chunked"))
            made.chunked = value != QLatin1String("false") && value != QLatin1String("0");
    }
    *request = made;
    return true;
}

QByteArray answer(const QString &url, const QByteArray &dataJson)
{
    return "{\"data\":" + dataJson + ",\"url\":" + jsonOf(url) + '}';
}

QByteArray failure(const QString &url, const QByteArray &statusText)
{
    return "{\"error\":" + jsonOf(QString::fromLatin1(statusText)) + ",\"url\":" + jsonOf(url) + '}';
}

}
