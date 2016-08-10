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
 * Contributor(s): Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "../MTLX_api.h"

#include "../tinyxml2/tinyxml2.h"

extern "C" {
#include "DNA_material_types.h"
}

/* Only applicable for a mix rgb node with a single input connected. */
static void mtlx_add_math_operator(tinyxml2::XMLDocument &doc,
                                   tinyxml2::XMLElement *parent,
                                   int optype)
{
	const char *opname;

	switch (optype) {
		case MA_RAMP_ADD:
			opname = "add";
			break;
		case MA_RAMP_MULT:
			opname = "multiply";
			break;
		case MA_RAMP_SUB:
			opname = "subtract";
			break;
		case MA_RAMP_DIV:
			opname = "divide";
			break;
		default:
		case MA_RAMP_BLEND:
		case MA_RAMP_BURN:
		case MA_RAMP_COLOR:
		case MA_RAMP_DARK:
		case MA_RAMP_DIFF:
		case MA_RAMP_DODGE:
		case MA_RAMP_HUE:
		case MA_RAMP_LIGHT:
		case MA_RAMP_LINEAR:
		case MA_RAMP_OVERLAY:
		case MA_RAMP_SAT:
		case MA_RAMP_SCREEN:
		case MA_RAMP_SOFT:
		case MA_RAMP_VAL:
			return;
	}

	tinyxml2::XMLElement *optype_param = doc.NewElement(opname);
	//collection->SetAttribute("name", collection_name);

	tinyxml2::XMLElement *input_param = doc.NewElement("parameter");
	input_param->SetAttribute("name", "in");
	input_param->SetAttribute("type", "opgraphnode"); // TODO
	input_param->SetAttribute("value", "n4"); // TODO

	optype_param->InsertEndChild(input_param);

	float amount = 0.4f; // TODO

	tinyxml2::XMLElement *amount_param = doc.NewElement("parameter");
	amount_param->SetAttribute("name", "amount");
	amount_param->SetAttribute("type", "float"); // TODO
	amount_param->SetAttribute("amount", amount);

	optype_param->InsertEndChild(amount_param);

	// TODO: channels param.

	parent->InsertEndChild(optype_param);
}

static void mtlx_add_geom_reference(tinyxml2::XMLDocument &doc,
                                    tinyxml2::XMLElement *parent,
                                    const char *collection_name,
                                    const char *geom_str)
{
	tinyxml2::XMLElement *collection = doc.NewElement("collection");
	collection->SetAttribute("name", collection_name);

	tinyxml2::XMLElement *collectionadd = doc.NewElement("collectionadd");
	collectionadd->SetAttribute("name", collection_name);
	collectionadd->SetAttribute("geom", geom_str);

	collection->InsertEndChild(collectionadd);

	parent->InsertEndChild(collection);
}

static void mtlx_add_shader_reference(tinyxml2::XMLDocument &doc,
                                      tinyxml2::XMLElement *parent,
                                      const char *shader_name,
                                      const char *shader_type,
                                      const char *shaderprogram)
{
	tinyxml2::XMLElement *shader = doc.NewElement("shader");
	/* Required atrributes. */
	shader->SetAttribute("name", shader_name);
	shader->SetAttribute("shadertype", shader_type);
	shader->SetAttribute("shaderprogram", shaderprogram);
	/* Optionnal attributes. */
	shader->SetAttribute("aovset", "");
	shader->SetAttribute("xpos", 0.0f);
	shader->SetAttribute("ypos", 0.0f);

	parent->InsertEndChild(shader);
}

void MTLX_export(const char *filename)
{
	tinyxml2::XMLDocument doc;
	doc.InsertFirstChild(doc.NewDeclaration());

	tinyxml2::XMLElement *root = doc.NewElement("materialx");
	root->SetAttribute("version", "1.32");
	root->SetAttribute("cms", "ocio");

	doc.InsertEndChild(root);

	mtlx_add_geom_reference(doc, root, "c_plastic", "/a/g1,/a/g2,/a/g5");
	mtlx_add_geom_reference(doc, root, "c_metal", "/a/g3,/a/g4");
	mtlx_add_geom_reference(doc, root, "c_headlampAssembly", "/a/lamp1/housing*Mesh");
	mtlx_add_geom_reference(doc, root, "c_setgeom", "/b//*");

	mtlx_add_shader_reference(doc, root, "srf1", "surface", "basic_surface");
	mtlx_add_shader_reference(doc, root, "dsp1", "displacement", "noise_bump");

	mtlx_add_math_operator(doc, root, MA_RAMP_ADD);
	mtlx_add_math_operator(doc, root, MA_RAMP_SUB);
	mtlx_add_math_operator(doc, root, MA_RAMP_MULT);
	mtlx_add_math_operator(doc, root, MA_RAMP_DIV);

#if 0
	tinyxml2::XMLElement *image = doc.NewElement("image");
	image->SetAttribute("name", "in1");
	image->SetAttribute("type", "color3");
	image->SetAttribute("colorspace", "lin_p3dci");

	root->InsertEndChild(image);

	tinyxml2::XMLElement *image_param = doc.NewElement("parameter");
	image_param->SetAttribute("name", "file");
	image_param->SetAttribute("type", "filename");
	image_param->SetAttribute("type", "input1.tif");
	image_param->SetAttribute("colorspace", "srgb_texture");

	image->InsertEndChild(image_param);
#endif

	tinyxml2::XMLError eResult = doc.SaveFile(filename);

	if (eResult != tinyxml2::XML_SUCCESS) {
		printf("Error: %i\n", eResult);
	}
}
