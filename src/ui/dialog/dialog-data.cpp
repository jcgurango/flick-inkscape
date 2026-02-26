// SPDX-License-Identifier: GPL-2.0-or-later

#include <map>

#include <glibmm/i18n.h>
#include <glibmm/ustring.h>

#include "config.h" // Needed for WITH_GSPELL

#include "ui/dialog/dialog-data.h"
#include "ui/icon-names.h"  // INKSCAPE_ICON macro

/*
 * In an ideal world, this information would be in .ui files for each
 * dialog (the .ui file would describe a dialog wrapped by a notebook
 * tab). At the moment we create each dialog notebook tab on the fly
 * so we need a place to keep this information.
 */

std::map<std::string, DialogData> const &get_dialog_data()
{
    static std::map<std::string, DialogData> dialog_data;

    // Note the "AttrDialog" is now part of the "XMLDialog" and the "Style" dialog is part of the
    // "Selectors" dialog. Also note that the "AttrDialog" does not correspond to SP_VERB_DIALOG_ATTR!!!
    // (That would be the "ObjectAttributes" dialog.)

    if (dialog_data.empty()) {
        dialog_data = {
        // clang-format off
    {"AlignDistribute",    {_("_Align and Distribute"), INKSCAPE_ICON("dialog-align-and-distribute"), DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"ClipPanel",          {_("Clip Panel"),             INKSCAPE_ICON("edit-paste"),                  DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"CloneTiler",         {_("Create Tiled Clones"),   INKSCAPE_ICON("dialog-tile-clones"),          DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"DocumentProperties", {_("_Document Properties"),  INKSCAPE_ICON("document-properties"),         DialogData::Settings,       ScrollProvider::NOPROVIDE }},
    {"DocumentResources",  {_("_Document Resources"),   INKSCAPE_ICON("document-resources"),          DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
    {"Export",             {_("_Export"),               INKSCAPE_ICON("document-export"),             DialogData::Basic,          ScrollProvider::PROVIDE   }},
    {"ExtensionsGallery",  {_("_Extension Gallery"),   INKSCAPE_ICON("dialog-extensions"),           DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"FillStroke",         {_("_Fill and Stroke"),      INKSCAPE_ICON("dialog-fill-and-stroke"),      DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"FilterEffects",      {_("Filter _Editor"),        INKSCAPE_ICON("dialog-filters"),              DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
    {"FilterGallery",      {_("Filter Gallery"),        INKSCAPE_ICON("color-filters"),               DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
    {"Find",               {_("_Find/Replace"),         INKSCAPE_ICON("edit-find"),                   DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"FontCollections",    {_("_Font Collections"),     INKSCAPE_ICON("font_collections"),            DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
    {"Glyphs",             {_("_Unicode Characters"),   INKSCAPE_ICON("accessories-character-map"),   DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"IconPreview",        {_("Icon Preview"),          INKSCAPE_ICON("dialog-icon-preview"),         DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"LivePathEffect",     {_("Path E_ffects"),         INKSCAPE_ICON("dialog-path-effects"),         DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
    {"Memory",             {_("About _Memory"),         INKSCAPE_ICON("dialog-memory"),               DialogData::Diagnostics,    ScrollProvider::PROVIDE   }},
    {"Messages",           {_("_Messages"),             INKSCAPE_ICON("dialog-messages"),             DialogData::Diagnostics,    ScrollProvider::NOPROVIDE }},
    {"ObjectProperties",   {_("_Object Properties"),    INKSCAPE_ICON("dialog-object-properties"),    DialogData::Settings,       ScrollProvider::NOPROVIDE }},
    {"Objects",            {_("Layers and Object_s"),   INKSCAPE_ICON("dialog-objects"),              DialogData::Basic,          ScrollProvider::PROVIDE   }},
    {"PaintServers",       {_("_Paint Servers"),        INKSCAPE_ICON("dialog-paint-server"),         DialogData::Advanced,       ScrollProvider::PROVIDE   }},
    {"Preferences",        {_("P_references"),          INKSCAPE_ICON("preferences-system"),          DialogData::Settings,       ScrollProvider::PROVIDE   }},
    {"Selectors",          {_("_Selectors and CSS"),    INKSCAPE_ICON("dialog-selectors"),            DialogData::Advanced,       ScrollProvider::PROVIDE   }},
    {"SVGFonts",           {_("SVG Font Editor"),       INKSCAPE_ICON("dialog-svg-font"),             DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
    {"Swatches",           {_("S_watches"),             INKSCAPE_ICON("swatches"),                    DialogData::Basic,          ScrollProvider::PROVIDE   }},
    {"Symbols",            {_("S_ymbols"),              INKSCAPE_ICON("symbols"),                     DialogData::Basic,          ScrollProvider::PROVIDE   }},
    {"Text",               {_("_Text and Font"),        INKSCAPE_ICON("dialog-text-and-font"),        DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"Trace",              {_("_Trace Bitmap"),         INKSCAPE_ICON("bitmap-trace"),                DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"Transform",          {_("Transfor_m"),            INKSCAPE_ICON("dialog-transform"),            DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"UndoHistory",        {_("Undo _History"),         INKSCAPE_ICON("edit-undo-history"),           DialogData::Basic,          ScrollProvider::NOPROVIDE }},
    {"XMLEditor",          {_("_XML Editor"),           INKSCAPE_ICON("dialog-xml-editor"),           DialogData::Advanced,       ScrollProvider::NOPROVIDE }},
#if WITH_GSPELL
    {"Spellcheck",         {_("Check Spellin_g"),       INKSCAPE_ICON("tools-check-spelling"),        DialogData::Basic,          ScrollProvider::NOPROVIDE }},
#endif
#if DEBUG
    {"Prototype",          {_("Prototype"),             INKSCAPE_ICON("document-properties"),         DialogData::Other,          ScrollProvider::NOPROVIDE }},
#endif
        // clang-format on
        };
    }
    return dialog_data;
}

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
