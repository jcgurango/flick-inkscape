// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Pipe mode: window-based pipe protocol over stdin/stdout.
 *
 * Protocol (stdin):
 *   OPEN                              — open a new window, responds with OPEN <id> on stdout
 *   LOAD <id> content-length:<N>\n<filename>\n<N bytes SVG>  — load SVG into window
 *   CLOSE <id>                        — close a window
 *
 * Protocol (stdout):
 *   OPEN <id>                         — response to OPEN
 *   SAVE <id> content-length:<N>\n<filename>\n<N bytes SVG>  — user saved (Ctrl+S)
 *   CLOSE <id>                        — user closed window
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pipe-mode.h"

#include <iostream>

#include "desktop.h"
#include "document.h"
#include "inkscape-application.h"
#include "inkscape-window.h"
#include "xml/repr.h"

PipeMode *PipeMode::_instance = nullptr;

PipeMode::PipeMode()
{
    _instance = this;
}

PipeMode::~PipeMode()
{
    stop();
    _instance = nullptr;
}

void PipeMode::start()
{
    // Create the Channel on the main thread — Glib::Dispatcher requires this
    auto [source, dest] = Inkscape::Async::Channel::create();
    _channel_dest = std::move(dest);

    // Hold the application so it doesn't quit with no windows
    auto app = InkscapeApplication::instance();
    if (app && app->gio_app()) {
        app->gio_app()->hold();
    }

    _reader_thread = std::thread(&PipeMode::reader_thread_func, this, std::move(source));
}

void PipeMode::stop()
{
    _channel_dest.close();

    if (_reader_thread.joinable()) {
        std::cin.setstate(std::ios::eofbit);
        _reader_thread.detach();
    }

    auto app = InkscapeApplication::instance();
    if (app && app->gio_app()) {
        app->gio_app()->release();
    }
}

// --- Reader thread (stdin) ---

void PipeMode::reader_thread_func(Inkscape::Async::Channel::Source source)
{
    enum class State { COMMAND, LOAD_FILENAME, LOAD_CONTENT };
    State state = State::COMMAND;
    int load_window_id = 0;
    size_t load_content_length = 0;
    std::string load_filename;

    std::string line;
    while (std::getline(std::cin, line)) {
        switch (state) {

        case State::COMMAND: {
            if (line == "OPEN") {
                source.run([this] { handle_open(); });

            } else if (line.compare(0, 5, "LOAD ") == 0) {
                // LOAD <id> content-length:<N>
                auto rest = line.substr(5);
                auto sp = rest.find(' ');
                if (sp == std::string::npos) {
                    std::cerr << "PipeMode: Malformed LOAD: " << line << std::endl;
                    break;
                }
                try {
                    load_window_id = std::stoi(rest.substr(0, sp));
                } catch (...) {
                    std::cerr << "PipeMode: Invalid window ID in: " << line << std::endl;
                    break;
                }
                const std::string cl_prefix = "content-length:";
                auto cl_part = rest.substr(sp + 1);
                if (cl_part.compare(0, cl_prefix.size(), cl_prefix) != 0) {
                    std::cerr << "PipeMode: Missing content-length in: " << line << std::endl;
                    break;
                }
                try {
                    load_content_length = std::stoul(cl_part.substr(cl_prefix.size()));
                } catch (...) {
                    std::cerr << "PipeMode: Invalid content-length in: " << line << std::endl;
                    break;
                }
                state = State::LOAD_FILENAME;

            } else if (line.compare(0, 6, "CLOSE ") == 0) {
                try {
                    int id = std::stoi(line.substr(6));
                    source.run([this, id] { handle_close(id); });
                } catch (...) {
                    std::cerr << "PipeMode: Invalid CLOSE ID: " << line << std::endl;
                }

            } else if (!line.empty()) {
                std::cerr << "PipeMode: Unknown command: " << line << std::endl;
            }
            break;
        }

        case State::LOAD_FILENAME: {
            load_filename = line;

            // Read exactly load_content_length bytes of SVG data
            std::string svg_data(load_content_length, '\0');
            if (load_content_length > 0) {
                std::cin.read(&svg_data[0], load_content_length);
                if (std::cin.gcount() != static_cast<std::streamsize>(load_content_length)) {
                    std::cerr << "PipeMode: Short read, expected " << load_content_length
                              << " bytes, got " << std::cin.gcount() << std::endl;
                    state = State::COMMAND;
                    break;
                }
            }

            int wid = load_window_id;
            std::string fname = std::move(load_filename);
            std::string data = std::move(svg_data);
            source.run([this, wid, fname = std::move(fname),
                        data = std::move(data)]() mutable {
                handle_load(wid, std::move(fname), std::move(data));
            });

            state = State::COMMAND;
            break;
        }

        default:
            break;
        }
    }

    source.close();
}

// --- Main-thread handlers ---

void PipeMode::handle_open()
{
    auto app = InkscapeApplication::instance();
    if (!app) return;

    SPDocument *doc = app->document_new();
    if (!doc) {
        std::cerr << "PipeMode: Failed to create new document" << std::endl;
        return;
    }

    InkscapeWindow *window = app->window_open(doc);
    if (!window) {
        std::cerr << "PipeMode: Failed to open window" << std::endl;
        return;
    }
    window->set_visible(true);

    int id = _next_id++;
    _id_to_window[id] = window;
    _window_to_id[window] = id;
    _doc_to_id[doc] = id;

    write_line("OPEN " + std::to_string(id));
}

void PipeMode::handle_load(int window_id, std::string filename, std::string svg_data)
{
    auto app = InkscapeApplication::instance();
    if (!app) return;

    auto it = _id_to_window.find(window_id);
    if (it == _id_to_window.end()) {
        std::cerr << "PipeMode: Unknown window ID " << window_id << std::endl;
        return;
    }
    InkscapeWindow *window = it->second;
    SPDocument *old_doc = window->get_document();

    SPDocument *new_doc = app->document_open(svg_data);
    if (!new_doc) {
        std::cerr << "PipeMode: Failed to parse SVG for window " << window_id << std::endl;
        return;
    }
    new_doc->setDocumentFilename(filename.c_str());
    new_doc->setVirgin(false);

    SPDesktop *desktop = window->get_desktop();
    double zoom = desktop->current_zoom();
    Geom::Point center = desktop->current_center();

    app->document_swap(window, new_doc);

    desktop->zoom_absolute(center, zoom, false);

    // Update mappings
    _doc_to_id.erase(old_doc);
    _doc_to_id[new_doc] = window_id;

    // Close old document if no other windows reference it
    if (app->document_window_count(old_doc) == 0) {
        app->document_close(old_doc);
    }
}

void PipeMode::handle_close(int window_id)
{
    auto it = _id_to_window.find(window_id);
    if (it == _id_to_window.end()) {
        std::cerr << "PipeMode: Unknown window ID " << window_id << std::endl;
        return;
    }
    InkscapeWindow *window = it->second;

    _closing_programmatically.insert(window_id);

    auto app = InkscapeApplication::instance();
    if (app) {
        app->destroy_window(window, false);
    }

    _closing_programmatically.erase(window_id);
}

// --- Called by inkscape-application on window destruction ---

void PipeMode::on_window_destroyed(InkscapeWindow *window)
{
    auto it = _window_to_id.find(window);
    if (it == _window_to_id.end()) return;

    int id = it->second;

    // Clean up mappings
    SPDocument *doc = window->get_document();
    if (doc) _doc_to_id.erase(doc);
    _id_to_window.erase(id);
    _window_to_id.erase(it);

    // Emit CLOSE only if the user closed it (not us via handle_close)
    if (_closing_programmatically.count(id) == 0) {
        write_line("CLOSE " + std::to_string(id));
    }
}

// --- Save interception (called from file.cpp) ---

void PipeMode::write_save(SPDocument *doc)
{
    auto it = _doc_to_id.find(doc);
    if (it == _doc_to_id.end()) return;

    int window_id = it->second;
    const char *fname = doc->getDocumentFilename();
    std::string filename = fname ? fname : "";
    Glib::ustring svg_content = sp_repr_save_buf(doc->getReprDoc());

    write_message("SAVE " + std::to_string(window_id) +
                      " content-length:" + std::to_string(svg_content.bytes()),
                  filename, svg_content);
}

bool PipeMode::is_pipe_document(SPDocument *doc) const
{
    return _doc_to_id.find(doc) != _doc_to_id.end();
}

// --- Stdout helpers ---

void PipeMode::write_line(const std::string &line)
{
    std::lock_guard<std::mutex> lock(_stdout_mutex);
    std::cout << line << "\n" << std::flush;
}

void PipeMode::write_message(const std::string &header, const std::string &filename,
                             const std::string &content)
{
    std::lock_guard<std::mutex> lock(_stdout_mutex);
    std::cout << header << "\n" << filename << "\n" << content << std::flush;
}
