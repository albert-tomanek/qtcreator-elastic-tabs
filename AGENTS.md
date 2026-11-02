# AGENTS.md

Qt Creator plugin implementing [elastic tabstops](https://nick-gravgaard.com/elastic-tabstops/).
User-facing docs are in `README.md`; this file is for working on the code.

## Layout

- `src/elastictabstops.{h,cpp}` — the engine. **No Qt Creator dependencies**:
  `computeTabStops()` (pure algorithm), `relayoutBlock()`, and `Handler`, which
  attaches to any `QPlainTextEdit`. Keep it that way so it stays testable.
- `src/elastictabstopsplugin.cpp` — Qt Creator glue: settings (`AspectContainer`),
  options page, Edit ▸ Advanced toggle, attaching a `Handler` to every
  `TextEditorWidget` via `EditorManager::editorOpened`.
- `src/ElasticTabstops.json.in` — plugin metadata (configured by CMake).
- `tests/tst_elastictabstops.cpp` — QtTest, runs offscreen on a plain `QPlainTextEdit`.
- `3rdparty/qtcreator-src/` — sparse checkout of Qt Creator **v16.0.1**, headers
  only. Not ours; don't edit. Git-ignored.

## Build & test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j"$(nproc)"
QTEST_DISABLE_STACK_DUMP=1 QT_QPA_PLATFORM=offscreen prlimit --as=2147483648 ./build/tst_elastictabstops
```

**Always cap memory and disable QtTest's stack dump.** On a crash QtTest
attaches gdb automatically, and that nearly took down the user's machine.
Wrap anything heavy (tests, gdb, headless Qt Creator) in `prlimit --as=…`.

Build is against the Debian-installed Qt Creator 16.0.1 (`/usr/lib/x86_64-linux-gnu/qtcreator`)
using headers from `3rdparty/`. There is no plugin SDK package on Debian.
The header checkout must match the installed version exactly; on a Qt Creator
upgrade, re-clone at the new tag and set `QTC_VERSION`.

## Verifying inside Qt Creator (headless)

```bash
QT_LOGGING_RULES="qtc.elastictabstops.debug=true" QT_QPA_PLATFORM=offscreen \
  timeout -s INT 20 prlimit --as=4294967296 \
  qtcreator -settingspath <scratch dir> -pluginpath "$PWD/build" sample.c
```

Expect `Attached to …EditorWidget` and `Block N tab stops …` lines, which
should stop once highlighting settles (no endless re-layout). Use a throwaway
`-settingspath`, never the user's real settings.

If the plugin is listed (`qtcreator -pluginpath build -version`) but never
initializes, Qt Creator rejected it and reported the reason only in a GUI dialog
you can't see offscreen. Read the error with a small program that calls
`ExtensionSystem::PluginManager::setPluginPaths()` and prints
`PluginSpec::errorString()` for each plugin.

## Gotchas learned the hard way

- Qt Creator 16 requires `"VendorId"` in the plugin JSON; without it the plugin
  silently doesn't load.
- `QPlainTextDocumentLayout` uses the document-wide `QTextOption` for every
  block, so per-line tabs are applied by re-laying out the block's `QTextLayout`
  in a viewport `Paint` event filter. Qt only re-lays a block when it changes
  (edits, `setFormats` from the highlighter), so the custom layout persists.
  Preserve existing line positions/widths in `relayoutBlock()`: Qt Creator's
  line spacing and other features depend on them.
- `Handler` must **not** be a child of the editor, and must not touch the editor
  once it's being destroyed. `QWidget::~QWidget` emits `destroyed()` (and then
  deletes children) *before* `QPointer`s to it are cleared, and Qt Creator's
  `TextEditorWidget` has already deleted its document by then. The handler
  deletes itself on `destroyed()` after nulling `m_editor`, so it skips the
  restore. `tst_ElasticTabstops::editorDestroyedAfterItsDocument` covers this.
- Changing one line can move its neighbours' tab stops outside the paint
  region, so `apply()` triggers a full viewport repaint when it changed anything.
- Don't use `QPlainTextEdit::firstVisibleBlock()` (protected); the engine uses
  `cursorForPosition()` for the visible range.

## Conventions

- Qt Creator style: 4-space indent, `m_` members, `Utils::` aspects for settings.
- UI strings are plain literals (no translation setup).
- Not a git repo yet; ask before initializing one or committing.
