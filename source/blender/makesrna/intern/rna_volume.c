/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_volume_types.h"

#include "BKE_volume.h"

#ifdef RNA_RUNTIME

#include "BLI_listbase.h"
#include "BLI_math.h"

static PointerRNA rna_Volume_active_field_get(PointerRNA *ptr)
{
    Volume *volume = (Volume *)ptr->data;
    VolumeData *field = BKE_volume_field_current(volume);
    return rna_pointer_inherit_refine(ptr, &RNA_VolumeData, field);
}

static void rna_Volume_active_field_index_range(PointerRNA *ptr, int *min, int *max,
                                                int *UNUSED(softmin), int *UNUSED(softmax))
{
    Volume *volume = (Volume *)ptr->data;
    *min = 0;
    *max = max_ii(0, BLI_listbase_count(&volume->fields) - 1);
}

static int rna_Volume_active_field_index_get(PointerRNA *ptr)
{
    Volume *volume = (Volume *)ptr->data;
    VolumeData *field = (VolumeData *)volume->fields.first;
    int i = 0;

    for (; field; field = field->next, i++) {
        if (field->flags & VOLUME_DATA_CURRENT)
            return i;
    }
    return 0;
}

static void rna_Volume_active_field_index_set(struct PointerRNA *ptr, int value)
{
    Volume *volume = (Volume *)ptr->data;
    VolumeData *field = (VolumeData *)volume->fields.first;
    int i = 0;

    for (; field; field = field->next, i++) {
        if (i == value)
            field->flags |= VOLUME_DATA_CURRENT;
        else
            field->flags &= ~VOLUME_DATA_CURRENT;
    }
}

#else

static void rna_def_volume_data(BlenderRNA *brna)
{
    StructRNA *srna = RNA_def_struct(brna, "VolumeData", NULL);
    RNA_def_struct_ui_text(srna, "Volume Data", "Volume Data");

    PropertyRNA *prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
    RNA_def_property_string_sdna(prop, NULL, "name");
    RNA_def_property_ui_text(prop, "Name", "Field name");
    RNA_def_struct_name_property(srna, prop);
}

static void rna_def_volume(BlenderRNA *brna)
{
	StructRNA *srna = RNA_def_struct(brna, "Volume", "ID");
	RNA_def_struct_ui_text(srna, "Volume", "Volume data-block");
	RNA_def_struct_ui_icon(srna, ICON_MOD_SMOKE);
	RNA_def_struct_sdna(srna, "Volume");

	PropertyRNA *prop = RNA_def_property(srna, "fields", PROP_COLLECTION, PROP_NONE);
    RNA_def_property_collection_sdna(prop, NULL, "fields", NULL);
	RNA_def_property_struct_type(prop, "VolumeData");

	prop = RNA_def_property(srna, "active_field", PROP_POINTER, PROP_NONE);
	RNA_def_property_struct_type(prop, "VolumeData");
	RNA_def_property_pointer_funcs(prop, "rna_Volume_active_field_get", NULL, NULL, NULL);
	RNA_def_property_ui_text(prop, "Active Volume field", "");

	prop = RNA_def_property(srna, "active_field_index", PROP_INT, PROP_UNSIGNED);
	RNA_def_property_int_funcs(prop, "rna_Volume_active_field_index_get",
	                           "rna_Volume_active_field_index_set",
	                           "rna_Volume_active_field_index_range");
	RNA_def_property_ui_text(prop, "Active Volume Index", "");

	rna_def_volume_data(brna);
}

void RNA_def_volume(BlenderRNA *brna)
{
	rna_def_volume(brna);
}

#endif
