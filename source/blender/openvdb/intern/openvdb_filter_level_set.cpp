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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

extern "C" {
#include "DNA_modifier_types.h"
}

#include <openvdb/tools/LevelSetFilter.h>

#include "openvdb_capi.h"
#include "openvdb_intern.h"

using namespace openvdb;

void OpenVDB_filter_level_set(FloatGrid::Ptr level_set, FloatGrid::Ptr filter_mask,
							  std::vector<LevelSetFilterSettings> &settings)
{
	typedef FloatGrid Mask;
	typedef tools::LevelSetFilter<FloatGrid, Mask> Filter;
	const Mask *mask = filter_mask.get();

	Filter filter(*level_set);

	filter.setTemporalScheme(math::TVD_RK1);

	for (int i = 0; i < settings.size(); ++i) {
		LevelSetFilterSettings &lsfs = settings[i];

		switch (lsfs.accuracy) {
			case MOD_PART_MESH_ACC_FISRT: filter.setSpatialScheme(math::FIRST_BIAS); break;
			case MOD_PART_MESH_ACC_SECOND: filter.setSpatialScheme(math::SECOND_BIAS); break;
			case MOD_PART_MESH_ACC_THIRD: filter.setSpatialScheme(math::THIRD_BIAS); break;
			case MOD_PART_MESH_ACC_WENO5: filter.setSpatialScheme(math::WENO5_BIAS); break;
			case MOD_PART_MESH_ACC_HJWENO5: filter.setSpatialScheme(math::HJWENO5_BIAS); break;
		}

		switch (lsfs.type) {
			case MOD_PART_MESH_MEDIAN:
				for (int i = 0; i < lsfs.iterations; ++i) {
					filter.median(lsfs.width, mask);
				}
				break;
			case MOD_PART_MESH_MEAN:
				for (int i = 0; i < lsfs.iterations; ++i) {
					filter.mean(lsfs.width, mask);
				}
				break;
			case MOD_PART_MESH_GAUSSIAN:
				for (int i = 0; i < lsfs.iterations; ++i) {
					filter.gaussian(lsfs.width, mask);
				}
				break;
			case MOD_PART_MESH_MEAN_CURV:
				for (int i = 0; i < lsfs.iterations; ++i) {
					filter.meanCurvature(mask);
				}
				break;
			case MOD_PART_MESH_LAPLACIAN:
				for (int i = 0; i < lsfs.iterations; ++i) {
					filter.laplacian(mask);
				}
				break;
			case MOD_PART_MESH_OFFSET:
				for (int i = 0; i < lsfs.iterations; ++i) {
					filter.offset(lsfs.offset, mask);
				}
				break;
		}
	}
}
