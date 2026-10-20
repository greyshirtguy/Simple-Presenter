[Simple Presenter](../../README.md) › [What it can do](../../README.md#what-it-can-do)

# Bibles

ProPresenter's Bible view, under ProPresenter's names: a passage is looked up and made
into slides in a theme. **Bible** in the toolbar, after Reflow (an open book), or
**Ctrl+Alt+B**, which is ProPresenter's key for it, brings a bar up over the slides; the
button again, Show, or Edit goes back to the presentation that was being viewed, or to
the one clicked in the list at the left since. The grid under the bar shows only what
Bible mode makes: nothing at first, and after that the slides it made last (until the
app is closed). A presentation clicked in the list is marked there but not shown: it is
the one Copy to Current Presentation puts the slides into, and the one leaving Bible
mode goes to. A presentation's own buttons over the slides (Destination, Slide Show,
Arrangements) are not there for Bible mode's slides.

Nearly every translation is someone's copyright, and ProPresenter's are licensed
through Renewed Vision, which is nothing to do with this app. Three are in the public
domain and come with it; others are had the two ways FreeShow has them:

- **Included**, first in the Bible menu: the **King James Version** (1769), **Young's
  Literal Translation** (1898) and the **Bible in Basic English** (1949, 1964), taken
  from eBible.org, which has them as public domain: the KJV except in the United
  Kingdom, where the Crown's letters patent apply, and the BBE in the United States
  (it says no more of the BBE than that). The KJV is the one chosen until another is.
  [bibles/README.md](../../bibles/README.md) says where they are from and what was done to
  them; they cannot be taken out.

- **An XML Bible, imported.** **Import XML Bible…** is at the foot of the Bible menu,
  and in the settings under Bibles. Four kinds of file are read: Zefania, OSIS, Beblia
  and OpenSong, which between them are how most Bibles are passed about as files. The
  file is copied into the app's own folder
  (`~/.local/share/SimplePresenter/SimplePresenter/Bibles`), so that it stays whatever
  becomes of the one imported; the settings list what has been imported and take one
  out again. Whether a file may be used is for whoever imports it. The words of Jesus
  are red where the file marks them (Zefania's red, OSIS's `<q who="Jesus">`).
- **api.bible**, the American Bible Society's service, asked over the internet. It
  gives a key to whoever registers an app there, free for use that is not commercial,
  and gives that key the Bibles its publishers allow; their terms are that a passage is
  fetched when it is wanted and not stored, that the copyright is shown with it, and
  that what was shown is reported back (their "FUMS", which is told the passage, a
  random number for the computer and one for the run, and nothing else of who looked).
  So a passage is asked for every time it is looked up, nothing of it is kept but the
  slides made of it, its copyright is said under the bar, and the report goes as it
  comes. The app comes with no key: whoever wants api.bible's Bibles registers there
  for a key of their own, asks there for the Bibles they want, and types the key in the
  settings, under Bibles, where it is kept with the app's settings. (One can be built
  into a copy of the app: [Building from source](../installing.md#building-from-source).)

The bar has three lines. The first finds the passage, and has the options:

- **The Bible**, and **Book**: ProPresenter's Book menu, the Old Testament and the New
  (and any other books a file has), each book with its chapters.
- **The passage**, typed as it is usually written and found with Enter:
  `John 3:16`, `jn 3 16-18`, `Psalm 23`, `Gen 1:1-2:3`, `Ruth 1-2`,
  `1 Cor 13:4-7`, `John 3:16, 18, 20-21`, `Jude 3`. A book's name can be cut short
  while it is still only one book's, and a file in another language is found by its
  own names for the books as well.
- **The cog** opens a panel of how the slides are made: ProPresenter's four Bible Slide
  Options, **Show Verse Numbers** (small and raised), **Break on New Verse** (each
  verse begins a slide), **Display Translation** (its short name after the reference)
  and **Preserve Font Color** (the red); its references, **Verse References** (each
  slide its own verses), **Passage Each** (the whole passage on every slide),
  **Passage Last** (on the last only) and **No Reference**; and **Flow**, how the words
  are shared out across the slides (below).

The second line is the theme:

- **Theme**, and its slide: each slide is a copy of the theme slide, with everything
  on it. **Verse** is the text box the words go in (one called Verse, Scripture,
  Bible, Text or Lyrics is taken to begin with), and **Reference** the one the
  passage's reference goes in, or **With Verse**, on a line of its own after the words
  in the same box (or run on after them, in a Single Line box). A box called Copyright gets the translation's copyright; the
  theme's other text boxes are left empty. **Plain (no theme)** is white words on
  black. The boxes chosen are remembered for each theme slide, by the box itself
  rather than its name, so a box with no name, or with another box's name, can be
  chosen, and going to another theme and back keeps the choice.
- The box the words go in is used as the theme has it, but that it never scales its
  font up or down: the words are laid out at the theme's size, and every slide's are in
  it. A **Single Line** or **Replace Line Returns** box keeps that: a theme can run
  verses on as one line (a lower third, say), and a reference With Verse then runs on
  after the words instead of starting a line of its own. A box that puts each word or
  each letter on a line of its own does not, here.

The third line says what was found, on how many slides, and its copyright. The slides
are in the grid under it, a presentation of their own, and are clicked, stepped through
and taken live as any are, ringed when live whatever is being browsed at the left.
Each is labelled with its own verses, and they are one group, named for the passage.
Until they are saved they are kept in a folder that goes when the app is closed. Which
theme, which boxes and the options are remembered from one start to the next.

Under the slides are two more things of Bible mode's:

- **← Verse** and **Verse →**, midway between the transition and the thumbnails' size:
  the passage grows by the verse before it or the one after, on into the chapter
  before or after, but not out of its book (the bar says when it is at the book's
  start or end, and the passage stays as it was). Of api.bible only the verse added is
  asked for. If a slide of the passage is live, the slides up to it (Verse →) or from
  it (← Verse) stay exactly as they were, and it stays the live slide, ringed as such;
  only the rest are flowed again, with the verse added among them. With nothing live,
  the whole passage is flowed again, as it would be had it been typed.
- **Save**, at the left of the transition: **Save as Presentation**, into a library,
  under the passage's name; **Save to Playlist**, the same, with a row for it at the
  foot of a playlist; and **Copy to Current Presentation**, which puts the slides into
  the presentation that was being viewed or has been clicked since, after the slide
  that is live in it, or at its end.

## How the words are flowed

ProPresenter fills each slide with as much as fits in the box and goes on to the next:
that is **Fill Each Slide** here. It leaves two things that look wrong on a screen: the
last slide is often a few words after full ones, and a slide ends wherever the box is
full, in the middle of a phrase, when a comma or a full stop a word away is where anyone
reading it aloud would have paused. **Even, at Natural Pauses** is this app's own, and
what it starts with. It makes as many slides as Fill would, never more, and chooses
where they break so that:

- they break where the words pause: best at the end of a sentence that is also the end
  of a verse, then the end of a sentence, the end of a verse, a semicolon, colon or
  dash, a comma, and only last between two words with nothing between them;
- a break inside a sentence does not leave a word or two of a phrase on their own
  ("… and" at the foot of a slide, "him." at the top of the next);
- the slides hold about as much as each other, so that a slide's worth of words too
  many goes two or three ways rather than full, full and a scrap.

Those pull against each other, and all of them are weighed together for every place
the passage could break: a comma near the middle can win over a full stop at the far
end, and a phrase is broken between words only where it is longer than a slide or where
keeping it whole would leave the slides far from even. Nothing goes on a slide that
does not fit its box, which is measured as it will be drawn. With Break on New Verse,
each verse is flowed the same way by itself.

Where it cannot find the pauses (the maintainer asked where this way of doing it
fails):

- A phrase longer than a slide with no comma in it (a list of names, a translation
  sparing with commas) has to be broken between words; it is broken at the place
  nearest even that leaves no word or two on their own.
- A word wider than the box overflows it, whatever is done.
- A full stop that ends no sentence ("St.", "v. 3", initials) is taken for one.
- A quotation is not followed: a break inside one is weighed as any other, and the
  opening quote mark is not carried over to the next slide.
- Poetry breaks at its lines, which an XML file seldom keeps, so the Psalms in most
  modern translations are flowed as prose.
- The marks differ between translations, so the same passage breaks in different
  places in each.
- Languages written without spaces between words (Chinese, Japanese, Thai) have no
  words to break between, and nothing is tried for them.
- Even leaves each slide a little emptier than Fill: a passage that just overflows one
  slide is two about half full. That is the point of it, but some will prefer Fill.

The weights are a first setting, tried on made-up passages in the unit tests
(`tests/unit/tst_bibleflow.cpp`) and not yet on real Bible text: they are what to change
if a passage breaks where it should not, and they are said in `src/bibleflow.h`.

## What is a guess, or not done

ProPresenter's help says what its Bible view has, and not how it is laid out: the bar
and its three lines are a guess (rearranged as the maintainer asked: the options
behind the cog; Save, ← Verse and Verse → under the slides; no Find), and so are the
names Even and Fill and where they sit, the red (a soft red, `#ff5a4f`), the slides'
labels and their group's name, Passage Last to begin with, Verse References being
there without Break on New Verse, the text boxes picked by their names to begin with,
a box called Copyright, where Copy to Current Presentation puts the slides, and which
Bible is chosen until one is (the first imported, else api.bible's NIV, NKJV or KJV,
else its first in English). As the maintainer asked or agreed: ← Verse and Verse →
growing the passage and stopping at its book; the slides round the live one kept;
Single Line kept and font scaling not; a presentation clicked in Bible mode not shown
but gone to after; and no presentation buttons over Bible mode's slides.

Not here yet, of ProPresenter's: searching the Bible by a word, and a second
translation beside the first. The ESV is not on api.bible (Crossway has a service of
its own, which this does not ask). No other online source is asked.
