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

/** \file BKE_armature.h
 *  \ingroup bke
 *  \since January 2016
 *  \author Kevin Dietrich
 */

#ifndef __BKE_VOLUME_H__
#define __BKE_VOLUME_H__

struct Main;
struct Object;
struct Volume;
struct VolumeData;

struct Volume *BKE_volume_add(struct Main *bmain, const char *name);
struct Volume *BKE_volume_from_object(struct Object *ob);

void BKE_volume_free(struct Volume *volume);

struct VolumeData *BKE_volume_field_current(struct Volume *volume);

#endif /* __BKE_VOLUME_H__ */
