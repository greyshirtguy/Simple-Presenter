#include "messagefile.h"

#include "action.pb.h"
#include "messages.pb.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <functional>

namespace messagefile {

namespace {

using Document = rv::data::MessageDocument;
using FileMessage = rv::data::Message;
using FileToken = rv::data::Message::Token;
using FileValue = rv::data::Message::TokenValue;

QString pathOf(const QString &workspace)
{
    return QDir(workspace).filePath(QStringLiteral("Configuration/Messages"));
}

QString text(const std::string &string)
{
    return QString::fromStdString(string);
}

// (The reading and the writing of the file are as workspacefiles.h has them. They are
// not taken from there so that this file stands by itself, with nothing of the app
// behind it.)
//
// A workspace with no file, or with one that has nothing in it (ProPresenter leaves
// such a file in a workspace that has never had a message), has no messages.
QString readFile(const QString &workspace, Document *document)
{
    QFile file(pathOf(workspace));
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read the messages: %1").arg(file.errorString());
    const QByteArray bytes = file.readAll();
    if (!document->ParseFromArray(bytes.constData(), int(bytes.size())))
        return QStringLiteral("The workspace's messages are not in a form that can be read");
    return {};
}

QString writeFile(const QString &workspace, const Document &document)
{
    std::string bytes;
    if (!document.SerializeToString(&bytes))
        return QStringLiteral("Cannot encode the messages");
    QDir().mkpath(QFileInfo(pathOf(workspace)).absolutePath());
    QSaveFile file(pathOf(workspace));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size()) || !file.commit())
        return QStringLiteral("Cannot write the messages: %1").arg(file.errorString());
    return {};
}

int indexOf(const Document &document, const QString &id)
{
    const std::string wanted = id.toStdString();
    for (int i = 0; i < document.messages_size(); ++i) {
        if (document.messages(i).uuid().string() == wanted)
            return i;
    }
    return -1;
}

// Changes one message of the file: `change` is given it, and says whether it changed
// anything, or what is wrong with what was asked. The file is written only for a change.
QString changeMessage(const QString &workspace, const QString &id, const std::function<QString(FileMessage *, bool *)> &change)
{
    Document document;
    QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int index = indexOf(document, id);
    if (index < 0)
        return QStringLiteral("That message is no longer there");
    bool changed = false;
    error = change(document.mutable_messages(index), &changed);
    if (!error.isEmpty() || !changed)
        return error;
    return writeFile(workspace, document);
}

// What a token was last given, by the token's id; null if it was given nothing.
const FileValue *valueOf(const FileMessage &message, const std::string &tokenId)
{
    for (const FileValue &value : message.token_values()) {
        if (value.token_id().string() == tokenId)
            return &value;
    }
    return nullptr;
}

FileValue *valueOf(FileMessage *message, const std::string &tokenId)
{
    for (int i = 0; i < message->token_values_size(); ++i) {
        if (message->token_values(i).token_id().string() == tokenId)
            return message->mutable_token_values(i);
    }
    return nullptr;
}

Token readToken(const FileMessage &message, const FileToken &token)
{
    Token read;
    read.id = text(token.uuid().string());
    const FileValue *value = valueOf(message, token.uuid().string());
    if (token.has_timer()) {
        read.kind = QStringLiteral("timer");
        read.name = text(token.timer().name());
        read.timerId = text(token.timer().timer_uuid().string());
        if (value && value->has_timer() && value->timer().has_format()) {
            const rv::data::Timer::Format &format = value->timer().format();
            read.hours = int(format.hour());
            read.minutes = int(format.minute());
            read.seconds = int(format.second());
            read.hundredths = int(format.millisecond());
            read.hundredthsUnderMinute = format.show_milliseconds_under_minute_only();
        }
    } else if (token.has_clock()) {
        read.kind = QStringLiteral("clock");
        if (value && value->has_clock() && value->clock().has_format()) {
            const rv::data::Clock::Format &format = value->clock().format();
            read.date = int(format.date_type());
            read.time = int(format.time_format());
            read.twentyFour = format.military_time_enabled();
        }
    } else {
        read.kind = QStringLiteral("text");
        read.name = text(token.text().name());
        if (value && value->has_text())
            read.value = text(value->text().value());
    }
    return read;
}

Message readMessage(const FileMessage &message)
{
    Message read;
    read.id = text(message.uuid().string());
    read.title = text(message.title());
    read.text = text(message.message_text());
    for (const FileToken &token : message.tokens())
        read.tokens.append(readToken(message, token));
    if (message.has_template_()) {
        read.theme = text(message.template_().name());
        read.themeSlide = text(message.template_().slide_uuid().string());
        read.themeSlideName = text(message.template_().slide_name());
        read.themeSlideIndex = int(message.template_().slide_index());
    }
    read.clear = int(message.clear_type());
    read.seconds = message.time_to_remove();
    return read;
}

// Makes a token of the file, and what it is first given (see the guesses in the header).
void writeToken(FileMessage *message, const Token &token)
{
    FileToken *made = message->add_tokens();
    made->mutable_uuid()->set_string(token.id.toStdString());
    FileValue *value = message->add_token_values();
    value->mutable_token_id()->set_string(token.id.toStdString());
    if (token.kind == QLatin1String("timer")) {
        made->mutable_timer()->set_name(token.name.toStdString());
        made->mutable_timer()->mutable_timer_uuid()->set_string(token.timerId.toStdString());
        value->set_token_name(token.name.toStdString());
        rv::data::Timer::Format *format = value->mutable_timer()->mutable_format();
        format->set_hour(rv::data::Timer::Format::Style(qBound(0, token.hours, 4)));
        format->set_minute(rv::data::Timer::Format::Style(qBound(0, token.minutes, 4)));
        format->set_second(rv::data::Timer::Format::Style(qBound(0, token.seconds, 4)));
        format->set_millisecond(rv::data::Timer::Format::Style(qBound(0, token.hundredths, 4)));
    } else if (token.kind == QLatin1String("clock")) {
        made->mutable_clock();
        rv::data::Clock::Format *format = value->mutable_clock()->mutable_format();
        format->set_date_type(rv::data::Clock::Format::DateFormatterStyle(qBound(0, token.date, 4)));
        format->set_time_format(rv::data::Clock::Format::DateFormatterStyle(qBound(0, token.time, 4)));
        format->set_military_time_enabled(token.twentyFour);
    } else {
        made->mutable_text()->set_name(token.name.toStdString());
        value->set_token_name(token.name.toStdString());
        value->mutable_text()->set_value(token.value.toStdString());
    }
}

} // namespace

QStringList usualTokens()
{
    return {QStringLiteral("Name"), QStringLiteral("Room"), QStringLiteral("Message")};
}

QString placeOf(const QString &tokenId)
{
    return QStringLiteral("${") + tokenId + QLatin1Char('}');
}

QList<Piece> pieces(const Message &message)
{
    QList<Piece> found;
    static const QRegularExpression place(QStringLiteral("\\$\\{([^{}]*)\\}"));
    qsizetype at = 0;
    const auto words = [&found](const QString &text) {
        if (text.isEmpty())
            return;
        if (!found.isEmpty() && !found.last().token)
            found.last().text += text;
        else
            found.append({false, text});
    };
    QRegularExpressionMatchIterator each = place.globalMatch(message.text);
    while (each.hasNext()) {
        const QRegularExpressionMatch match = each.next();
        const QString id = match.captured(1);
        const bool ours = std::any_of(message.tokens.cbegin(), message.tokens.cend(), [&id](const Token &token) { return token.id == id; });
        words(message.text.mid(at, match.capturedStart() - at));
        if (ours)
            found.append({true, id});
        else
            words(match.captured(0));
        at = match.capturedEnd();
    }
    words(message.text.mid(at));
    return found;
}

QList<Message> messages(const QString &workspace, QString *error)
{
    Document document;
    const QString problem = readFile(workspace, &document);
    if (error)
        *error = problem;
    QList<Message> found;
    if (!problem.isEmpty())
        return found;
    for (const FileMessage &message : document.messages())
        found.append(readMessage(message));
    return found;
}

QString add(const QString &workspace, const QString &id, const QString &title)
{
    Document document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    if (id.isEmpty() || indexOf(document, id) >= 0)
        return QStringLiteral("A message needs an id of its own");
    // (An id and a title, and nothing else: as ProPresenter leaves one it has just made.)
    FileMessage *message = document.add_messages();
    message->mutable_uuid()->set_string(id.toStdString());
    message->set_title(title.toStdString());
    return writeFile(workspace, document);
}

QString rename(const QString &workspace, const QString &id, const QString &title)
{
    if (title.trimmed().isEmpty())
        return QStringLiteral("A message needs a name");
    return changeMessage(workspace, id, [&](FileMessage *message, bool *changed) {
        *changed = text(message->title()) != title;
        message->set_title(title.toStdString());
        return QString();
    });
}

QString remove(const QString &workspace, const QString &id)
{
    Document document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int index = indexOf(document, id);
    if (index < 0)
        return QStringLiteral("That message is no longer there");
    document.mutable_messages()->DeleteSubrange(index, 1);
    return writeFile(workspace, document);
}

QString move(const QString &workspace, const QString &id, int by)
{
    Document document;
    const QString error = readFile(workspace, &document);
    if (!error.isEmpty())
        return error;
    const int index = indexOf(document, id);
    if (index < 0)
        return QStringLiteral("That message is no longer there");
    const int to = index + (by < 0 ? -1 : 1);
    if (by == 0 || to < 0 || to >= document.messages_size())
        return {};
    document.mutable_messages()->SwapElements(index, to);
    return writeFile(workspace, document);
}

QString setText(const QString &workspace, const QString &id, const QString &wanted, const QList<Token> &tokens)
{
    return changeMessage(workspace, id, [&](FileMessage *message, bool *changed) {
        if (text(message->message_text()) != wanted) {
            message->set_message_text(wanted.toStdString());
            *changed = true;
        }
        // The tokens that are still wanted: named among `tokens`, and standing in the text
        const auto wantedToken = [&](const std::string &tokenId) {
            const QString asText = text(tokenId);
            return wanted.contains(placeOf(asText))
                && std::any_of(tokens.cbegin(), tokens.cend(), [&asText](const Token &token) { return token.id == asText; });
        };
        for (int i = message->tokens_size() - 1; i >= 0; --i) {
            const std::string tokenId = message->tokens(i).uuid().string();
            if (wantedToken(tokenId))
                continue;
            message->mutable_tokens()->DeleteSubrange(i, 1);
            for (int v = message->token_values_size() - 1; v >= 0; --v) {
                if (message->token_values(v).token_id().string() == tokenId)
                    message->mutable_token_values()->DeleteSubrange(v, 1);
            }
            *changed = true;
        }
        for (const Token &token : tokens) {
            const std::string tokenId = token.id.toStdString();
            const bool has = std::any_of(message->tokens().cbegin(), message->tokens().cend(),
                                         [&tokenId](const FileToken &had) { return had.uuid().string() == tokenId; });
            if (has || token.id.isEmpty() || !wanted.contains(placeOf(token.id)))
                continue;
            writeToken(message, token);
            *changed = true;
        }
        return QString();
    });
}

QString setValue(const QString &workspace, const QString &id, const QString &tokenId, const QString &wanted)
{
    return changeMessage(workspace, id, [&](FileMessage *message, bool *changed) {
        const std::string token = tokenId.toStdString();
        const auto found = std::find_if(message->tokens().cbegin(), message->tokens().cend(),
                                        [&token](const FileToken &had) { return had.uuid().string() == token; });
        if (found == message->tokens().cend() || found->has_timer() || found->has_clock())
            return QStringLiteral("That is not a token of this message that is given words");
        FileValue *value = valueOf(message, token);
        if (value && value->has_text() && text(value->text().value()) == wanted)
            return QString();
        if (!value) {
            // (A token that has never been given anything is given nothing by being
            // given no words.)
            if (wanted.isEmpty())
                return QString();
            value = message->add_token_values();
            value->mutable_token_id()->set_string(token);
            value->set_token_name(found->text().name());
        }
        value->mutable_text()->set_value(wanted.toStdString());
        *changed = true;
        return QString();
    });
}

QList<ActionValue> actionValues(const rv::data::Action &action)
{
    QList<ActionValue> values;
    for (const FileValue &value : action.message().content()) {
        ActionValue read;
        read.tokenId = text(value.token_id().string());
        read.name = text(value.token_name());
        read.kind = value.has_timer() ? QStringLiteral("timer") : value.has_clock() ? QStringLiteral("clock") : QStringLiteral("text");
        if (value.has_text())
            read.value = text(value.text().value());
        values.append(read);
    }
    return values;
}

void setActionValues(rv::data::Action *action, const QList<ActionValue> &values)
{
    rv::data::Action::MessageType *message = action->mutable_message();
    for (const ActionValue &wanted : values) {
        if (wanted.kind != QLatin1String("text") || wanted.tokenId.isEmpty())
            continue;
        FileValue *value = nullptr;
        for (int i = 0; i < message->content_size() && !value; ++i) {
            if (text(message->content(i).token_id().string()) == wanted.tokenId)
                value = message->mutable_content(i);
        }
        if (!value) {
            value = message->add_content();
            value->mutable_token_id()->set_string(wanted.tokenId.toStdString());
        }
        value->set_token_name(wanted.name.toStdString());
        value->mutable_text()->set_value(wanted.value.toStdString());
    }
}

QString setTheme(const QString &workspace, const QString &id, const QString &theme, const QString &slideId, const QString &slideName, int slideIndex)
{
    return changeMessage(workspace, id, [&](FileMessage *message, bool *changed) {
        if (theme.isEmpty()) {
            *changed = message->has_template_();
            message->clear_template_();
            return QString();
        }
        const bool same = message->has_template_() && text(message->template_().name()) == theme
                       && text(message->template_().slide_uuid().string()) == slideId;
        if (same)
            return QString();
        // (Made afresh: whatever else the old one said, an id of the theme's own
        // perhaps, was said of another theme.)
        message->clear_template_();
        rv::data::TemplateIdentification *named = message->mutable_template_();
        named->set_name(theme.toStdString());
        named->mutable_slide_uuid()->set_string(slideId.toStdString());
        named->set_slide_name(slideName.toStdString());
        named->set_slide_index(uint32_t(qMax(0, slideIndex)));
        *changed = true;
        return QString();
    });
}

QString setClear(const QString &workspace, const QString &id, int clear, double seconds)
{
    if (clear != 0 && clear != 1)
        return QStringLiteral("A message goes by hand, or after a time");
    return changeMessage(workspace, id, [&](FileMessage *message, bool *changed) {
        const double wanted = clear == 1 ? qMax(1.0, seconds) : message->time_to_remove();
        *changed = int(message->clear_type()) != clear || message->time_to_remove() != wanted;
        message->set_clear_type(rv::data::Message::ClearType(clear));
        // (How long is kept while it goes by hand: it is what "after a time" starts
        // from if that is chosen again.)
        message->set_time_to_remove(wanted);
        return QString();
    });
}

} // namespace messagefile
