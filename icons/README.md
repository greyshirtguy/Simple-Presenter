# The pictures in this folder

These are the pictures ProPresenter has for things this app has too: a look, the
screens, the stage, a timer, a prop, a macro, a message, clearing each layer, and
playing and pausing. The app shows them in the toolbar, on the buttons that clear a
layer, on the tabs of the show controls, and wherever an action on a slide or in a
macro is pictured. Someone who knows ProPresenter knows them at a glance, which is the
whole reason for using them instead of drawings of this app's own.

Simple Presenter is not ProPresenter and has nothing to do with Renewed Vision. Where
ProPresenter's pack has no picture for a thing (Search, Themes, Show, Edit, Simple View,
Media and Settings in the toolbar, among others), the app draws its own.

## Where they are from

The icon pack published with the Bitfocus Companion module for ProPresenter's API:

    https://github.com/bitfocus/companion-module-renewedvision-propresenter-api
    IconPack.zip, in the root of that repository

That repository is under the MIT licence, which is given whole at the foot of this
page, as it asks. The pack has forty pictures; the eighteen here are the ones the app
uses.

## How these files were made

A picture in the pack is 72 pixels square, with a drawing about 26 pixels high in the
middle of it, white and three quarters solid. `make.py`, beside this, turns each into
what is here: the drawing alone, four times the size, with clean edges, as a shape to
be given a colour (the one picture with colours of its own, the green screen, keeps
them). Its opening comment says how and why. To have another of the pack's pictures,
add its name to the list in `make.py` and run it on the unpacked pack:

    python3 icons/make.py <the unpacked IconPack folder>

then add the new file to the list in `CMakeLists.txt`, and ask for it by name with
`ProIcon` (`qml/ProIcon.qml`; `src/iconprovider.h` says how a picture gets from here to
the screen).

## The licence of the pack's repository

    MIT License

    Copyright (c) 2022 Bitfocus AS - Open Source

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
