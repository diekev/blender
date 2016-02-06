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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): ruesp83.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
 #ifndef __BKE_LAYER_H__
#define __BKE_LAYER_H__

/** \file BKE_image_layer.h
 *  \ingroup bke
 *  \since March 2012
 *  \author ruesp83
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Image;
struct ImageLayer;
struct ImBuf;

/* call from library */

struct ImageLayer *BKE_image_layer_new(struct Image *ima, const char *name);

/* Removes all image layers from the image "ima" */
void BKE_image_free_layers(struct Image *ima);
 
/* Frees an image layer and associated memory */
void BKE_image_layer_free(struct ImageLayer *layer);
 
/* Removes the currently selected image layer */
int BKE_image_layer_remove(struct Image *ima, const int action);

/* Removes the currently selected image layer */
struct ImageLayer *BKE_image_layer_duplicate_current(struct Image *ima);
struct ImageLayer *BKE_image_layer_duplicate(struct Image *ima, struct ImageLayer *layer);

/* Adds another image layer and selects it */
struct ImageLayer *BKE_image_add_image_layer(struct Image *ima, const char *name, int depth, float color[4], int order);

/* Adds the base layer of images that points Image->ibufs.first */
void BKE_image_add_image_layer_base(struct Image *ima);
 
/* Returns the index of the currently selected image layer */
short BKE_image_get_current_layer_index(struct Image *ima);
 
/* Selects the image layer with the number specified in "value" */
void BKE_image_set_current_layer(struct Image *ima, short value);
 
/* Returns the image layer that is currently selected */
struct ImageLayer *BKE_image_get_current_layer(struct Image *ima);

int BKE_image_layer_is_locked(struct Image *ima);
 
/* Fills the current selected image layer with the color given */
void BKE_image_layer_color_fill(struct Image *ima, float color[4]);

void BKE_image_layer_blend(struct ImBuf *dest, struct ImBuf *base, struct ImBuf *layer, float opacity, short mode, short background);

struct ImageLayer *BKE_image_layer_merge(struct Image *ima, struct ImageLayer *iml, struct ImageLayer *iml_next);

void BKE_image_merge_visible_layers(struct Image *ima);

void BKE_image_layer_get_background_color(float col[4], struct ImageLayer *layer);

#ifdef __cplusplus
}
#endif

#endif

