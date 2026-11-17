#pragma once

#include <QByteArray>
#include <QString>

// The app started as an MCP server on its standard input and output (mcp.h says what
// MCP is and how its messages are answered): `SimplePresenter --mcp`.
//
// Why there is such a thing. A program that lets an AI model use tools (Claude's
// desktop app, an editor's assistant) is most often set up by being told a command to
// run: it starts the command itself, writes MCP's messages to its standard input, one a
// line, and reads the answers from its standard output. So that the person need install
// nothing else and keep nothing else running, the command is this app itself, started
// with `--mcp`: it opens no window, no log and no workspace, and is not the show. It
// answers what the tools are by itself, and carries a tool out by asking the app that
// *is* running the show, through the extended API (extendedapi.h) on this computer:
// each tool is one request to that API, which is why the two cannot differ.
//
// So the app must be running, with Enable Network and Enable Extended API on in its
// settings, for a tool to do anything; while it is not, the tools are still listed and
// each call says what to switch on, in words the model can pass on.
//
// Where the app is asked: at 127.0.0.1 on the port the app's own settings name (the
// HTTP API's), read from the same settings file; or wherever `--mcp-api <address>` says
// ("http://192.168.1.20:50001"), for a show that runs on another computer, which must
// have opened its extended API to the network.
//
// Nothing but MCP's messages is written to standard output, as MCP requires; what
// there is to say besides goes to standard error. It ends when its standard input is
// closed, which is how the program that started it says it is done.
namespace mcpstdio {

// Whether the command line asks for this (`--mcp` among its arguments)
bool asked(int argc, char *argv[]);
// Runs it until standard input ends: the process's exit code
int run(int argc, char *argv[], const QString &version);

// One request made of the API, and what came back: for tests, and for run()
struct Response
{
    // 0 if the app could not be reached at all
    int status = 0;
    QByteArray body;
};
Response request(const QString &host, int port, const QByteArray &method, const QString &path, const QByteArray &body, int timeoutMs = 60000);

// An HTTP response read from the bytes of one: its status and its body, whole. False
// until all of it has come.
bool parseResponse(const QByteArray &bytes, Response *response);

}
