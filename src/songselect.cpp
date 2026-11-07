#include "songselect.h"

#include <QRegularExpression>
#include <QStringList>

namespace songselect {

namespace {

// The names a part may have, which a block's first line is one of (with a number or a
// letter after it, or not) when it is the part's label. The first few are the ones
// seen in SongSelect's files; the rest are what else a song's parts are usually
// called, so that a label is not taken for a line of words.
const QRegularExpression &labelPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral("^(?:verse|chorus|pre-chorus|pre chorus|prechorus|post-chorus|post chorus|bridge|interlude|intro|outro|ending|end|"
                       "tag|vamp|refrain|instrumental|misc|hook|coda|turnaround|channel|rap|spoken|break|reprise|exhortation|part)"
                       "(?:\\s*\\d+[a-z]?|\\s+[a-z])?$"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// "CCLI Song #6280644", with or without a space before the number
const QRegularExpression &songNumberPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^CCLI Song\\s*#\\s*(\\d+)"), QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// "© 2012 Common Hymnal Publishing; Innerland", the year and the publishers each there
// or not; "(c)" and "Copyright" do as well as the sign
const QRegularExpression &rightsPattern()
{
    static const QRegularExpression pattern(QStringLiteral("^(?:") + QChar(0x00A9) + QStringLiteral("|\\(c\\)|Copyright)\\s*(\\d{4})?\\s*(.*)$"),
                                            QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

// Whether a line is one of the footer's, by how the footer's lines start
bool isFooterLine(const QString &line)
{
    return songNumberPattern().match(line).hasMatch() || rightsPattern().match(line).hasMatch()
        || line.startsWith(QStringLiteral("CCLI Licen"), Qt::CaseInsensitive) || line.startsWith(QStringLiteral("For use solely"), Qt::CaseInsensitive);
}

// The details from the footer's lines: the authors are the lines before the song's
// number that are nothing else
void readFooter(const QStringList &lines, Song *song)
{
    QStringList authors;
    bool numbered = false;
    for (const QString &line : lines) {
        if (const QRegularExpressionMatch number = songNumberPattern().match(line); number.hasMatch()) {
            song->songNumber = number.captured(1);
            numbered = true;
            continue;
        }
        if (const QRegularExpressionMatch rights = rightsPattern().match(line); rights.hasMatch()) {
            song->year = rights.captured(1).toInt();
            QStringList publishers;
            for (const QString &publisher : rights.captured(2).split(u';')) {
                if (!publisher.trimmed().isEmpty())
                    publishers.append(publisher.trimmed());
            }
            song->publisher = publishers.join(u'\n');
            continue;
        }
        if (isFooterLine(line))
            continue;
        if (!numbered && song->year == 0 && song->publisher.isEmpty())
            authors.append(line);
    }
    song->author = authors.join(QStringLiteral(", "));
}

}

bool isLabel(const QString &line)
{
    return labelPattern().match(line.trimmed()).hasMatch();
}

Song parse(const QString &text)
{
    Song song;
    QString body = text;
    if (body.startsWith(QChar(0xFEFF)))
        body.remove(0, 1);
    body.replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(u'\r', u'\n');

    // The blocks: runs of lines with something on them, parted by blank lines (a line
    // of nothing but spaces is blank), each line without the spaces round it
    QList<QStringList> blocks;
    QStringList block;
    for (const QString &raw : body.split(u'\n')) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) {
            if (!block.isEmpty())
                blocks.append(block);
            block.clear();
        } else {
            block.append(line);
        }
    }
    if (!block.isEmpty())
        blocks.append(block);
    if (blocks.isEmpty())
        return song;

    // The title is the first line. Whatever else its block holds (nothing, in
    // SongSelect's files) goes on as a block of its own.
    song.title = blocks.first().takeFirst();
    if (blocks.first().isEmpty())
        blocks.removeFirst();

    // The footer: the block with the song's number in it, or, with none, a last block
    // that starts as a footer does. From it on, nothing is words, and the details may
    // be spread over the blocks after it.
    int footer = -1;
    for (int i = int(blocks.size()) - 1; i >= 0 && footer < 0; --i) {
        for (const QString &line : blocks[i]) {
            if (songNumberPattern().match(line).hasMatch()) {
                footer = i;
                break;
            }
        }
    }
    if (footer < 0 && !blocks.isEmpty() && isFooterLine(blocks.last().first()))
        footer = int(blocks.size()) - 1;
    if (footer >= 0) {
        QStringList details;
        for (int i = footer; i < blocks.size(); ++i)
            details += blocks[i];
        readFooter(details, &song);
        blocks.remove(footer, blocks.size() - footer);
    }

    for (const QStringList &lines : blocks) {
        chords::Section section;
        int from = 0;
        if (isLabel(lines.first())) {
            section.name = lines.first();
            from = 1;
        }
        for (int i = from; i < lines.size(); ++i)
            section.lines.append({lines[i], {}});
        // A label with nothing under it has no words for a slide
        if (section.lines.isEmpty())
            continue;
        song.sections.append(section);
    }
    return song;
}

}
