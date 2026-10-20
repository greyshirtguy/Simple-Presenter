[Simple Presenter](../README.md)

# Helping yourself, with an AI

Nobody supports this app, and that is not going to change. But the app was written by an
AI model, its source is all here, and most of what goes wrong is particular to one
computer: a driver, a font, a display, a workspace that was moved. An AI coding agent
running on *your* computer can see all of that, which nobody answering a question from
far away could. It can read the app's log, build the app from this source, run it on a
copy of your workspace, add checks of its own and work out what is wrong. So the help
on offer is this: bring your own.

The repository is set up for it. [AGENTS.md](../AGENTS.md) tells an agent what the app is,
what must never be broken, and how to keep its hands off your files, and
[docs/agent-playbook.md](agent-playbook.md) is its guide to finding out what is
wrong on someone's computer. Any coding agent that reads `AGENTS.md` will find them. The
one this app was built with is [Claude Code](https://code.claude.com/docs/en/quickstart):

1. Install it. In a terminal:

   ```
   curl -fsSL https://claude.ai/install.sh | bash
   ```

   It needs a paid Claude plan (Pro or above) or an API account; the free plan does not
   include it. What that costs changes, so look it up.
2. Get this source, and start the agent in it:

   ```
   git clone --recurse-submodules https://github.com/greyshirtguy/Simple-Presenter.git
   cd Simple-Presenter
   claude
   ```

3. Say what is wrong in your own words: what you did, what you expected, what happened.
   For example: *"Simple Presenter stutters when it plays a video. Read AGENTS.md and
   the playbook it points to, look at the app's latest log, and tell me what you find
   before you change anything."*

**This is not support, and it has risks.** Know them before you start:

- **It is a program that runs commands on your computer**, with your say-so. It asks
  before it runs something or changes a file: read what it asks, and say no to what you
  do not understand. Do not switch the asking off.
- **It can be wrong, and sound sure.** It may say a thing is fixed that is not. Ask it
  what it checked.
- **It sees your files.** A workspace has your songs in it and a good deal about your
  church, and what the agent reads goes to the company that runs it, under that
  company's terms and not this project's. The instructions here tell it to send nothing
  of yours anywhere else. They are instructions, not a lock.
- **It should only ever work on a copy of your workspace.** The instructions say so.
  Keep a backup of your own anyway, as you should with this app in any case.
- **It costs money, and can run out.** A plan has limits, and a long hunt can reach them.
- **If it changes the app for you, you are then running a version of your own**, which
  nobody else has. That is fine, and is what the source is for. Ask it to write down
  what it changed.
- **Nobody here checks what it does.** A fix that everyone needs can be offered back as
  a pull request, under the rules in `AGENTS.md`; that anyone will read it is not
  promised.

It is cheap to try, and may well be all the help you need. It is also the only kind
there is.
