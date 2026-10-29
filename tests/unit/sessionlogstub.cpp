// What a unit test that finds fonts (fontresolver.cpp) needs of the session log: its one
// static write, which says what font is drawn in place of one not installed. The real log
// (sessionlog.cpp) is the app's, with its windows; a test has nowhere to write it.

#include "sessionlog.h"

void SessionLog::write(const char *, const QString &)
{
}
