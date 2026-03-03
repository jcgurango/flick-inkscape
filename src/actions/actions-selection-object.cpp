// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 *
 * Actions related to manipulation a selection of objects which don't require desktop.
 *
 * Authors:
 *   Sushant A A <sushant.co19@gmail.com>
 *
 * Copyright (C) 2021 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/*
 * Note: Actions must be app level as different windows can have different selections
 *       and selections must also work from the command line (without GUI).
 */

#include <giomm.h>
#include <glibmm/i18n.h>

#include "actions-selection-object.h"
#include "actions-helper.h"
#include "document-undo.h"
#include "inkscape-application.h"
#include "page-manager.h"
#include "pipe-mode.h"
#include "selection.h"

#include "ui/dialog/dialog-container.h" // Used by select_object_link() to open dialog to add hyperlink.
#include "ui/icon-names.h"

void
select_object_group(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();
    selection->group();
    Inkscape::DocumentUndo::done(selection->document(), C_("Verb", "Group"), INKSCAPE_ICON("object-group"));
}

void
select_object_create_clip(InkscapeApplication* app)
{
    auto *pm = PipeMode::instance();
    if (!pm) return;

    Inkscape::Selection *selection = app->get_active_selection();
    if (!selection || selection->isEmpty()) return;

    auto *group_node = selection->group();
    if (!group_node) return;

    Inkscape::DocumentUndo::done(selection->document(), _("Create Clip"), INKSCAPE_ICON("object-group"));

    const char *id = group_node->attribute("id");
    int window_id = pm->get_window_id(selection->document());
    if (id && window_id >= 0) {
        pm->write_line("NCLIP " + std::to_string(window_id) + " " + std::string(id));
    }
}

void
select_object_ungroup(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    selection->ungroup();
    Inkscape::DocumentUndo::done(selection->document(), _("Ungroup"), INKSCAPE_ICON("object-ungroup"));
}

void
select_object_ungroup_pop(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    // Pop Selected Objects out of Group
    selection->popFromGroup();
}

void
select_object_link(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    // Group with <a>
    auto anchor = selection->group(true);
    selection->set(anchor);

    // Open dialog to set link.
    selection->desktop()->getContainer()->new_dialog("ObjectProperties");
    Inkscape::DocumentUndo::done(selection->document(), _("Anchor"), INKSCAPE_ICON("object-group"));
}

void
selection_top(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    // Raise to Top
    selection->raiseToTop();
}

void
selection_raise(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    // Raise
    selection->raise();
}

void
selection_lower(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    // Lower
    selection->lower();
}

void
selection_bottom(InkscapeApplication* app)
{
    Inkscape::Selection *selection = app->get_active_selection();

    // Lower to Bottom
    selection->lowerToBottom();
}

void
selection_stack_up(InkscapeApplication *app)
{
    auto selection = app->get_active_selection();
    selection->stackUp();
}

void
selection_stack_down(InkscapeApplication *app)
{
    auto selection = app->get_active_selection();
    selection->stackDown();
}

void
selection_make_bitmap_copy(InkscapeApplication *app)
{
    auto selection = app->get_active_selection();

    // Make a Bitmap Copy
    selection->createBitmapCopy();
}

void
page_fit_to_selection(InkscapeApplication *app)
{
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }

    document->getPageManager().fitToSelection(selection);
    Inkscape::DocumentUndo::done(document, _("Resize page to fit"), INKSCAPE_ICON("tool-pages"));
}

std::vector<std::vector<Glib::ustring>> raw_data_selection_object =
{
    // clang-format off
    { "app.selection-group",                NC_("Verb", "Group"),                        "Select",   N_("Group selected objects")},
    { "app.selection-create-clip",          N_("Create Clip"),                           "Select",   N_("Group selected objects and create a clip for pipe mode")},
    { "app.selection-ungroup",              N_("Ungroup"),                               "Select",   N_("Ungroup selected objects")},
    { "app.selection-ungroup-pop",          N_("Pop Selected Objects out of Group"),     "Select",   N_("Pop selected objects out of group")},
    { "app.selection-link",                 NC_("Hyperlink|Verb", "Link"),               "Select",   N_("Add an anchor to selected objects")},

    { "app.selection-top",                  N_("Raise to Top"),                          "Select",   N_("Raise selection to top")},
    { "app.selection-raise",                N_("Raise"),                                 "Select",   N_("Raise selection one step")},
    { "app.selection-lower",                N_("Lower"),                                 "Select",   N_("Lower selection one step")},
    { "app.selection-bottom",               N_("Lower to Bottom"),                       "Select",   N_("Lower selection to bottom")},

    { "app.selection-stack-up",             N_("Move up the Stack"),                     "Select",   N_("Move the selection up in the stack order")},
    { "app.selection-stack-down",           N_("Move down the Stack"),                   "Select",   N_("Move the selection down in the stack order")},

    { "app.selection-make-bitmap-copy",     N_("Make a Bitmap Copy"),                    "Select",   N_("Export selection to a bitmap and insert it into document")},
    { "app.page-fit-to-selection",          N_("Resize Page to Selection"),              "Page",     N_("Fit the page to the current selection or the drawing if there is no selection")}
    // clang-format on
};

void
add_actions_selection_object(InkscapeApplication* app)
{
    auto *gapp = app->gio_app();

    // clang-format off
    // See actions-layer.cpp for "enter-group" and "exit-group".
    gapp->add_action( "selection-group",              sigc::bind(sigc::ptr_fun(&select_object_group),           app));
    gapp->add_action( "selection-create-clip",        sigc::bind(sigc::ptr_fun(&select_object_create_clip),     app));
    gapp->add_action( "selection-ungroup",            sigc::bind(sigc::ptr_fun(&select_object_ungroup),         app));
    gapp->add_action( "selection-ungroup-pop",        sigc::bind(sigc::ptr_fun(&select_object_ungroup_pop),     app));
    gapp->add_action( "selection-link",               sigc::bind(sigc::ptr_fun(&select_object_link),            app));

    gapp->add_action( "selection-top",                sigc::bind(sigc::ptr_fun(&selection_top),                 app));
    gapp->add_action( "selection-raise",              sigc::bind(sigc::ptr_fun(&selection_raise),               app));
    gapp->add_action( "selection-lower",              sigc::bind(sigc::ptr_fun(&selection_lower),               app));
    gapp->add_action( "selection-bottom",             sigc::bind(sigc::ptr_fun(&selection_bottom),              app));

    gapp->add_action( "selection-stack-up",           sigc::bind(sigc::ptr_fun(&selection_stack_up),            app));
    gapp->add_action( "selection-stack-down",         sigc::bind(sigc::ptr_fun(&selection_stack_down),          app));

    gapp->add_action( "selection-make-bitmap-copy",   sigc::bind(sigc::ptr_fun(&selection_make_bitmap_copy),    app));
    gapp->add_action( "page-fit-to-selection",        sigc::bind(sigc::ptr_fun(&page_fit_to_selection),         app));
    // clang-format on

    app->get_action_extra_data().add_data(raw_data_selection_object);
}
