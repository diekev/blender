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
 * Contributor(s): Kevin Dietrich.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "DNA_modifier_types.h"
#include "DNA_poseidon_types.h"

#include "RNA_define.h"
#include "rna_internal.h"

#include "BKE_poseidon.h"

#ifdef RNA_RUNTIME

static char *rna_PoseidonDomainSettings_path(PointerRNA *ptr)
{
	PoseidonDomainSettings *settings = (PoseidonDomainSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->pmd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].domain_settings", name_esc);
}

static char *rna_PoseidonFlowSettings_path(PointerRNA *ptr)
{
	PoseidonFlowSettings *settings = (PoseidonFlowSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->pmd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].flow_settings", name_esc);
}

static char *rna_PoseidonCollSettings_path(PointerRNA *ptr)
{
	PoseidonCollSettings *settings = (PoseidonCollSettings *)ptr->data;
	ModifierData *md = (ModifierData *)settings->pmd;
	char name_esc[sizeof(md->name) * 2];

	BLI_strescape(name_esc, md->name, sizeof(name_esc));
	return BLI_sprintfN("modifiers[\"%s\"].coll_settings", name_esc);
}

#else

static void rna_def_poseidon_domain_settings(BlenderRNA *brna)
{
	StructRNA *srna = RNA_def_struct(brna, "PoseidonDomainSettings", NULL);
	RNA_def_struct_ui_text(srna, "Domain Settings", "Poseidon domain settings");
	RNA_def_struct_sdna(srna, "PoseidonDomainSettings");
	RNA_def_struct_path_func(srna, "rna_PoseidonDomainSettings_path");

	PropertyRNA *prop = RNA_def_property(srna, "cache", PROP_POINTER, PROP_NONE);
	RNA_def_property_flag(prop, PROP_NEVER_NULL);
	RNA_def_property_pointer_sdna(prop, NULL, "cache");
	RNA_def_property_ui_text(prop, "Point Cache", "");
}

static void rna_def_poseidon_flow_settings(BlenderRNA *brna)
{
	StructRNA *srna = RNA_def_struct(brna, "PoseidonFlowSettings", NULL);
	RNA_def_struct_ui_text(srna, "Flow Settings", "Poseidon flow settings");
	RNA_def_struct_sdna(srna, "PoseidonFlowSettings");
	RNA_def_struct_path_func(srna, "rna_PoseidonFlowSettings_path");
}

static void rna_def_poseidon_coll_settings(BlenderRNA *brna)
{
	StructRNA *srna = RNA_def_struct(brna, "PoseidonCollSettings", NULL);
	RNA_def_struct_ui_text(srna, "Collision Settings", "Poseidon collision settings");
	RNA_def_struct_sdna(srna, "PoseidonCollSettings");
	RNA_def_struct_path_func(srna, "rna_PoseidonCollSettings_path");
}

void RNA_def_poseidon(BlenderRNA *brna)
{
	rna_def_poseidon_domain_settings(brna);
	rna_def_poseidon_flow_settings(brna);
	rna_def_poseidon_coll_settings(brna);
}

#endif
