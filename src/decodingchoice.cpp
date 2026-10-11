#include "decodingchoice.h"

#include <QList>

namespace decodingchoice {

const char kVariable[] = "QT_FFMPEG_DECODING_HW_DEVICE_TYPES";

namespace {

Choice g_session;

// The names in a value of the variable, as Qt takes them: split at the commas, with
// nothing made of an empty one.
QList<QByteArray> names(const QByteArray &value)
{
    QList<QByteArray> found;
    const QList<QByteArray> parts = value.split(',');
    for (const QByteArray &part : parts) {
        const QByteArray name = part.trimmed().toLower();
        if (!name.isEmpty())
            found.append(name);
    }
    return found;
}

// A list that is one comma: set, and naming nothing. (An empty value would do as well;
// a comma cannot be mistaken for a variable that is not set.)
QByteArray none()
{
    return QByteArrayLiteral(",");
}

} // namespace

Mode modeOf(const QString &saved)
{
    if (saved == QLatin1String("processor"))
        return Mode::ProcessorOnly;
    if (saved == QLatin1String("hardware"))
        return Mode::GraphicsChip;
    return Mode::Automatic;
}

QString nameOf(Mode mode)
{
    switch (mode) {
    case Mode::ProcessorOnly:
        return QStringLiteral("processor");
    case Mode::GraphicsChip:
        return QStringLiteral("hardware");
    case Mode::Automatic:
        break;
    }
    return QStringLiteral("auto");
}

bool namesNone(const QByteArray &value)
{
    return names(value).isEmpty();
}

Choice choose(const Asked &asked)
{
    Choice choice;
    choice.guarded = asked.guarded;
    choice.guardReason = asked.guarded ? asked.guardReason : QString();

    // Asked for outright, one way or the other: the command line first, then the
    // setting. (Both of the command line's at once is the careful one of the two.)
    const bool software = asked.softwareFlag || (!asked.hardwareFlag && asked.mode == Mode::ProcessorOnly);
    const bool hardware = !asked.softwareFlag && (asked.hardwareFlag || asked.mode == Mode::GraphicsChip);
    if (software) {
        choice.softwareOnly = true;
        choice.by = asked.softwareFlag ? By::CommandLine : By::Settings;
        choice.set = none();
        return choice;
    }
    if (hardware) {
        // The variable is left as it was found: what was asked for is that the guard
        // should not hold, and not that anyone's own list of decoders should go.
        choice.by = asked.hardwareFlag ? By::CommandLine : By::Settings;
        if (asked.environmentSet) {
            choice.softwareOnly = namesNone(asked.environment);
            choice.kept = QString::fromLatin1(names(asked.environment).join(", "));
        }
        choice.overridden = asked.guarded && !choice.softwareOnly;
        return choice;
    }
    if (asked.environmentSet) {
        choice.by = By::Environment;
        choice.softwareOnly = namesNone(asked.environment);
        choice.kept = QString::fromLatin1(names(asked.environment).join(", "));
        return choice;
    }
    if (asked.guarded) {
        choice.softwareOnly = true;
        choice.by = By::Guard;
        choice.set = none();
    }
    return choice;
}

QString inWords(const Choice &choice)
{
    const QString variable = QLatin1String(kVariable);
    // Asked for by the environment, whoever else spoke: the variable names what is tried.
    if (choice.by == By::Environment || (!choice.kept.isEmpty() && !choice.softwareOnly) || (choice.softwareOnly && choice.set.isNull())) {
        return choice.softwareOnly
            ? QStringLiteral("by the processor only: %1 is set, and names no hardware decoder").arg(variable)
            : QStringLiteral("by the graphics chip where it can be, kept to %1 by %2").arg(choice.kept, variable);
    }
    if (choice.softwareOnly) {
        switch (choice.by) {
        case By::CommandLine:
            return QStringLiteral("by the processor only, as --software-decoding asks");
        case By::Settings:
            return QStringLiteral("by the processor only, as the settings ask");
        case By::Guard:
            return QStringLiteral("by the processor only, because %1").arg(choice.guardReason);
        default:
            break;
        }
    }
    QString said = QStringLiteral("by the graphics chip where its driver offers that, and otherwise by the processor");
    if (choice.overridden) {
        said += QStringLiteral(": %1 for that, although %2")
                    .arg(choice.by == By::CommandLine ? QStringLiteral("--hardware-decoding asks") : QStringLiteral("the settings ask"),
                         choice.guardReason);
    }
    return said;
}

QString logLine(const Choice &choice)
{
    return QStringLiteral("video is decoded %1%2").arg(inWords(choice),
        choice.softwareOnly ? QStringLiteral(" (hardware decoding is not tried)") : QString());
}

void setSession(const Choice &choice)
{
    g_session = choice;
}

const Choice &session()
{
    return g_session;
}

} // namespace decodingchoice
