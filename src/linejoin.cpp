#include "linejoin.h"

#include <QList>

RichText joinedLines(const RichText &text, const QString &separator)
{
    TextParagraph one;
    TextRun first;
    bool firstFound = false;
    for (const TextParagraph &paragraph : text.paragraphs) {
        if (!paragraph.runs.isEmpty()) {
            first = paragraph.runs.first();
            firstFound = true;
            break;
        }
    }
    if (!text.paragraphs.isEmpty()) {
        one.alignment = text.paragraphs.first().alignment;
        one.lineHeight = text.paragraphs.first().lineHeight;
        one.lineHeightIsMultiple = text.paragraphs.first().lineHeightIsMultiple;
    } else {
        one.alignment = Qt::AlignHCenter;
    }

    // The lines, each a list of runs, cut where a paragraph ends or a line breaks
    QList<QList<TextRun>> lines;
    for (const TextParagraph &paragraph : text.paragraphs) {
        lines.append(QList<TextRun>());
        for (const TextRun &run : paragraph.runs) {
            QString pieceText;
            const auto put = [&lines, &run](const QString &words) {
                if (words.isEmpty())
                    return;
                TextRun piece = run;
                piece.text = words;
                lines.last().append(piece);
            };
            for (const QChar c : run.text) {
                if (c == QChar::LineSeparator || c == u'\n' || c == QChar::ParagraphSeparator) {
                    put(pieceText);
                    pieceText.clear();
                    lines.append(QList<TextRun>());
                } else {
                    pieceText += c;
                }
            }
            put(pieceText);
        }
    }

    // Each line trimmed at both ends, through as many runs as are only space; and a line
    // with nothing left left out
    for (QList<TextRun> &line : lines) {
        while (!line.isEmpty()) {
            QString &words = line.first().text;
            qsizetype start = 0;
            while (start < words.size() && words.at(start).isSpace())
                ++start;
            words.remove(0, start);
            if (!words.isEmpty())
                break;
            line.removeFirst();
        }
        while (!line.isEmpty()) {
            QString &words = line.last().text;
            qsizetype end = words.size();
            while (end > 0 && words.at(end - 1).isSpace())
                --end;
            words.truncate(end);
            if (!words.isEmpty())
                break;
            line.removeLast();
        }
    }

    for (const QList<TextRun> &line : std::as_const(lines)) {
        if (line.isEmpty())
            continue;
        if (!one.runs.isEmpty() && !separator.isEmpty()) {
            TextRun between = one.runs.last();
            between.text = separator;
            one.runs.append(between);
        }
        one.runs += line;
    }
    if (one.runs.isEmpty()) {
        TextRun empty = firstFound ? first : TextRun();
        empty.text.clear();
        one.runs.append(empty);
    }

    RichText result;
    result.paragraphs.append(one);
    return result;
}
