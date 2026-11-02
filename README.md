# Elastic Tabstops for Qt Creator

A Qt Creator plugin implementing [elastic tabstops](https://nick-gravgaard.com/elastic-tabstops/):
a tab character ends a *cell*, and the cells in a column of consecutive lines
are all as wide as the widest one, so tab-separated text stays aligned no
matter how long its contents get.

```
int␉x;␉// a            int            x;  // a
unsigned long␉y;␉// bb  →  unsigned long  y;  // bb
```

Built for **Qt Creator 16.0.1** (Debian package `qtcreator` 16.0.1, Qt 6.8.2).

## Usage

- **Edit ▸ Advanced ▸ Elastic Tabstops** toggles it on/off for all editors.
- **Preferences ▸ Text Editor ▸ Elastic Tabstops**: cell padding, minimum
  cell width (defaults to your normal tab size, so indentation looks
  unchanged) and how far to look beyond the visible area for column blocks.

Only how text is *displayed* changes; the file still contains plain tabs.

## Building

Debian doesn't ship a Qt Creator plugin SDK, so the build uses headers from a
Qt Creator source checkout of the same version and links against the installed
libraries:

```bash
git clone --depth 1 --branch v16.0.1 --filter=blob:none --sparse \
    https://github.com/qt-creator/qt-creator.git 3rdparty/qtcreator-src
git -C 3rdparty/qtcreator-src sparse-checkout set src/libs src/plugins/coreplugin src/plugins/texteditor
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

For a different Qt Creator version or location, set `QTC_VERSION`,
`QTC_SOURCE_DIR` and `QTC_LIB_DIR`. The checkout must match the installed
version exactly (plugins are not ABI compatible across versions).

Install into your user plugin directory
(`~/.local/share/data/QtProject/qtcreator/plugins/16.0.1`):

```bash
cmake --install build
```

or try it without installing: `qtcreator -pluginpath "$PWD/build"`.

## Tests

The layout engine (`src/elastictabstops.*`) has no Qt Creator dependency and is
tested on a plain `QPlainTextEdit`:

```bash
QT_QPA_PLATFORM=offscreen ./build/tst_elastictabstops
```

Debug output from inside Qt Creator: `QT_LOGGING_RULES="qtc.elastictabstops.debug=true"`.

## How it works

`QPlainTextDocumentLayout` lays every line out with the document-wide
`QTextOption`, so there's no API for per-line tab stops. Right before the
editor viewport paints, the plugin computes the elastic tab stops for the
visible lines (following column blocks up and down past the visible area)
and re-lays out those lines' `QTextLayout`s with per-line tab positions,
keeping the line geometry Qt chose. Qt only lays a line out again after it
changes, so the result is used for painting, mouse hit testing and cursor
movement too. Cell widths are measured with the editor font and cached per
line.

## Limitations

- Cell widths are measured with the editor's base font. With a proportional
  font where bold or italic highlighting changes the glyph width, columns may
  be off by a little (padding usually absorbs it).
- With text wrapping enabled, a line that wraps differently under elastic tabs
  can end up with the wrong height until it is edited.
- The horizontal scroll range is computed by Qt from the fixed-tab layout, so
  very wide elastic lines may not be fully scrollable.
- Features that compute columns from the tab size (e.g. the column number in
  the status bar, the right-margin line) still assume fixed tabs.
