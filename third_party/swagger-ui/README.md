# Swagger UI

The library that draws the API's pages of documentation (`/v1/doc/index.html` and
`/sp/v1/doc/index.html`: see `src/httpapidoc.h`), which is what draws ProPresenter's own
page at that address. Bundling it was agreed with the maintainer on 2026-11-17.

What is here is from the official release, **swagger-ui-dist 5.34.1**
(https://github.com/swagger-api/swagger-ui, published to npm on 2026-10-30), as it
comes, not one byte changed:

| File | |
| --- | --- |
| `swagger-ui-bundle.js` | Swagger UI and everything it is made of, in one file |
| `swagger-ui-bundle.js.LICENSE.txt` | The licences of what is in that file, which it points to |
| `swagger-ui.css` | Its looks |
| `LICENSE`, `NOTICE` | The Apache License 2.0, which Swagger UI is under, and its notice |

The package was fetched from `https://registry.npmjs.org/swagger-ui-dist/-/swagger-ui-dist-5.34.1.tgz`
and checked against the hash npm publishes for it
(in the registry's entry for the version):

```
sha512-VSr8fXfI93obYaQicLJbLUKGRxDojj/SfRJDDLCioGTUBj4BK/Nj4eE9jCeCAnJxVLi7iS+08PbiZLyrQKBkJQ==
```

The files here are these, by `sha256sum`:

```
36eeb4d5640b9c036b1d0f854499af1cceebbdb260b790a03a18be5e8488fbf5  swagger-ui-bundle.js
c3cfe8248c4b0b89673600fb829924eafbac6de4bc4b2b06f5f5243483074d0a  swagger-ui.css
```

Left out of the package, as the app's page has no use for them: the standalone preset
(a bar across the top for typing another address into), the other forms of the bundle,
the source maps, the package's own starting page (the app's is `src/apidoc.html`), and
Swagger's pictures.

These three files, the two notices and the app's own page are carried in the app
(CMakeLists.txt, the `apidoc` resource) and served from it, so the page asks nothing of
the internet; `LICENSE` and `NOTICE` are served beside it, at `/v1/doc/LICENSE` and
`/v1/doc/NOTICE`.

To take a newer release: fetch the package of that version from npm, check it against
the hash npm gives for it, replace these files with the package's of the same names,
put the version, the date and the two sums right above, and look at both pages in a
browser.

Swagger is a trademark of SmartBear Software. The app has nothing to do with them.
