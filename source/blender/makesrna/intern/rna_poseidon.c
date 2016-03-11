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

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_poseidon_types.h"

#include "RNA_define.h"
#include "rna_internal.h"

#include "BKE_pointcache.h"
#include "BKE_poseidon.h"

#include "WM_types.h"

#include "poseidon_capi.h"

#ifdef RNA_RUNTIME

#include "BKE_depsgraph.h"

static void rna_Poseidon_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	UNUSED_VARS(bmain, scene);
}

static void rna_Poseidon_dependency_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	rna_Poseidon_update(bmain, scene, ptr);
	DAG_relations_tag_update(bmain);
}

static void rna_Poseidon_resetCache(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	PoseidonDomainSettings *settings = (PoseidonDomainSettings *)ptr->data;

	if (settings->pmd && settings->pmd->domain) {
		settings->pmd->domain->cache->flag |= PTCACHE_OUTDATED;
	}

	DAG_id_tag_update(ptr->id.data, OB_RECALC_DATA);
	UNUSED_VARS(bmain, scene);
}

static void rna_Poseidon_reset(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	PoseidonDomainSettings *settings = (PoseidonDomainSettings *)ptr->data;

	BKE_poseidon_modifier_reset(settings->pmd);
	rna_Poseidon_resetCache(bmain, scene, ptr);

	rna_Poseidon_update(bmain, scene, ptr);
}

static void rna_Poseidon_reset_dependency(Main *bmain, Scene *scene, PointerRNA *ptr)
{
	PoseidonDomainSettings *settings = (PoseidonDomainSettings *)ptr->data;

	BKE_poseidon_modifier_reset(settings->pmd);

	if (settings->pmd && settings->pmd->domain) {
		settings->pmd->domain->cache->flag |= PTCACHE_OUTDATED;
	}

	rna_Poseidon_dependency_update(bmain, scene, ptr);
}

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

	static EnumPropertyItem advection_scheme_items[] = {
	    { ADVECT_SEMI, "SEMI_LAGRANGIAN", 0, "Semi-Lagrangian", "" },
	    { ADVECT_MID, "MID_POINT", 0, "Mid-Point", "" },
	    { ADVECT_RK3, "RK3", 0, "3rd Order Runge-Kutta", "" },
	    { ADVECT_RK4, "RK4", 0, "4th Order Runge-Kutta", "" },
	    { ADVECT_MAC, "MAC", 0, "MacCormack", "" },
	    { ADVECT_BFECC, "BFECC", 0, "BEFCC", "Back and Forth Error Compensation" },
	    { 0, NULL, 0, NULL, NULL },
	};

	prop = RNA_def_property(srna, "advection_scheme", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "advection");
	RNA_def_property_enum_items(prop, advection_scheme_items);
	RNA_def_property_ui_text(prop, "Advection Scheme", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	static EnumPropertyItem limiter_scheme_items[] = {
	    { LIMITER_NONE, "NONE", 0, "None", "" },
	    { LIMITER_CLAMP, "CLAMP", 0, "Clamp", "" },
	    { LIMITER_REVERT, "REVERT", 0, "Revert to First Order", "" },
	    { 0, NULL, 0, NULL, NULL },
	};

	prop = RNA_def_property(srna, "limiter_scheme", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "limiter");
	RNA_def_property_enum_items(prop, limiter_scheme_items);
	RNA_def_property_ui_text(prop, "Limiter Scheme", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	static EnumPropertyItem point_advect_items[] = {
	    { INTEGR_RK1, "RK1", 0, "Forward Euler", "" },
	    { INTEGR_RK2, "RK2", 0, "2nd Order Runge-Kutta", "" },
	    { INTEGR_RK3, "RK3", 0, "3rd Order Runge-Kutta", "" },
	    { INTEGR_RK4, "RK4", 0, "4th Order Runge-Kutta", "" },
	    { 0, NULL, 0, NULL, NULL },
	};

	prop = RNA_def_property(srna, "point_advect", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "point_advect");
	RNA_def_property_enum_items(prop, point_advect_items);
	RNA_def_property_ui_text(prop, "Integration:", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	static EnumPropertyItem fluid_type_items[] = {
	    { MOD_POSEIDON_TYPE_GAS, "GAS", 0, "Gas", "Simulate Gaseous Fluids" },
	    { MOD_POSEIDON_TYPE_LIQUID, "LIQUID", 0, "Liquid", "Simulate Liquid Fluids" },
	    { 0, NULL, 0, NULL, NULL },
	};

	prop = RNA_def_property(srna, "fluid_type", PROP_ENUM, PROP_NONE);
	RNA_def_property_enum_sdna(prop, NULL, "fluid_type");
	RNA_def_property_enum_items(prop, fluid_type_items);
	RNA_def_property_ui_text(prop, "Fluid Type", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "voxel_size", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "voxel_size");
	RNA_def_property_range(prop, 0.0, 10.0);
	RNA_def_property_ui_range(prop, 0.0, 10.0, 0.02, 3);
	RNA_def_property_ui_text(prop, "Voxel Size", "Size of the voxels");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, "rna_Poseidon_reset");

	prop = RNA_def_property(srna, "flip_ratio", PROP_FLOAT, PROP_NONE);
	RNA_def_property_float_sdna(prop, NULL, "flip_ratio");
	RNA_def_property_range(prop, 0.0, 1.0);
	RNA_def_property_ui_range(prop, 0.0, 1.0, 0.02, 3);
	RNA_def_property_ui_text(prop, "Flip Ratio", "");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);

	prop = RNA_def_property(srna, "part_per_cell", PROP_INT, PROP_NONE);
	RNA_def_property_int_sdna(prop, NULL, "part_per_cell");
	RNA_def_property_range(prop, 3.0, 12.0);
	RNA_def_property_ui_text(prop, "Particles Count", "Number of Particles Per Voxel");
	RNA_def_property_update(prop, NC_OBJECT | ND_MODIFIER, NULL);
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
