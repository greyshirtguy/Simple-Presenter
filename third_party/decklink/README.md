# Blackmagic DeckLink SDK headers

`include/` is the `Linux/include` folder of Blackmagic Design's DeckLink SDK 16.0, as it
comes, every file: the headers the app is built against to send a screen out through a
DeckLink card (see `src/decklink.h`), and `DeckLinkAPIDispatch.cpp`, which finds
Blackmagic's own library on the computer when the app runs.

They are here under the licence at the top of each of them. Blackmagic's End User
License Agreement for the SDK says its clauses on use and sub-licensing do not apply to
the files of its `Linux/Include` folder. Nothing else of the SDK is here: not its
documentation, and not its samples, which that agreement keeps to the use of whoever
downloaded it.

The library that does the work, `libDeckLinkAPI.so`, is not here and not in the app's
package either. It comes with Blackmagic's Desktop Video software, which a computer with
a DeckLink card has installed for the card, and the app looks for it only when a screen
is set to go out through a card. The app asks it for the interfaces of Desktop Video
14.3 to 15.3.1, which these headers keep beside 16.0's own (the files with `_v15_3_1`
in their names), so it works with Desktop Video 14.3 or later, not only 16.0.

DeckLink and Desktop Video are Blackmagic Design's. This app has nothing to do with them.
