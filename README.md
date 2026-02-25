Inkscape. Draw Freely.
======================

[https://inkscape.org/](https://inkscape.org/)

Inkscape is a Free and open source vector graphics editor. It offers a rich set
of features and is widely used for both artistic and technical illustrations
such as cartoons, clip art, logos, typography, diagramming and flowcharting.
It uses vector graphics to allow for sharp printouts and renderings at
unlimited resolution and is not bound to a fixed number of pixels like raster
graphics. Inkscape uses the standardized SVG file format as its main format,
which is supported by many other applications including web browsers.

SVG Features include basic shapes, paths, text, markers, clones,
alpha blending, transforms, gradients, and grouping.
In addition, Inkscape supports Creative Commons meta-data, node-editing,
layers, complex path operations, text-on-path, and SVG XML editing.
It also imports several formats like EPS, Postscript,
JPEG, PNG, BMP, and TIFF and exports PNG as well as multiple vector-based
formats.

Inkscape's main motivations are to provide the Open Source community
with a fully W3C compliant XML, SVG, and CSS2 drawing tool emphasizing a
lightweight core with powerful features added as extensions, and the
establishment of a friendly, open, community-oriented development
processes.

[![build status](https://gitlab.com/inkscape/inkscape/badges/master/pipeline.svg)](https://gitlab.com/inkscape/inkscape/-/commits/master)
[![Build Status](https://ci.appveyor.com/api/projects/status/gitlab/inkscape/inkscape?branch=master&svg=true)](https://ci.appveyor.com/project/inkscape/inkscape)

For installation, please see: [INSTALL.md](INSTALL.md)

----

Build command to avoid the poppler madness (MSYS2): cmake -G Ninja -DCMAKE_INSTALL_PREFIX="${PWD}/install_dir" -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=OFF -DWITH_INTERNAL_2GEOM=ON -DWITH_INTERNAL_CAIRO=OFF -DENABLE_POPPLER=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..

----

## Pipe Mode

`--pipe-mode` turns Inkscape into a pipe-controlled GUI editor. An external
process manages windows and loads SVG documents via stdin, and receives
every document change and window-close notifications via stdout. This
enables programmatic integration with other tools while keeping the full
Inkscape GUI.

```
inkscape --pipe-mode
```

Inkscape starts with no windows and waits for commands on stdin.

### Protocol

All commands and responses are newline-delimited text. Commands that carry
SVG data use a `content-length` header to frame the binary payload.

#### stdin → Inkscape

**OPEN** — Open a new empty window.

```
OPEN
```

Inkscape responds on stdout with `OPEN <id>` (see below).

**LOAD** — Load an SVG document into an existing window.

```
LOAD <window-id> content-length:<N>
<filename>
<N bytes of SVG data>
```

- `<window-id>` — the integer ID returned by a previous `OPEN` response.
- `content-length` — byte length of the SVG data only (not including the
  filename line).
- `<filename>` — display name shown in the title bar (e.g. `drawing.svg`).
- The SVG data follows immediately after the filename line's newline, with
  no additional separator. Exactly `<N>` bytes are read.

Loading into a window that already has content replaces the document,
preserving zoom and scroll position.

**CLOSE** — Close a window.

```
CLOSE <window-id>
```

The window is closed without a save-changes prompt.

#### stdout ← Inkscape

**OPEN** — A new window was created (response to an `OPEN` command).

```
OPEN <window-id>
```

The integer `<window-id>` identifies this window in all subsequent commands.

**SAVE** — The document changed (emitted after every undo-committed
operation: edits, undo, redo).

```
SAVE <window-id> content-length:<N>
<filename>
<N bytes of SVG data>
```

The format mirrors `LOAD`. The filename is whatever was last set by `LOAD`
for that window. A `SAVE` is emitted after each logical user action (not
during intermediate states like mid-drag). Documents in pipe mode have no
dirty/clean state — Ctrl+S is a no-op and there are no save prompts.
"Save As" is disabled (the user is prompted to use "Save a Copy" instead,
which saves to disk without affecting the pipe).

**CLOSE** — The user closed a window (via the window's close button or
File → Close).

```
CLOSE <window-id>
```

This is only emitted when the *user* closes the window, not in response to
a `CLOSE` command from stdin.

### Lifecycle

- Inkscape stays alive as long as stdin is open or any pipe-mode windows
  remain, even after stdin reaches EOF.
- Closing stdin (EOF) does not close existing windows — the user can
  continue editing. Changes are still streamed to stdout until the
  pipe is closed.
- Window IDs are monotonically increasing integers starting at 1 and are
  never reused within a session.

### Example session

```
→  OPEN
←  OPEN 1
→  LOAD 1 content-length:132
→  drawing.svg
→  <svg xmlns="http://www.w3.org/2000/svg" width="200" height="200">
→    <rect width="100" height="100" fill="red"/>
→  </svg>
                          (user moves the rectangle)
←  SAVE 1 content-length:198
←  drawing.svg
←  <svg xmlns="http://www.w3.org/2000/svg" ...> ... </svg>
                          (user changes fill color)
←  SAVE 1 content-length:205
←  drawing.svg
←  <svg xmlns="http://www.w3.org/2000/svg" ...> ... </svg>
→  OPEN
←  OPEN 2
→  CLOSE 1
                          (user closes window 2 manually)
←  CLOSE 2
```

(`→` = stdin to Inkscape, `←` = stdout from Inkscape)
