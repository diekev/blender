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
 * Contributor(s): Konrad Kleine,
 *				   Fabio Russo,
 *				   Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/layer.c
 *  \ingroup bke
 */
 
#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_utildefines.h"

#include "BKE_image.h"
#include "BKE_layer.h"
 
ImageLayer *image_layer_new(Image *ima, const char *name)
{
	ImageLayer *layer = MEM_callocN(sizeof(ImageLayer), "image_layer");

	if (!layer) {
		return NULL;
	}

	strcpy(layer->name, name);
	imagelayer_unique_name(layer, ima);
	layer->next = layer->prev = NULL;

	layer->background = IMA_LAYER_BG_ALPHA;
	layer->opacity = 1.0f;
	layer->mode = IMA_LAYER_NORMAL;
	layer->type = IMA_LAYER_LAYER;
	layer->visible = IMA_LAYER_VISIBLE;
	layer->select = IMA_LAYER_SEL_CURRENT;
	layer->locked = 0;
	zero_v4(layer->default_color);

	return layer;
}

void image_layer_free(ImageLayer *layer)
{
	ImBuf *ibuf;
 
	if (!layer)
		return;

	while ((ibuf = (ImBuf *)layer->ibufs.first)) {
		BLI_remlink(&layer->ibufs, ibuf);
 
		if (ibuf->userdata) {
			MEM_freeN(ibuf->userdata);
			ibuf->userdata = NULL;
		}

		IMB_freeImBuf(ibuf);
	}

	if (layer->preview_ibuf) {
		IMB_freeImBuf(layer->preview_ibuf);
		layer->preview_ibuf = NULL;
	}

	MEM_freeN(layer);
}

void image_free_image_layers(struct Image *ima)
{
	ImageLayer *layer;
 
	if (ima->imlayers.first == NULL)
		return;

	while ((layer = (ImageLayer *)ima->imlayers.first)) {
		BLI_remlink(&ima->imlayers, layer);
		image_layer_free(layer);
	}

	ima->num_layers = 0;
}

ImageLayer *imalayer_get_current(Image *ima)
{
	ImageLayer *layer;
	
	if (ima == NULL)
		return NULL;
 
	for (layer = ima->imlayers.last; layer; layer = layer->prev) {
		if (layer->select & IMA_LAYER_SEL_CURRENT)
			return layer;
	}
 
	return NULL;
}

short image_get_current_layer_index(Image *ima)
{
	if (ima == NULL)
		return 0;
 
	return ima->active_layer;
}
 
void imalayer_set_current_act(Image *ima, short index)
{
	ImageLayer *layer;
	short i;
 
	if (ima == NULL)
		return;
	
	for (layer = ima->imlayers.last, i = BLI_listbase_count(&ima->imlayers) - 1; layer; layer = layer->prev, i--) {
		if (i == index) {
			layer->select = IMA_LAYER_SEL_CURRENT;
			ima->active_layer = i;
		}
		else
			layer->select = !IMA_LAYER_SEL_CURRENT;
	}
}

int imalayer_is_locked(Image *ima)
{
	ImageLayer *layer= NULL;

	layer = imalayer_get_current(ima);

	if (layer)
		return layer->locked;

	return 0;
}

void imalayer_fill_color(Image *ima, float color[4])
{
	ImBuf *ibuf = NULL;
	unsigned char *rect = NULL;
	float *rect_float = NULL;
	void *lock;
 
	if (ima == NULL)
		return;
	
	ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock, IMA_IBUF_LAYER);

	if (ibuf) {
		if (ibuf->flags & IB_rectfloat)
			rect_float = (float*)ibuf->rect_float;
		else
			rect = (unsigned char*)ibuf->rect;
 
		BKE_image_buf_fill_color(rect, rect_float, ibuf->x, ibuf->y, color);
	}
 
	BKE_image_release_ibuf(ima, ibuf, lock);
}

ImageLayer *image_duplicate_current_image_layer(Image *ima)
{
	ImageLayer *layer = NULL, *dup_layer = NULL;
	char dup_name[sizeof(layer->name)];
	ImBuf *ibuf, *new_ibuf;
	void *lock;
 
	if (ima == NULL)
		return NULL;
 
	layer = imalayer_get_current(ima);

	if (!strstr(layer->name, "_copy")) {
		BLI_snprintf(dup_name, sizeof(dup_name), "%s_copy", layer->name);
	}
	else {
		BLI_snprintf(dup_name, sizeof(dup_name), "%s", layer->name);
	}

	dup_layer = image_layer_new(ima, dup_name);

	if (dup_layer) {
		ibuf = BKE_image_acquire_ibuf(ima, NULL, &lock, IMA_IBUF_LAYER);

		if (ibuf) {
			new_ibuf = IMB_dupImBuf(ibuf);
			BLI_addtail(&dup_layer->ibufs, new_ibuf);

			dup_layer->next = dup_layer->prev = NULL;

			if (layer) {
				BLI_insertlinkbefore(&ima->imlayers, layer, dup_layer);
			}
			else {
				BLI_addhead(&ima->imlayers, layer);
			}

			imalayer_set_current_act(ima, image_get_current_layer_index(ima));

			BLI_strncpy(dup_layer->file_path, layer->file_path, sizeof(layer->file_path));
			dup_layer->opacity = layer->opacity;
			dup_layer->background = layer->background;
			dup_layer->mode = layer->mode;
			dup_layer->type = IMA_LAYER_LAYER;
			dup_layer->visible = layer->visible;
			dup_layer->locked = layer->locked;
			copy_v4_v4(dup_layer->default_color, layer->default_color);
		}

		BKE_image_release_ibuf(ima, ibuf, lock);
		ima->num_layers += 1;
	}
	return dup_layer;
}

ImageLayer *image_duplicate_layer(Image *ima, ImageLayer *layer)
{
	ImageLayer *new_layer = NULL;
	ImBuf *ibuf, *new_ibuf;
 
	if (ima == NULL)
		return NULL;

	new_layer = image_layer_new(ima, layer->name);
	if (new_layer) {
		ibuf = (ImBuf *)layer->ibufs.first;
		if (ibuf) {
			new_ibuf = IMB_dupImBuf(ibuf);
			BLI_addtail(&new_layer->ibufs, new_ibuf);

			new_layer->next = new_layer->prev = NULL;

			BLI_strncpy(new_layer->name, layer->name, sizeof(layer->name));
			BLI_strncpy(new_layer->file_path, layer->file_path, sizeof(layer->file_path));
			new_layer->opacity = layer->opacity;
			new_layer->background = layer->background;
			new_layer->mode = layer->mode;
			new_layer->type = layer->type;
			new_layer->visible = layer->visible;
			new_layer->select = layer->select;
			new_layer->locked = layer->locked;
			copy_v4_v4(new_layer->default_color, layer->default_color);
		}
	}
	return new_layer;
}
 
int image_remove_layer(Image *ima, const int action)
{
	ImageLayer *layer= NULL;
	 
	if (ima == NULL)
		return false;

	if (action & IMA_LAYER_DEL_SELECTED) {
		if (ima->num_layers > 1) {
			layer = imalayer_get_current(ima);
			if (layer) {
				BLI_remlink(&ima->imlayers, layer);
				image_layer_free(layer);
			}
			/* Ensure the first element in list gets selected (if any) */
			if (ima->imlayers.first) {
				if (image_get_current_layer_index(ima) != 1)
					imalayer_set_current_act(ima, image_get_current_layer_index(ima));
				else
					imalayer_set_current_act(ima, image_get_current_layer_index(ima) - 1);
			}
			ima->num_layers -= 1;
		}
		else
			return -1;
	}
	else {
		if (ima->num_layers > 1) {
			for (layer = ima->imlayers.last; layer; layer = layer->prev) {
				if (!(layer->visible & IMA_LAYER_VISIBLE)) {
					BLI_remlink(&ima->imlayers, layer);
					image_layer_free(layer);
					if (ima->imlayers.first) {
						if (image_get_current_layer_index(ima) != 1)
							imalayer_set_current_act(ima, image_get_current_layer_index(ima));
						else
							imalayer_set_current_act(ima, image_get_current_layer_index(ima) - 1);
					}
					ima->num_layers -= 1;
				}
			}
		}
		else
			return -1;
	}
	return true;
}

static float blend_normal(float B, float L, float O)
{
	return (O * (L) + (1.0f - O) * B);
}

static float blend_lighten(const float B, const float L, float O)
{
	return (O * ((L > B) ? L : B) + (1.0f - O) * B);
}

static float blend_darken(const float B, const float L, float O)
{
	return (O * ((L > B) ? B : L) + (1.0f - O) * B);
}

static float blend_multiply(const float B, const float L, float O)
{
	return (O * ((B * L) / 1.0f) + (1.0f - O) * B);
}

static float blend_average(const float B, const float L, float O)
{
	return (O * ((B + L) / 2) + (1.0f - O) * B);
}

static float blend_add(const float B, const float L, float O)
{
	return (O * (MIN2(1.0f, (B + L))) + (1.0f - O) * B);
}

static float blend_subtract(const float B, const float L, float O)
{
	return (O * ((B + L < 1.0f) ? 0 : (B + L - 1.0f)) + (1.0f - O) * B);
}

static float blend_difference(const float B, const float L, float O)
{
	return (O * (abs(B - L)) + (1.0f - O) * B);
}

static float blend_negation(const float B, const float L, float O)
{
	return (O * (1.0f - abs(1.0f - B - L)) + (1.0f - O) * B);
}

static float blend_screen(const float B, const float L, float O)
{
	return (O * (1.0f - ((1.0f - B) * (1 - L))) + (1.0f - O) * B);
}

static float blend_exclusion(const float B, const float L, float O)
{
	return (O * (B + L - 2 * B * L) + (1.0f - O) * B);
}

static float blend_overlay(const float B, const float L, float O)
{
	return (O * ((L < 0.5f) ? (2 * B * L) : (1.0f - 2 * (1.0f - B) * (1.0f - L))) + (1.0f - O) * B);
}

static float blend_soft_light(const float B, const float L, float O)
{
	return (O * ((1.0f - B) * blend_multiply(B, L, O) + B * blend_screen(B, L, O)) + (1.0f - O) * B);
}

static float blend_hard_light(const float B, const float L, float O)
{
	return (O * ((B < 0.5f) ? (2 * L * B) : (1.0f - 2 * (1.0f - L) * (1.0f - B))) + (1.0f - O) * B);
}

static float blend_color_dodge(const float B, const float L, float O)
{
	return (O * (B / (1.0f - L)) + (1.0f - O) * B);
}

static float blend_color_burn(const float B, const float L, float O)
{
	return (O * (1.0f - ((1.0f - B) / L)) + (1.0f - O) * B);
}

static float blend_inverse_color_burn(const float B, const float L, float O)
{
	return (O * (1.0f - ((1.0f - L) / B)) + (1.0f - O) * B);
}

static float blend_soft_burn(const float B, const float L, float O)
{
	float r;

	if (B + L < 1.0f) {
		r = (O * ((0.5f * L) / (1.0f - B)) + (1.0f - O) * B);
	}
	else {
		r = (O * (1.0f - (0.5f * (1.0f - B)) / L) + (1.0f - O) * B);
	}
	return r;
}

static float blend_linear_dodge(const float B, const float L, float O)
{
	return (O * (min_ff(1.0f, (B + L))) + (1.0f - O) * B);
}

static float blend_linear_burn(const float B, const float L, float O)
{
	return (O * ((B + L < 1.0f) ? 0 : (B + L - 1.0f)) + (1.0f - O) * B);
}

static float blend_linear_light(const float B, const float L, float O)
{
	return (O * ((2 * L) < 0.5f) ? ((B + (2 * L) < 1.0f) ? 0 : (B + (2 * L) - 1.0f)) : (min_ff(1.0f, (B + (2 * (L - 0.5f))))) + (1.0f - O) * B);
}

static float blend_vivid_light(const float B, const float L, float O)
{
	float r;

	if (B < 0.5f) {
		r = blend_color_burn(B * 2, L, O);
	}
	else {
		r = blend_color_dodge((2 * (B - 0.5f)), L, O);
	}
	return (O * r + (1.0f - O) * B);
}

static float blend_pin_light(const float B, const float L, float O)
{
	return (O * (L < 0.5f) ? (((2 * L) > B) ? B : (2 * L)) : (((2 * (L - 0.5f)) > B) ? (2 * (L - 0.5f)) : B) + (1.0f - O) * B);
}

static float blend_hard_mix(const float B, const float L, float O)
{
	return (O * ((blend_vivid_light(B, L, O) < 0.5f) ? 0 : 1.0f) + (1.0f - O) * B);
}

static void copy_co(int rect, float *fp, char *cp, float co)
{
	/* rect_float */
	if (rect == 0)
		*fp = co;
				
	/* rect */
	if (rect == 1)
		*cp = FTOCHAR(co);
}

ImBuf *imalayer_blend(ImBuf *base, ImBuf *layer, float opacity, short mode, short background, bool *has_realloc)
{
	ImBuf *dest;
	int i, flag;
	int bg_x, bg_y, diff_x;
	float (*blend_callback)(float B, float L, float O) = NULL;	//Mode callback

	float as, ab, ao, co, aoco;
	float *fp_b, *fp_l, *fp_d;
	char *cp_b, *cp_l, *cp_d;
	float f_br, f_bg, f_bb, f_ba;
	float f_lr, f_lg, f_lb, f_la;
	//clock_t start,end;
	//double tempo;
	//start=clock();

	flag = 0;

	if (!base) {
		return layer; // IMB_dupImBuf(layer);
	}

	if (opacity == 0.0f)
		return base;

	// XXX(kevin) - figure that whole memory duplication nonsense.
	if (has_realloc) {
		*has_realloc = true;
	}

	dest = IMB_dupImBuf(base);

	bg_x = base->x;
	bg_y = base->y;
	if (base->x > layer->x)
		bg_x = layer->x;
	if (base->y > layer->y)
		bg_y = layer->y;

	diff_x = abs(base->x - layer->x);

	switch (mode) {
		case IMA_LAYER_NORMAL:
			blend_callback = blend_normal;
			break;
		case IMA_LAYER_MULTIPLY:
			blend_callback = blend_multiply;
			break;
		case IMA_LAYER_SCREEN:
			blend_callback = blend_screen;
			break;
		case IMA_LAYER_OVERLAY:
			blend_callback = blend_overlay;
			break;
		case IMA_LAYER_SOFT_LIGHT:
			blend_callback = blend_soft_light;
			break;
		case IMA_LAYER_HARD_LIGHT:
			blend_callback = blend_hard_light;
			break;
		case IMA_LAYER_COLOR_DODGE:
			blend_callback = blend_color_dodge;
			break;
		case IMA_LAYER_LINEAR_DODGE:
			blend_callback = blend_linear_dodge;
			break;
		case IMA_LAYER_COLOR_BURN:
			blend_callback = blend_color_burn;
			break;
		case IMA_LAYER_LINEAR_BURN:
			blend_callback = blend_linear_burn;
			break;
		case IMA_LAYER_AVERAGE:
			blend_callback = blend_average;
			break;
		case IMA_LAYER_ADD:
			blend_callback = blend_add;
			break;
		case IMA_LAYER_SUBTRACT:
			blend_callback = blend_subtract;
			break;
		case IMA_LAYER_DIFFERENCE:
			blend_callback = blend_difference;
			break;
		case IMA_LAYER_LIGHTEN:
			blend_callback = blend_lighten;
			break;
		case IMA_LAYER_DARKEN:
			blend_callback = blend_darken;
			break;
		case IMA_LAYER_NEGATION:
			blend_callback = blend_negation;
			break;
		case IMA_LAYER_EXCLUSION:
			blend_callback = blend_exclusion;
			break;
		case IMA_LAYER_LINEAR_LIGHT:
			blend_callback = blend_linear_light;
			break;
		case IMA_LAYER_VIVID_LIGHT:
			blend_callback = blend_vivid_light;
			break;
		case IMA_LAYER_PIN_LIGHT:
			blend_callback = blend_pin_light;
			break;
		case IMA_LAYER_HARD_MIX:
			blend_callback = blend_hard_mix;
			break;
		case IMA_LAYER_INVERSE_COLOR_BURN:
			blend_callback = blend_inverse_color_burn;
			break;
		case IMA_LAYER_SOFT_BURN:
			blend_callback = blend_soft_burn;
			break;
	}

	/* 
	* Ao*Co = As * (1 - Ab) * Cs + As * Ab * B(Cb, Cs) + (1 - As) * Ab * Cb
	* Ao = As + Ab * (1 - As)
	* Co = Co / Ao
	*/

	fp_b = NULL;
	fp_l = fp_d = fp_b;
	cp_b = NULL;
	cp_l = cp_d = cp_b;

	if (base->rect_float) {
		flag = 0;
		fp_b = (float *) base->rect_float;
		fp_l = (float *) layer->rect_float;
		fp_d = (float *) dest->rect_float;
	}

	if (base->rect) {
		flag = 1;
		cp_b = (char *) base->rect;
		cp_l = (char *) layer->rect;
		cp_d = (char *) dest->rect;
	}

	for (i = bg_x * bg_y; i > 0; i--) {
		if (base->rect_float) {
			f_ba = fp_b[3];
			f_la = fp_l[3];
		}

		if (base->rect) {
			f_ba = ((float)cp_b[3]) / 255.0f;
			f_la = ((float)cp_l[3]) / 255.0f;
		}

		if (((background & IMA_LAYER_BG_ALPHA) && ((f_la != 0.0f) || (f_ba != 0.0f))) ||
		   ((!(background & IMA_LAYER_BG_ALPHA)) && ((f_la != 0.0f) && (f_ba != 0.0f)))) {
		//if ((f_la != 0.0f) && (f_ba != 0.0f)) {
			if (base->rect_float) {
				f_br = fp_b[0];
				f_bg = fp_b[1];
				f_bb = fp_b[2];
			
				f_lr = fp_l[0];
				f_lg = fp_l[1];
				f_lb = fp_l[2];
			}

			if (base->rect) {
				f_br = (((float)cp_b[0]) / 255.0f);
				f_bg = (((float)cp_b[1]) / 255.0f);
				f_bb = (((float)cp_b[2]) / 255.0f);
			
				f_lr = (((float)cp_l[0]) / 255.0f);
				f_lg = (((float)cp_l[1]) / 255.0f);
				f_lb = (((float)cp_l[2]) / 255.0f);
				//printf("base: %d %d %d %d\n", cp_b[0], cp_b[1], cp_b[2], cp_b[3]);
				//printf("layer: %d %d %d %d\n", cp_l[0], cp_l[1], cp_l[2], cp_l[3]);
			}

			if ((f_la != 0.0f) && (f_ba != 0.0f)) {
				//printf("1\n");
				as = f_la;
				ab = f_ba;
				ao = as + ab * (1 - as);
				copy_co(flag, &fp_d[3], &cp_d[3], ao);

				/* ...p_d[0] */
				aoco = as * (1 - ab) * f_lr + as * ab * blend_callback(f_br, f_lr, opacity) + (1 - as) * ab * f_br;
				co = aoco / ao;
				CLAMP(co, 0.0f, 1.0f);
				copy_co(flag, &fp_d[0], &cp_d[0], co);

				/* ...p_d[1] */
				aoco = as * (1 - ab) * f_lg + as * ab * blend_callback(f_bg, f_lg, opacity) + (1 - as) * ab * f_bg;
				co = aoco / ao;
				CLAMP(co, 0.0f, 1.0f);
				copy_co(flag, &fp_d[1], &cp_d[1], co);

				/* ...p_d[2] */
				aoco = as * (1 - ab) * f_lb + as * ab * blend_callback(f_bb, f_lb, opacity) + (1 - as) * ab * f_bb;
				co = aoco / ao;
				CLAMP(co, 0.0f, 1.0f);
				copy_co(flag, &fp_d[2], &cp_d[2], co);
				//printf("blend: %d %d %d %d\n\n", cp_d[0], cp_d[1], cp_d[2], cp_d[3]);
			}
			else {
				if (background & IMA_LAYER_BG_ALPHA) {
					/*if (f_ba != 0.0f) {
						printf("2\n");
						copy_co(flag, &fp_d[0], &cp_d[0], f_br);
						copy_co(flag, &fp_d[1], &cp_d[1], f_bg);
						copy_co(flag, &fp_d[2], &cp_d[2], f_bb);
						copy_co(flag, &fp_d[3], &cp_d[3], f_ba);
					}*/
					//else {
					if (f_la != 0.0f) {
						//printf("3\n");
						copy_co(flag, &fp_d[0], &cp_d[0], f_lr);
						copy_co(flag, &fp_d[1], &cp_d[1], f_lg);
						copy_co(flag, &fp_d[2], &cp_d[2], f_lb);
						copy_co(flag, &fp_d[3], &cp_d[3], f_la);
					}
				}
			}
		}

		if (base->rect_float) {
			if (((i % bg_x) == 0) && (i != (bg_x * bg_y))) {
				if (base->x > layer->x) {
					fp_b = fp_b + diff_x * 4;
					fp_d = fp_d + diff_x * 4;
				}
				else
					fp_l = fp_l + diff_x * 4;
			}
			fp_b += 4;
			fp_l += 4; 
			fp_d += 4;
		}

		if (base->rect) {
			if (((i % bg_x) == 0) && (i != (bg_x * bg_y))) {
				if (base->x > layer->x) {
					cp_b = cp_b + diff_x * 4;
					cp_d = cp_d + diff_x * 4;
				}
				else
					cp_l = cp_l + diff_x * 4;
			}
			cp_b += 4;
			cp_l += 4; 
			cp_d += 4;
		}
	}

	//end=clock();
	//tempo=((double)(end-start))/CLOCKS_PER_SEC;
	//printf("Elapsed time: %f seconds.\n", tempo);
	return dest;
}

struct ImageLayer *merge_layers(Image *ima, ImageLayer *iml, ImageLayer *iml_next)
{
	ImBuf *ibuf, *result_ibuf;
	 /* merge layers */
	result_ibuf = imalayer_blend((ImBuf*)((ImageLayer*)iml_next->ibufs.first),
	                             (ImBuf*)((ImageLayer*)iml->ibufs.first),
								 iml->opacity, iml->mode,
	                             ((ImageLayer*)iml_next->ibufs.first)->background, NULL);
	
	iml_next->background = IMA_LAYER_BG_RGB;
	iml_next->file_path[0] = '\0';
	copy_v4_fl(iml_next->default_color, -1.0f);

	/* Delete old ibuf */
	ibuf = (ImBuf *)iml_next->ibufs.first;
	BLI_remlink(&iml_next->ibufs, ibuf);
 
	if (ibuf->userdata) {
		MEM_freeN(ibuf->userdata);
		ibuf->userdata = NULL;
	}
	IMB_freeImBuf(ibuf);

	/* add new ibuf */
	BLI_addtail(&iml_next->ibufs, result_ibuf);

	/* delete the layer merge */
	BLI_remlink(&ima->imlayers, iml);
	image_layer_free(iml);
	
	return iml_next;
}

/* Non destructive */
void merge_layers_visible_nd(Image *ima)
{
	ImageLayer *layer;
	ImBuf *ibuf, *result_ibuf, *next_ibuf;
	short background;

	result_ibuf = NULL;
	background = ((ImageLayer *)ima->imlayers.last)->background;
	
	for (layer = (ImageLayer *)ima->imlayers.last; layer; layer = layer->prev) {
		if (layer->visible & IMA_LAYER_VISIBLE) {
			ibuf = (ImBuf*)((ImageLayer*)layer->ibufs.first);
				
			if (ibuf) {
				next_ibuf = imalayer_blend(result_ibuf, ibuf, layer->opacity, layer->mode, background, NULL);
				
				if (result_ibuf)
					IMB_freeImBuf(result_ibuf);
				
				result_ibuf = next_ibuf;
				next_ibuf = NULL;
			}
		}
	}

	BKE_image_replace_ibuf(ima, result_ibuf);
}

static int imagelayer_find_name_dupe(const char *name, ImageLayer *iml, Image *ima)
{
	ImageLayer *layer;

	for (layer = ima->imlayers.last; layer; layer = layer->prev) {
		if (iml != layer) {
			if (!strcmp(layer->name, name)) {
				return 1;
			}
		}
	}

	return 0;
}

static bool imagelayer_unique_check(void *arg, const char *name)
{
	struct {Image *ima; void *iml;} *data= arg;
	return imagelayer_find_name_dupe(name, data->iml, data->ima);
}

void imagelayer_unique_name(ImageLayer *iml, Image *ima)
{
	struct {Image *ima; void *iml;} data;
	data.ima = ima;
	data.iml = iml;

	BLI_uniquename_cb(imagelayer_unique_check, &data, "Layer", '.', iml->name, sizeof(iml->name));
}

ImageLayer *image_add_image_layer(Image *ima, const char *name, int depth, float color[4], int order)
{
	ImageLayer *layer_act, *im_l = NULL;
	ImBuf *ibuf, *imaibuf;

	if (ima == NULL)
		return NULL;

 	layer_act = imalayer_get_current(ima);

	/* Deselect other layers */
	layer_act->select = !IMA_LAYER_SEL_CURRENT;
	
	im_l = image_layer_new(ima, name);
	if (im_l) {
		imaibuf = BKE_image_get_first_ibuf(ima);
		ibuf = add_ibuf_size(imaibuf->x, imaibuf->y, im_l->name, depth, ima->gen_flag, 0, color, &ima->colorspace_settings);
		BKE_image_release_ibuf(ima, imaibuf, NULL);

		BLI_addtail(&im_l->ibufs, ibuf);
		if (order == 2) { /*Head*/
			BLI_addhead(&ima->imlayers, im_l);
			ima->active_layer = 0;
		}
		else if (order == -1) { /*Before*/
			/* Layer Act
			 * --> Add Layer
			 */
			BLI_insertlinkafter(&ima->imlayers, layer_act , im_l);
			ima->active_layer += 1;
			imalayer_set_current_act(ima, ima->active_layer);
		}
		else { /*After*/
			/* --> Add Layer
			 * Layer Act
			 */
			BLI_insertlinkbefore(&ima->imlayers, layer_act , im_l);
			//ima->Act_Layers -= 1;
			imalayer_set_current_act(ima, ima->active_layer);
		}
		ima->num_layers += 1;

		if (color[3] == 1.0f)
			im_l->background = IMA_LAYER_BG_RGB;
		copy_v4_v4(im_l->default_color, color);
	}
 
	return im_l;
}
 
/* TODO: (kwk) Base image layer needs proper locking... */
void image_add_image_layer_base(Image *ima)
{
	if (!ima) {
		return;
	}

	ImageLayer *layer = image_layer_new(ima, "Background");

	if (layer) {
		layer->type = IMA_LAYER_BASE; /* BASE causes no free on deletion of layer */
		ima->active_layer = 0;
		ima->num_layers = 1;
		//ima->use_layers = true;

		/* Get ImBuf from ima */
		ImBuf *ima_ibuf = BKE_image_get_first_ibuf(ima);

		if (ima_ibuf) {
			ImBuf *ibuf = IMB_dupImBuf(ima_ibuf);
			BKE_image_release_ibuf(ima, ima_ibuf, NULL);

			BLI_addtail(&layer->ibufs, ibuf);
		}
		BLI_addhead(&ima->imlayers, layer);
	}
}

void imagelayer_get_background_color(float col[4], ImageLayer *layer)
{
	static float alpha_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
	static float black_color[4] = {0.0f, 0.0f, 0.0f, 1.0f};
	static float white_color[4] = {1.0f, 1.0f, 1.0f, 1.0f};

	if (layer) {
		if (layer->background & IMA_LAYER_BG_WHITE)
			copy_v4_v4(col, white_color);
		else if (layer->background & IMA_LAYER_BG_ALPHA)
			copy_v4_v4(col, alpha_color);
		else {
			if (layer->default_color[0] != -1)
				copy_v4_v4(col, layer->default_color);
			else
				copy_v4_v4(col, black_color);
		}
	}
	else {
		copy_v4_v4(col, black_color);
	}
}
