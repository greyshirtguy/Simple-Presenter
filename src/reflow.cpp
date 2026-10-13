#include "reflow.h"

namespace reflow {

namespace {

// What a caret may be among without its being part of either slide's words: a space, a
// tab, a line break. (Not every character Unicode calls a space: a line of chords
// with no words is made of wide and zero-width spaces, which are that line's own; see
// chords::placeholders.)
bool isGap(QChar character)
{
    return character == u' ' || character == u'\t' || character == u'\n' || character == QChar::LineSeparator
        || character == QChar::ParagraphSeparator;
}

// A paragraph as the one given is set, with no words: one empty run, set as `like`.
TextParagraph emptied(const TextParagraph &paragraph, const TextRun &like)
{
    TextParagraph result = paragraph;
    TextRun run = like;
    run.text.clear();
    result.runs = {run};
    return result;
}

// Adds a run to a paragraph. A run with no words is kept only where the paragraph has
// no other, to say how the paragraph is set, and gives way to the first that has
// words. (Two runs set alike that end up side by side are left as two: nothing here
// compares how runs are set, and the file's text is written the same either way.)
void append(TextParagraph *paragraph, const TextRun &run)
{
    if (paragraph->runs.size() == 1 && paragraph->runs.first().text.isEmpty()) {
        paragraph->runs.first() = run;
        return;
    }
    if (!run.text.isEmpty() || paragraph->runs.isEmpty())
        paragraph->runs.append(run);
}

// One text straight after another, with no line break put between: the second's first
// paragraph goes on from the first's last.
RichText spliced(const RichText &first, const RichText &second)
{
    if (first.paragraphs.isEmpty())
        return second;
    if (second.paragraphs.isEmpty())
        return first;
    RichText result = first;
    TextParagraph &last = result.paragraphs.last();
    for (const TextRun &run : second.paragraphs.first().runs)
        append(&last, run);
    for (qsizetype i = 1; i < second.paragraphs.size(); ++i)
        result.paragraphs.append(second.paragraphs.at(i));
    return result;
}

// The run a character is in, and the paragraph: for the look of what is typed there.
// `place` past the end is the last character's.
struct Look
{
    TextRun run;
    TextParagraph paragraph;
    bool found = false;
};
Look lookAt(const RichText &text, int place)
{
    Look look;
    int at = 0;
    for (const TextParagraph &paragraph : text.paragraphs) {
        for (const TextRun &run : paragraph.runs) {
            // (The first run there is, until a better one is come to; and every run
            // on the way, so that a place past the end has the last.)
            if (!look.found || place >= at) {
                look.run = run;
                look.paragraph = paragraph;
                look.found = true;
            }
            at += int(run.text.size());
            if (place < at)
                return look;
        }
        // The break after this paragraph
        ++at;
        if (place < at)
            return look;
    }
    return look;
}

} // namespace

int length(const RichText &text)
{
    int count = 0;
    for (const TextParagraph &paragraph : text.paragraphs) {
        for (const TextRun &run : paragraph.runs)
            count += int(run.text.size());
    }
    return count + int(qMax(qsizetype(0), text.paragraphs.size() - 1));
}

QString words(const RichText &text)
{
    QString all;
    for (qsizetype i = 0; i < text.paragraphs.size(); ++i) {
        if (i > 0)
            all += u'\n';
        for (const TextRun &run : text.paragraphs.at(i).runs)
            all += run.text;
    }
    all.replace(QChar::LineSeparator, u'\n');
    return all;
}

Split splitAt(const QString &all, int caret)
{
    const int size = int(all.size());
    Split split;
    split.leftEnd = qBound(0, caret, size);
    split.rightStart = split.leftEnd;
    while (split.leftEnd > 0 && isGap(all.at(split.leftEnd - 1)))
        --split.leftEnd;
    while (split.rightStart < size && isGap(all.at(split.rightStart)))
        ++split.rightStart;
    return split;
}

RichText cut(const RichText &text, int from, int to)
{
    RichText result;
    const int end = qBound(0, to, length(text));
    const int start = qBound(0, from, end);
    int at = 0;
    for (qsizetype i = 0; i < text.paragraphs.size(); ++i) {
        const TextParagraph &paragraph = text.paragraphs.at(i);
        int size = 0;
        for (const TextRun &run : paragraph.runs)
            size += int(run.text.size());
        // A paragraph has a part in what is taken if any of its characters are in the
        // stretch, or if the break before it or the break after it is: a break that is
        // taken has a line on either side of it, though one of them may be empty.
        const bool charactersIn = at < end && at + size > start;
        const bool breakBeforeIn = i > 0 && at - 1 >= start && at - 1 < end;
        const bool breakAfterIn = i + 1 < text.paragraphs.size() && at + size >= start && at + size < end;
        if (charactersIn || breakBeforeIn || breakAfterIn) {
            TextParagraph part = paragraph;
            part.runs.clear();
            int place = at;
            for (const TextRun &run : paragraph.runs) {
                const int runEnd = place + int(run.text.size());
                const int a = qMax(place, start);
                const int b = qMin(runEnd, end);
                if (b > a) {
                    TextRun piece = run;
                    piece.text = run.text.mid(a - place, b - a);
                    append(&part, piece);
                }
                place = runEnd;
            }
            if (part.runs.isEmpty())
                part = emptied(paragraph, lookAt(text, qBound(at, start, at + size)).run);
            result.paragraphs.append(part);
        }
        at += size + 1;
    }
    if (result.paragraphs.isEmpty()) {
        const Look look = lookAt(text, start);
        result.paragraphs.append(emptied(look.paragraph, look.run));
    }
    return result;
}

RichText joined(const RichText &first, const RichText &second)
{
    if (length(second) == 0)
        return first.paragraphs.isEmpty() ? second : first;
    if (length(first) == 0)
        return second;
    RichText result = first;
    result.paragraphs.append(second.paragraphs);
    return result;
}

RichText retyped(const RichText &was, const QString &now)
{
    const QString before = words(was);
    if (before == now)
        return was;
    int same = 0;
    const int shorter = int(qMin(before.size(), now.size()));
    while (same < shorter && before.at(same) == now.at(same))
        ++same;
    int tail = 0;
    while (tail < shorter - same && before.at(before.size() - 1 - tail) == now.at(now.size() - 1 - tail))
        ++tail;
    // What was typed takes the look of the character before it; at the very start,
    // of the character it was typed before.
    const Look look = lookAt(was, same > 0 ? same - 1 : 0);
    RichText typed;
    const QStringList lines = now.mid(same, now.size() - same - tail).split(u'\n');
    for (const QString &line : lines) {
        TextParagraph paragraph = look.paragraph;
        TextRun run = look.run;
        run.text = line;
        paragraph.runs = {run};
        typed.paragraphs.append(paragraph);
    }
    RichText result = same > 0 ? cut(was, 0, same) : RichText();
    result = result.paragraphs.isEmpty() ? typed : spliced(result, typed);
    if (tail > 0)
        result = spliced(result, cut(was, int(before.size()) - tail, int(before.size())));
    return result;
}

QList<chords::Chord> chordsLeft(const QList<chords::Chord> &all, const Split &split)
{
    QList<chords::Chord> kept;
    for (const chords::Chord &chord : all) {
        if (chord.at < split.leftEnd)
            kept.append(chord);
    }
    return kept;
}

QList<chords::Chord> chordsRight(const QList<chords::Chord> &all, const Split &split)
{
    QList<chords::Chord> moved;
    bool atStart = false;
    for (const chords::Chord &chord : all)
        atStart = atStart || chord.at == split.rightStart;
    for (const chords::Chord &chord : all) {
        if (chord.at >= split.rightStart) {
            moved.append({chord.at - split.rightStart, chord.name});
        } else if (chord.at >= split.leftEnd && !atStart) {
            moved.append({0, chord.name});
            atStart = true;
        }
    }
    return moved;
}

QList<chords::Chord> chordsJoined(const QList<chords::Chord> &first, int firstLength, const QList<chords::Chord> &second,
                                  bool brokenBetween)
{
    QList<chords::Chord> all = first;
    for (const chords::Chord &chord : second)
        all.append({chord.at + firstLength + (brokenBetween ? 1 : 0), chord.name});
    return all;
}

} // namespace reflow
