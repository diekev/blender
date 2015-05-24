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

#ifndef __PARTICLELIST_H__
#define __PARTICLELIST_H__

#include <openvdb/openvdb.h>

using namespace openvdb;

/* This class implements the interface needed by tools::ParticlesToLevelSet.
 * It is based on the one found in openvdb/unittest/TestParticlesToLevelSet.cc.
 */
class ParticleList {
protected:
	struct Particle {
		Vec3R pos, vel;
		Real rad;
	};

	Real m_radius_scale;
	Real m_velocity_scale;
	std::vector<Particle> m_particle_list;
	bool m_has_radius, m_has_velocity;

public:
	/* Required for bucketing */
	typedef Vec3R value_type;

	ParticleList(size_t size, Real rad_scale = 1.0, Real vel_scale = 1.0)
		: m_radius_scale(rad_scale)
		, m_velocity_scale(vel_scale)
	{
		m_has_radius = false;
		m_has_velocity = false;
		m_particle_list.reserve(size);
	}

	void add(const Vec3R &p, const Real &r, const Vec3R &v)
	{
		Particle pa;
		pa.pos = p;
		pa.rad = r;
		pa.vel = v;
		m_particle_list.push_back(pa);
	}

	Real &radius_scale()
	{
		return m_radius_scale;
	}

	const Real &radius_scale() const
	{
		return m_radius_scale;
	}

	Real &velocity_scale()
	{
		return m_velocity_scale;
	}

	bool has_radius() const
	{
		return m_has_radius;
	}

	bool has_velocity() const
	{
		return m_has_velocity;
	}

	void has_radius(const bool b)
	{
		m_has_radius = b;
	}

	void has_velocity(const bool b)
	{
		m_has_velocity = b;
	}

	/*! \return coordinate bbox in the space of the specified transfrom */
	CoordBBox getBBox(const GridBase &grid)
	{
		CoordBBox bbox;
		Coord &min = bbox.min(), &max = bbox.max();

		for (int n = 0, e = this->size(); n < e; ++n) {
			const Vec3d xyz = grid.worldToIndex(this->pos(n));
			const Real r = this->radius(n) / grid.voxelSize()[0];

			for (int i = 0; i < 3; ++i) {
				min[i] = math::Min(min[i], math::Floor(xyz[i] - r));
				max[i] = math::Max(max[i], math::Ceil(xyz[i] + r));
			}
		}

		return bbox;
	}

	Vec3R pos(int n) const
	{
		return m_particle_list[n].pos;
	}

	Vec3R vel(int n) const
	{
		return m_velocity_scale * m_particle_list[n].vel;
	}

	Real radius(int n) const
	{
		return m_radius_scale * m_particle_list[n].rad;
	}

	/* The methods below are the only ones required by tools::ParticleToLevelSet
	 * We return by value since the radius and velocities are modified by the
	 * scaling factors!
	 * Also these methods are all assumed to be thread-safe.
	 */

	/* Return the total number of particles in list.
	 * Always required!
	 */
	int size() const
	{
		return m_particle_list.size();
	}

	/* Get the world space position of n'th particle.
	 * Required by ParticlesToLevelSet::rasterizeSphere(*this, radius).
	 */
	void getPos(size_t n, Vec3R &pos) const
	{
		pos = m_particle_list[n].pos;
	}

	void getPosRad(size_t n, Vec3R &pos, Real &rad) const
	{
		pos = m_particle_list[n].pos;
		rad = m_radius_scale * m_particle_list[n].rad;
	}

	void getPosRadVel(size_t n,  Vec3R &pos, Real &rad, Vec3R &vel) const
	{
		pos = m_particle_list[n].pos;
		rad = m_radius_scale * m_particle_list[n].rad;
		vel = m_velocity_scale * m_particle_list[n].vel;
	}

	/* The method below is only required for attribute transfer. */
	void getAtt(size_t n, Index32 &att) const { att = Index32(n); }
};

#endif // __PARTICLELIST_H__
