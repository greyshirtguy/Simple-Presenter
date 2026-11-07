#include "songimport.h"

#include "proconvert.h"
#include "songselect.h"
#include "themefile.h"
#include "workspacefiles.h"

#include "template.pb.h"

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>

namespace songimport {

QString readTextFile(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("Cannot read %1: %2").arg(QFileInfo(path).fileName(), file.errorString());
        return {};
    }
    // Such a file is a few kilobytes of text. Anything large is something else.
    if (file.size() > 2 * 1024 * 1024) {
        *error = QStringLiteral("%1 is too large to be a song").arg(QFileInfo(path).fileName());
        return {};
    }
    const QByteArray bytes = file.readAll();
    // UTF-8 as nearly all are, with or without a mark at the start; a file that is
    // not (an old one, typed on Windows) is read as Latin-1, which at least keeps
    // every accented letter of western Europe.
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8(bytes);
    if (utf8.hasError())
        text = QString::fromLatin1(bytes);
    return text;
}

chords::Song readFile(const QString &path, QString *error)
{
    const QString text = readTextFile(path, error);
    if (!error->isEmpty())
        return {};
    const chords::Song song = chords::parseSong(text);
    if (song.sections.isEmpty()) {
        *error = QStringLiteral("%1 has no words in it").arg(QFileInfo(path).fileName());
        return {};
    }
    return song;
}

rv::data::Presentation build(const QList<chords::Section> &sections, const QString &name, int linesPerSlide, const Credits &credits,
                             const QString &key)
{
    rv::data::Presentation presentation = proconvert::newPresentation(name);
    presentation.mutable_last_modified_date()->set_seconds(QDateTime::currentSecsSinceEpoch());

    if (!credits.title.isEmpty() || !credits.artist.isEmpty() || !credits.author.isEmpty() || !credits.publisher.isEmpty()
        || credits.year > 0 || !credits.songNumber.isEmpty()) {
        auto *ccli = presentation.mutable_ccli();
        ccli->set_song_title(credits.title.toStdString());
        if (!credits.artist.isEmpty())
            ccli->set_artist_credits(credits.artist.toStdString());
        if (!credits.author.isEmpty())
            ccli->set_author(credits.author.toStdString());
        if (!credits.publisher.isEmpty())
            ccli->set_publisher(credits.publisher.toStdString());
        if (credits.year > 0)
            ccli->set_copyright_year(uint32_t(credits.year));
        bool isNumber = false;
        const uint number = credits.songNumber.toUInt(&isNumber);
        if (isNumber)
            ccli->set_song_number(number);
        if (credits.display)
            ccli->set_display(true);
    }
    if (chords::keyNumber(key) >= 0) {
        using Scale = rv::data::MusicKeyScale;
        for (Scale *target : {presentation.mutable_music()->mutable_original(), presentation.mutable_music()->mutable_user()}) {
            target->set_music_key(Scale::MusicKey(chords::keyNumber(key)));
            target->set_music_scale(chords::keyIsMinor(key) ? Scale::MUSIC_SCALE_MINOR : Scale::MUSIC_SCALE_MAJOR);
        }
    }

    const QSizeF size(1920, 1080);
    for (const chords::Section &section : sections) {
        // A part with no name in the file is a verse like any other to whoever runs
        // the words; but naming it one would be a guess, so it is left unnamed, in a
        // group with no name, which shows as plain slides.
        auto *group = presentation.add_cue_groups();
        group->mutable_group()->mutable_uuid()->set_string(workspace::newUuid());
        group->mutable_group()->set_name(section.name.toStdString());
        group->mutable_group()->mutable_hotkey();

        const QList<QList<chords::Line>> slides = chords::slidesOf(section, linesPerSlide);
        for (const QList<chords::Line> &lines : slides) {
            rv::data::Cue *cue = proconvert::addBlankCue(&presentation, std::string(), size);
            group->add_cue_identifiers()->set_string(cue->uuid().string());
            rv::data::Slide *slide = cue->mutable_actions(0)->mutable_slide()->mutable_presentation()->mutable_base_slide();

            // The text box: the whole slide but for a margin, named as ProPresenter
            // names the words of a song, which is the name a stage layout or a theme
            // looks for.
            rv::data::Slide::Element made = proconvert::makeTextElement(*slide, nullptr);
            rv::data::Graphics::Element *element = made.mutable_element();
            element->set_name("Lyrics");
            element->mutable_bounds()->mutable_origin()->set_x(96);
            element->mutable_bounds()->mutable_origin()->set_y(54);
            element->mutable_bounds()->mutable_size()->set_width(size.width() - 192);
            element->mutable_bounds()->mutable_size()->set_height(size.height() - 108);

            QStringList words;
            QList<chords::Chord> chordList;
            int start = 0;
            for (const chords::Line &line : lines) {
                words << line.text;
                for (const chords::Chord &chord : line.chords)
                    chordList.append({start + chord.at, chord.name});
                start += int(line.text.size()) + 1;
            }
            rv::data::Graphics::Text *text = element->mutable_text();
            const RichText model = proconvert::readText(*text);
            TextRun format = model.firstRun();
            format.size = 84;
            proconvert::writeText(text, RichText::plain(words.join(u'\n'), format, Qt::AlignHCenter));
            proconvert::writeChords(text, chordList);
            *slide->add_elements() = made;
        }
    }
    return presentation;
}

rv::data::Presentation build(const chords::Song &song, const QString &name, int linesPerSlide)
{
    Credits credits;
    credits.title = song.title;
    credits.artist = song.artist;
    credits.publisher = song.copyright;
    credits.songNumber = song.ccli;
    return build(song.sections, name, linesPerSlide, credits, song.key);
}

QString importFile(const QString &file, const QString &library, int linesPerSlide, QString *made)
{
    QString error;
    const chords::Song song = readFile(file, &error);
    if (!error.isEmpty())
        return error;
    if (!QFileInfo(library).isDir())
        return QStringLiteral("There is no library to put it in");

    QString unique;
    const QString path = proconvert::presentationFileFor(library, song.title.isEmpty() ? QFileInfo(file).completeBaseName() : song.title,
                                                         QStringLiteral("Song"), &unique);
    error = proconvert::writePresentation(path, build(song, unique, linesPerSlide));
    if (error.isEmpty())
        *made = path;
    return error;
}

QString importLyrics(const QString &text, const QString &library, int linesPerSlide, const QString &workspace, const QString &themePlace,
                     const QString &themeSlideId, QString *made)
{
    const songselect::Song song = songselect::parse(text);
    if (song.sections.isEmpty())
        return song.title.isEmpty() ? QStringLiteral("There are no words to make a song of") : QStringLiteral("%1 has no words in it").arg(song.title);
    if (!QFileInfo(library).isDir())
        return QStringLiteral("There is no library to put it in");

    // The theme slide first, so that nothing is written if it is not to be found
    rv::data::Template::Document theme;
    const rv::data::Template::Slide *themed = nullptr;
    QSet<QString> themeElements;
    if (!themePlace.isEmpty()) {
        QString error;
        if (!themefile::read(workspace, themePlace, &theme, &error))
            return error;
        themed = themefile::slideOf(theme, themeSlideId);
        if (!themed)
            return QStringLiteral("The theme \"%1\" no longer has that slide").arg(themePlace);
        themeElements = themefile::elementIds(workspace);
    }

    QString unique;
    const QString path = proconvert::presentationFileFor(library, song.title, QStringLiteral("Song"), &unique);
    Credits credits;
    credits.title = song.title;
    credits.author = song.author;
    credits.publisher = song.publisher;
    credits.year = song.year;
    credits.songNumber = song.songNumber;
    // Shown by the copyright display when the song came with its details, as
    // ProPresenter's own files from SongSelect are; words pasted with none have only
    // their title there, as a ChordPro song has, and the display off (a GUESS at what
    // ProPresenter does for a song typed in: that its display starts off)
    credits.display = !song.author.isEmpty() || !song.publisher.isEmpty() || song.year > 0 || !song.songNumber.isEmpty();
    rv::data::Presentation presentation = build(song.sections, unique, linesPerSlide, credits, QString());
    if (themed) {
        for (rv::data::Cue &cue : *presentation.mutable_cues()) {
            for (rv::data::Action &action : *cue.mutable_actions()) {
                if (action.has_slide() && action.slide().has_presentation())
                    themefile::dress(action.mutable_slide()->mutable_presentation()->mutable_base_slide(), themed->base_slide(), themeElements);
            }
        }
    }
    const QString error = proconvert::writePresentation(path, presentation);
    if (error.isEmpty())
        *made = path;
    return error;
}

}
