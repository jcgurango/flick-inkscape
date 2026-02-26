import { spawn } from "child_process";
import { resolve } from "path";
import readline from "readline";

const INKSCAPE = resolve("build/install_dir/bin/inkscape.exe");

const svg1 = `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200">
  <rect x="10" y="10" width="180" height="180" fill="red" stroke="black" stroke-width="2"/>
  <text x="100" y="110" text-anchor="middle" font-size="20" fill="white">Hello Pipe</text>
</svg>`;

const svg2 = `<svg xmlns="http://www.w3.org/2000/svg" width="300" height="300">
  <circle cx="150" cy="150" r="120" fill="blue" stroke="black" stroke-width="2"/>
  <text x="150" y="160" text-anchor="middle" font-size="24" fill="white">Second Doc</text>
</svg>`;

const svg1_updated = `<svg xmlns="http://www.w3.org/2000/svg" width="200" height="200">
  <rect x="10" y="10" width="180" height="180" fill="green" stroke="black" stroke-width="2"/>
  <text x="100" y="110" text-anchor="middle" font-size="20" fill="white">Updated!</text>
</svg>`;

const clipStar = `<svg xmlns="http://www.w3.org/2000/svg" width="100" height="100">
  <polygon points="50,5 20,99 95,39 5,39 80,99" fill="gold" stroke="black" stroke-width="2"/>
</svg>`;

// Track open windows and change count
const windows = new Map(); // id -> { filename }
let saveCount = 0;

function sendOpen(proc) {
  console.log(`\n>>> OPEN`);
  proc.stdin.write("OPEN\n");
}

function sendLoad(proc, windowId, filename, svgData) {
  const buf = Buffer.from(svgData, "utf-8");
  const header = `LOAD ${windowId} content-length:${buf.length}\n${filename}\n`;
  console.log(`\n>>> LOAD window ${windowId} "${filename}" (${buf.length} bytes)`);
  proc.stdin.write(header);
  proc.stdin.write(buf);
}

function sendClose(proc, windowId) {
  console.log(`\n>>> CLOSE ${windowId}`);
  proc.stdin.write(`CLOSE ${windowId}\n`);
}

function sendClip(proc, clipId, clipName, svgData) {
  const buf = Buffer.from(svgData, "utf-8");
  const header = `CLIP ${clipId} content-length:${buf.length}\n${clipName}\n`;
  console.log(`\n>>> CLIP "${clipId}" name="${clipName}" (${buf.length} bytes)`);
  proc.stdin.write(header);
  proc.stdin.write(buf);
}

function sendUclip(proc, clipId) {
  console.log(`\n>>> UCLIP ${clipId}`);
  proc.stdin.write(`UCLIP ${clipId}\n`);
}

// Parse stdout protocol messages
function parseStdout(stream, handlers) {
  let buffer = "";
  let expecting = null; // { windowId, contentLength, filename }

  stream.on("data", (chunk) => {
    buffer += chunk.toString();

    while (true) {
      if (!expecting) {
        const lineEnd = buffer.indexOf("\n");
        if (lineEnd === -1) break;

        const line = buffer.slice(0, lineEnd);
        buffer = buffer.slice(lineEnd + 1);

        // OPEN <id>
        const openMatch = line.match(/^OPEN (\d+)$/);
        if (openMatch) {
          handlers.onOpen(parseInt(openMatch[1]));
          continue;
        }

        // CLOSE <id>
        const closeMatch = line.match(/^CLOSE (\d+)$/);
        if (closeMatch) {
          handlers.onClose(parseInt(closeMatch[1]));
          continue;
        }

        // UNDO <id>
        const undoMatch = line.match(/^UNDO (\d+)$/);
        if (undoMatch) {
          handlers.onUndo(parseInt(undoMatch[1]));
          continue;
        }

        // REDO <id>
        const redoMatch = line.match(/^REDO (\d+)$/);
        if (redoMatch) {
          handlers.onRedo(parseInt(redoMatch[1]));
          continue;
        }

        // NCLIP <element-id>
        const nclipMatch = line.match(/^NCLIP (.+)$/);
        if (nclipMatch) {
          handlers.onNclip(nclipMatch[1]);
          continue;
        }

        // SAVE <id> content-length:<N>
        const saveMatch = line.match(/^SAVE (\d+) content-length:(\d+)$/);
        if (saveMatch) {
          expecting = {
            windowId: parseInt(saveMatch[1]),
            contentLength: parseInt(saveMatch[2]),
            filename: null,
          };
          continue;
        }

        console.log(`[stdout] ${line}`);
        continue;
      }

      // Reading filename line
      if (expecting.filename === null) {
        const lineEnd = buffer.indexOf("\n");
        if (lineEnd === -1) break;
        expecting.filename = buffer.slice(0, lineEnd);
        buffer = buffer.slice(lineEnd + 1);
      }

      // Reading content
      const available = Buffer.byteLength(buffer, "utf-8");
      if (available < expecting.contentLength) break;

      const buf = Buffer.from(buffer, "utf-8");
      const content = buf.subarray(0, expecting.contentLength).toString("utf-8");
      buffer = buf.subarray(expecting.contentLength).toString("utf-8");

      handlers.onSave(expecting.windowId, expecting.filename, content);
      expecting = null;
    }
  });
}

// --- Main ---

const args = ["--pipe-mode", "--delegate-undo-stack"];
console.log(`Starting Inkscape: ${INKSCAPE} ${args.join(" ")}`);
const proc = spawn(INKSCAPE, args, {
  stdio: ["pipe", "pipe", "pipe"],
});

proc.stderr.on("data", (d) => {
  for (const line of d.toString().split("\n")) {
    if (line.trim()) console.log(`[stderr] ${line}`);
  }
});

proc.on("exit", (code) => {
  console.log(`\nInkscape exited with code ${code}`);
  process.exit(0);
});

parseStdout(proc.stdout, {
  onOpen(id) {
    windows.set(id, { filename: null });
    console.log(`\n<<< OPEN ${id}  (${windows.size} window(s) open)`);
  },
  onClose(id) {
    windows.delete(id);
    console.log(`\n<<< CLOSE ${id}  (${windows.size} window(s) open)`);
  },
  onSave(id, filename, content) {
    saveCount++;
    console.log(
      `\n<<< SAVE #${saveCount} window ${id} "${filename}" (${Buffer.byteLength(content)} bytes)`
    );
  },
  onNclip(elementId) {
    console.log(`\n<<< NCLIP element "${elementId}"`);
  },
  onUndo(id) {
    console.log(`\n<<< UNDO window ${id}`);
  },
  onRedo(id) {
    console.log(`\n<<< REDO window ${id}`);
  },
});

// Interactive menu
const rl = readline.createInterface({ input: process.stdin, output: process.stdout });

function showMenu() {
  const ids = [...windows.keys()];
  console.log(`
Open windows: ${ids.length ? ids.join(", ") : "(none)"}

Commands:
  open              OPEN a new window
  load <id>         LOAD red rect SVG into window <id> as "test.svg"
  load2 <id>        LOAD blue circle SVG into window <id> as "circle.svg"
  load3 <id>        LOAD green rect SVG into window <id> as "updated.svg"
  close <id>        CLOSE window <id>
  clip <id> <name>  Register a clip (gold star SVG)
  uclip <id>        Remove a clip
  eof               Close stdin (Inkscape keeps running)
  quit              Kill Inkscape and exit

  (SAVE messages stream automatically on every edit — just watch)
`);
  rl.question("> ", (answer) => {
    const parts = answer.trim().split(/\s+/);
    const cmd = parts[0];
    const id = parts[1];

    switch (cmd) {
      case "open":
        sendOpen(proc);
        break;
      case "load":
        if (!id) { console.log("Usage: load <window-id>"); break; }
        sendLoad(proc, parseInt(id), "test.svg", svg1);
        break;
      case "load2":
        if (!id) { console.log("Usage: load2 <window-id>"); break; }
        sendLoad(proc, parseInt(id), "circle.svg", svg2);
        break;
      case "load3":
        if (!id) { console.log("Usage: load3 <window-id>"); break; }
        sendLoad(proc, parseInt(id), "updated.svg", svg1_updated);
        break;
      case "close":
        if (!id) { console.log("Usage: close <window-id>"); break; }
        sendClose(proc, parseInt(id));
        break;
      case "clip": {
        if (!id) { console.log("Usage: clip <id> <name>"); break; }
        const clipName = parts.slice(2).join(" ") || id;
        sendClip(proc, id, clipName, clipStar);
        break;
      }
      case "uclip":
        if (!id) { console.log("Usage: uclip <id>"); break; }
        sendUclip(proc, id);
        break;
      case "eof":
        console.log(">>> Closing stdin (EOF)");
        proc.stdin.end();
        break;
      case "quit":
        console.log(">>> Killing Inkscape");
        proc.kill();
        rl.close();
        return;
      default:
        console.log("Unknown command");
    }
    showMenu();
  });
}

setTimeout(showMenu, 1000);
