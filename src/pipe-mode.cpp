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
 *   SAVE <id> content-length:<N>\n<filename>\n<N bytes SVG>  — emitted on every document change
 *   CLOSE <id>                        — user closed window
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pipe-mode.h"

#include <iostream>

#include <glibmm/main.h>

#include "desktop.h"
#include "document.h"
#include "document-undo.h"
#include "inkscape-application.h"
#include "inkscape-window.h"
#include "actions/actions-undo-document.h"
#include "xml/node-observer.h"
#include "xml/repr.h"

// --- Undo observer that forwards all state changes to PipeMode ---

class PipeMode::PipeUndoObserver : public Inkscape::UndoStackObserver
{
public:
    PipeUndoObserver(PipeMode *pm, SPDocument *doc)
        : UndoStackObserver(), _pm(pm), _doc(doc) {}

    // Commits are handled by commit_signal (which fires after setModifiedSinceSave)
    void notifyUndoCommitEvent(Inkscape::Event *) override {}
    // Undo/redo fire after setModifiedSinceSave, so we can handle them here
    void notifyUndoEvent(Inkscape::Event *) override       { _pm->on_document_changed(_doc); }
    void notifyRedoEvent(Inkscape::Event *) override       { _pm->on_document_changed(_doc); }
    void notifyClearUndoEvent() override {}
    void notifyClearRedoEvent() override {}

private:
    PipeMode *_pm;
    SPDocument *_doc;
};

// --- Freeze-top observer: prevents structural changes to root's children ---
// Instead of reverting inside the callback (which crashes), we defer an undo
// to an idle handler so the current tree mutation finishes cleanly first.

class PipeMode::FreezeTopObserver : public Inkscape::XML::NodeObserver
{
public:
    FreezeTopObserver(SPDocument *doc) : _doc(doc) {}

    void notifyChildAdded(Inkscape::XML::Node &, Inkscape::XML::Node &,
                          Inkscape::XML::Node *) override { schedule_revert(); }

    void notifyChildRemoved(Inkscape::XML::Node &, Inkscape::XML::Node &,
                            Inkscape::XML::Node *) override { schedule_revert(); }

    void notifyChildOrderChanged(Inkscape::XML::Node &, Inkscape::XML::Node &,
                                 Inkscape::XML::Node *, Inkscape::XML::Node *) override { schedule_revert(); }

    void set_reverting(bool r) { _reverting = r; }

private:
    void schedule_revert()
    {
        if (_reverting || _pending) return;
        _pending = true;
        Glib::signal_idle().connect_once([this]() {
            _reverting = true;
            Inkscape::DocumentUndo::undo(_doc);
            _reverting = false;
            _pending = false;
        });
    }

    SPDocument *_doc;
    bool _reverting = false;
    bool _pending = false;
};

// ---

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

    // Disconnect all observers and signal connections before shutdown
    for (auto &[doc, obs] : _freeze_observers) {
        auto *root = doc->getReprRoot();
        if (root) root->removeObserver(*obs);
    }
    _freeze_observers.clear();
    _frozen_top_windows.clear();

    for (auto &[doc, conn] : _commit_connections) {
        conn.disconnect();
    }
    _commit_connections.clear();
    for (auto &[doc, obs] : _observers) {
        doc->removeUndoObserver(*obs);
    }
    _observers.clear();

    auto app = InkscapeApplication::instance();
    if (app && app->gio_app()) {
        app->gio_app()->release();
    }
}

// --- Reader thread (stdin) ---

void PipeMode::reader_thread_func(Inkscape::Async::Channel::Source source)
{
    enum class State { COMMAND, LOAD_FILENAME, CLIP_NAME };
    State state = State::COMMAND;
    int load_window_id = 0;
    size_t load_content_length = 0;
    std::string clip_id;
    size_t clip_content_length = 0;

    std::string line;
    while (std::getline(std::cin, line)) {
        switch (state) {

        case State::COMMAND: {
            if (line == "OPEN" || line == "OPEN freeze-top") {
                bool freeze = (line == "OPEN freeze-top");
                source.run([this, freeze] { handle_open(freeze); });

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

            } else if (line.compare(0, 5, "CLIP ") == 0) {
                // CLIP <id> content-length:<N>
                auto rest = line.substr(5);
                auto sp = rest.find(' ');
                if (sp == std::string::npos) {
                    std::cerr << "PipeMode: Malformed CLIP: " << line << std::endl;
                    break;
                }
                clip_id = rest.substr(0, sp);
                const std::string cl_prefix = "content-length:";
                auto cl_part = rest.substr(sp + 1);
                if (cl_part.compare(0, cl_prefix.size(), cl_prefix) != 0) {
                    std::cerr << "PipeMode: Missing content-length in: " << line << std::endl;
                    break;
                }
                try {
                    clip_content_length = std::stoul(cl_part.substr(cl_prefix.size()));
                } catch (...) {
                    std::cerr << "PipeMode: Invalid content-length in: " << line << std::endl;
                    break;
                }
                state = State::CLIP_NAME;

            } else if (line.compare(0, 6, "UCLIP ") == 0) {
                std::string uid = line.substr(6);
                source.run([this, uid = std::move(uid)]() mutable {
                    handle_uclip(std::move(uid));
                });

            } else if (line.compare(0, 6, "DIRTY ") == 0) {
                try {
                    int id = std::stoi(line.substr(6));
                    source.run([this, id] { handle_dirty(id); });
                } catch (...) {
                    std::cerr << "PipeMode: Invalid DIRTY ID: " << line << std::endl;
                }

            } else if (line.compare(0, 8, "UNDIRTY ") == 0) {
                try {
                    int id = std::stoi(line.substr(8));
                    source.run([this, id] { handle_undirty(id); });
                } catch (...) {
                    std::cerr << "PipeMode: Invalid UNDIRTY ID: " << line << std::endl;
                }

            } else if (!line.empty()) {
                std::cerr << "PipeMode: Unknown command: " << line << std::endl;
            }
            break;
        }

        case State::LOAD_FILENAME: {
            std::string filename = line;

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
            source.run([this, wid, fname = std::move(filename),
                        data = std::move(svg_data)]() mutable {
                handle_load(wid, std::move(fname), std::move(data));
            });

            state = State::COMMAND;
            break;
        }

        case State::CLIP_NAME: {
            std::string name = line;

            // Read exactly clip_content_length bytes of SVG data
            std::string svg_data(clip_content_length, '\0');
            if (clip_content_length > 0) {
                std::cin.read(&svg_data[0], clip_content_length);
                if (std::cin.gcount() != static_cast<std::streamsize>(clip_content_length)) {
                    std::cerr << "PipeMode: Short read for CLIP, expected " << clip_content_length
                              << " bytes, got " << std::cin.gcount() << std::endl;
                    state = State::COMMAND;
                    break;
                }
            }

            source.run([this, cid = std::move(clip_id), cname = std::move(name),
                        data = std::move(svg_data)]() mutable {
                handle_clip(std::move(cid), std::move(cname), std::move(data));
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

void PipeMode::handle_open(bool freeze_top)
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

    connect_document(doc);

    // Document starts clean
    doc->setModifiedSinceSave(false);

    // Force undo/redo always enabled when delegating (must be after _doc_to_id registration)
    if (_delegate_undo) {
        enable_undo_actions(doc, true, true);
    }

    // Freeze top-level structure if requested
    if (freeze_top) {
        _frozen_top_windows.insert(id);
        attach_freeze_observer(doc);
    }

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

    // Suppress SAVE emission during swap
    _loading = true;
    // Preserve window geometry if the filename hasn't changed (e.g. undo/redo reload)
    const char *old_fname = old_doc->getDocumentFilename();
    _preserve_geometry = (old_fname && filename == old_fname);

    // Detach freeze observer before swap (if any)
    bool is_frozen = _frozen_top_windows.count(window_id) > 0;
    if (is_frozen) {
        detach_freeze_observer(old_doc);
    }

    // Disconnect observer from old doc, swap, connect to new doc
    disconnect_document(old_doc);
    _doc_to_id.erase(old_doc);

    app->document_swap(window, new_doc);

    _doc_to_id[new_doc] = window_id;
    connect_document(new_doc);

    // Reattach freeze observer to new doc
    if (is_frozen) {
        attach_freeze_observer(new_doc);
    }

    // Restore zoom when geometry was preserved (filename unchanged)
    if (_preserve_geometry) {
        desktop->zoom_absolute(center, zoom, false);
    }

    // New content starts clean
    new_doc->setModifiedSinceSave(false);

    // Force undo/redo always enabled when delegating (must be after _doc_to_id registration)
    if (_delegate_undo) {
        enable_undo_actions(new_doc, true, true);
    }

    _loading = false;
    _preserve_geometry = false;

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

// --- Clip handlers ---

void PipeMode::handle_clip(std::string clip_id, std::string clip_name, std::string svg_data)
{
    _clips[clip_id] = ClipData{clip_id, clip_name, std::move(svg_data)};
    _clips_changed.emit();
}

void PipeMode::handle_uclip(std::string clip_id)
{
    if (_clips.erase(clip_id) > 0) {
        _clips_changed.emit();
    }
}

// --- Dirty state handlers ---

void PipeMode::handle_dirty(int window_id)
{
    auto it = _id_to_window.find(window_id);
    if (it == _id_to_window.end()) {
        std::cerr << "PipeMode: Unknown window ID " << window_id << std::endl;
        return;
    }
    SPDocument *doc = it->second->get_document();
    if (doc) {
        doc->setModifiedSinceSave(true);
    }
}

void PipeMode::handle_undirty(int window_id)
{
    auto it = _id_to_window.find(window_id);
    if (it == _id_to_window.end()) {
        std::cerr << "PipeMode: Unknown window ID " << window_id << std::endl;
        return;
    }
    SPDocument *doc = it->second->get_document();
    if (doc) {
        doc->setModifiedSinceSave(false);
    }
}

// --- Freeze-top observer management ---

void PipeMode::attach_freeze_observer(SPDocument *doc)
{
    if (!doc) return;
    auto *root = doc->getReprRoot();
    if (!root) return;

    auto obs = std::make_unique<FreezeTopObserver>(doc);
    root->addObserver(*obs);
    _freeze_observers[doc] = std::move(obs);
}

void PipeMode::detach_freeze_observer(SPDocument *doc)
{
    auto it = _freeze_observers.find(doc);
    if (it != _freeze_observers.end()) {
        auto *root = doc->getReprRoot();
        if (root) {
            root->removeObserver(*it->second);
        }
        _freeze_observers.erase(it);
    }
}

// --- Document change observer callback ---

void PipeMode::on_document_changed(SPDocument *doc)
{
    if (_loading) return;

    auto it = _doc_to_id.find(doc);
    if (it == _doc_to_id.end()) return;

    int window_id = it->second;
    const char *fname = doc->getDocumentFilename();
    std::string filename = fname ? fname : "";
    Glib::ustring svg_content = sp_repr_save_buf(doc->getReprDoc());

    write_message("SAVE " + std::to_string(window_id) +
                      " content-length:" + std::to_string(svg_content.bytes()),
                  filename, svg_content);

    // Keep the document permanently clean — no dirty indicator, no save prompts
    doc->setModifiedSinceSave(false);
}

// --- Document signal management ---

void PipeMode::connect_document(SPDocument *doc)
{
    // Observer for undo/redo (fires after setModifiedSinceSave)
    auto obs = std::make_unique<PipeUndoObserver>(this, doc);
    doc->addUndoObserver(*obs);
    _observers[doc] = std::move(obs);

    // commit_signal for new commits (fires after setModifiedSinceSave)
    _commit_connections[doc] = doc->connectCommit(
        [this, doc]() { on_document_changed(doc); });
}

void PipeMode::disconnect_document(SPDocument *doc)
{
    auto cit = _commit_connections.find(doc);
    if (cit != _commit_connections.end()) {
        cit->second.disconnect();
        _commit_connections.erase(cit);
    }

    auto it = _observers.find(doc);
    if (it != _observers.end()) {
        doc->removeUndoObserver(*it->second);
        _observers.erase(it);
    }
}

// --- Called by inkscape-application on window destruction ---

void PipeMode::on_window_destroyed(InkscapeWindow *window)
{
    auto it = _window_to_id.find(window);
    if (it == _window_to_id.end()) return;

    int id = it->second;

    // Clean up observer and mappings
    SPDocument *doc = window->get_document();
    if (doc) {
        detach_freeze_observer(doc);
        disconnect_document(doc);
        _doc_to_id.erase(doc);
    }
    _frozen_top_windows.erase(id);
    _id_to_window.erase(id);
    _window_to_id.erase(it);

    // Emit CLOSE only if the user closed it (not us via handle_close)
    if (_closing_programmatically.count(id) == 0) {
        write_line("CLOSE " + std::to_string(id));
    }
}

bool PipeMode::is_pipe_document(SPDocument *doc) const
{
    return _doc_to_id.find(doc) != _doc_to_id.end();
}

int PipeMode::get_window_id(SPDocument *doc) const
{
    auto it = _doc_to_id.find(doc);
    return it != _doc_to_id.end() ? it->second : -1;
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
