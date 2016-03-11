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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#pragma once

#include <openvdb/tools/VolumeAdvect.h>

#include "../poseidon_capi.h"

static openvdb::tools::Scheme::SemiLagrangian advection_scheme(int scheme)
{
	switch (scheme) {
		default:
		case ADVECT_SEMI:
			return openvdb::tools::Scheme::SEMI;
		case ADVECT_MID:
			return openvdb::tools::Scheme::MID;
		case ADVECT_RK3:
			return openvdb::tools::Scheme::RK3;
		case ADVECT_RK4:
			return openvdb::tools::Scheme::RK4;
		case ADVECT_MAC:
			return openvdb::tools::Scheme::MAC;
		case ADVECT_BFECC:
			return openvdb::tools::Scheme::BFECC;
	}
}

static openvdb::tools::Scheme::Limiter limiter_scheme(int scheme)
{
	switch (scheme) {
		default:
		case LIMITER_NONE:
			return openvdb::tools::Scheme::NO_LIMITER;
		case LIMITER_CLAMP:
			return openvdb::tools::Scheme::CLAMP;
		case LIMITER_REVERT:
			return openvdb::tools::Scheme::REVERT;
	}
}
