[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Messages

A **message** is something to say on the screens over everything else: a parent wanted
at the creche, a car with its lights on. Messages are the sixth tab of the show
controls, the one with ProPresenter's paper plane on it. They are shown on a layer of
their own, over the props, any number at once; and they are ProPresenter's own, kept
in its file of them (`Configuration/Messages`).

At the top of the tab are the workspace's messages, by name; under them, with a line
between that is dragged to share out the height, what the one picked is made of; and at
the bottom right the button that shows it and hides it. The `+` makes a message, with
its name ready to be typed over. A right click on one shows or hides it, renames it or
removes it.

What a message is made of, from the top:

- **Its words**, in a box. Among them can be **tokens**: places where something is put
  in when the message is shown. A token is a small rounded label in the box, which is
  moved over and deleted as one thing. The words are saved a moment after the typing
  stops.
- **The box that adds a token.** Its arrow has the tokens there are to pick from:
  **Name**, **Room** and **Message**, which are given words by whoever shows the
  message; and **System Clock** and **Timer** (which leads to the workspace's
  [timers](timers.md)), which need nothing typed and go on changing while the message is
  shown. A name typed into the box, and Enter, makes a token of that name. A token goes
  in where the caret is. One the message has already is used again, so that one typing
  fills every place it stands in.
- **A box for each token that is given words**, to type them in before the message is
  shown. They are kept with the message.
- **Theme**: the slide of a [theme](themes.md) the message is drawn in, from the menu of
  themes a slide's own menu has. The theme slide is shown as it is, with the message's
  words in its first text box, in that box's font and colour. With no theme, the words
  are white on a dark band across the foot of the screen.
- **Hide**: how the message goes away. **Manually**, or **After Time**, which leads to
  5, 10, 15, 20, 30, 45 or 60 seconds.

**Show** puts the message on the screens, and the button then says **Hide**. What is
on the screens is the message as it was when it was shown: words typed while it is
shown are not there until it is shown again, and while the message is other than what
is being shown the button says Show, which does that. Its time, if it has one, starts
again then.

The messages have a clear button of their own, the fifth of the layers', and **F6**. A
[clear group](clear-groups.md) that has the messages clears them, as Clear All does; an
action on a slide or in a macro that clears the messages, or shows one, is done; and a
[look](looks.md) says which screens get the messages, as it does for the props.

What is a guess, or not done:

- ProPresenter's own pane was described, not looked at. The order of things in it and
  what each is called are as described. That a token is used again where one of its
  name is asked for twice, that a message which is shown is not changed by typing into
  it, where the tab stands among the others, and that F6 is the key that clears the
  messages, are guesses.
- How a theme is used for a message is a guess: its first text box takes the words
  (the first thing on the theme slide with words in it, which is how dressing a slide
  in a theme takes a theme's text boxes; or, on a slide where nothing has words, the
  first thing on it), and the rest of the theme slide is shown with it.
- The file was learnt from its format and from one file of someone else's: no
  workspace here had a message in it. Where this writes what that one file has no
  example of (what a text token was given, a clock), it is a guess that ProPresenter
  writes it so.
- A timer in a message shows the timer as it stands. ProPresenter keeps a setting of
  the timer with the token, and how its time is written: the first is kept and not
  gone by (the timer is not set going), and the second is gone by and cannot be
  changed here. A timer put in here is written in minutes and seconds.
- A message set in ProPresenter to go when its timers have run out stays until it is
  hidden.
- An action that shows a message cannot be made here, only run; and what such an
  action says the message's tokens are to be given is not read. The message is shown
  with what they were last given here.
- Whether a message can be seen on the network (ProPresenter's web notifications) is
  kept as the file has it, and nothing is done about it.
- The tab has little room in a small window, and what a message is made of scrolls.
