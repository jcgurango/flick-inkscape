// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Clip Panel dialog — displays SVG clips registered by pipe-mode controller.
 */
/*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_CLIP_PANEL_H
#define INKSCAPE_UI_DIALOG_CLIP_PANEL_H

#include <gtkmm/liststore.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <sigc++/connection.h>

#include "ui/dialog/dialog-base.h"

namespace Inkscape::UI::Dialog {

class ClipPanel : public DialogBase
{
public:
    ClipPanel();
    ~ClipPanel() override;

private:
    void rebuild();
    void on_drag_data_get(const Glib::RefPtr<Gdk::DragContext> &context,
                          Gtk::SelectionData &selection_data,
                          guint info, guint time);

    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() { add(clip_id); add(clip_name); }
        Gtk::TreeModelColumn<Glib::ustring> clip_id;
        Gtk::TreeModelColumn<Glib::ustring> clip_name;
    };

    ModelColumns _columns;
    Glib::RefPtr<Gtk::ListStore> _store;
    Gtk::ScrolledWindow _scrolled_window;
    Gtk::TreeView _tree_view;

    sigc::connection _clips_changed_conn;
};

} // namespace Inkscape::UI::Dialog

#endif // INKSCAPE_UI_DIALOG_CLIP_PANEL_H

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
