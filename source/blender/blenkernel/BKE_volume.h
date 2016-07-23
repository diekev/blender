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

/** \file BKE_volume.h
 *  \ingroup bke
 *  \since January 2016
 *  \author Kevin Dietrich
 */

#ifndef __BKE_VOLUME_H__
#define __BKE_VOLUME_H__

struct BoundBox;
struct Main;
struct Object;
struct Scene;
struct Volume;
struct VolumeData;

/* ---------------------- Volume ID handling ---------------------- */

struct Volume *BKE_volume_add(struct Main *bmain, const char *name);

void BKE_volume_make_local(struct Main *bmain, struct Volume *volume, bool lib_local);

struct Volume *BKE_volume_copy(struct Main *bmain, struct Volume *volume);

void BKE_volume_free(struct Volume *volume);

/* Called from the depsgraph (object_update.c). */
void BKE_volume_update(struct Scene *scene, struct Object *ob);

/* ---------------------- Acces Methods ---------------------- */

/**
 * Return a pointer to the volume from this object.
 */
struct Volume *BKE_volume_from_object(struct Object *ob);

/**
 * Return the bounding box of this object.
 */
struct BoundBox *BKE_volume_boundbox_get(struct Object *ob);

/**
 * Return the active volume field.
 */
struct VolumeData *BKE_volume_field_current(const struct Volume *volume);

/* ---------------------- I/O ---------------------- */

/**
 * Unpack volume data from the .blend file, or load them from an external file.
 */
void BKE_volume_load(struct Main *bmain, struct Volume *volume);

/**
 * Load volume data from an external file.
 */
void BKE_volume_load_from_file(struct Volume *volume, const char *filename);

/**
 * Prepare volume data for packing in the .blend file.
 */
void BKE_volume_prepare_write(struct Main *bmain, struct Volume *volume);

/* ---------------------- Conversion ---------------------- */

/**
 * Convert the given mesh object to a volume.
 */
void BKE_mesh_to_volume(struct Main *bmain, struct Scene *scene, struct Object *ob);

#endif /* __BKE_VOLUME_H__ */
