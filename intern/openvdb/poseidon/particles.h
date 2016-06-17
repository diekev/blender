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
 * The Original Code is Copyright (C) 2015-2016 Kevin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#pragma once

#include <openvdb/openvdb.h>
#include <openvdb/tools/PointIndexGrid.h>

typedef openvdb::tools::PointIndexGrid PointIndexGrid;

struct Particle {
	openvdb::math::Vec3s vel;
	openvdb::math::Vec3s pos;
};

/**
 * This ParticleList class is meant to implement the interfaces required by the
 * following OpenVDB tools:
 *     - openvdb::tools::PointAdvect
 *     - openvdb::tools::PointIndexGrid
 *     - openvdb::tools::PointScatter
 *
 * TODO: separate particles' properties in different vectors to improve cache
 *       coherency?
 */
class ParticleList {
	std::vector<Particle> m_particles;

public:
	/* Required for bucketing */
	typedef openvdb::math::Vec3s value_type;
	typedef openvdb::math::Vec3s PosType;

	ParticleList() = default;

	~ParticleList()
	{
		m_particles.clear();
	}

	/**
	 * Return the size of the particles array. Always required!
	 */
	size_t size() const
	{
		return m_particles.size();
	}

	void reserve(const size_t num)
	{
		m_particles.reserve(num);
	}

	/**
	 * Return the particle at the n'th position in the array.
	 */
	Particle *at(const size_t n)
	{
		return &(m_particles[n]);
	}

	const Particle *at(const size_t n) const
	{
		return &(m_particles[n]);
	}

	/* Required methods. */

	/**
	 * Add a particle at the given pos.
	 */
	void add(const value_type &pos)
	{
		Particle part;
		part.pos = pos;
		part.vel = value_type(0.0f);

		m_particles.push_back(part);
	}

	/**
	 * Get the world space position of the n'th particle.
	 */
	void getPos(const size_t n, value_type &pos) const
	{
		pos = m_particles[n].pos;
	}

	value_type &operator[](const size_t n)
	{
		return m_particles[n].pos;
	}
};

/* Scatters particles in the interior of a level set */
void create_particles(openvdb::FloatGrid::Ptr level_set,
                      ParticleList &particles,
                      const int part_per_cell);

/* Transfer velocity values from the particles to the grid */
void rasterize_particles(ParticleList &particles,
                         const PointIndexGrid &index_grid,
                         openvdb::Vec3SGrid &velocity);

void advect_particles(ParticleList &particles,
                      openvdb::Vec3SGrid::Ptr velocity,
                      const float dt,
                      const int order);

void resample_particles(ParticleList &particles);

void interpolate_pic_flip(openvdb::Vec3SGrid::Ptr &velocity,
                          openvdb::Vec3SGrid::Ptr &velocity_old,
                          ParticleList &particles,
                          const float flip_ratio);
