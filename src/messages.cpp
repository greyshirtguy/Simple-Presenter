#include "messages.h"

#include "messagefile.h"
#include "proconvert.h"
#include "richtext.h"
#include "workspacefiles.h"

#include "slide.pb.h"

#include <QColor>
#include <QJSValue>
#include <QUuid>

namespace {

QVariantMap tokenMap(const messagefile::Token &token)
{
    return {
        {"id", token.id}, {"kind", token.kind}, {"name", token.name}, {"timerId", token.timerId}, {"value", token.value},
        {"hours", token.hours}, {"minutes", token.minutes}, {"seconds", token.seconds}, {"hundredths", token.hundredths},
        {"hundredthsUnderMinute", token.hundredthsUnderMinute}, {"date", token.date}, {"time", token.time}, {"twentyFour", token.twentyFour},
    };
}

messagefile::Token tokenOf(const QVariantMap &map)
{
    messagefile::Token token;
    token.id = map.value("id").toString();
    token.kind = map.value("kind", QStringLiteral("text")).toString();
    token.name = map.value("name").toString();
    token.timerId = map.value("timerId").toString();
    token.value = map.value("value").toString();
    return token;
}

// The map QML handed over where it might have handed null instead
QVariantMap mapOf(const QVariant &value)
{
    if (value.metaType() == QMetaType::fromType<QJSValue>())
        return value.value<QJSValue>().toVariant().toMap();
    return value.toMap();
}

// The slide a message has that has no theme: nothing but its words, white and bold, on
// a dark band across the foot of the screen that lets a little of what is under it
// show. It is made as any slide is, by way of ProPresenter's own form of one, so that
// it has everything a slide map has.
QVariantMap plainSlide()
{
    static const QVariantMap made = [] {
        rv::data::Slide slide;
        slide.set_draws_background_color(false);
        slide.mutable_size()->set_width(1920);
        slide.mutable_size()->set_height(1080);
        slide.mutable_uuid()->set_string(workspace::newUuid());
        rv::data::Slide::Element element = proconvert::makeTextElement(slide, nullptr);
        rv::data::Graphics::Text *text = element.mutable_element()->mutable_text();
        TextRun format = proconvert::readText(*text).firstRun();
        format.applyFormat({{"size", 64.0}, {"bold", true}, {"color", QColor(Qt::white)}});
        proconvert::writeText(text, RichText::plain(QStringLiteral("Message"), format, Qt::AlignHCenter));
        const QString id = QString::fromStdString(element.element().uuid().string());
        *slide.add_elements() = element;
        proconvert::applyChanges(&slide, id, {
            {"name", QStringLiteral("Message")}, {"x", 0.0}, {"y", 840.0}, {"width", 1920.0}, {"height", 180.0},
            {"fillKind", QStringLiteral("color")}, {"fillColor", QColor(0, 0, 0, 200)}, {"fillOn", true},
            {"textScale", int(rv::data::Graphics::Text::SCALE_BEHAVIOR_SCALE_FONT_DOWN)},
        });
        return proconvert::toSlideMap(slide, QString());
    }();
    return made;
}

}

QStringList Messages::usualTokens() const
{
    return messagefile::usualTokens();
}

QString Messages::open(const QString &workspace)
{
    m_workspace = workspace;
    if (!m_words.isEmpty()) {
        m_words.clear();
        ++m_revision;
        emit wordsChanged();
    }
    return reload();
}

QString Messages::reload()
{
    QString error;
    m_messages.clear();
    if (!m_workspace.isEmpty()) {
        const QList<messagefile::Message> messages = messagefile::messages(m_workspace, &error);
        for (const messagefile::Message &message : messages) {
            QVariantList tokens;
            for (const messagefile::Token &token : message.tokens)
                tokens.append(tokenMap(token));
            QVariantList pieces;
            for (const messagefile::Piece &piece : messagefile::pieces(message))
                pieces.append(QVariantMap {{"token", piece.token}, {"text", piece.text}});
            m_messages.append(QVariantMap {
                {"id", message.id},
                {"name", message.title},
                {"text", message.text},
                {"pieces", pieces},
                {"tokens", tokens},
                {"theme", message.theme},
                {"themeSlide", message.themeSlide},
                {"themeSlideName", message.themeSlideName},
                {"themeSlideIndex", message.themeSlideIndex},
                {"clear", message.clear},
                {"seconds", message.seconds},
            });
        }
    }
    emit changed();
    return error;
}

QVariantMap Messages::add(const QString &name)
{
    const QString id = newId();
    const QString error = messagefile::add(m_workspace, id, name);
    reload();
    return {{"id", error.isEmpty() ? id : QString()}, {"error", error}};
}

QString Messages::rename(const QString &id, const QString &name)
{
    const QString error = messagefile::rename(m_workspace, id, name);
    reload();
    return error;
}

QString Messages::remove(const QString &id)
{
    const QString error = messagefile::remove(m_workspace, id);
    reload();
    return error;
}

QString Messages::move(const QString &id, int by)
{
    const QString error = messagefile::move(m_workspace, id, by);
    reload();
    return error;
}

QString Messages::setText(const QString &id, const QString &text, const QVariantList &tokens)
{
    QList<messagefile::Token> wanted;
    for (const QVariant &token : tokens)
        wanted.append(tokenOf(mapOf(token)));
    const QString error = messagefile::setText(m_workspace, id, text, wanted);
    reload();
    return error;
}

QString Messages::setValue(const QString &id, const QString &tokenId, const QString &value)
{
    const QString error = messagefile::setValue(m_workspace, id, tokenId, value);
    reload();
    return error;
}

QString Messages::setTheme(const QString &id, const QString &theme, const QString &slideId, const QString &slideName, int slideIndex)
{
    const QString error = messagefile::setTheme(m_workspace, id, theme, slideId, slideName, slideIndex);
    reload();
    return error;
}

QString Messages::setClear(const QString &id, int clear, double seconds)
{
    const QString error = messagefile::setClear(m_workspace, id, clear, seconds);
    reload();
    return error;
}

QString Messages::newId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
}

QVariantMap Messages::slideOf(const QString &id, const QVariant &themeSlide) const
{
    QVariantMap slide = mapOf(themeSlide);
    if (slide.isEmpty())
        slide = plainSlide();
    QVariantList elements = slide.value("elements").toList();
    // The first thing on it with words in it; or, where nothing has, the first thing.
    qsizetype box = -1;
    for (qsizetype i = 0; i < elements.size() && box < 0; ++i) {
        const QVariantMap element = elements.at(i).toMap();
        if (element.value("hasText").toBool() && !element.value("text").value<RichText>().isEmpty())
            box = i;
    }
    if (box < 0 && !elements.isEmpty())
        box = 0;
    if (box >= 0) {
        QVariantMap element = elements.at(box).toMap();
        const RichText own = element.value("text").value<RichText>();
        // Its words are the message's, asked for as they are at the moment by what
        // draws it (see SlideElement.qml), in the one format the box's own text is in.
        element.insert("linkKind", QStringLiteral("message"));
        element.insert("linkMessageId", id);
        element.insert("hasText", true);
        element.insert("visible", true);
        element.insert("displayText", QVariant::fromValue(RichText::plain(QString(), own.firstRun(),
                                                                           own.paragraphs.isEmpty() ? Qt::AlignHCenter : own.paragraphs.first().alignment)));
        elements[box] = element;
    }
    slide.insert("elements", elements);
    return slide;
}

QString Messages::words(const QString &id) const
{
    return m_words.value(id);
}

void Messages::setWords(const QString &id, const QString &words)
{
    const auto had = m_words.constFind(id);
    if (had != m_words.constEnd() && had.value() == words)
        return;
    m_words.insert(id, words);
    ++m_revision;
    emit wordsChanged();
}

void Messages::forget(const QString &id)
{
    m_words.remove(id);
}
