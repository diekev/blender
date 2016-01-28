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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_image_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_IMAGE_TYPES_H__
#define __DNA_IMAGE_TYPES_H__

#include "DNA_defs.h"
#include "DNA_ID.h"
#include "DNA_color_types.h"  /* for color management */

struct PackedFile;
struct Scene;
struct anim;
struct MovieCache;
struct RenderResult;
struct GPUTexture;

/* ImageUser is in Texture, in Nodes, Background Image, Image Window, .... */
/* should be used in conjunction with an ID * to Image. */
typedef struct ImageUser {
	struct Scene *scene;		/* to retrieve render result */

	int framenr;				/* movies, sequences: current to display */
	int frames;					/* total amount of frames to use */
	int offset, sfra;			/* offset within movie, start frame in global time */
	char fie_ima, cycl;		/* fields/image in movie, cyclic flag */
	char ok;

	char multiview_eye;			/* multiview current eye - for internal use of drawing routines */
	short pass;
	short pad;

	short multi_index, view, layer;	 /* listbase indices, for menu browsing or retrieve buffer */
	short flag;
	
	char use_layer_ima, pad2[7];
} ImageUser;

typedef struct ImageAnim {
	struct ImageAnim *next, *prev;
	struct anim *anim;
} ImageAnim;

typedef struct ImageView {
	struct ImageView *next, *prev;
	char name[64];			/* MAX_NAME */
	char filepath[1024];	/* 1024 = FILE_MAX */
} ImageView;

typedef struct ImagePackedFile {
	struct ImagePackedFile *next, *prev;
	struct PackedFile *packedfile;
	char filepath[1024];	/* 1024 = FILE_MAX */
} ImagePackedFile;

typedef struct RenderSlot {
	char name[64];  /* 64 = MAX_NAME */
} RenderSlot;

/* iuser->flag */
#define	IMA_ANIM_ALWAYS		1
#define IMA_ANIM_REFRESHED	2
/* #define IMA_DO_PREMUL	4 */
#define IMA_NEED_FRAME_RECALC	8
#define IMA_SHOW_STEREO		16

/* **************** IMAGE LAYER ********************* */

typedef struct ImageLayer {
	struct ImageLayer *next, *prev;
	/* struct MovieCache *cache; */
	char name[64];
	char file_path[1024];
	float opacity;
	short background;
	short mode;
	short type;
	short visible;
	short select;
	short locked;
	int pad1;
	float default_color[4];
	int pad2;
	ListBase ibufs;
	struct ImBuf *preview_ibuf;
} ImageLayer;

/* ImageLayer.mode */
typedef enum ImageLayerMode {
	IMA_LAYER_NORMAL = 0,

	IMA_LAYER_MULTIPLY = 1,
	IMA_LAYER_SCREEN = 2,
	IMA_LAYER_OVERLAY = 3,
	IMA_LAYER_SOFT_LIGHT = 4,
	IMA_LAYER_HARD_LIGHT = 5,

	IMA_LAYER_COLOR_DODGE = 6,
	IMA_LAYER_LINEAR_DODGE = 7,
	IMA_LAYER_COLOR_BURN = 8,
	IMA_LAYER_LINEAR_BURN = 9,

	IMA_LAYER_AVERAGE = 10,
	IMA_LAYER_ADD = 11,
	IMA_LAYER_SUBTRACT = 12,
	IMA_LAYER_DIFFERENCE = 13,
	IMA_LAYER_LIGHTEN = 14,
	IMA_LAYER_DARKEN = 15,

	IMA_LAYER_NEGATION = 16,
	IMA_LAYER_EXCLUSION = 17,

	IMA_LAYER_LINEAR_LIGHT = 18,
	IMA_LAYER_VIVID_LIGHT = 19,
	IMA_LAYER_PIN_LIGHT = 20,
	IMA_LAYER_HARD_MIX = 21,
	IMA_LAYER_INVERSE_COLOR_BURN = 22,
	IMA_LAYER_SOFT_BURN = 23
} ImageLayerMode;

#define IMA_LAYER_MAX_LEN	64

/* ImageLayer.background */
#define IMA_LAYER_BG_RGB		(1 << 0)
#define IMA_LAYER_BG_WHITE		(1 << 1)
#define IMA_LAYER_BG_ALPHA		(1 << 2)
#define IMA_LAYER_BG_IMAGE		(1 << 3)

/* ImageLayer.type */
#define IMA_LAYER_BASE		(1 << 0)
#define IMA_LAYER_LAYER		(1 << 1)

/* ImageLayer.visible */
#define IMA_LAYER_VISIBLE	(1 << 0)

/* ImageLayer.select */
#define IMA_LAYER_SEL_CURRENT	(1 << 0)
#define	IMA_LAYER_SEL_PREVIOUS	(1 << 1)
#define	IMA_LAYER_SEL_NEXT		(1 << 2)
#define IMA_LAYER_SEL_TOP		(1 << 3)
#define IMA_LAYER_SEL_BOTTOM	(1 << 4)

/* ImageLayer.locked */
#define IMA_LAYER_LOCK			1
#define IMA_LAYER_LOCK_ALPHA	2

/* Option for delete the layer*/
#define IMA_LAYER_DEL_SELECTED	(1 << 0)
#define IMA_LAYER_DEL_HIDDEN	(1 << 1)

/* Option for open a image*/
#define IMA_LAYER_OPEN_IMAGE	(1 << 0)
#define IMA_LAYER_OPEN_LAYER	(1 << 1)

/* Option for Image Node */
#define IMA_USE_LAYER	(1 << 0)

enum {
	TEXTARGET_TEXTURE_2D = 0,
	TEXTARGET_TEXTURE_CUBE_MAP = 1,
	TEXTARGET_COUNT = 2
};

typedef struct Image {
	ID id;
	
	char name[1024];			/* file path, 1024 = FILE_MAX */
	
	struct MovieCache *cache;	/* not written in file */
	struct ImBuf *preview_ibuf;
	struct GPUTexture *gputexture[2]; /* not written in file 2 = TEXTARGET_COUNT */
	
	/* sources from: */
	ListBase anims;
	struct RenderResult *rr;

	struct RenderResult *renders[8]; /* IMA_MAX_RENDER_SLOT */
	short render_slot, last_render_slot;

	int flag;
	short source, type;
	int lastframe;

	/* texture page */
	short tpageflag, totbind;
	short xrep, yrep;
	short twsta, twend;
	unsigned int bindcode[2]; /* only for current image... 2 = TEXTARGET_COUNT */
	char pad1[4];
	unsigned int *repbind;	/* for repeat of parts of images */
	
	struct PackedFile *packedfile DNA_DEPRECATED; /* deprecated */
	struct ListBase packedfiles;
	struct PreviewImage *preview;

	/* game engine tile animation */
	float lastupdate;
	int lastused;
	short animspeed;

	short ok;
	
	/* for generated images */
	int gen_x, gen_y;
	char gen_type, gen_flag;
	short gen_depth;
	float gen_color[4];
	
	/* display aspect - for UV editing images resized for faster openGL display */
	float aspx, aspy;

	/* color management */
	ColorManagedColorspaceSettings colorspace_settings;
	char alpha_mode;

	char pad[5];

	/* Multiview */
	char eye; /* for viewer node stereoscopy */
	char views_format;
	ListBase views;  /* ImageView */
	struct Stereo3dFormat *stereo3d_format;

	RenderSlot render_slots[8];  /* 8 = IMA_MAX_RENDER_SLOT */

	/* image layers */
	int active_layer;
	int num_layers;
	short use_layers;
	short color_space;
	int pad4;
	struct ListBase imlayers;
} Image;


/* **************** IMAGE ********************* */

/* Image.flag */
enum {
	IMA_FIELDS              = (1 << 0),
	IMA_STD_FIELD           = (1 << 1),
#ifdef DNA_DEPRECATED
	IMA_DO_PREMUL           = (1 << 2),  /* deprecated, should not be used */
#endif
	IMA_REFLECT             = (1 << 4),
	IMA_NOCOLLECT           = (1 << 5),
	//IMA_DONE_TAG          = (1 << 6),  // UNUSED
	IMA_OLD_PREMUL          = (1 << 7),
	// IMA_CM_PREDIVIDE     = (1 << 8),  /* deprecated, should not be used */
	IMA_USED_FOR_RENDER     = (1 << 9),
	IMA_USER_FRAME_IN_RANGE = (1 << 10), /* for image user, but these flags are mixed */
	IMA_VIEW_AS_RENDER      = (1 << 11),
	IMA_IGNORE_ALPHA        = (1 << 12),
	IMA_DEINTERLACE         = (1 << 13),
	IMA_USE_VIEWS           = (1 << 14),
	// IMA_IS_STEREO        = (1 << 15), /* deprecated */
	// IMA_IS_MULTIVIEW     = (1 << 16), /* deprecated */
};

/* Image.tpageflag */
#define IMA_TILES			1
#define IMA_TWINANIM		2
#define IMA_COLCYCLE		4	/* Depreciated */
#define IMA_MIPMAP_COMPLETE 8   /* all mipmap levels in OpenGL texture set? */
#define IMA_CLAMP_U			16 
#define IMA_CLAMP_V			32
#define IMA_TPAGE_REFRESH	64
#define IMA_GLBIND_IS_DATA	128 /* opengl image texture bound as non-color data */

/* ima->type and ima->source moved to BKE_image.h, for API */

/* render */
#define IMA_MAX_RENDER_TEXT		512
#define IMA_MAX_RENDER_SLOT		8

/* gen_flag */
#define IMA_GEN_FLOAT		1

/* alpha_mode */
enum {
	IMA_ALPHA_STRAIGHT = 0,
	IMA_ALPHA_PREMUL = 1,
};

/* color_space */
#define IMA_COL_RGB		(1<<0)
#define IMA_COL_GRAY	(1<<1)

#endif
