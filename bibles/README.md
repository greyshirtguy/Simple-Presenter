# The Bibles that come with the app

Three Bibles in the public domain, which Bible mode offers under **Included**, before
any that are imported and those of api.bible:

| File | Bible | From eBible.org | What eBible.org says of its copyright |
|---|---|---|---|
| `kjv.xml` | King James Version (1769, without the Apocrypha) | [`eng-kjv2006`](https://ebible.org/Scriptures/details.php?id=eng-kjv2006) | Public domain, but for the United Kingdom: there the Crown's letters patent, which have no end, mean that printing it or bringing printed copies in needs permission (Cambridge University Press, Oxford University Press and Collins have the right to print it) |
| `ylt.xml` | Young's Literal Translation (1898) | [`engylt`](https://ebible.org/Scriptures/details.php?id=engylt) | Public domain |
| `bbe.xml` | Bible in Basic English (1949, 1964) | [`engBBE`](https://ebible.org/Scriptures/details.php?id=engBBE) | Public domain in the United States: printed in 1965 by Cambridge University Press and published there without a copyright notice |

That last one says the United States, and no more. Its translator, S. H. Hooke, died in
1968, and in a country that counts copyright from a translator's death it may not be in
the public domain yet. Nothing here is legal advice: the copyright of each is what
eBible.org says of it, and whether to send them out with the app is the maintainer's to
decide.

`included.json` is the list the app reads: each file, its name and short name.

## Where they are from, and what was done to them

The words are eBible.org's "BibleWorks import (VPL)" downloads of each
(`https://ebible.org/Scriptures/<id>_vpl.zip`, fetched on 2026-10-15): the words of each
verse and nothing else, no notes, headings or paragraph breaks, in the usual numbering of
the 66 books (31,102 verses). `make.py`, beside this, writes each as a Zefania XML file,
which is what the app imports (`src/biblexml.h`), with the books numbered and named as the
app names them:

    python3 -I bibles/make.py <a folder with the three zips>

Two things are changed on the way, and nothing else of the words:

- The KJV's paragraph marks (¶, which the 1769 text has before many verses) are taken
  out: on a slide they are a mark before the words, not a word.
- A verse with no words is left out. The BBE has sixteen (Matthew 17:21 and others that
  some editions do not have).

Spaces are tidied: one between words, none at either end of a verse.

The Zefania files of these three that are passed about (the "SF_2009" ones of the Free
Bible Software Group) were looked at first and not taken: in the BBE, from Genesis 42 on,
verses are a place out with the next one's number written into them (about 1,370 of
them), and the YLT writes its quotation marks and apostrophes as backticks.

## How they get to the app

The build copies them beside the program (`build/bibles/`), and the package puts them in
`/usr/share/simplepresenter/bibles/` (`CMakeLists.txt`). The app looks in the first and
then the second (`src/bibles.h`), reads only the list at first, and reads a Bible when it
is first looked in, as it does an imported one. They are not copied anywhere else and
cannot be taken out in the settings. `tests/unit/tst_bible.cpp` reads each with the app's
own reader and counts its books, chapters and verses.

To add another of eBible.org's: give its id, file and names in `make.py`'s list, run it,
and add the file to `includedBibles` in `CMakeLists.txt` and to the unit test.
