#include "bibleflow.h"

#include <cmath>
#include <limits>

namespace bibleflow {

namespace {

// What a word ends with, the closing marks after it set aside: "world.”" ends with ".".
QChar lastMark(const QString &word)
{
    static const QString closers = QStringLiteral("\"'”’)]»›");
    qsizetype end = word.size();
    while (end > 0 && closers.contains(word.at(end - 1)))
        --end;
    return end > 0 ? word.at(end - 1) : QChar();
}

bool sentenceEnd(const QString &word)
{
    const QChar c = lastMark(word);
    return c == u'.' || c == u'!' || c == u'?' || c == QChar(0x2026);
}

bool clauseEnd(const QString &word)
{
    const QChar c = lastMark(word);
    return c == u';' || c == u':' || c == QChar(0x2014) || c == QChar(0x2013) || word == u"-";
}

// The cost of a break after a word, with the extra for leaving a word or two of a phrase
// on one side of it: a phrase being what lies between this break and the nearest better
// ones (of lower cost) either side.
int fullCost(const QList<Word> &words, int after, int from, int to)
{
    const int cost = breakCost(words, after);
    if (cost <= 2 || after >= to - 1)
        return cost;
    int back = 1;
    for (int k = after - 1; k >= from && breakCost(words, k) >= cost; --k)
        ++back;
    int forward = 1;
    for (int k = after + 1; k < to - 1 && breakCost(words, k) >= cost; ++k)
        ++forward;
    return back <= 2 || forward <= 2 ? cost + 8 : cost;
}

}

QList<Word> wordsOf(const QList<bible::PassageVerse> &verses)
{
    QList<Word> words;
    for (int v = 0; v < verses.size(); ++v) {
        const bible::Verse &verse = verses.at(v).verse;
        const QString &text = verse.text;
        bool first = true;
        qsizetype i = 0;
        while (i < text.size()) {
            while (i < text.size() && text.at(i).isSpace())
                ++i;
            const qsizetype start = i;
            while (i < text.size() && !text.at(i).isSpace())
                ++i;
            if (i == start)
                break;
            // Red if the middle of it is in a red stretch
            const qsizetype middle = (start + i) / 2;
            bool red = false;
            for (const auto &[at, length] : verse.red) {
                if (middle >= at && middle < at + length)
                    red = true;
            }
            words.append({text.mid(start, i - start), v, first, red});
            first = false;
        }
    }
    return words;
}

int breakCost(const QList<Word> &words, int after)
{
    if (after < 0 || after >= words.size() - 1)
        return 0;
    const QString &word = words.at(after).text;
    const bool verseEnd = words.at(after + 1).first;
    if (sentenceEnd(word))
        return verseEnd ? 0 : 1;
    if (verseEnd)
        return clauseEnd(word) ? 2 : 3;
    if (clauseEnd(word))
        return 4;
    if (lastMark(word) == u',')
        return 6;
    return 20;
}

QList<Slide> flow(const QList<Word> &words, const Options &options, const Fits &fits)
{
    QList<Slide> slides;
    const int count = int(words.size());
    if (count == 0)
        return slides;

    // The stretches flowed each by itself: every verse, with Break on New Verse; else the
    // whole passage.
    QList<std::pair<int, int>> stretches;
    for (int i = 0; i < count; ++i) {
        if (i == 0 || (options.breakOnNewVerse && words.at(i).first))
            stretches.append({i, i + 1});
        else
            stretches.last().second = i + 1;
    }

    for (const auto &[from, to] : stretches) {
        // The furthest each word can start a slide and go to. A slide that holds the words
        // from i to j fits whenever one from a later word to j, or from i to an earlier
        // word, does, so this goes forward with both ends and asks `fits` about twice
        // for each word, however long the passage.
        QList<int> furthest(to - from + 1, 0);
        int end = from + 1;
        for (int i = from; i < to; ++i) {
            end = std::max(end, i + 1);
            while (end < to && fits(i, end + 1))
                ++end;
            furthest[i - from] = end; // (a word too long even alone has a slide to itself)
        }

        // Fill: as much on each slide as goes, which is ProPresenter's way, and how many
        // slides there are to be either way
        QList<Slide> filled;
        for (int i = from; i < to; i = furthest[i - from])
            filled.append({i, furthest[i - from]});
        if (options.mode == Mode::Fill || filled.size() == 1) {
            slides += filled;
            continue;
        }

        // Even: the cheapest way to break the stretch into that many slides
        const int n = int(filled.size());
        const int length = to - from;
        QList<double> sizes(length + 1, 0); // the length of the first so many words, with a space after each
        for (int i = 0; i < length; ++i)
            sizes[i + 1] = sizes[i] + words.at(from + i).text.size() + 1;
        const double share = sizes[length] / n;
        QList<int> breakCosts(length, 0);
        for (int i = 0; i < length; ++i)
            breakCosts[i] = fullCost(words, from + i, from, to);
        const double never = std::numeric_limits<double>::infinity();
        // best[k][j]: the least cost of the first j words on k slides; came[k][j]: where
        // the last of those slides starts
        QList<QList<double>> best(n + 1, QList<double>(length + 1, never));
        QList<QList<int>> came(n + 1, QList<int>(length + 1, -1));
        best[0][0] = 0;
        for (int k = 1; k <= n; ++k) {
            for (int j = 1; j <= length; ++j) {
                const double ending = j < length ? breakCosts[j - 1] : 0;
                for (int i = j - 1; i >= 0 && furthest[i] - from >= j; --i) {
                    if (best[k - 1][i] == never)
                        continue;
                    const double off = (sizes[j] - sizes[i] - share) / share;
                    const double cost = best[k - 1][i] + ending + 40 * off * off;
                    if (cost < best[k][j]) {
                        best[k][j] = cost;
                        came[k][j] = i;
                    }
                }
            }
        }
        if (best[n][length] == never) {
            slides += filled;
            continue;
        }
        QList<Slide> chosen;
        for (int k = n, j = length; k > 0; --k) {
            const int i = came[k][j];
            chosen.prepend({from + i, from + j});
            j = i;
        }
        slides += chosen;
    }
    return slides;
}

}
