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
#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_lattice.h"
#include "BKE_particle.h"
#include "BKE_scene.h"

#include "DNA_modifier_types.h"
#include "DNA_scene_types.h"
}

#include <openvdb/tools/LevelSetUtil.h>
#include <openvdb/tools/ParticlesToLevelSet.h>

#include "openvdb_intern.h"
#include "particlelist.h"

ParticleList create_particle_list(Scene *scene, Object *ob, ParticleSystem *psys,
								  math::Transform::Ptr transform,
								  float radius_scale, float velocity_scale)
{
	ParticleList part_list(psys->totpart, radius_scale, velocity_scale);
	int valid_particles = 0;

	if (psys) {
		ParticleSimulationData sim;
		ParticleKey state;

		if (psys->part->size > 0.0f) {
			part_list.has_radius(true);
		}

		/* TODO(kevin): this isn't right at all */
		if (psys->part->normfac > 0.0f) {
			part_list.has_velocity(true);
		}

		sim.scene = scene;
		sim.ob = ob;
		sim.psys = psys;

		psys->lattice_deform_data = psys_create_lattice_deform_data(&sim);

		for (int p = 0; p < psys->totpart; p++) {
			if (psys->particles[p].flag & (PARS_NO_DISP | PARS_UNEXIST)) {
				continue;
			}

			state.time = BKE_scene_frame_get(scene);

			if (psys_get_particle_state(&sim, p, &state, 0) == 0) {
				continue;
			}

			float pos[3], vel[3];

			/* location */
			copy_v3_v3(pos, state.co);

			/* velocity */
			sub_v3_v3v3(vel, state.co, psys->particles[p].prev_state.co);

			Vec3f npos(pos[0], pos[1], pos[2]);
			npos = transform->worldToIndex(npos);

			Vec3f nvel(vel[0], vel[1], vel[2]);

			part_list.add(npos,
						  psys->particles[p].size * part_list.radius_scale(),
						  nvel * psys->part->normfac * part_list.velocity_scale());

			valid_particles++;
		}

		if (psys->lattice_deform_data) {
			end_latt_deform(psys->lattice_deform_data);
			psys->lattice_deform_data = NULL;
		}

		BLI_assert(valid_particles == part_list.size());
	}

	return part_list;
}

static void convert_to_levelset(ParticleList part_list, FloatGrid::Ptr grid,
								const ParticlesToLevelSetSettings &settings)
{
	/* Note: the second template argument here is the particles' attributes type,
	 * if any. As this function will later call ParticleList::getAtt(index, attribute),
	 * we pass void for two reasons: first, no attributes are defined for the
	 * particles (yet), and second, disable using attributes for generating
	 * the level set.
	 *
	 * TODO(kevin): quite useless to know that if we don't have the third argument...
	 */
	tools::ParticlesToLevelSet<FloatGrid, void> raster(*grid);
	/* a grain size of zero disables threading */
	raster.setGrainSize(1);
	raster.setRmin(settings.min_part_radius);
	raster.setRmax(1e15f);

	if (settings.generate_trails && part_list.has_velocity()) {
		raster.rasterizeTrails(part_list, settings.trail_size);
	}
	else {
		raster.rasterizeSpheres(part_list);
	}

	if (raster.ignoredParticles()) {
		if (raster.getMinCount() > 0) {
			std::cout << "Minimun voxel radius is too high!\n";
			std::cout << raster.getMinCount() << " particles are ignored!\n";
		}
		if (raster.getMaxCount() > 0) {
			std::cout << "Maximum voxel radius is too low!\n";
			std::cout << raster.getMaxCount() << " particles are ignored!\n";
		}
	}
}

void OpenVDB_from_particles(FloatGrid::Ptr level_set, FloatGrid::Ptr mask_grid,
							const ParticlesToLevelSetSettings &settings, ParticleList Pa)
{
	FloatGrid::Ptr mask_grid_min;
	math::Transform::Ptr transform = math::Transform::createLinearTransform(settings.voxel_size);
	const float background = settings.voxel_size * settings.half_width;

	level_set->setTransform(transform);

	convert_to_levelset(Pa, level_set, settings);

	if (settings.generate_mask) {
		if (!mask_grid) {
			mask_grid = FloatGrid::create(background);
			mask_grid->setGridClass(GRID_LEVEL_SET);
		}
		if (settings.mask_width > 0.0f) {
			std::cout << "Generating mask from level set\n";
			mask_grid->setTransform(transform->copy());
			Pa.radius_scale() *= (1.0f + settings.mask_width);
			convert_to_levelset(Pa, mask_grid, settings);

			if (settings.mask_width < 1.0f) {
				mask_grid_min = FloatGrid::create(background);
				mask_grid_min->setGridClass(GRID_LEVEL_SET);
				mask_grid_min->setTransform(transform->copy());
				Pa.radius_scale() *= (1.0f - settings.mask_width) / (1.0f + settings.mask_width);
				convert_to_levelset(Pa, mask_grid_min, settings);

				tools::csgDifference(*mask_grid, *mask_grid_min);
			}
		}

		tools::sdfToFogVolume(*mask_grid);
	}
}
