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
OPEN [freeze-top]
```

Inkscape responds on stdout with `OPEN <id>` (see below).

The optional `freeze-top` parameter prevents structural and attribute
changes to the root `<svg>` element's direct children. When active:
- No elements can be inserted at the top level
- No top-level elements can be reordered
- No top-level elements can be deleted
- No attributes on top-level content elements can be modified

Internal elements (`sodipodi:namedview`, `svg:defs`, `svg:metadata`,
`svg:title`, `svg:desc`) are exempt from attribute freezing so that
Inkscape internals continue to function normally. The freeze persists
across `LOAD` commands on the same window.

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

**CLIP** — Register (or update) a clip in the Clip Panel.

```
CLIP <clip-id> content-length:<N>
<clip-name>
<N bytes of SVG data>
```

- `<clip-id>` is an opaque string chosen by the controller. If a clip with
  the same ID already exists, it is replaced.
- `<clip-name>` is the display name shown in the Clip Panel.
- The SVG data follows immediately after the name line's newline.

Clips appear in the Clip Panel dialog (only visible in pipe mode). Users
can drag clips from the panel onto the canvas to insert them.

**UCLIP** — Remove a clip from the Clip Panel.

```
UCLIP <clip-id>
```

If the clip ID does not exist, the command is silently ignored.

**DIRTY** — Mark a window's document as modified (shows `*` in title bar).

```
DIRTY <window-id>
```

**UNDIRTY** — Mark a window's document as clean (removes `*` from title bar).

```
UNDIRTY <window-id>
```

These commands let the controller manage the save-state indicator. By
default, pipe-mode documents are always clean (no asterisk). Use `DIRTY`
after receiving a `SAVE` to indicate unsaved changes, and `UNDIRTY` after
persisting to clear the indicator.

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
during intermediate states like mid-drag).
"Save As" is disabled (the user is prompted to use "Save a Copy" instead,
which saves to disk without affecting the pipe).

**REQUESTSAVE** — The user pressed Ctrl+S or used File → Save.

```
REQUESTSAVE <window-id>
```

The controller should persist the latest `SAVE` content and then send
`UNDIRTY <window-id>` to clear the title bar indicator. File → New is
disabled in pipe mode.

**NCLIP** — The user created a clip via Object → Create Clip. The selected
objects are grouped and the group's SVG ID is reported.

```
NCLIP <element-id>
```

The `<element-id>` is the `id` attribute of the newly created `<svg:g>`
element in the document. This is emitted after the group is created and
the undo step is recorded (a `SAVE` will follow). Only available in pipe
mode.

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

### `--delegate-undo-stack`

When used together with `--pipe-mode`, this option delegates undo/redo to
the controlling process:

```
inkscape --pipe-mode --delegate-undo-stack
```

Instead of maintaining an internal undo stack, Inkscape sends `UNDO` and
`REDO` messages to stdout when the user presses Ctrl+Z / Ctrl+Y (or uses
the Edit menu). The controlling process is responsible for tracking history
and responding with the appropriate `LOAD` command.

**stdout ← Inkscape (additional messages):**

```
UNDO <window-id>
REDO <window-id>
```

Behavior changes with `--delegate-undo-stack`:
- Undo/Redo buttons and menu items are always enabled
- Ctrl+Z / Ctrl+Y emit protocol messages instead of modifying the document
- The Undo History dialog is hidden
- `SAVE` messages are no longer emitted for undo/redo operations (since
  the internal undo stack is not used)

The controlling process should maintain its own history of `SAVE` snapshots
and `LOAD` the appropriate version when it receives `UNDO` or `REDO`.
