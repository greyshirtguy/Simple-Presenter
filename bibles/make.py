#!/usr/bin/env python3
"""Makes the Bibles that come with the app (this folder's kjv.xml, ylt.xml and bbe.xml, and included.json) from
eBible.org's files of them.

    python3 bibles/make.py <a folder with eng-kjv2006_vpl.zip, engylt_vpl.zip and engBBE_vpl.zip>

The zips are eBible.org's "BibleWorks import (VPL)" downloads (https://ebible.org/Scriptures/<id>_vpl.zip): the
words of each verse and nothing else, no notes or headings. This takes the XML form of them (<id>_vpl.xml) and writes
each Bible as a Zefania XML file, the kind the app imports (src/biblexml.h), with the books numbered and named as the
app names them (src/bible.cpp). Two things are changed on the way, both said in README.md here: the KJV's paragraph
marks (the pilcrow, which the 1769 text has before many verses) are taken out, and a verse with no words (the BBE
leaves sixteen out, as some editions do) is left out. Spaces are tidied: one between words, none at either end.
Nothing else of the words is touched.

Run it with python3 -I: the zips are downloaded files, and this reads them with the standard library alone.
"""

import json
import os
import re
import sys
import zipfile
from xml.etree import ElementTree
from xml.sax.saxutils import escape, quoteattr

HERE = os.path.dirname(os.path.abspath(__file__))

# The 66 books in their order, by the USFM code eBible.org's files use, named as the app names them
BOOKS = [
    ("GEN", "Genesis"), ("EXO", "Exodus"), ("LEV", "Leviticus"), ("NUM", "Numbers"), ("DEU", "Deuteronomy"),
    ("JOS", "Joshua"), ("JDG", "Judges"), ("RUT", "Ruth"), ("1SA", "1 Samuel"), ("2SA", "2 Samuel"),
    ("1KI", "1 Kings"), ("2KI", "2 Kings"), ("1CH", "1 Chronicles"), ("2CH", "2 Chronicles"), ("EZR", "Ezra"),
    ("NEH", "Nehemiah"), ("EST", "Esther"), ("JOB", "Job"), ("PSA", "Psalms"), ("PRO", "Proverbs"),
    ("ECC", "Ecclesiastes"), ("SNG", "Song of Songs"), ("ISA", "Isaiah"), ("JER", "Jeremiah"),
    ("LAM", "Lamentations"), ("EZK", "Ezekiel"), ("DAN", "Daniel"), ("HOS", "Hosea"), ("JOL", "Joel"),
    ("AMO", "Amos"), ("OBA", "Obadiah"), ("JON", "Jonah"), ("MIC", "Micah"), ("NAM", "Nahum"), ("HAB", "Habakkuk"),
    ("ZEP", "Zephaniah"), ("HAG", "Haggai"), ("ZEC", "Zechariah"), ("MAL", "Malachi"), ("MAT", "Matthew"),
    ("MRK", "Mark"), ("LUK", "Luke"), ("JHN", "John"), ("ACT", "Acts"), ("ROM", "Romans"),
    ("1CO", "1 Corinthians"), ("2CO", "2 Corinthians"), ("GAL", "Galatians"), ("EPH", "Ephesians"),
    ("PHP", "Philippians"), ("COL", "Colossians"), ("1TH", "1 Thessalonians"), ("2TH", "2 Thessalonians"),
    ("1TI", "1 Timothy"), ("2TI", "2 Timothy"), ("TIT", "Titus"), ("PHM", "Philemon"), ("HEB", "Hebrews"),
    ("JAS", "James"), ("1PE", "1 Peter"), ("2PE", "2 Peter"), ("1JN", "1 John"), ("2JN", "2 John"),
    ("3JN", "3 John"), ("JUD", "Jude"), ("REV", "Revelation"),
]

# What is made, in the order the app offers them: the eBible.org id, the file, its name and short name, and what is
# said of its copyright (eBible.org's own words, shortened; see README.md)
BIBLES = [
    ("eng-kjv2006", "kjv.xml", "King James Version", "KJV",
     "Public domain, except in the United Kingdom, where the Crown's letters patent apply."),
    ("engylt", "ylt.xml", "Young's Literal Translation", "YLT", "Public domain."),
    ("engBBE", "bbe.xml", "Bible in Basic English", "BBE", "Public domain in the United States."),
]


def verses(archive, ebible_id):
    """The verses of an eBible.org VPL zip, in order: (book code, chapter, verse, words)."""
    with zipfile.ZipFile(archive) as z:
        root = ElementTree.fromstring(z.read(ebible_id + "_vpl.xml"))
    for v in root.iter("v"):
        words = "".join(v.itertext()).replace("\u00b6", " ")
        words = re.sub(r"\s+", " ", words).strip()
        yield v.get("b"), int(v.get("c")), int(v.get("v")), words


def zefania(ebible_id, name, abbreviation, rights, rows):
    """The Bible as Zefania XML: one line to a verse."""
    numbers = {code: (i + 1, book) for i, (code, book) in enumerate(BOOKS)}
    out = ['<?xml version="1.0" encoding="utf-8"?>',
           "<!-- Made by bibles/make.py from eBible.org's %s_vpl.zip (https://ebible.org/Scriptures/details.php?id=%s) -->"
           % (ebible_id, ebible_id),
           '<XMLBIBLE biblename=%s type="x-bible" status="v" version="2.0.1.18" revision="0">' % quoteattr(name),
           "<INFORMATION><title>%s</title><identifier>%s</identifier><language>ENG</language><rights>%s</rights>"
           "<source>https://ebible.org/Scriptures/details.php?id=%s</source></INFORMATION>"
           % (escape(name), escape(abbreviation), escape(rights), ebible_id)]
    book = chapter = None
    counts = {"books": 0, "chapters": 0, "verses": 0, "left out": 0}
    for code, c, v, words in rows:
        if code not in numbers:
            raise SystemExit("%s: a book that is not one of the 66: %s" % (ebible_id, code))
        if not words:
            counts["left out"] += 1
            continue
        if code != book:
            if book is not None:
                out.append("</CHAPTER></BIBLEBOOK>")
            number, title = numbers[code]
            out.append('<BIBLEBOOK bnumber="%d" bname=%s>' % (number, quoteattr(title)))
            book, chapter = code, None
            counts["books"] += 1
        if c != chapter:
            if chapter is not None:
                out.append("</CHAPTER>")
            out.append('<CHAPTER cnumber="%d">' % c)
            chapter = c
            counts["chapters"] += 1
        out.append('<VERS vnumber="%d">%s</VERS>' % (v, escape(words)))
        counts["verses"] += 1
    out.append("</CHAPTER></BIBLEBOOK>")
    out.append("</XMLBIBLE>")
    return "\n".join(out) + "\n", counts


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    folder = sys.argv[1]
    included = []
    for ebible_id, file, name, abbreviation, rights in BIBLES:
        text, counts = zefania(ebible_id, name, abbreviation, rights,
                               verses(os.path.join(folder, ebible_id + "_vpl.zip"), ebible_id))
        with open(os.path.join(HERE, file), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        included.append({"file": file, "name": name, "abbreviation": abbreviation, "language": "English",
                         "source": "https://ebible.org/Scriptures/details.php?id=" + ebible_id})
        print("%s: %d books, %d chapters, %d verses (%d with no words left out)"
              % (file, counts["books"], counts["chapters"], counts["verses"], counts["left out"]))
    with open(os.path.join(HERE, "included.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(included, f, indent=2, ensure_ascii=False)
        f.write("\n")


if __name__ == "__main__":
    main()
