#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// The benchmark (--benchmark <file>): times the things an operator waits for, on a
// workspace of its own making, and writes them to a file as a line each.
//
// It is for telling whether a change to the app has made it slower. So what it times
// has to be the same from one run to the next, which a real workspace is not: the
// benchmark makes its own, with three presentations of fixed content (words; shapes,
// gradients and pictures cut to shapes; words over pictures), and throws it away when
// it is done. The user's workspaces and settings are not read or touched.
//
// The run itself is qml/Benchmark.qml, which works the app as an operator would. This
// is what that needs and QML cannot do: a fine clock, the processor time and memory the
// app has used, and somewhere to write the answers.
//
// benchmark/run.py runs it several times over and compares the result with
// benchmark/baseline.txt; see benchmark/README.md.
class Benchmark : public QObject
{
    Q_OBJECT

public:
    Benchmark(const QString &reportPath, QObject *parent = nullptr);

    // Writes the benchmark's workspace into `folder`, which should be empty. Returns
    // an error message, empty on success.
    static QString makeWorkspace(const QString &folder);
    // Milliseconds from when the program was started to now: counted from the start of
    // the process, before any of the program's own code ran.
    static double sinceStart();

    // The same, for the run: every time in the report is on this clock.
    Q_INVOKABLE double now() const { return sinceStart(); }
    // Milliseconds of processor time the app has used, on all its threads
    Q_INVOKABLE double cpu() const;
    // Megabytes of memory the app holds now, and the most it has held
    Q_INVOKABLE double memory() const;
    Q_INVOKABLE double peakMemory() const;
    // One measurement, for the report, said on the terminal as it is made
    Q_INVOKABLE void record(const QString &name, double value, const QString &unit);
    // Writes the report and ends the app.
    Q_INVOKABLE void finish();

private:
    QString m_reportPath;
    QStringList m_lines;
};
