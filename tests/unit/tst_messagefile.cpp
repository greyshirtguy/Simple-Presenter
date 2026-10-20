// Messages (src/messagefile.h): the workspace's list of them, read and changed, tried on
// a file made as ProPresenter wrote the one that was looked at, on a workspace that has
// no file, and on one whose file has nothing in it.

#include "messagefile.h"

#include "action.pb.h"
#include "messages.pb.h"

#include <google/protobuf/unknown_field_set.h>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <sys/stat.h>

using namespace messagefile;
using Document = rv::data::MessageDocument;
using FileMessage = rv::data::Message;

namespace {

const QString kTimed = QStringLiteral("5D1DAC57-0000-4AD0-A096-CCCB37FF425B");
const QString kBare = QStringLiteral("68F5B8E9-0000-4259-A990-D1863BC56C78");
const QString kTimerToken = QStringLiteral("B4AFBF0E-0000-4F77-8831-CEA895044B8C");
const QString kTimer = QStringLiteral("0E45D0AF-0000-4A31-BCFD-0F5A3358E225");

QString pathOf(const QString &workspace)
{
    return workspace + "/Configuration/Messages";
}

Document fileOf(const QString &workspace)
{
    Document document;
    QFile file(pathOf(workspace));
    if (file.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = file.readAll();
        document.ParseFromArray(bytes.constData(), int(bytes.size()));
    }
    return document;
}

std::string encoded(const google::protobuf::Message &message)
{
    std::string bytes;
    message.SerializeToString(&bytes);
    return bytes;
}

// Which file it is on the disk: a file that is written again, however the same, is
// another file under the same name, so this tells a file that was left alone from one
// that was written.
quint64 fileItself(const QString &workspace)
{
    struct stat info;
    return ::stat(pathOf(workspace).toLocal8Bit().constData(), &info) == 0 ? quint64(info.st_ino) : 0;
}

// A file as ProPresenter left the one that was looked at (the ids and the names apart,
// which are made up here): a message that shows a timer and goes when the timer has
// run out, in a slide of a theme, which can be seen on the network; and one that has
// only just been made. With things in the file, and in a message, that this code knows
// nothing of.
void writeProPresenters(const QString &workspace)
{
    Document document;
    document.mutable_application_info()->set_platform(rv::data::ApplicationInfo::PLATFORM_MACOS);
    document.mutable_application_info()->mutable_application_version()->set_major_version(18);
    FileMessage *timed = document.add_messages();
    timed->mutable_uuid()->set_string(kTimed.toStdString());
    timed->set_title("Service Countdown");
    timed->set_visible_on_network(true);
    timed->mutable_template_()->set_name("Stream");
    timed->mutable_template_()->mutable_slide_uuid()->set_string("11FC4BA5-0000-4F7F-9286-B125F7EE3487");
    timed->mutable_template_()->set_slide_name("Timer Overlay");
    timed->mutable_template_()->set_slide_index(2);
    timed->set_clear_type(FileMessage::CLEAR_TYPE_AFTER_TIMERS);
    timed->set_message_text("${" + kTimerToken.toStdString() + "}");
    FileMessage::Token *token = timed->add_tokens();
    token->mutable_uuid()->set_string(kTimerToken.toStdString());
    token->mutable_timer()->set_name("Service (10:02)");
    token->mutable_timer()->mutable_timer_uuid()->set_string(kTimer.toStdString());
    FileMessage::TokenValue *value = timed->add_token_values();
    value->mutable_token_id()->set_string(kTimerToken.toStdString());
    value->mutable_timer()->mutable_configuration()->mutable_countdown_to_time()->set_time_of_day(36120);
    value->mutable_timer()->mutable_format()->set_minute(rv::data::Timer::Format::STYLE_SHORT);
    value->mutable_timer()->mutable_format()->set_second(rv::data::Timer::Format::STYLE_LONG);
    value->set_token_name("Timer to 10:02");
    timed->GetReflection()->MutableUnknownFields(timed)->AddVarint(901, 7);
    FileMessage *bare = document.add_messages();
    bare->mutable_uuid()->set_string(kBare.toStdString());
    bare->set_title("New Message");
    document.GetReflection()->MutableUnknownFields(&document)->AddVarint(900, 42);
    const std::string bytes = encoded(document);
    QDir().mkpath(workspace + "/Configuration");
    QFile file(pathOf(workspace));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(bytes.data(), qint64(bytes.size()));
}

Token textToken(const QString &id, const QString &name)
{
    Token token;
    token.id = id;
    token.kind = QStringLiteral("text");
    token.name = name;
    return token;
}

}

class TestMessageFile : public QObject
{
    Q_OBJECT

private slots:
    // ---- reading

    void aWorkspaceWithNoFileHasNoMessagesAndNothingIsWritten()
    {
        QTemporaryDir workspace;
        QString error = QStringLiteral("not asked yet");
        QVERIFY(messages(workspace.path(), &error).isEmpty());
        QCOMPARE(error, QString());
        QVERIFY(!QFile::exists(pathOf(workspace.path())));
    }

    void aFileWithNothingInItHasNoMessages()
    {
        // (ProPresenter leaves such a file in a workspace that has never had a message.)
        QTemporaryDir workspace;
        QDir().mkpath(workspace.path() + "/Configuration");
        QFile file(pathOf(workspace.path()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        QString error;
        QVERIFY(messages(workspace.path(), &error).isEmpty());
        QCOMPARE(error, QString());
        QCOMPARE(QFileInfo(pathOf(workspace.path())).size(), 0);
    }

    void aFileThatIsNoSuchFileIsSaidToBe()
    {
        QTemporaryDir workspace;
        QDir().mkpath(workspace.path() + "/Configuration");
        QFile file(pathOf(workspace.path()));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("\xff\xff\xff\xff not a file of messages");
        file.close();
        QString error;
        QVERIFY(messages(workspace.path(), &error).isEmpty());
        QVERIFY(!error.isEmpty());
        QVERIFY(!add(workspace.path(), "A", "One").isEmpty());
    }

    void aMessageIsReadAsProPresenterWroteIt()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        QString error;
        const QList<Message> read = messages(workspace.path(), &error);
        QCOMPARE(error, QString());
        QCOMPARE(read.size(), 2);
        const Message &timed = read.at(0);
        QCOMPARE(timed.id, kTimed);
        QCOMPARE(timed.title, QStringLiteral("Service Countdown"));
        QCOMPARE(timed.text, QStringLiteral("${") + kTimerToken + QStringLiteral("}"));
        QCOMPARE(timed.theme, QStringLiteral("Stream"));
        QCOMPARE(timed.themeSlide, QStringLiteral("11FC4BA5-0000-4F7F-9286-B125F7EE3487"));
        QCOMPARE(timed.themeSlideName, QStringLiteral("Timer Overlay"));
        QCOMPARE(timed.themeSlideIndex, 2);
        QCOMPARE(timed.clear, 2);
        QCOMPARE(timed.tokens.size(), 1);
        const Token &token = timed.tokens.at(0);
        QCOMPARE(token.id, kTimerToken);
        QCOMPARE(token.kind, QStringLiteral("timer"));
        QCOMPARE(token.name, QStringLiteral("Service (10:02)"));
        QCOMPARE(token.timerId, kTimer);
        // How its time is written: minutes as they come, seconds in two figures
        QCOMPARE(token.hours, 0);
        QCOMPARE(token.minutes, 1);
        QCOMPARE(token.seconds, 2);
        QCOMPARE(token.hundredths, 0);
        const Message &bare = read.at(1);
        QCOMPARE(bare.id, kBare);
        QCOMPARE(bare.title, QStringLiteral("New Message"));
        QCOMPARE(bare.text, QString());
        QVERIFY(bare.tokens.isEmpty());
        QCOMPARE(bare.theme, QString());
        QCOMPARE(bare.clear, 0);
    }

    void aTextIsItsWordsAndItsTokensInOrder()
    {
        Message message;
        message.tokens = {textToken("AAA", "Name"), textToken("BBB", "Room")};
        message.text = QStringLiteral("${AAA}, please come to ${BBB} now ${CCC}${AAA}");
        const QList<Piece> found = pieces(message);
        QCOMPARE(found.size(), 5);
        QVERIFY(found.at(0).token);
        QCOMPARE(found.at(0).text, QStringLiteral("AAA"));
        QVERIFY(!found.at(1).token);
        QCOMPARE(found.at(1).text, QStringLiteral(", please come to "));
        QVERIFY(found.at(2).token);
        QCOMPARE(found.at(2).text, QStringLiteral("BBB"));
        // What looks like a token and is none of the message's is words
        QVERIFY(!found.at(3).token);
        QCOMPARE(found.at(3).text, QStringLiteral(" now ${CCC}"));
        QVERIFY(found.at(4).token);
        QCOMPARE(found.at(4).text, QStringLiteral("AAA"));
        QCOMPARE(placeOf("AAA"), QStringLiteral("${AAA}"));
        message.text = QStringLiteral("no tokens at all");
        QCOMPARE(pieces(message).size(), 1);
        message.text.clear();
        QVERIFY(pieces(message).isEmpty());
        QCOMPARE(usualTokens(), QStringList({"Name", "Room", "Message"}));
    }

    // ---- making, naming, moving, removing

    void theFirstMessageMakesTheFile()
    {
        QTemporaryDir workspace;
        QCOMPARE(add(workspace.path(), "11111111-AAAA", "Parent Wanted"), QString());
        const Document file = fileOf(workspace.path());
        QCOMPARE(file.messages_size(), 1);
        // An id and a title and nothing else, as ProPresenter leaves one it has just made
        FileMessage expected;
        expected.mutable_uuid()->set_string("11111111-AAAA");
        expected.set_title("Parent Wanted");
        QCOMPARE(encoded(file.messages(0)), encoded(expected));
        QVERIFY(!add(workspace.path(), "11111111-AAAA", "Again").isEmpty());
        QVERIFY(!add(workspace.path(), "", "No id").isEmpty());
    }

    void aNewMessageGoesLastAndTheOthersAreLeftToTheByte()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        QCOMPARE(add(workspace.path(), "22222222-BBBB", "Car Lights"), QString());
        const Document after = fileOf(workspace.path());
        QCOMPARE(after.messages_size(), 3);
        QCOMPARE(encoded(after.messages(0)), encoded(before.messages(0)));
        QCOMPARE(encoded(after.messages(1)), encoded(before.messages(1)));
        QCOMPARE(after.messages(2).title(), std::string("Car Lights"));
        // What the file says of itself, and what this knows nothing of, are still there
        QCOMPARE(encoded(after.application_info()), encoded(before.application_info()));
        QCOMPARE(after.GetReflection()->GetUnknownFields(after).field_count(), 1);
        QCOMPARE(after.messages(0).GetReflection()->GetUnknownFields(after.messages(0)).field_count(), 1);
    }

    void aNameIsChangedAndNothingElse()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        QCOMPARE(rename(workspace.path(), kTimed, "Countdown"), QString());
        Document expected = before;
        expected.mutable_messages(0)->set_title("Countdown");
        QCOMPARE(encoded(fileOf(workspace.path())), encoded(expected));
        // The same name again writes nothing
        const quint64 file = fileItself(workspace.path());
        QCOMPARE(rename(workspace.path(), kTimed, "Countdown"), QString());
        QCOMPARE(fileItself(workspace.path()), file);
        QVERIFY(!rename(workspace.path(), kTimed, "  ").isEmpty());
        QVERIFY(!rename(workspace.path(), "no such message", "Name").isEmpty());
    }

    void aMessageIsMovedAndRemoved()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        QCOMPARE(move(workspace.path(), kBare, -1), QString());
        Document moved = fileOf(workspace.path());
        QCOMPARE(encoded(moved.messages(0)), encoded(before.messages(1)));
        QCOMPARE(encoded(moved.messages(1)), encoded(before.messages(0)));
        // Past either end it stays, and nothing is written
        const quint64 file = fileItself(workspace.path());
        QCOMPARE(move(workspace.path(), kBare, -1), QString());
        QCOMPARE(move(workspace.path(), kTimed, 1), QString());
        QCOMPARE(fileItself(workspace.path()), file);
        QCOMPARE(remove(workspace.path(), kBare), QString());
        const Document after = fileOf(workspace.path());
        QCOMPARE(after.messages_size(), 1);
        QCOMPARE(encoded(after.messages(0)), encoded(before.messages(0)));
        QVERIFY(!remove(workspace.path(), kBare).isEmpty());
    }

    // ---- the text and its tokens

    void aTextWithTokensOfEachKindIsWrittenAsTheFileKeepsThem()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        Token name = textToken("AAAA-1", "Name");
        Token clock;
        clock.id = "AAAA-2";
        clock.kind = "clock";
        Token timer;
        timer.id = "AAAA-3";
        timer.kind = "timer";
        timer.name = "Sermon";
        timer.timerId = "TIMER-9";
        const QString text = QStringLiteral("${AAAA-1}: it is ${AAAA-2}, with ${AAAA-3} to go");
        QCOMPARE(setText(workspace.path(), kBare, text, {name, clock, timer}), QString());
        const FileMessage written = fileOf(workspace.path()).messages(1);
        QCOMPARE(written.message_text(), text.toStdString());
        QCOMPARE(written.tokens_size(), 3);
        QCOMPARE(written.token_values_size(), 3);
        // A text token: its name, and no words yet, with its name beside them
        QCOMPARE(written.tokens(0).uuid().string(), std::string("AAAA-1"));
        QVERIFY(written.tokens(0).has_text());
        QCOMPARE(written.tokens(0).text().name(), std::string("Name"));
        QCOMPARE(written.token_values(0).token_id().string(), std::string("AAAA-1"));
        QCOMPARE(written.token_values(0).token_name(), std::string("Name"));
        QVERIFY(written.token_values(0).has_text());
        QCOMPARE(written.token_values(0).text().value(), std::string());
        // The clock: the time alone, the short way
        QVERIFY(written.tokens(1).has_clock());
        QVERIFY(written.token_values(1).has_clock());
        QCOMPARE(int(written.token_values(1).clock().format().date_type()), 0);
        QCOMPARE(int(written.token_values(1).clock().format().time_format()), 1);
        QVERIFY(!written.token_values(1).clock().format().military_time_enabled());
        // A timer: which, by id and by name, and how its time is written; nothing of how it is set
        QVERIFY(written.tokens(2).has_timer());
        QCOMPARE(written.tokens(2).timer().name(), std::string("Sermon"));
        QCOMPARE(written.tokens(2).timer().timer_uuid().string(), std::string("TIMER-9"));
        QCOMPARE(written.token_values(2).token_name(), std::string("Sermon"));
        QVERIFY(!written.token_values(2).timer().has_configuration());
        QCOMPARE(int(written.token_values(2).timer().format().minute()), 1);
        QCOMPARE(int(written.token_values(2).timer().format().second()), 2);
        // And it is read back as it was given
        const Message read = messages(workspace.path(), nullptr).at(1);
        QCOMPARE(read.text, text);
        QCOMPARE(read.tokens.size(), 3);
        QCOMPARE(read.tokens.at(0).kind, QStringLiteral("text"));
        QCOMPARE(read.tokens.at(1).kind, QStringLiteral("clock"));
        QCOMPARE(read.tokens.at(1).time, 1);
        QCOMPARE(read.tokens.at(2).kind, QStringLiteral("timer"));
        QCOMPARE(read.tokens.at(2).timerId, QStringLiteral("TIMER-9"));
        QCOMPARE(pieces(read).size(), 6);
        // The other message was not touched
        writeProPresenters(workspace.path() + "/again");
        QCOMPARE(encoded(fileOf(workspace.path()).messages(0)), encoded(fileOf(workspace.path() + "/again").messages(0)));
    }

    void aTokenTheMessageHasIsLeftAsItIsAndOneNoLongerInTheTextGoes()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        // Words put round the timer's token, and a token of its own added: the timer's
        // token, and what it was given (how the timer is set, which this never wrote), stay.
        Token timer;
        timer.id = kTimerToken;
        timer.kind = "timer";
        timer.name = "Another name altogether";
        const QString text = QStringLiteral("Starting in ") + placeOf(kTimerToken) + QStringLiteral(" for ") + placeOf("NEW-1");
        QCOMPARE(setText(workspace.path(), kTimed, text, {timer, textToken("NEW-1", "Room")}), QString());
        FileMessage after = fileOf(workspace.path()).messages(0);
        QCOMPARE(after.tokens_size(), 2);
        QCOMPARE(encoded(after.tokens(0)), encoded(before.messages(0).tokens(0)));
        QCOMPARE(encoded(after.token_values(0)), encoded(before.messages(0).token_values(0)));
        QCOMPARE(after.tokens(1).text().name(), std::string("Room"));
        // The same again writes nothing
        const quint64 file = fileItself(workspace.path());
        QCOMPARE(setText(workspace.path(), kTimed, text, {timer, textToken("NEW-1", "Room")}), QString());
        QCOMPARE(fileItself(workspace.path()), file);
        // The timer's token taken out of the text: it goes, with what it was given
        QCOMPARE(setText(workspace.path(), kTimed, QStringLiteral("For ") + placeOf("NEW-1"), {timer, textToken("NEW-1", "Room")}), QString());
        after = fileOf(workspace.path()).messages(0);
        QCOMPARE(after.tokens_size(), 1);
        QCOMPARE(after.token_values_size(), 1);
        QCOMPARE(after.tokens(0).uuid().string(), std::string("NEW-1"));
        // A token named but standing nowhere in the text is not made
        QCOMPARE(setText(workspace.path(), kTimed, QStringLiteral("For ") + placeOf("NEW-1"), {textToken("NEW-1", "Room"), textToken("NEW-2", "Name")}), QString());
        QCOMPARE(fileOf(workspace.path()).messages(0).tokens_size(), 1);
        // Everything else of the message is as it was
        FileMessage expected = before.messages(0);
        expected.set_message_text(after.message_text());
        *expected.mutable_tokens() = after.tokens();
        *expected.mutable_token_values() = after.token_values();
        QCOMPARE(encoded(fileOf(workspace.path()).messages(0)), encoded(expected));
    }

    void aTextTokenIsGivenWords()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        QCOMPARE(setText(workspace.path(), kBare, placeOf("T-1") + QStringLiteral(" to ") + placeOf("T-2"), {textToken("T-1", "Name"), textToken("T-2", "Room")}), QString());
        QCOMPARE(setValue(workspace.path(), kBare, "T-2", "Creche"), QString());
        const Message read = messages(workspace.path(), nullptr).at(1);
        QCOMPARE(read.tokens.at(0).value, QString());
        QCOMPARE(read.tokens.at(1).value, QStringLiteral("Creche"));
        QCOMPARE(fileOf(workspace.path()).messages(1).token_values(1).token_name(), std::string("Room"));
        // The same words again write nothing
        const quint64 file = fileItself(workspace.path());
        QCOMPARE(setValue(workspace.path(), kBare, "T-2", "Creche"), QString());
        QCOMPARE(fileItself(workspace.path()), file);
        // A timer's token is given none, nor is a token the message has not
        QVERIFY(!setValue(workspace.path(), kTimed, kTimerToken, "words").isEmpty());
        QVERIFY(!setValue(workspace.path(), kBare, "no such token", "words").isEmpty());
        QCOMPARE(fileItself(workspace.path()), file);
    }

    // ---- its look, and how it goes

    void aThemeIsNamedAsProPresenterNamesOneAndTakenOffAgain()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        const Document before = fileOf(workspace.path());
        QCOMPARE(setTheme(workspace.path(), kBare, "Lower Third", "SLIDE-1", "Message", 1), QString());
        FileMessage after = fileOf(workspace.path()).messages(1);
        QCOMPARE(after.template_().name(), std::string("Lower Third"));
        QCOMPARE(after.template_().slide_uuid().string(), std::string("SLIDE-1"));
        QCOMPARE(after.template_().slide_name(), std::string("Message"));
        QCOMPARE(after.template_().slide_index(), 1u);
        // The same one again writes nothing
        const quint64 file = fileItself(workspace.path());
        QCOMPARE(setTheme(workspace.path(), kBare, "Lower Third", "SLIDE-1", "Message", 1), QString());
        QCOMPARE(fileItself(workspace.path()), file);
        // Taken off, the message is what it was
        QCOMPARE(setTheme(workspace.path(), kBare, "", "", "", 0), QString());
        QCOMPARE(encoded(fileOf(workspace.path()).messages(1)), encoded(before.messages(1)));
        // Another theme for the one that had one: nothing else of it changes
        QCOMPARE(setTheme(workspace.path(), kTimed, "Lower Third", "SLIDE-2", "Plain", 0), QString());
        after = fileOf(workspace.path()).messages(0);
        FileMessage expected = before.messages(0);
        expected.mutable_template_()->set_name("Lower Third");
        expected.mutable_template_()->mutable_slide_uuid()->set_string("SLIDE-2");
        expected.mutable_template_()->set_slide_name("Plain");
        expected.mutable_template_()->set_slide_index(0);
        QCOMPARE(encoded(after), encoded(expected));
    }

    void howItGoesIsByHandOrAfterATime()
    {
        QTemporaryDir workspace;
        writeProPresenters(workspace.path());
        QCOMPARE(setClear(workspace.path(), kBare, 1, 15), QString());
        Message read = messages(workspace.path(), nullptr).at(1);
        QCOMPARE(read.clear, 1);
        QCOMPARE(read.seconds, 15.0);
        // By hand again: how long is kept, for when a time is chosen again
        QCOMPARE(setClear(workspace.path(), kBare, 0, 0), QString());
        read = messages(workspace.path(), nullptr).at(1);
        QCOMPARE(read.clear, 0);
        QCOMPARE(read.seconds, 15.0);
        const quint64 file = fileItself(workspace.path());
        QCOMPARE(setClear(workspace.path(), kBare, 0, 0), QString());
        QCOMPARE(fileItself(workspace.path()), file);
        // ProPresenter's third way is not one to choose here
        QVERIFY(!setClear(workspace.path(), kBare, 2, 0).isEmpty());
        QCOMPARE(fileItself(workspace.path()), file);
    }

    // ---- what an action that shows a message gives its tokens

    void anActionsValuesAreReadAsProPresenterWritesThem()
    {
        // As ProPresenter wrote one in a presentation of the maintainer's: the timer
        // token's value, with the timer's setting and how its time is written, and here
        // a text token's beside it as the messages file has one.
        rv::data::Action action;
        action.set_type(rv::data::Action::ACTION_TYPE_MESSAGE);
        action.mutable_message()->mutable_message_identificaton()->set_parameter_name("Countdown");
        rv::data::Message::TokenValue *timer = action.mutable_message()->add_content();
        timer->mutable_token_id()->set_string("T-1");
        timer->mutable_timer()->mutable_configuration()->mutable_countdown_to_time()->set_time_of_day(36000);
        timer->mutable_timer()->mutable_format()->set_second(rv::data::Timer::Format::STYLE_LONG);
        timer->set_token_name("Service CountDown");
        rv::data::Message::TokenValue *name = action.mutable_message()->add_content();
        name->mutable_token_id()->set_string("N-1");
        name->mutable_text()->set_value("Sam");
        name->set_token_name("Name");
        const QList<ActionValue> values = actionValues(action);
        QCOMPARE(values.size(), 2);
        QCOMPARE(values.at(0).tokenId, "T-1");
        QCOMPARE(values.at(0).kind, "timer");
        QCOMPARE(values.at(0).name, "Service CountDown");
        QCOMPARE(values.at(0).value, QString());
        QCOMPARE(values.at(1).kind, "text");
        QCOMPARE(values.at(1).value, "Sam");
        // One with no tokens has none
        rv::data::Action bare;
        bare.mutable_message();
        QVERIFY(actionValues(bare).isEmpty());
    }

    void givingAnActionsTokensWordsKeepsWhatElseItSays()
    {
        rv::data::Action action;
        rv::data::Message::TokenValue *timer = action.mutable_message()->add_content();
        timer->mutable_token_id()->set_string("T-1");
        timer->mutable_timer()->mutable_configuration()->mutable_countdown_to_time()->set_time_of_day(36000);
        timer->set_token_name("Service CountDown");
        rv::data::Message::TokenValue *name = action.mutable_message()->add_content();
        name->mutable_token_id()->set_string("N-1");
        name->mutable_text()->set_value("Sam");
        name->set_token_name("Name");
        const std::string timerBefore = timer->SerializeAsString();
        setActionValues(&action, {
            {"N-1", "Name", "text", "Alex"},
            {"R-1", "Room", "text", "Creche"},
            // (Only words are taken: the timer's is left as it is)
            {"T-1", "Service CountDown", "timer", "ignored"},
        });
        QCOMPARE(action.message().content_size(), 3);
        QCOMPARE(action.message().content(0).SerializeAsString(), timerBefore);
        QCOMPARE(action.message().content(1).text().value(), "Alex");
        QCOMPARE(action.message().content(2).token_id().string(), "R-1");
        QCOMPARE(action.message().content(2).token_name(), "Room");
        QCOMPARE(action.message().content(2).text().value(), "Creche");
        // Empty words are words: given, and written
        setActionValues(&action, {{"N-1", "Name", "text", ""}});
        QVERIFY(action.message().content(1).has_text());
        QCOMPARE(action.message().content(1).text().value(), "");
        QCOMPARE(action.message().content_size(), 3);
    }
};

QTEST_APPLESS_MAIN(TestMessageFile)
#include "tst_messagefile.moc"
