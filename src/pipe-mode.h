// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Pipe mode: window-based pipe protocol over stdin/stdout.
 *
 * Copyright (C) 2026 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef INKSCAPE_PIPE_MODE_H
#define INKSCAPE_PIPE_MODE_H

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>

#include <sigc++/connection.h>

#include "async/channel.h"
#include "undo-stack-observer.h"

class InkscapeWindow;
class SPDocument;

class PipeMode
{
public:
    PipeMode();
    ~PipeMode();

    PipeMode(PipeMode const &) = delete;
    PipeMode &operator=(PipeMode const &) = delete;

    void start();
    void stop();

    void set_delegate_undo(bool delegate) { _delegate_undo = delegate; }
    bool delegate_undo() const { return _delegate_undo; }
    bool preserve_geometry() const { return _preserve_geometry; }

    void on_window_destroyed(InkscapeWindow *window);

    bool is_pipe_document(SPDocument *doc) const;
    int get_window_id(SPDocument *doc) const;

    void write_line(const std::string &line);

    static PipeMode *instance() { return _instance; }

private:
    void reader_thread_func(Inkscape::Async::Channel::Source source);

    // Main-thread handlers dispatched from reader thread
    void handle_open();
    void handle_load(int window_id, std::string filename, std::string svg_data);
    void handle_close(int window_id);

    // Called by the undo observer on commit/undo/redo
    void on_document_changed(SPDocument *doc);

    void connect_document(SPDocument *doc);
    void disconnect_document(SPDocument *doc);

    void write_message(const std::string &header, const std::string &filename,
                       const std::string &content);

    int _next_id = 1;
    std::map<int, InkscapeWindow *> _id_to_window;
    std::map<InkscapeWindow *, int> _window_to_id;
    std::map<SPDocument *, int> _doc_to_id;
    std::set<int> _closing_programmatically;

    // Undo observer per document (handles undo/redo only)
    class PipeUndoObserver;
    std::map<SPDocument *, std::unique_ptr<PipeUndoObserver>> _observers;

    // commit_signal connections per document (handles new commits)
    std::map<SPDocument *, sigc::connection> _commit_connections;

    bool _delegate_undo = false;

    // Suppress emitting SAVE during LOAD (the commit from document_swap)
    bool _loading = false;
    // Suppress setup_view window geometry changes when filename hasn't changed
    bool _preserve_geometry = false;

    Inkscape::Async::Channel::Dest _channel_dest;
    std::mutex _stdout_mutex;
    std::thread _reader_thread;

    static PipeMode *_instance;
};

#endif // INKSCAPE_PIPE_MODE_H
