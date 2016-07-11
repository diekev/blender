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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2016 Kévin Dietrich.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 */

#include "cycles_alembic.h"

#ifdef WITH_ALEMBIC_HDF5
#  include </opt/lib/alembic/include/Alembic/AbcCoreHDF5/All.h>
#endif
#include </opt/lib/alembic/include/Alembic/AbcCoreOgawa/All.h>

#if 0
#include </opt/lib/alembic/include/Alembic/AbcGeom/All.h>
#include </opt/lib/alembic/include/Alembic/Abc/All.h>
#include <Alembic/AbcMaterial/IMaterial.h>
#else
#include </opt/lib/alembic/include/Alembic/Abc/Base.h>
#include </opt/lib/alembic/include/Alembic/Abc/ErrorHandler.h>
#include </opt/lib/alembic/include/Alembic/Abc/Foundation.h>

#include </opt/lib/alembic/include/Alembic/Abc/ArchiveInfo.h>
#include </opt/lib/alembic/include/Alembic/Abc/Argument.h>
#include </opt/lib/alembic/include/Alembic/Abc/IArchive.h>
#include </opt/lib/alembic/include/Alembic/Abc/IArrayProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/IBaseProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/ICompoundProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/IObject.h>
#include </opt/lib/alembic/include/Alembic/Abc/ISampleSelector.h>
#include </opt/lib/alembic/include/Alembic/Abc/IScalarProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/ISchema.h>
#include </opt/lib/alembic/include/Alembic/Abc/ISchemaObject.h>
#include </opt/lib/alembic/include/Alembic/Abc/ITypedArrayProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/ITypedScalarProperty.h>

#include </opt/lib/alembic/include/Alembic/Abc/OArchive.h>
#include </opt/lib/alembic/include/Alembic/Abc/OArrayProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/OBaseProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/OCompoundProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/OObject.h>
#include </opt/lib/alembic/include/Alembic/Abc/OScalarProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/OSchema.h>
#include </opt/lib/alembic/include/Alembic/Abc/OSchemaObject.h>
#include </opt/lib/alembic/include/Alembic/Abc/OTypedArrayProperty.h>
#include </opt/lib/alembic/include/Alembic/Abc/OTypedScalarProperty.h>

#include </opt/lib/alembic/include/Alembic/Abc/Reference.h>
#include </opt/lib/alembic/include/Alembic/Abc/SourceName.h>

#include </opt/lib/alembic/include/Alembic/Abc/TypedArraySample.h>
#include </opt/lib/alembic/include/Alembic/Abc/TypedPropertyTraits.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/ArchiveBounds.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/GeometryScope.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/Basis.h>
#include </opt/lib/alembic/include/Alembic/AbcGeom/ICurves.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/IFaceSet.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/IGeomBase.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/IGeomParam.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/FilmBackXformOp.h>
#include </opt/lib/alembic/include/Alembic/AbcGeom/CameraSample.h>
#include </opt/lib/alembic/include/Alembic/AbcGeom/ICamera.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/ILight.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/INuPatch.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/OPoints.h>
#include </opt/lib/alembic/include/Alembic/AbcGeom/IPoints.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/IPolyMesh.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/ISubD.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/XformOp.h>
#include </opt/lib/alembic/include/Alembic/AbcGeom/XformSample.h>
#include </opt/lib/alembic/include/Alembic/AbcGeom/IXform.h>

#include </opt/lib/alembic/include/Alembic/AbcGeom/Visibility.h>

#include </opt/lib/alembic/include/Alembic/AbcMaterial/IMaterial.h>
#endif

#if 0
#include "background.h"
#include "camera.h"
#include "mesh.h"
#include "nodes.h"
#include "object.h"
#include "scene.h"
#else
#include "../render/background.h"
#include "../render/camera.h"
#include "../render/mesh.h"
#include "../render/nodes.h"
#include "../render/object.h"
#include "../render/scene.h"
#endif

CCL_NAMESPACE_BEGIN

using namespace Alembic;
using namespace Abc;
using namespace AbcGeom;
using namespace AbcMaterial;

struct AbcReadState {
	Scene *scene;    /* scene pointer */
	float time;
	Transform tfm;    /* current transform state */
	bool smooth;    /* smooth normal state */
	Shader *shader;      /* current shader */
	string base;    /* base path to current file*/
	float dicing_rate;  /* current dicing rate */
	Mesh::DisplacementMethod displacement_method;
};

static IArchive open_archive(const std::string &filename)
{
	Alembic::AbcCoreAbstract::ReadArraySampleCachePtr cache_ptr;
	IArchive archive;

	try {
		archive = IArchive(Alembic::AbcCoreOgawa::ReadArchive(),
		                   filename.c_str(), ErrorHandler::kThrowPolicy,
		                   cache_ptr);
	}
	catch (const Exception &e) {
		std::cerr << e.what() << '\n';

#ifdef WITH_ALEMBIC_HDF5
		try {
			archive = IArchive(Alembic::AbcCoreHDF5::ReadArchive(),
			                   filename.c_str(), ErrorHandler::kThrowPolicy,
			                   cache_ptr);
		}
		catch (const Exception &) {
			std::cerr << e.what() << '\n';
			return IArchive();
		}
#else
		return IArchive();
#endif
	}

	return archive;
}

static ISampleSelector get_sample_selector(const AbcReadState &state)
{
	return ISampleSelector(state.time, ISampleSelector::kFloorIndex);
}

static void get_transform(AbcReadState &state, const IObject &object)
{
	state.tfm = transform_identity();

	IXform ixform;

	if (IXform::matches(object.getMetaData())) {
		ixform = IXform(object, kWrapExisting);
	}
	else if (IXform::matches(object.getParent().getMetaData())) {
		ixform = IXform(object.getParent(), kWrapExisting);
	}
	else {
		return;
	}

	const IXformSchema &schema(ixform.getSchema());

	if (!schema.valid()) {
		return;
	}

	ISampleSelector sample_sel(state.time);
	XformSample xs;
	schema.get(xs, sample_sel);

	const Imath::M44d &xform = xs.getMatrix();

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			state.tfm[i][j] = static_cast<float>(xform[i][j]);
		}
	}
}

static Mesh *add_mesh(Scene *scene, const Transform& tfm)
{
	/* create mesh */
	Mesh *mesh = new Mesh();
	scene->meshes.push_back(mesh);

	/* create object*/
	Object *object = new Object();
	object->mesh = mesh;
	object->tfm = tfm;
	scene->objects.push_back(object);

	return mesh;
}

static void read_mesh(const AbcReadState &state, const IPolyMesh &object)
{
	/* add mesh */
	Mesh *mesh = add_mesh(state.scene, state.tfm);
	mesh->used_shaders.push_back(state.shader);

	/* read state */
	int shader = 0; //state.shader;
	bool smooth = state.smooth;

	mesh->displacement_method = state.displacement_method;

	ISampleSelector ss = get_sample_selector(state);
	IPolyMeshSchema schema = object.getSchema();

	IPolyMeshSchema::Sample sample;
	schema.get(sample, ss);

	int totverts = sample.getPositions()->size();
	int totfaces = sample.getFaceCounts()->size();
	const V3f *P = sample.getPositions()->get();
	const int32_t *indices = sample.getFaceIndices()->get();
	const int32_t *nverts = sample.getFaceCounts()->get();

	size_t tottris = 0;

	for(int i = 0; i < totfaces; ++i) {
		int n = nverts[i];

		if (n == 3) {
			++tottris;
		}
		else if (n == 4) {
			tottris += 2;
		}
	}

	mesh->reserve_mesh(totverts, tottris);

	/* create vertices */
	float3 *verts = mesh->verts.resize(totverts);
	for(int i = 0; i < totverts; i++) {
		verts[i] = make_float3(P[i].x, P[i].y, P[i].z);
	}

	/* create triangles */
	int index_offset = 0;

	for(int i = 0; i < totfaces; i++) {
		int n = nverts[i];
		/* XXX TODO only supports tris and quads atm,
	   * need a proper tessellation algorithm in cycles.
	   */
		if (n > 4) {
			printf("%d-sided face found, only triangles and quads are supported currently", n);
			n = 4;
		}

		for(int j = 0; j < n - 2; j++) {
			int v0 = indices[index_offset];
			int v1 = indices[index_offset + j + 1];
			int v2 = indices[index_offset + j + 2];

			assert(v0 < (int)totverts);
			assert(v1 < (int)totverts);
			assert(v2 < (int)totverts);

			mesh->add_triangle(v0, v1, v2, shader, smooth);
		}

		index_offset += n;
	}

	/* temporary for test compatibility */
	mesh->attributes.remove(ATTR_STD_VERTEX_NORMAL);
}

static void read_camera(const AbcReadState &state, const ICamera &object)
{
	/* add mesh */
	Camera *camera = state.scene->camera;

	camera->width = 800;
	camera->height = 500;

	ISampleSelector ss = get_sample_selector(state);
	ICameraSchema schema = object.getSchema();

	CameraSample sample;
	schema.get(sample, ss);

	const float lens = static_cast<float>(sample.getFocalLength());
	const float apperture_x = static_cast<float>(sample.getHorizontalAperture());
	const float apperture_y = static_cast<float>(sample.getVerticalAperture());
	const float near = static_cast<float>(sample.getNearClippingPlane());
	const float far = static_cast<float>(sample.getFarClippingPlane());
	const float fstop = static_cast<float>(sample.getFStop());
	const float focaldistance = static_cast<float>(sample.getFocusDistance());
//	const float h_film_offset = sample.getHorizontalFilmOffset();
//	const float v_film_offset = sample.getVerticalFilmOffset();
	const float film_aspect = apperture_x / apperture_y;

	camera->focaldistance = lens;

	camera->nearclip = std::max(0.1f, near);
	camera->farclip = far;
	camera->focaldistance = focaldistance;

	camera->aperturesize = (lens*1e-3f)/(2.0f*fstop);
	camera->aperture_ratio = film_aspect;

	camera->matrix = state.tfm;

	camera->need_update = true;
	camera->update();
}

static void read_object(const IObject &object, AbcReadState &state)
{
	if (!object.valid()) {
		return;
	}

	for (int i = 0; i < object.getNumChildren(); ++i) {
		IObject child = object.getChild(i);

		if (!child.valid()) {
			continue;
		}

		const MetaData &md = child.getMetaData();

		get_transform(state, child);

		if (IXform::matches(md)) {
			/* Pass for now. */
		}
		else if (IPolyMesh::matches(md)) {
			read_mesh(state, IPolyMesh(child, kWrapExisting));
		}
		else if (ISubD::matches(md)) {
			/* Pass for now. */
		}
		else if (INuPatch::matches(md)) {
			/* Pass for now. */
		}
		else if (ICamera::matches(md)) {
			read_camera(state, ICamera(child, kWrapExisting));
		}
		else if (IPoints::matches(md)) {
			/* Pass for now. */
		}
		else if (IMaterial::matches(md)) {
			/* Pass for now. */
		}
		else if (ILight::matches(md)) {
			/* Pass for now. */
		}
		else if (IFaceSet::matches(md)) {
			/* Pass, those are handled in the mesh reader. */
		}
		else if (ICurves::matches(md)) {
			/* Pass for now. */
		}
		else {
			assert(false);
		}

		read_object(child, state);
	}
}

static Shader *background_shader(Scene *scene)
{
	ShaderGraph *graph = new ShaderGraph();

	BackgroundNode *bg = new BackgroundNode();
	bg->color = make_float3(0.2f, 0.2f, 0.2f);
	graph->add(bg);

	graph->connect(bg->output("Background"), graph->output()->input("Surface"));

	Shader *shader = new Shader();
	shader->name = "bg";
	shader->graph = graph;
	scene->shaders.push_back(shader);

	return shader;
}

static void read_archive(IArchive &archive, Scene *scene)
{
	AbcReadState state;
	state.scene = scene;
	state.shader = scene->default_background;

	scene->background->shader = background_shader(scene);

	read_object(archive.getTop(), state);

	scene->params.bvh_type = SceneParams::BVH_STATIC;
}

void abc_read_file(Scene *scene, const char *filepath)
{
	IArchive archive = open_archive(filepath);

	if (!archive.valid()) {
		return;
	}

	read_archive(archive, scene);
}

CCL_NAMESPACE_END
