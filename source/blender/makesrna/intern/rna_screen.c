/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <stddef.h>
#include <stdlib.h>

#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "rna_internal.h"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_workspace_types.h"

#include "BKE_screen.h"

#include "ED_info.h"
#include "ED_space_api.h"

const EnumPropertyItem rna_enum_region_type_items[] = {
    {RGN_TYPE_WINDOW, "WINDOW", 0, "Window", ""},
    {RGN_TYPE_HEADER, "HEADER", 0, "Header", ""},
    {RGN_TYPE_CHANNELS, "CHANNELS", 0, "Channels", ""},
    {RGN_TYPE_TEMPORARY, "TEMPORARY", 0, "Temporary", ""},
    {RGN_TYPE_UI, "UI", 0, "UI", ""},
    {RGN_TYPE_TOOLS, "TOOLS", 0, "Tools", ""},
    {RGN_TYPE_TOOL_PROPS, "TOOL_PROPS", 0, "Tool Properties", ""},
    {RGN_TYPE_PREVIEW, "PREVIEW", 0, "Preview", ""},
    {RGN_TYPE_HUD, "HUD", 0, "Floating Region", ""},
    {RGN_TYPE_NAV_BAR, "NAVIGATION_BAR", 0, "Navigation Bar", ""},
    {RGN_TYPE_EXECUTE, "EXECUTE", 0, "Execute Buttons", ""},
    {RGN_TYPE_FOOTER, "FOOTER", 0, "Footer", ""},
    {RGN_TYPE_TOOL_HEADER, "TOOL_HEADER", 0, "Tool Header", ""},
    {RGN_TYPE_XR, "XR", 0, "XR", ""},
    {0, NULL, 0, NULL, NULL},
};

#include "ED_screen.h"

#include "WM_api.h"
#include "WM_types.h"

#ifdef RNA_RUNTIME

#  include "RNA_access.h"

#  include "BKE_global.h"
#  include "BKE_screen.h"
#  include "BKE_workspace.h"

#  include "DEG_depsgraph.h"

#  include "UI_interface.h"
#  include "UI_view2d.h"

#  ifdef WITH_PYTHON
#    include "BPY_extern.h"
#  endif

static void rna_Screen_bar_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->data;
  screen->do_draw = true;
  screen->do_refresh = true;
}

static void rna_Screen_redraw_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->data;

  /* the settings for this are currently only available from a menu in the TimeLine,
   * hence refresh=SPACE_ACTION, as timeline is now in there
   */
  ED_screen_animation_timer_update(screen, screen->redraws_flag);
}

static bool rna_Screen_is_animation_playing_get(PointerRNA *UNUSED(ptr))
{
  /* can be NULL on file load, T42619 */
  wmWindowManager *wm = G_MAIN->wm.first;
  return wm ? (ED_screen_animation_playing(wm) != NULL) : 0;
}

static bool rna_Screen_is_scrubbing_get(PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->data;
  return screen->scrubbing;
}

static int rna_region_alignment_get(PointerRNA *ptr)
{
  ARegion *region = ptr->data;
  return RGN_ALIGN_ENUM_FROM_MASK(region->alignment);
}

static bool rna_Screen_fullscreen_get(PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->data;
  return (screen->state == SCREENMAXIMIZED);
}

static int rna_Area_type_get(PointerRNA *ptr)
{
  ScrArea *area = (ScrArea *)ptr->data;
  /* Usually 'spacetype' is used. It lags behind a bit while switching area
   * type though, then we use 'butspacetype' instead (T41435). */
  return (area->butspacetype == SPACE_EMPTY) ? area->spacetype : area->butspacetype;
}

static void rna_Area_type_set(PointerRNA *ptr, int value)
{
  if (ELEM(value, SPACE_TOPBAR, SPACE_STATUSBAR)) {
    /* Special case: An area can not be set to show the top-bar editor (or
     * other global areas). However it should still be possible to identify
     * its type from Python. */
    return;
  }

  ScrArea *area = (ScrArea *)ptr->data;
  /* Empty areas are locked. */
  if ((value == SPACE_EMPTY) || (area->spacetype == SPACE_EMPTY)) {
    return;
  }

  area->butspacetype = value;
}

static void rna_Area_type_update(bContext *C, PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->owner_id;
  ScrArea *area = (ScrArea *)ptr->data;

  /* Running update without having called 'set', see: T64049 */
  if (area->butspacetype == SPACE_EMPTY) {
    return;
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win;
  /* XXX this call still use context, so we trick it to work in the right context */
  for (win = wm->windows.first; win; win = win->next) {
    if (screen == WM_window_get_active_screen(win)) {
      wmWindow *prevwin = CTX_wm_window(C);
      ScrArea *prevsa = CTX_wm_area(C);
      ARegion *prevar = CTX_wm_region(C);

      CTX_wm_window_set(C, win);
      CTX_wm_area_set(C, area);
      CTX_wm_region_set(C, NULL);

      ED_area_newspace(C, area, area->butspacetype, true);
      ED_area_tag_redraw(area);

      /* Unset so that rna_Area_type_get uses spacetype instead. */
      area->butspacetype = SPACE_EMPTY;

      /* It is possible that new layers becomes visible. */
      if (area->spacetype == SPACE_VIEW3D) {
        DEG_tag_on_visible_update(CTX_data_main(C), false);
      }

      CTX_wm_window_set(C, prevwin);
      CTX_wm_area_set(C, prevsa);
      CTX_wm_region_set(C, prevar);
      break;
    }
  }
}

typedef struct SpaceTypeAddItemCallbackData {
  EnumPropertyItem *items;
  int totitem;
  int totcustom;
} SpaceTypeAddItemCallbackData;

static void add_item_for_space_type_cb(const SpaceType *spacetype, void *data)
{
  SpaceTypeAddItemCallbackData *cb_data = (SpaceTypeAddItemCallbackData *)(data);

  if (cb_data->totcustom == 0) {
    EnumPropertyItem md_item;
    md_item.value = 0;
    md_item.identifier = "";
    md_item.icon = 0;
    md_item.name = "Custom";
    md_item.description = "";

    RNA_enum_item_add(&cb_data->items, &cb_data->totitem, &md_item);
  }

  EnumPropertyItem md_item;
  md_item.value = (SPACE_CUSTOM + cb_data->totcustom) << 16;
  md_item.description = NULL;
  md_item.name = spacetype->name;
  md_item.identifier = spacetype->idname;
  md_item.icon = ICON_NONE;  // TODO(space_type) spacetype->icon;

  RNA_enum_item_add(&cb_data->items, &cb_data->totitem, &md_item);
  cb_data->totcustom += 1;
}

static const EnumPropertyItem *rna_Area_ui_type_itemf(bContext *C,
                                                      PointerRNA *ptr,
                                                      PropertyRNA *UNUSED(prop),
                                                      bool *r_free)
{
  EnumPropertyItem *item = NULL;
  int totitem = 0;

  ScrArea *area = (ScrArea *)ptr->data;
  const EnumPropertyItem *item_from = rna_enum_space_type_items;
  if (area->spacetype != SPACE_EMPTY) {
    item_from += 1; /* +1 to skip SPACE_EMPTY */
  }

  for (; item_from->identifier; item_from++) {
    if (ELEM(item_from->value, SPACE_TOPBAR, SPACE_STATUSBAR)) {
      continue;
    }

    SpaceType *st = item_from->identifier[0] ? BKE_spacetype_from_id(item_from->value) : NULL;
    int totitem_prev = totitem;
    if (st && st->space_subtype_item_extend != NULL) {
      st->space_subtype_item_extend(C, &item, &totitem);
      while (totitem_prev < totitem) {
        item[totitem_prev++].value |= item_from->value << 16;
      }
    }
    else {
      RNA_enum_item_add(&item, &totitem, item_from);
      item[totitem_prev++].value = item_from->value << 16;
    }
  }

  SpaceTypeAddItemCallbackData callback_data;
  callback_data.items = item;
  callback_data.totitem = totitem;
  callback_data.totcustom = 0;
  BKE_spacetype_foreach_custom_type(add_item_for_space_type_cb, &callback_data);
  item = callback_data.items;
  totitem = callback_data.totitem;

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int rna_Area_ui_type_get(PointerRNA *ptr)
{
  ScrArea *area = ptr->data;
  /* This is for the Python API which may inspect empty areas. */
  if (UNLIKELY(area->spacetype == SPACE_EMPTY)) {
    return SPACE_EMPTY;
  }
  const int area_type = rna_Area_type_get(ptr);
  const bool area_changing = area->butspacetype != SPACE_EMPTY;
  int value = area_type << 16;

  /* Area->type can be NULL when not yet initialized (for example when accessed
   * through the outliner or API when not visible), or it can be wrong while
   * the area type is changing.
   * So manually do the lookup in those cases, but do not actually change area->type
   * since that prevents a proper exit when the area type is changing.
   * Logic copied from `ED_area_init()`. */
  SpaceType *type = area->type;
  if (type == NULL || area_changing) {
    type = BKE_spacetype_from_id(area_type);
    if (type == NULL) {
      type = BKE_spacetype_from_id(SPACE_VIEW3D);
    }
    BLI_assert(type != NULL);
  }
  if (type->space_subtype_item_extend != NULL) {
    value |= area_changing ? area->butspacetype_subtype : type->space_subtype_get(area);
  }
  return value;
}

static void rna_Area_ui_type_set(PointerRNA *ptr, int value)
{
  ScrArea *area = ptr->data;
  const int space_type = value >> 16;
  /* Empty areas are locked. */
  if ((space_type == SPACE_EMPTY) || (area->spacetype == SPACE_EMPTY)) {
    return;
  }
  SpaceType *st = BKE_spacetype_from_id(space_type);

  rna_Area_type_set(ptr, space_type);

  if (st && st->space_subtype_item_extend != NULL) {
    area->butspacetype_subtype = value & 0xffff;
  }
}

static void rna_Area_ui_type_update(bContext *C, PointerRNA *ptr)
{
  ScrArea *area = ptr->data;
  SpaceType *st = BKE_spacetype_from_id(area->butspacetype);

  rna_Area_type_update(C, ptr);

  if ((area->type == st) && (st->space_subtype_item_extend != NULL)) {
    st->space_subtype_set(area, area->butspacetype_subtype);
  }
  area->butspacetype_subtype = 0;

  ED_area_tag_refresh(area);
}

static PointerRNA rna_Region_data_get(PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->owner_id;
  ARegion *region = ptr->data;

  if (region->regiondata != NULL) {
    if (region->regiontype == RGN_TYPE_WINDOW) {
      /* We could make this static, it won't change at run-time. */
      SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
      if (region->type == BKE_regiontype_from_id(st, region->regiontype)) {
        PointerRNA newptr;
        RNA_pointer_create(&screen->id, &RNA_RegionView3D, region->regiondata, &newptr);
        return newptr;
      }
    }
  }
  return PointerRNA_NULL;
}

static void rna_View2D_region_to_view(struct View2D *v2d, float x, float y, float result[2])
{
  UI_view2d_region_to_view(v2d, x, y, &result[0], &result[1]);
}

static void rna_View2D_view_to_region(
    struct View2D *v2d, float x, float y, bool clip, int result[2])
{
  if (clip) {
    UI_view2d_view_to_region_clip(v2d, x, y, &result[0], &result[1]);
  }
  else {
    UI_view2d_view_to_region(v2d, x, y, &result[0], &result[1]);
  }
}

static const char *rna_Screen_statusbar_info_get(struct bScreen *UNUSED(screen),
                                                 Main *bmain,
                                                 bContext *C)
{
  return ED_info_statusbar_string(bmain, CTX_data_scene(C), CTX_data_view_layer(C));
}

/* Region registration */

static void region_init(struct wmWindowManager *wm, struct ARegion *region)
{
  ExtensionRNA *ext_rna = &region->type->rna_ext;

  PointerRNA ptr;
  RNA_pointer_create(NULL, ext_rna->srna, region, &ptr);

  /* RNA_struct_find_function(&ptr, "init"); */
  extern FunctionRNA rna_Region_init_func;
  FunctionRNA *func = &rna_Region_init_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "window_manager", &wm);

  ext_rna->call(NULL, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void region_exit(struct wmWindowManager *wm, struct ARegion *region)
{
  ExtensionRNA *ext_rna = &region->type->rna_ext;

  PointerRNA ptr;
  RNA_pointer_create(NULL, ext_rna->srna, region, &ptr);

  /* RNA_struct_find_function(&ptr, "exit"); */
  extern FunctionRNA rna_Region_exit_func;
  FunctionRNA *func = &rna_Region_exit_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "window_manager", &wm);

  ext_rna->call(NULL, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static void region_draw(const struct bContext *C, struct ARegion *region)
{
  ExtensionRNA *ext_rna = &region->type->rna_ext;

  PointerRNA ptr;
  RNA_pointer_create(NULL, ext_rna->srna, region, &ptr);

  /* RNA_struct_find_function(&ptr, "draw"); */
  extern FunctionRNA rna_Region_draw_func;
  FunctionRNA *func = &rna_Region_draw_func;

  ParameterList list;
  RNA_parameter_list_create(&list, &ptr, func);
  RNA_parameter_set_lookup(&list, "context", &C);

  ext_rna->call(NULL, &ptr, func, &list);

  RNA_parameter_list_free(&list);
}

static StructRNA *rna_Region_refine(PointerRNA *ptr)
{
  ARegion *region = (ARegion *)ptr->data;
  return (region->type && region->type->rna_ext.srna) ? region->type->rna_ext.srna : &RNA_Region;
}

static void rna_Region_unregister(Main *bmain, StructRNA *type)
{
  ARegionType *art = RNA_struct_blender_type_get(type);

  if (!art) {
    return;
  }

  SpaceType *st = BKE_spacetype_from_idname(art->space_type);
  if (!st) {
    return;
  }

  RNA_struct_free_extension(type, &art->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  BKE_regiontype_unregister(bmain, st, art);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, NULL);
}

static void region_properties_draw(const bContext *C, ARegion *region)
{
  /* TODO: figure out how to set contexts. */
  ED_region_panels_ex(C, region, (const char *[]){CTX_data_mode_string(C), NULL});
}

static void region_header_init(wmWindowManager *UNUSED(wm), ARegion *region)
{
  ED_region_header_init(region);
}

static void init_default_region_type(ARegionType *art)
{
  /* All custom regions use a panels layout. */
  art->layout = ED_region_panels_layout;

  switch (art->regionid) {
    case RGN_TYPE_WINDOW: {
      art->init = ED_region_panels_init;
      art->draw = ED_region_window_draw;
      art->keymapflag = ED_KEYMAP_GIZMO | ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_GPENCIL;
      break;
    }
    case RGN_TYPE_HEADER: {
      art->init = region_header_init;
      art->draw = ED_region_header;
      art->prefsizey = HEADERY;
      art->layout = NULL;
      art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_HEADER;
      break;
    }
    case RGN_TYPE_FOOTER: {
      art->init = region_header_init;
      art->draw = ED_region_header;
      art->layout = NULL;
      art->prefsizey = HEADERY;
      art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FOOTER;
      break;
    }
    case RGN_TYPE_UI: {
      art->init = ED_region_panels_init;
      art->draw = region_properties_draw;
      art->prefsizex = UI_SIDEBAR_PANEL_WIDTH;
      art->keymapflag = ED_KEYMAP_FRAMES | ED_KEYMAP_UI;
      break;
    }
    case RGN_TYPE_NAV_BAR: {
      art->init = ED_region_panels_init;
      art->draw = ED_region_panels;
      art->prefsizex = UI_NAVIGATION_REGION_WIDTH;
      art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_NAVBAR;
      break;
    }
    case RGN_TYPE_EXECUTE: {
      art->init = ED_region_panels_init;
      art->draw = ED_region_panels;
      art->prefsizex = UI_NAVIGATION_REGION_WIDTH;
      art->keymapflag = ED_KEYMAP_UI;
      break;
    }
    case RGN_TYPE_TOOLS:
    case RGN_TYPE_TOOL_PROPS: {
      art->init = ED_region_panels_init;
      art->draw = ED_region_panels;
      art->prefsizex = 240;
      art->prefsizey = 60;
      art->keymapflag = ED_KEYMAP_UI;
      break;
    }
    case RGN_TYPE_TOOL_HEADER: {
      art->init = region_header_init;
      art->draw = ED_region_header;
      art->layout = NULL;
      art->prefsizey = HEADERY;
      art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
      break;
    }
    default: {
      art->init = ED_region_panels_init;
      art->draw = ED_region_panels;
      break;
    }
  }
}

static StructRNA *rna_Region_register(Main *bmain,
                                      ReportList *reports,
                                      void *data,
                                      const char *identifier,
                                      StructValidateFunc validate,
                                      StructCallbackFunc call,
                                      StructFreeFunc free)
{
  ARegionType dummyart = {NULL};
  ARegion dummyregion = {NULL};
  PointerRNA dummyptr;
  int have_function[3];

  /* setup dummy region & region type to store static properties in */
  dummyregion.type = &dummyart;
  RNA_pointer_create(NULL, &RNA_Region, &dummyregion, &dummyptr);

  /* validate the python class */
  if (validate(&dummyptr, data, have_function) != 0) {
    return NULL;
  }

  if (strlen(identifier) >= sizeof(dummyart.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering region class: '%s' is too long, maximum length is %d",
                identifier,
                (int)sizeof(dummyart.idname));
    return NULL;
  }

  SpaceType *st = BKE_spacetype_from_idname(dummyart.space_type);

  if (!st) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering region class: SpaceType '%s' is unknown for region '%s'",
                dummyart.space_type,
                identifier);
    return NULL;
  }

  if (st->spaceid != SPACE_CUSTOM) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering region class: SpaceType '%s' is not a custom space, cannot register "
                "regions for builtin spaces",
                dummyart.space_type);
    return NULL;
  }

  /* check if we have registered this region type before, and remove it */
  LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
    if (STREQ(art->idname, dummyart.idname)) {
      rna_Region_unregister(bmain, art->rna_ext.srna);
      break;
    }
  }

  /* create a new region type */
  ARegionType *art;
  if (dummyart.regionid == RGN_TYPE_HUD) {
    art = ED_area_type_hud(st->idname);
    BLI_addhead(&st->regiontypes, art);
  }
  else {
    art = BKE_spacetype_add_region_type(st, dummyart.regionid, "python region");
  }

  /* Skip prev and next pointers! BKE_spacetype_add_region_type modifies them, we don't want to
   * reset them. */
  memcpy((char *)(art) + sizeof(void *) * 2,
         (char *)(&dummyart) + sizeof(void *) * 2,
         sizeof(dummyart) - sizeof(void *) * 2);

  art->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, art->idname, &RNA_Region);
  art->rna_ext.data = data;
  art->rna_ext.call = call;
  art->rna_ext.free = free;
  RNA_struct_blender_type_set(art->rna_ext.srna, art);

  if (dummyart.regionid != RGN_TYPE_HUD) {
    /* Initialize some defaults. */
    init_default_region_type(art);

    /* Keep the defaults if not overriden. */
    art->init = (have_function[0]) ? region_init : art->init;
    art->exit = (have_function[1]) ? region_exit : art->exit;
    art->draw = (have_function[2]) ? region_draw : art->draw;
  }

  rna_SpaceType_def_show_region_props(st, art->regionid);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, NULL);

  return art->rna_ext.srna;
}

static void rna_Region_default_init(wmWindowManager *wm, ARegion *region)
{
  switch (region->regiontype) {
    case RGN_TYPE_HEADER:
    case RGN_TYPE_FOOTER:
    case RGN_TYPE_TOOL_HEADER:
    case RGN_TYPE_TOOLS:
    case RGN_TYPE_TOOL_PROPS:
      ED_region_header_init(region);
      break;
    case RGN_TYPE_HUD:
      break;
    default:
      ED_region_panels_init(wm, region);
      break;
  }
}

static void rna_Region_add_keymap(ARegion *region, wmKeyMap *keymap)
{
  WM_event_add_keymap_handler_v2d_mask(&region->handlers, keymap);
}

static void rna_Region_theme_clear_color(ARegion *region)
{
  switch (region->regiontype) {
    case RGN_TYPE_HEADER:
    case RGN_TYPE_TOOL_HEADER:
      UI_ThemeClearColor(TH_HEADER);
      break;
    default:
      UI_ThemeClearColor(TH_BACK);
      break;
  }
}

static void rna_Region_draw_handlers(ARegion *region, bContext *C, int draw_mode)
{
  ED_region_draw_cb_draw(C, region, draw_mode);
}

#else

/* Area.spaces */
static void rna_def_area_spaces(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_def_property_srna(cprop, "AreaSpaces");
  srna = RNA_def_struct(brna, "AreaSpaces", NULL);
  RNA_def_struct_sdna(srna, "ScrArea");
  RNA_def_struct_ui_text(srna, "Area Spaces", "Collection of spaces");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "spacedata.first");
  RNA_def_property_struct_type(prop, "Space");
  RNA_def_property_ui_text(prop, "Active Space", "Space currently being displayed in this area");
}

static void rna_def_area_api(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_function(srna, "tag_redraw", "ED_area_tag_redraw");

  func = RNA_def_function(srna, "header_text_set", "ED_area_status_text");
  RNA_def_function_ui_description(func, "Set the header status text");
  parm = RNA_def_string(
      func, "text", NULL, 0, "Text", "New string for the header, None clears the text");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_property_clear_flag(parm, PROP_NEVER_NULL);
}

static void rna_def_area(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Area", NULL);
  RNA_def_struct_ui_text(srna, "Area", "Area in a subdivided screen, containing an editor");
  RNA_def_struct_sdna(srna, "ScrArea");

  prop = RNA_def_property(srna, "spaces", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "spacedata", NULL);
  RNA_def_property_struct_type(prop, "Space");
  RNA_def_property_ui_text(prop,
                           "Spaces",
                           "Spaces contained in this area, the first being the active space "
                           "(NOTE: Useful for example to restore a previously used 3D view space "
                           "in a certain area to get the old view orientation)");
  rna_def_area_spaces(brna, prop);

  prop = RNA_def_property(srna, "regions", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "regionbase", NULL);
  RNA_def_property_struct_type(prop, "Region");
  RNA_def_property_ui_text(prop, "Regions", "Regions this area is subdivided in");

  prop = RNA_def_property(srna, "show_menus", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", HEADER_NO_PULLDOWN);
  RNA_def_property_ui_text(prop, "Show Menus", "Show menus in the header");

  /* Note on space type use of #SPACE_EMPTY, this is not visible to the user,
   * and script authors should be able to assign this value, however the value may be set
   * and needs to be read back by script authors.
   *
   * This happens when an area is full-screen (when #ScrArea.full is set).
   * in this case reading the empty value is needed, but it should never be set, see: T87187. */
  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "spacetype");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  RNA_def_property_enum_default(prop, SPACE_VIEW3D);
  RNA_def_property_enum_funcs(prop, "rna_Area_type_get", "rna_Area_type_set", NULL);
  RNA_def_property_ui_text(prop, "Editor Type", "Current editor type for this area");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Area_type_update");

  prop = RNA_def_property(srna, "ui_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, DummyRNA_NULL_items); /* in fact dummy */
  RNA_def_property_enum_default(prop, SPACE_VIEW3D << 16);
  RNA_def_property_enum_funcs(
      prop, "rna_Area_ui_type_get", "rna_Area_ui_type_set", "rna_Area_ui_type_itemf");
  RNA_def_property_ui_text(prop, "Editor Type", "Current editor type for this area");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_Area_ui_type_update");

  prop = RNA_def_property(srna, "x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "totrct.xmin");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "X Position", "The window relative vertical location of the area");

  prop = RNA_def_property(srna, "y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "totrct.ymin");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Y Position", "The window relative horizontal location of the area");

  prop = RNA_def_property(srna, "width", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "winx");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Width", "Area width");

  prop = RNA_def_property(srna, "height", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "winy");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Height", "Area height");

  rna_def_area_api(srna);
}

static void rna_def_view2d_api(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

  static const float view_default[2] = {0.0f, 0.0f};
  static const int region_default[2] = {0.0f, 0.0f};

  func = RNA_def_function(srna, "region_to_view", "rna_View2D_region_to_view");
  RNA_def_function_ui_description(func, "Transform region coordinates to 2D view");
  parm = RNA_def_float(func, "x", 0, -FLT_MAX, FLT_MAX, "x", "Region x coordinate", -10000, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(func, "y", 0, -FLT_MAX, FLT_MAX, "y", "Region y coordinate", -10000, 10000);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float_array(func,
                             "result",
                             2,
                             view_default,
                             -FLT_MAX,
                             FLT_MAX,
                             "Result",
                             "View coordinates",
                             -10000.0f,
                             10000.0f);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
  RNA_def_function_output(func, parm);

  func = RNA_def_function(srna, "view_to_region", "rna_View2D_view_to_region");
  RNA_def_function_ui_description(func, "Transform 2D view coordinates to region");
  parm = RNA_def_float(
      func, "x", 0.0f, -FLT_MAX, FLT_MAX, "x", "2D View x coordinate", -10000.0f, 10000.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(
      func, "y", 0.0f, -FLT_MAX, FLT_MAX, "y", "2D View y coordinate", -10000.0f, 10000.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  RNA_def_boolean(func, "clip", 1, "Clip", "Clip coordinates to the visible region");
  parm = RNA_def_int_array(func,
                           "result",
                           2,
                           region_default,
                           INT_MIN,
                           INT_MAX,
                           "Result",
                           "Region coordinates",
                           -10000,
                           10000);
  RNA_def_parameter_flags(parm, PROP_THICK_WRAP, 0);
  RNA_def_function_output(func, parm);
}

static void rna_def_view2d(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "View2D", NULL);
  RNA_def_struct_ui_text(srna, "View2D", "Scroll and zoom for a 2D region");
  RNA_def_struct_sdna(srna, "View2D");

  /* TODO: more View2D properties could be exposed here (read-only). */
  prop = RNA_def_property(srna, "draw_rect", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "tot.xmin");
  RNA_def_property_array(prop, 4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "visible_rect", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "cur.xmin");
  RNA_def_property_array(prop, 4);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  rna_def_view2d_api(srna);
}

static void rna_def_region(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;
  PropertyRNA *parm;

  static const EnumPropertyItem alignment_types[] = {
      {RGN_ALIGN_NONE, "NONE", 0, "None", "Don't use any fixed alignment, fill available space"},
      {RGN_ALIGN_TOP, "TOP", 0, "Top", ""},
      {RGN_ALIGN_BOTTOM, "BOTTOM", 0, "Bottom", ""},
      {RGN_ALIGN_LEFT, "LEFT", 0, "Left", ""},
      {RGN_ALIGN_RIGHT, "RIGHT", 0, "Right", ""},
      {RGN_ALIGN_HSPLIT, "HORIZONTAL_SPLIT", 0, "Horizontal Split", ""},
      {RGN_ALIGN_VSPLIT, "VERTICAL_SPLIT", 0, "Vertical Split", ""},
      {RGN_ALIGN_FLOAT,
       "FLOAT",
       0,
       "Float",
       "Region floats on screen, doesn't use any fixed alignment"},
      {RGN_ALIGN_QSPLIT,
       "QUAD_SPLIT",
       0,
       "Quad Split",
       "Region is split horizontally and vertically"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "Region", NULL);
  RNA_def_struct_ui_text(srna, "Region", "Region in a subdivided screen area");
  RNA_def_struct_sdna(srna, "ARegion");
  RNA_def_struct_refine_func(srna, "rna_Region_refine");
  RNA_def_struct_register_funcs(srna, "rna_Region_register", "rna_Region_unregister", NULL);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "regiontype");
  RNA_def_property_enum_items(prop, rna_enum_region_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Region Type", "Type of this region");

  prop = RNA_def_property(srna, "x", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "winrct.xmin");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "X Position", "The window relative vertical location of the region");

  prop = RNA_def_property(srna, "y", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "winrct.ymin");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Y Position", "The window relative horizontal location of the region");

  prop = RNA_def_property(srna, "width", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "winx");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Width", "Region width");

  prop = RNA_def_property(srna, "height", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "winy");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Height", "Region height");

  prop = RNA_def_property(srna, "view2d", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "v2d");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "View2D", "2D view of the region");

  prop = RNA_def_property(srna, "alignment", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, alignment_types);
  RNA_def_property_enum_funcs(prop, "rna_region_alignment_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Alignment", "Alignment of the region within the area");

  prop = RNA_def_property(srna, "data", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Region Data", "Region specific data (the type depends on the region type)");
  RNA_def_property_struct_type(prop, "AnyType");
  RNA_def_property_pointer_funcs(prop, "rna_Region_data_get", NULL, NULL, NULL);

  RNA_def_function(srna, "tag_redraw", "ED_region_tag_redraw");

  func = RNA_def_function(srna, "default_init", "rna_Region_default_init");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  RNA_def_function_ui_description(func,
                                  "Default initialization function for Regions, this should be "
                                  "called by custom regions in their init() override");
  parm = RNA_def_pointer(func, "window_manager", "WindowManager", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "region", "Region", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "add_keymap", "rna_Region_add_keymap");
  RNA_def_function_ui_description(func, "Add a keymap to this region");
  parm = RNA_def_pointer(func, "keymap", "KeyMap", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw_grid", "ED_region_grid_draw");
  parm = RNA_def_float(
      func, "zoomx", 1.0f, 0.0f, 100.0f, "Zoom X", "Zoom along the X axis", 0.0f, 100.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(
      func, "zoomy", 1.0f, 0.0f, 100.0f, "Zoom Y", "Zoom along the Y axis", 0.0f, 100.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "x",
                       1.0f,
                       0.0f,
                       100.0f,
                       "Center X",
                       "Center of the grid along the X axis",
                       0.0f,
                       100.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_float(func,
                       "y",
                       1.0f,
                       0.0f,
                       100.0f,
                       "Center Y",
                       "Center of the grid along the X axis",
                       0.0f,
                       100.0f);
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "theme_clear_color", "rna_Region_theme_clear_color");
  RNA_def_function_ui_description(func, "Clear the region with its respective theme color");

  static const EnumPropertyItem region_draw_mode_items[] = {
      {REGION_DRAW_POST_PIXEL, "POST_PIXEL", 0, "Post Pixel", ""},
      {REGION_DRAW_POST_VIEW, "POST_VIEW", 0, "Post View", ""},
      {REGION_DRAW_PRE_VIEW, "PRE_VIEW", 0, "Pre View", ""},
      {REGION_DRAW_BACKDROP, "BACKDROP", 0, "Backdrop", ""},
      {0, NULL, 0, NULL, NULL},
  };

  func = RNA_def_function(srna, "draw_handlers", "rna_Region_draw_handlers");
  RNA_def_function_ui_description(
      func, "Call the various handlers set in the space via `draw_handler_add`");
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "draw_mode", region_draw_mode_items, REGION_DRAW_POST_PIXEL, "Draw Mode", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  /* Registration. */

  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->idname");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop, "Space Type", "The space where the region is going to be used in");

  prop = RNA_def_property(srna, "bl_space_type", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "type->space_type");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(
      prop, "Space Type", "The space where the region is going to be used in");

  prop = RNA_def_property(srna, "bl_region_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type->regionid");
  RNA_def_property_enum_default(prop, RGN_TYPE_HEADER);
  RNA_def_property_enum_items(prop, rna_enum_region_type_items);
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_def_property_ui_text(prop, "Region Type", "The type of region");

  func = RNA_def_function(srna, "init", NULL);
  RNA_def_function_ui_description(
      func,
      "Can be used to initialize handlers for this region. If not set, a default function will be "
      "called. If set, make sure to call Region.default_init()");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "window_manager", "WindowManager", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "exit", NULL);
  RNA_def_function_ui_description(func, "Called when the region is hidden or removed");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL | FUNC_ALLOW_WRITE);
  parm = RNA_def_pointer(func, "window_manager", "WindowManager", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);

  func = RNA_def_function(srna, "draw", NULL);
  RNA_def_function_ui_description(func, "Called when the region is hidden or removed");
  RNA_def_function_flag(func, FUNC_REGISTER_OPTIONAL);
  parm = RNA_def_pointer(func, "context", "Context", "", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
}

static void rna_def_screen(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "Screen", "ID");
  RNA_def_struct_sdna(srna, "Screen"); /* it is actually bScreen but for 2.5 the dna is patched! */
  RNA_def_struct_ui_text(
      srna, "Screen", "Screen data-block, defining the layout of areas in a window");
  RNA_def_struct_ui_icon(srna, ICON_WORKSPACE);

  /* collections */
  prop = RNA_def_property(srna, "areas", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "areabase", NULL);
  RNA_def_property_struct_type(prop, "Area");
  RNA_def_property_ui_text(prop, "Areas", "Areas the screen is subdivided into");

  /* readonly status indicators */
  prop = RNA_def_property(srna, "is_animation_playing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Screen_is_animation_playing_get", NULL);
  RNA_def_property_ui_text(prop, "Animation Playing", "Animation playback is active");

  prop = RNA_def_property(srna, "is_scrubbing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Screen_is_scrubbing_get", NULL);
  RNA_def_property_ui_text(
      prop, "User is Scrubbing", "True when the user is scrubbing through time");

  prop = RNA_def_property(srna, "is_temporary", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_sdna(prop, NULL, "temp", 1);
  RNA_def_property_ui_text(prop, "Temporary", "");

  prop = RNA_def_property(srna, "show_fullscreen", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Screen_fullscreen_get", NULL);
  RNA_def_property_ui_text(prop, "Maximize", "An area is maximized, filling this screen");

  /* Status Bar. */

  prop = RNA_def_property(srna, "show_statusbar", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SCREEN_COLLAPSE_STATUSBAR);
  RNA_def_property_ui_text(prop, "Show Status Bar", "Show status bar");
  RNA_def_property_update(prop, 0, "rna_Screen_bar_update");

  func = RNA_def_function(srna, "statusbar_info", "rna_Screen_statusbar_info_get");
  RNA_def_function_flag(func, FUNC_USE_MAIN | FUNC_USE_CONTEXT);
  parm = RNA_def_string(func, "statusbar_info", NULL, 0, "Status Bar Info", "");
  RNA_def_function_return(func, parm);

  /* Define Anim Playback Areas */
  prop = RNA_def_property(srna, "use_play_top_left_3d_editor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_REGION);
  RNA_def_property_ui_text(prop, "Top-Left 3D Editor", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_3d_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_3D_WIN);
  RNA_def_property_ui_text(prop, "All 3D Viewports", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_follow", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_FOLLOW);
  RNA_def_property_ui_text(prop, "Follow", "Follow current frame in editors");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_animation_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_ANIM_WIN);
  RNA_def_property_ui_text(prop, "Animation Editors", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_properties_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_BUTS_WIN);
  RNA_def_property_ui_text(prop, "Property Editors", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_image_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_ALL_IMAGE_WIN);
  RNA_def_property_ui_text(prop, "Image Editors", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_sequence_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_SEQ);
  RNA_def_property_ui_text(prop, "Sequencer Editors", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_node_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_NODES);
  RNA_def_property_ui_text(prop, "Node Editors", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");

  prop = RNA_def_property(srna, "use_play_clip_editors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "redraws_flag", TIME_CLIPS);
  RNA_def_property_ui_text(prop, "Clip Editors", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Screen_redraw_update");
}

void RNA_def_screen(BlenderRNA *brna)
{
  rna_def_screen(brna);
  rna_def_area(brna);
  rna_def_region(brna);
  rna_def_view2d(brna);
}

#endif
