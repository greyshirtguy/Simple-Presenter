#include "ccli.h"

namespace ccli {

using Token = rv::data::CopyrightLayout::Token;
using TokenType = rv::data::CopyrightLayout::TokenType;
using DisplayType = rv::data::CCLIDocument::DisplayType;

namespace {

// The tokens' kinds by the names QML has for them, in ProPresenter's order
const std::pair<TokenType, const char *> kinds[] = {
    {rv::data::CopyrightLayout::Text, "text"},           {rv::data::CopyrightLayout::Artist, "artist"},
    {rv::data::CopyrightLayout::Author, "author"},       {rv::data::CopyrightLayout::Publisher, "publisher"},
    {rv::data::CopyrightLayout::Title, "title"},         {rv::data::CopyrightLayout::CopyrightYear, "year"},
    {rv::data::CopyrightLayout::LicenseNumber, "license"}, {rv::data::CopyrightLayout::SongNumber, "songNumber"},
};

const std::pair<DisplayType, const char *> displays[] = {
    {rv::data::CCLIDocument::DISPLAY_TYPE_FIRST_SLIDE, "first"},
    {rv::data::CCLIDocument::DISPLAY_TYPE_LAST_SLIDE, "last"},
    {rv::data::CCLIDocument::DISPLAY_TYPE_FIRST_AND_LAST_SLIDE, "firstAndLast"},
    {rv::data::CCLIDocument::DISPLAY_TYPE_ALL_SLIDES, "all"},
};

QString kindName(TokenType type)
{
    for (const auto &[each, name] : kinds) {
        if (each == type)
            return QString::fromLatin1(name);
    }
    return QStringLiteral("text");
}

bool kindOf(const QString &name, TokenType *type)
{
    for (const auto &[each, known] : kinds) {
        if (name == QLatin1String(known)) {
            *type = each;
            return true;
        }
    }
    return false;
}

} // namespace

QVariantMap describe(const rv::data::CCLIDocument &document)
{
    QVariantList tokens;
    for (const Token &token : document.layout().tokens())
        tokens.append(QVariantMap {{"kind", kindName(token.token_type())}, {"text", QString::fromStdString(token.text())}});
    QString display = QStringLiteral("first");
    for (const auto &[each, name] : displays) {
        if (each == document.display_type())
            display = QString::fromLatin1(name);
    }
    return {
        {"enabled", document.enable_ccli_display()},
        {"license", QString::fromStdString(document.ccli_license())},
        {"display", display},
        {"tokens", tokens},
    };
}

QString change(rv::data::CCLIDocument *document, const QVariantMap &changes)
{
    if (changes.contains("display")) {
        const QString wanted = changes.value("display").toString();
        bool found = false;
        for (const auto &[each, name] : displays) {
            if (wanted == QLatin1String(name)) {
                found = true;
                if (document->display_type() != each)
                    document->set_display_type(each);
            }
        }
        if (!found)
            return QStringLiteral("There are no such slides to show the copyright on");
    }
    if (changes.contains("tokens")) {
        rv::data::CopyrightLayout layout;
        for (const QVariant &entry : changes.value("tokens").toList()) {
            const QVariantMap wanted = entry.toMap();
            TokenType type;
            if (!kindOf(wanted.value("kind").toString(), &type))
                return QStringLiteral("A part of the layout is of no kind there is");
            Token *token = layout.add_tokens();
            token->set_token_type(type);
            if (type == rv::data::CopyrightLayout::Text)
                token->set_text(wanted.value("text").toString().toStdString());
        }
        // (Only where it is to be other than it is: the file's own is kept otherwise.)
        if (layout.SerializeAsString() != document->layout().SerializeAsString())
            *document->mutable_layout()->mutable_tokens() = layout.tokens();
    }
    if (changes.contains("enabled") && document->enable_ccli_display() != changes.value("enabled").toBool())
        document->set_enable_ccli_display(changes.value("enabled").toBool());
    if (changes.contains("license") && document->ccli_license() != changes.value("license").toString().trimmed().toStdString())
        document->set_ccli_license(changes.value("license").toString().trimmed().toStdString());
    return {};
}

rv::data::CCLIDocument made(const std::function<std::string()> &newId)
{
    rv::data::CCLIDocument document;
    const std::pair<TokenType, const char *> layout[] = {
        {rv::data::CopyrightLayout::Title, nullptr},     {rv::data::CopyrightLayout::Text, "\n"},
        {rv::data::CopyrightLayout::Artist, nullptr},    {rv::data::CopyrightLayout::Text, "\n"},
        {rv::data::CopyrightLayout::Publisher, nullptr}, {rv::data::CopyrightLayout::Text, "\302\251 "},
        {rv::data::CopyrightLayout::CopyrightYear, nullptr}, {rv::data::CopyrightLayout::Text, "\n"},
        {rv::data::CopyrightLayout::LicenseNumber, nullptr},
    };
    for (const auto &[type, text] : layout) {
        Token *token = document.mutable_layout()->add_tokens();
        token->set_token_type(type);
        if (text)
            token->set_text(text);
    }
    rv::data::Slide *slide = document.mutable_template_()->mutable_base_slide();
    slide->mutable_uuid()->set_string(newId());
    slide->mutable_size()->set_width(1920);
    slide->mutable_size()->set_height(1080);
    return document;
}

QVariantMap describe(const rv::data::Presentation::CCLI &song)
{
    return {
        {"title", QString::fromStdString(song.song_title())},
        {"artist", QString::fromStdString(song.artist_credits())},
        {"author", QString::fromStdString(song.author())},
        {"publisher", QString::fromStdString(song.publisher())},
        {"year", int(song.copyright_year())},
        {"songNumber", int(song.song_number())},
        {"album", QString::fromStdString(song.album())},
        {"display", song.display()},
    };
}

void change(rv::data::Presentation::CCLI *song, const QVariantMap &changes)
{
    const auto text = [&](const char *key, const std::string &now, auto set) {
        if (changes.contains(QLatin1String(key)) && now != changes.value(QLatin1String(key)).toString().toStdString())
            set(changes.value(QLatin1String(key)).toString().toStdString());
    };
    text("title", song->song_title(), [song](const std::string &v) { song->set_song_title(v); });
    text("artist", song->artist_credits(), [song](const std::string &v) { song->set_artist_credits(v); });
    text("author", song->author(), [song](const std::string &v) { song->set_author(v); });
    text("publisher", song->publisher(), [song](const std::string &v) { song->set_publisher(v); });
    text("album", song->album(), [song](const std::string &v) { song->set_album(v); });
    const auto number = [&](const char *key, uint32_t now, auto set) {
        if (changes.contains(QLatin1String(key))) {
            const uint32_t wanted = uint32_t(qMax(0, changes.value(QLatin1String(key)).toInt()));
            if (wanted != now)
                set(wanted);
        }
    };
    number("year", song->copyright_year(), [song](uint32_t v) { song->set_copyright_year(v); });
    number("songNumber", song->song_number(), [song](uint32_t v) { song->set_song_number(v); });
    if (changes.contains("display") && song->display() != changes.value("display").toBool())
        song->set_display(changes.value("display").toBool());
}

QString compose(const QVariantList &tokens, const QVariantMap &song, const QString &license)
{
    // Each token's words: a detail's, empty where the song has none of it
    const auto value = [&](const QString &kind) -> QString {
        if (kind == QLatin1String("license"))
            return license;
        if (kind == QLatin1String("year") || kind == QLatin1String("songNumber")) {
            const int number = song.value(kind).toInt();
            return number > 0 ? QString::number(number) : QString();
        }
        return song.value(kind).toString();
    };
    struct Part
    {
        bool detail;
        QString words;
    };
    QList<Part> parts;
    for (const QVariant &entry : tokens) {
        const QVariantMap token = entry.toMap();
        const QString kind = token.value("kind").toString();
        if (kind == QLatin1String("text"))
            parts.append({false, token.value("text").toString()});
        else
            parts.append({true, value(kind).trimmed()});
    }
    // The words before a detail belong to it, and go with it where it has nothing to
    // show; those after the last detail belong to that one. (So a song with no artist
    // loses the line break before the artist, and keeps the one before the publisher.)
    // Words that are only spacing are not where the line begins.
    QString line;
    QString words;
    int last = -1;
    for (qsizetype i = 0; i < parts.size(); ++i) {
        if (parts.at(i).detail)
            last = int(i);
    }
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const Part &part = parts.at(i);
        if (!part.detail) {
            // After the last detail: that one's, where it showed
            if (i > last) {
                if (last >= 0 && !parts.at(last).words.isEmpty())
                    line += part.words;
                continue;
            }
            words += part.words;
            continue;
        }
        if (!part.words.isEmpty()) {
            if (!line.isEmpty() || !words.trimmed().isEmpty())
                line += words;
            line += part.words;
        }
        words.clear();
    }
    return line;
}

bool shownOn(const QString &display, int index, int count)
{
    if (count <= 0 || index < 0 || index >= count)
        return false;
    if (display == QLatin1String("all"))
        return true;
    const bool first = index == 0;
    const bool last = index == count - 1;
    if (display == QLatin1String("last"))
        return last;
    if (display == QLatin1String("firstAndLast"))
        return first || last;
    return first;
}

} // namespace ccli
