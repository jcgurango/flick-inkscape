// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Clip Panel dialog — displays SVG clips registered by pipe-mode controller.
 */
/*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "clip-panel.h"

#include <gtkmm/targetentry.h>

#include "pipe-mode.h"
#include "ui/pack.h"

namespace Inkscape::UI::Dialog {

ClipPanel::ClipPanel()
    : DialogBase("/dialogs/clip-panel", "ClipPanel")
{
    _name = "Clip Panel";

    _store = Gtk::ListStore::create(_columns);
    _tree_view.set_model(_store);
    _tree_view.set_headers_visible(false);
    _tree_view.append_column("Name", _columns.clip_name);

    // Enable drag source with SVG MIME type
    std::vector<Gtk::TargetEntry> targets;
    targets.emplace_back("image/svg+xml", Gtk::TargetFlags(0), 0);
    _tree_view.enable_model_drag_source(targets, Gdk::BUTTON1_MASK, Gdk::ACTION_COPY);

    _tree_view.signal_drag_data_get().connect(
        sigc::mem_fun(*this, &ClipPanel::on_drag_data_get));

    _scrolled_window.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
    _scrolled_window.add(_tree_view);

    UI::pack_start(*this, _scrolled_window, true, true);

    // Connect to PipeMode's clip-changed signal
    if (auto *pm = PipeMode::instance()) {
        _clips_changed_conn = pm->signal_clips_changed().connect(
            sigc::mem_fun(*this, &ClipPanel::rebuild));
        rebuild();
    }

    show_all_children();
}

ClipPanel::~ClipPanel()
{
    _clips_changed_conn.disconnect();
}

void ClipPanel::rebuild()
{
    _store->clear();

    auto *pm = PipeMode::instance();
    if (!pm) return;

    for (auto const &[id, clip] : pm->clips()) {
        auto row = *_store->append();
        row[_columns.clip_id] = clip.id;
        row[_columns.clip_name] = clip.name;
    }
}

void ClipPanel::on_drag_data_get(const Glib::RefPtr<Gdk::DragContext> &/*context*/,
                                 Gtk::SelectionData &selection_data,
                                 guint /*info*/, guint /*time*/)
{
    auto sel = _tree_view.get_selection();
    auto iter = sel->get_selected();
    if (!iter) return;

    Glib::ustring clip_id = (*iter)[_columns.clip_id];

    auto *pm = PipeMode::instance();
    if (!pm) return;

    auto const &clips = pm->clips();
    auto it = clips.find(clip_id.raw());
    if (it == clips.end()) return;

    auto const &svg = it->second.svg_data;
    selection_data.set("image/svg+xml", 8,
                       reinterpret_cast<const guint8 *>(svg.data()),
                       svg.size());
}

} // namespace Inkscape::UI::Dialog

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
