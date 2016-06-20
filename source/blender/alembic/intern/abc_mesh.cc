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
 * Contributor(s): Esteban Tovagliari, Cedric Paille, Kevin Dietrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "abc_mesh.h"

#include <algorithm>

#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_fluidsim.h"
#include "DNA_object_types.h"

#include "BLI_math_geom.h"
#include "BLI_string.h"

#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
}

using Alembic::Abc::FloatArraySample;
using Alembic::Abc::ICompoundProperty;
using Alembic::Abc::Int32ArraySample;
using Alembic::Abc::Int32ArraySamplePtr;
using Alembic::Abc::P3fArraySamplePtr;
using Alembic::Abc::V2fArraySample;
using Alembic::Abc::V3fArraySample;
using Alembic::Abc::C4fArraySample;

using Alembic::AbcGeom::IFaceSet;
using Alembic::AbcGeom::IFaceSetSchema;
using Alembic::AbcGeom::IObject;
using Alembic::AbcGeom::IPolyMesh;
using Alembic::AbcGeom::IPolyMeshSchema;
using Alembic::AbcGeom::ISampleSelector;
using Alembic::AbcGeom::ISubD;
using Alembic::AbcGeom::ISubDSchema;
using Alembic::AbcGeom::IV2fGeomParam;

using Alembic::AbcGeom::OArrayProperty;
using Alembic::AbcGeom::OBoolProperty;
using Alembic::AbcGeom::OC3fArrayProperty;
using Alembic::AbcGeom::OC3fGeomParam;
using Alembic::AbcGeom::OC4fGeomParam;
using Alembic::AbcGeom::OCompoundProperty;
using Alembic::AbcGeom::OFaceSet;
using Alembic::AbcGeom::OFaceSetSchema;
using Alembic::AbcGeom::OFloatGeomParam;
using Alembic::AbcGeom::OInt32GeomParam;
using Alembic::AbcGeom::ON3fArrayProperty;
using Alembic::AbcGeom::ON3fGeomParam;
using Alembic::AbcGeom::OPolyMesh;
using Alembic::AbcGeom::OPolyMeshSchema;
using Alembic::AbcGeom::OSubD;
using Alembic::AbcGeom::OSubDSchema;
using Alembic::AbcGeom::OV2fGeomParam;
using Alembic::AbcGeom::OV3fGeomParam;

using Alembic::AbcGeom::kFacevaryingScope;
using Alembic::AbcGeom::kVaryingScope;
using Alembic::AbcGeom::kVertexScope;
using Alembic::AbcGeom::kWrapExisting;
using Alembic::AbcGeom::UInt32ArraySample;
using Alembic::AbcGeom::N3fArraySamplePtr;
using Alembic::AbcGeom::IN3fGeomParam;

/* ************************************************************************** */

/* NOTE: Alembic's polygon winding order is clockwise, to match with Renderman. */

static void get_vertices(DerivedMesh *dm, std::vector<float> &points)
{
	points.clear();
	points.resize(dm->getNumVerts(dm) * 3);

	MVert *verts = dm->getVertArray(dm);

	for (int i = 0, e = dm->getNumVerts(dm); i < e; ++i) {
		copy_zup_yup(&points[i * 3], verts[i].co);
	}
}

static void get_topology(DerivedMesh *dm,
                         std::vector<int32_t> &poly_verts,
                         std::vector<int32_t> &loop_counts)
{
	const int num_poly = dm->getNumPolys(dm);
	const int num_loops = dm->getNumLoops(dm);
	MLoop *mloop = dm->getLoopArray(dm);
	MPoly *mpoly = dm->getPolyArray(dm);

	poly_verts.clear();
	loop_counts.clear();
	poly_verts.reserve(num_loops);
	loop_counts.reserve(num_poly);

	for (int i = 0; i < num_poly; ++i) {
		MPoly &poly = mpoly[i];
		loop_counts.push_back(poly.totloop);

		MLoop *loop = mloop + poly.loopstart + (poly.totloop - 1);

		for (int j = 0; j < poly.totloop; ++j, --loop) {
			poly_verts.push_back(loop->v);
		}
	}
}

void get_material_indices(DerivedMesh *dm, std::vector<int32_t> &indices)
{
	indices.clear();
	indices.reserve(dm->getNumTessFaces(dm));

	MPoly *mpolys = dm->getPolyArray(dm);

	for (int i = 1, e = dm->getNumPolys(dm); i < e; ++i) {
		MPoly *mpoly = &mpolys[i];
		indices.push_back(mpoly->mat_nr);
	}
}

void get_creases(DerivedMesh *dm,
                 std::vector<int32_t> &indices,
                 std::vector<int32_t> &lengths,
                 std::vector<float> &sharpnesses)
{
	const float factor = 1.0f / 255.0f;

	indices.clear();
	lengths.clear();
	sharpnesses.clear();

	MEdge *edge = dm->getEdgeArray(dm);

	for (int i = 0, e = dm->getNumEdges(dm); i < e; ++i) {
		const float sharpness = static_cast<float>(edge[i].crease) * factor;

		if (sharpness != 0.0f) {
			indices.push_back(edge[i].v1);
			indices.push_back(edge[i].v2);
			sharpnesses.push_back(sharpness);
		}
	}

	lengths.resize(sharpnesses.size(), 2);
}

static void get_normals(DerivedMesh *dm, std::vector<float> &normals)
{
	MPoly *mpoly = dm->getPolyArray(dm);
	MPoly *mp = mpoly;

	MLoop *mloop = dm->getLoopArray(dm);
	MLoop *ml = mloop;

	MVert *verts = dm->getVertArray(dm);

	const size_t num_normals = dm->getNumVerts(dm) * 3;

	normals.clear();
	normals.resize(num_normals);

	for (int i = 0, e = dm->getNumPolys(dm); i < e; ++i, ++mp) {
		float no[3];

		/* Flat shaded, use common normal for all verts. */
		if ((mp->flag & ME_SMOOTH) == 0) {
			BKE_mesh_calc_poly_normal(mp, ml, verts, no);
		}

		for (int j = 0; j < mp->totloop; ++ml, ++j) {
			const int index = ml->v;

			/* Smooth shaded, use individual vert normals. */
			if (mp->flag & ME_SMOOTH) {
				normal_short_to_float_v3(no, verts[index].no);
			}

			copy_zup_yup(&normals[index * 3], no);
		}
	}
}

/* *************** Modifiers *************** */

/* check if the mesh is a subsurf, ignoring disabled modifiers and
 * displace if it's after subsurf. */
static ModifierData *get_subsurf_modifier(Scene *scene, Object *ob)
{
	ModifierData *md = static_cast<ModifierData *>(ob->modifiers.last);

	for (; md; md = md->prev) {
		if (!modifier_isEnabled(scene, md, eModifierMode_Render)) {
			continue;
		}

		if (md->type == eModifierType_Subsurf) {
			SubsurfModifierData *smd = reinterpret_cast<SubsurfModifierData*>(md);

			if (smd->subdivType == ME_CC_SUBSURF) {
				return md;
			}
		}

		/* mesh is not a subsurf. break */
		if ((md->type != eModifierType_Displace) && (md->type != eModifierType_ParticleSystem)) {
			return NULL;
		}
	}

	return NULL;
}

static ModifierData *get_liquid_sim_modifier(Scene *scene, Object *ob)
{
	ModifierData *md = modifiers_findByType(ob, eModifierType_Fluidsim);

	if (md && (modifier_isEnabled(scene, md, eModifierMode_Render))) {
		FluidsimModifierData *fsmd = reinterpret_cast<FluidsimModifierData *>(md);

		if (fsmd->fss && fsmd->fss->type == OB_FLUIDSIM_DOMAIN) {
			return md;
		}
	}

	return NULL;
}

/* ************************************************************************** */

AbcMeshWriter::AbcMeshWriter(Scene *scene,
                             Object *ob,
                             AbcTransformWriter *parent,
                             uint32_t time_sampling,
                             ExportSettings &settings)
    : AbcObjectWriter(scene, ob, time_sampling, settings, parent)
{
	m_is_animated = isAnimated();
	m_subsurf_mod = NULL;
	m_has_per_face_materials = false;
	m_is_subd = false;

	/* If the object is static, use the default static time sampling. */
	if (!m_is_animated) {
		time_sampling = 0;
	}

	if (!m_settings.export_subsurfs_as_meshes) {
		m_subsurf_mod = get_subsurf_modifier(m_scene, m_object);
		m_is_subd = (m_subsurf_mod != NULL);
	}

	m_is_liquid = (get_liquid_sim_modifier(m_scene, m_object) != NULL);

	while (parent->alembicXform().getChildHeader(m_name)) {
		m_name.append("_");
	}

	if (m_settings.use_subdiv_schema && m_is_subd) {
		OSubD subd(parent->alembicXform(), m_name, m_time_sampling);
		m_subdiv_schema = subd.getSchema();
	}
	else {
		OPolyMesh mesh(parent->alembicXform(), m_name, m_time_sampling);
		m_mesh_schema = mesh.getSchema();

		OCompoundProperty typeContainer = m_mesh_schema.getUserProperties();
		OBoolProperty type(typeContainer, "meshtype");
		type.set(m_is_subd);
	}
}

AbcMeshWriter::~AbcMeshWriter()
{
	if (m_subsurf_mod) {
		m_subsurf_mod->mode &= ~eModifierMode_DisableTemporary;
	}
}

bool AbcMeshWriter::isAnimated() const
{
	/* Check if object has shape keys. */
	Mesh *me = static_cast<Mesh *>(m_object->data);

	if (me->key) {
		return true;
	}

	/* Test modifiers. */
	ModifierData *md = static_cast<ModifierData *>(m_object->modifiers.first);

	while (md) {
		if (md->type != eModifierType_Subsurf) {
			return true;
		}

		md = md->next;
	}

	return false;
}

void AbcMeshWriter::do_write()
{
	/* We have already stored a sample for this object. */
	if (!m_first_frame && !m_is_animated)
		return;

	DerivedMesh *dm = getFinalMesh();

	try {
		if (m_settings.use_subdiv_schema && m_subdiv_schema.valid()) {
			writeSubD(dm);
		}
		else {
			writeMesh(dm);
		}

		freeMesh(dm);
	}
	catch (...) {
		freeMesh(dm);
		throw;
	}
}

void AbcMeshWriter::writeMesh(DerivedMesh *dm)
{
	std::vector<float> points, normals;
	std::vector<int32_t> poly_verts, loop_counts;

	get_vertices(dm, points);
	get_topology(dm, poly_verts, loop_counts);

	if (m_first_frame) {
		writeCommonData(dm, m_mesh_schema);
	}

	m_mesh_sample = OPolyMeshSchema::Sample(
	                    V3fArraySample(
	                        (const Imath::V3f *) &points.front(),
	                        points.size() / 3),
	                    Int32ArraySample(poly_verts),
	                    Int32ArraySample(loop_counts));

	UVSample sample;
	if (m_settings.export_uvs) {
		const char *name = get_uv_sample(sample, m_custom_data_config, &dm->loopData);

		if (!sample.indices.empty() && !sample.uvs.empty()) {
			OV2fGeomParam::Sample uv_sample;
			uv_sample.setVals(V2fArraySample(&sample.uvs[0], sample.uvs.size()));
			uv_sample.setIndices(UInt32ArraySample(&sample.indices[0], sample.indices.size()));
			uv_sample.setScope(kFacevaryingScope);

			m_mesh_schema.setUVSourceName(name);
			m_mesh_sample.setUVs(uv_sample);
		}

		write_custom_data(m_mesh_schema.getArbGeomParams(), m_custom_data_config, &dm->loopData, CD_MLOOPUV);
	}

	if (m_settings.export_normals) {
		get_normals(dm, normals);

		ON3fGeomParam::Sample normals_sample;
		if (!normals.empty()) {
			normals_sample.setScope(kVertexScope);
			normals_sample.setVals(
			            V3fArraySample(
			                (const Imath::V3f *)&normals.front(),
			                normals.size() / 3));
		}

		m_mesh_sample.setNormals(normals_sample);
	}

	if (m_is_liquid) {
		std::vector<float> velocities;
		getVelocities(dm, velocities);

		m_mesh_sample.setVelocities(V3fArraySample(
		                                (const Imath::V3f *)&velocities.front(),
		                                velocities.size() / 3));
	}

	m_mesh_sample.setSelfBounds(bounds());

	m_mesh_schema.set(m_mesh_sample);

	writeArbGeoParams(dm);
}

void AbcMeshWriter::writeSubD(DerivedMesh *dm)
{
	std::vector<float> points, crease_sharpness;
	std::vector<int32_t> poly_verts, loop_counts;
	std::vector<int32_t> crease_indices, crease_lengths;

	get_vertices(dm, points);
	get_topology(dm, poly_verts, loop_counts);
	get_creases(dm, crease_indices, crease_lengths, crease_sharpness);

	if (m_first_frame) {
		/* create materials' facesets */
		writeCommonData(dm, m_subdiv_schema);
	}

	m_subdiv_sample = OSubDSchema::Sample(
	                      V3fArraySample(
	                          (const Imath::V3f *) &points.front(),
	                          points.size() / 3),
	                      Int32ArraySample(poly_verts),
	                      Int32ArraySample(loop_counts));

	UVSample sample;
	if (m_settings.export_uvs) {
		const char *name = get_uv_sample(sample, m_custom_data_config, &dm->loopData);

		if (!sample.indices.empty() && !sample.uvs.empty()) {
			OV2fGeomParam::Sample uv_sample;
			uv_sample.setVals(V2fArraySample(&sample.uvs[0], sample.uvs.size()));
			uv_sample.setIndices(UInt32ArraySample(&sample.indices[0], sample.indices.size()));
			uv_sample.setScope(kFacevaryingScope);

			m_subdiv_schema.setUVSourceName(name);
			m_subdiv_sample.setUVs(uv_sample);
		}

		write_custom_data(m_subdiv_schema.getArbGeomParams(), m_custom_data_config, &dm->loopData, CD_MLOOPUV);
	}

	if (!crease_indices.empty()) {
		m_subdiv_sample.setCreaseIndices(Int32ArraySample(crease_indices));
		m_subdiv_sample.setCreaseLengths(Int32ArraySample(crease_lengths));
		m_subdiv_sample.setCreaseSharpnesses(FloatArraySample(crease_sharpness));
	}

	m_subdiv_sample.setSelfBounds(bounds());
	m_subdiv_schema.set(m_subdiv_sample);

	writeArbGeoParams(dm);
}

template <typename Schema>
void AbcMeshWriter::writeCommonData(DerivedMesh *dm, Schema &schema)
{
	std::map< std::string, std::vector<int32_t> > geo_groups;
	getGeoGroups(dm, geo_groups);

	std::map< std::string, std::vector<int32_t>  >::iterator it;
	for (it = geo_groups.begin(); it != geo_groups.end(); ++it) {
		OFaceSet face_set = schema.createFaceSet(it->first);
		OFaceSetSchema::Sample samp;
		samp.setFaces(Int32ArraySample(it->second));
		face_set.getSchema().set(samp);
	}

	if (hasProperties(reinterpret_cast<ID *>(m_object->data))) {
		if (m_settings.export_props_as_geo_params) {
			writeProperties(reinterpret_cast<ID *>(m_object->data),
			                schema.getArbGeomParams(), false);
		}
		else {
			writeProperties(reinterpret_cast<ID *>(m_object->data),
			                schema.getUserProperties(), true);
		}
	}
}

DerivedMesh *AbcMeshWriter::getFinalMesh()
{
	/* We don't want subdivided mesh data */
	if (m_subsurf_mod) {
		m_subsurf_mod->mode |= eModifierMode_DisableTemporary;
	}

	DerivedMesh *dm = mesh_create_derived_render(m_scene, m_object, CD_MASK_MESH);

	if (m_subsurf_mod) {
		m_subsurf_mod->mode &= ~eModifierMode_DisableTemporary;
	}

	m_custom_data_config.pack_uvs = m_settings.pack_uv;
	m_custom_data_config.mpoly = dm->getPolyArray(dm);
	m_custom_data_config.mloop = dm->getLoopArray(dm);
	m_custom_data_config.totpoly = dm->getNumPolys(dm);
	m_custom_data_config.totloop = dm->getNumLoops(dm);
	m_custom_data_config.totvert = dm->getNumVerts(dm);

	return dm;
}

void AbcMeshWriter::freeMesh(DerivedMesh *dm)
{
	dm->release(dm);
}

void AbcMeshWriter::writeArbGeoParams(DerivedMesh *dm)
{
	if (m_is_liquid) {
		/* We don't need anything more for liquid meshes. */
		return;
	}

	if (m_settings.export_vcols) {
		if (m_subdiv_schema.valid()) {
			write_custom_data(m_subdiv_schema.getArbGeomParams(), m_custom_data_config, &dm->loopData, CD_MLOOPCOL);
		}
		else {
			write_custom_data(m_mesh_schema.getArbGeomParams(), m_custom_data_config, &dm->loopData, CD_MLOOPCOL);
		}
	}

	if (m_first_frame && m_has_per_face_materials) {
		std::vector<int32_t> faceVals;

		if (m_settings.export_face_sets || m_settings.export_mat_indices) {
			get_material_indices(dm, faceVals);
		}

		if (m_settings.export_face_sets) {
			OFaceSetSchema::Sample samp;
			samp.setFaces(Int32ArraySample(faceVals));
			m_face_set.getSchema().set(samp);
		}

		if (m_settings.export_mat_indices) {
			Alembic::AbcCoreAbstract::ArraySample samp(&(faceVals.front()),
			                                           m_mat_indices.getDataType(),
			                                           Alembic::Util::Dimensions(dm->getNumTessFaces(dm)));
			m_mat_indices.set(samp);
		}
	}
}

void AbcMeshWriter::getVelocities(DerivedMesh *dm, std::vector<float> &vels)
{
	const int totverts = dm->getNumVerts(dm);

	vels.clear();
	vels.resize(totverts * 3);

	ModifierData *md = get_liquid_sim_modifier(m_scene, m_object);
	FluidsimModifierData *fmd = reinterpret_cast<FluidsimModifierData *>(md);
	FluidsimSettings *fss = fmd->fss;

	if (fss->meshVelocities) {
		float *mesh_vels = reinterpret_cast<float *>(fss->meshVelocities);

		for (int i = 0; i < totverts; ++i) {
			copy_zup_yup(&vels[i * 3], mesh_vels);
			mesh_vels += 3;
		}
	}
	else {
		std::fill(vels.begin(), vels.end(), 0.0f);
	}
}

void AbcMeshWriter::getGeoGroups(
        DerivedMesh *dm,
        std::map<std::string, std::vector<int32_t> > &geo_groups)
{
	const int num_poly = dm->getNumPolys(dm);
	MPoly *polygons = dm->getPolyArray(dm);

	for (int i = 0; i < num_poly; ++i) {
		MPoly &current_poly = polygons[i];
		short mnr = current_poly.mat_nr;

		Material *mat = give_current_material(m_object, mnr + 1);

		if (!mat) {
			continue;
		}

		std::string name = get_id_name(&mat->id);

		if (geo_groups.find(name) == geo_groups.end()) {
			std::vector<int32_t> faceArray;
			geo_groups[name] = faceArray;
		}

		geo_groups[name].push_back(i);
	}

	if (geo_groups.size() == 0) {
		Material *mat = give_current_material(m_object, 1);

		std::string name = (mat) ? get_id_name(&mat->id) : "default";

		std::vector<int32_t> faceArray;

		for (int i = 0, e = dm->getNumTessFaces(dm); i < e; ++i) {
			faceArray.push_back(i);
		}

		geo_groups[name] = faceArray;
	}
}

/* ************************************************************************** */

/* Some helpers for mesh generation */
namespace utils {

void mesh_add_verts(Mesh *mesh, size_t len)
{
	if (len == 0) {
		return;
	}

	const int totvert = mesh->totvert + len;
	CustomData vdata;
	CustomData_copy(&mesh->vdata, &vdata, CD_MASK_MESH, CD_DEFAULT, totvert);
	CustomData_copy_data(&mesh->vdata, &vdata, 0, 0, mesh->totvert);

	if (!CustomData_has_layer(&vdata, CD_MVERT)) {
		CustomData_add_layer(&vdata, CD_MVERT, CD_CALLOC, NULL, totvert);
	}

	CustomData_free(&mesh->vdata, mesh->totvert);
	mesh->vdata = vdata;
	BKE_mesh_update_customdata_pointers(mesh, false);

	mesh->totvert = totvert;
}

static void mesh_add_mloops(Mesh *mesh, size_t len)
{
	if (len == 0) {
		return;
	}

	/* new face count */
	const int totloops = mesh->totloop + len;

	CustomData ldata;
	CustomData_copy(&mesh->ldata, &ldata, CD_MASK_MESH, CD_DEFAULT, totloops);
	CustomData_copy_data(&mesh->ldata, &ldata, 0, 0, mesh->totloop);

	if (!CustomData_has_layer(&ldata, CD_MLOOP)) {
		CustomData_add_layer(&ldata, CD_MLOOP, CD_CALLOC, NULL, totloops);
	}

	CustomData_free(&mesh->ldata, mesh->totloop);
	mesh->ldata = ldata;
	BKE_mesh_update_customdata_pointers(mesh, false);

	mesh->totloop = totloops;
}

static void mesh_add_mpolygons(Mesh *mesh, size_t len)
{
	if (len == 0) {
		return;
	}

	const int totpolys = mesh->totpoly + len;

	CustomData pdata;
	CustomData_copy(&mesh->pdata, &pdata, CD_MASK_MESH, CD_DEFAULT, totpolys);
	CustomData_copy_data(&mesh->pdata, &pdata, 0, 0, mesh->totpoly);

	if (!CustomData_has_layer(&pdata, CD_MPOLY)) {
		CustomData_add_layer(&pdata, CD_MPOLY, CD_CALLOC, NULL, totpolys);
	}

	CustomData_free(&mesh->pdata, mesh->totpoly);
	mesh->pdata = pdata;
	BKE_mesh_update_customdata_pointers(mesh, false);

	mesh->totpoly = totpolys;
}

static Material *find_material(Main *bmain, const char *name)
{
	Material *material = static_cast<Material *>(bmain->mat.first);
	Material *found_material = NULL;

	for (; material; material = static_cast<Material *>(material->id.next)) {
		if (BLI_strcaseeq(material->id.name+2, name) == true) {
			found_material = material;
			break;
		}
	}

	return found_material;
}

static void assign_materials(Main *bmain, Object *ob, const std::map<std::string, int> &mat_map)
{
	/* Clean up slots. */
	while (object_remove_material_slot(ob));

	bool can_assign = true;
	std::map<std::string, int>::const_iterator it = mat_map.begin();

	int matcount = 0;
	for (; it != mat_map.end(); ++it, ++matcount) {
		Material *curmat = give_current_material(ob, matcount);

		if (curmat != NULL) {
			continue;
		}

		if (!object_add_material_slot(ob)) {
			can_assign = false;
			break;
		}
	}

	if (can_assign) {
		it = mat_map.begin();

		for (; it != mat_map.end(); ++it) {
			std::string mat_name = it->first;
			Material *assigned_name = find_material(bmain, mat_name.c_str());

			if (assigned_name == NULL) {
				assigned_name = BKE_material_add(bmain, mat_name.c_str());
			}

			assign_material(ob, assigned_name, it->second, BKE_MAT_ASSIGN_OBJECT);
		}
	}
}

}  /* namespace utils */

/* ************************************************************************** */

template <typename Schema>
static bool has_animations(Schema &schema, ImportSettings *settings)
{
	if (settings->is_sequence) {
		return true;
	}

	if (!schema.isConstant()) {
		return true;
	}

	const ICompoundProperty &arb_geom_params = schema.getArbGeomParams();

	if (!arb_geom_params.valid()) {
		return false;
	}

	const size_t num_props = arb_geom_params.getNumProperties();

	for (size_t i = 0; i < num_props; ++i) {
		const Alembic::Abc::PropertyHeader &propHeader = arb_geom_params.getPropertyHeader(i);

		/* Check for animated UVs. */
		if (IV2fGeomParam::matches(propHeader) && Alembic::AbcGeom::isUV(propHeader)) {
            IV2fGeomParam uv_geom_param(arb_geom_params, propHeader.getName());

			if (!uv_geom_param.isConstant()) {
				return true;
			}
        }
	}

	return false;
}

AbcMeshReader::AbcMeshReader(const IObject &object, ImportSettings &settings, bool is_subd)
    : AbcObjectReader(object, settings)
{
	if (is_subd) {
		ISubD isubd_mesh(m_iobject, kWrapExisting);
		m_subd_schema = isubd_mesh.getSchema();
		get_min_max_time(m_subd_schema, m_min_time, m_max_time);
	}
	else {
		IPolyMesh ipoly_mesh(m_iobject, kWrapExisting);
		m_schema = ipoly_mesh.getSchema();
		get_min_max_time(m_schema, m_min_time, m_max_time);
	}
}

bool AbcMeshReader::valid() const
{
	return m_schema.valid() || m_subd_schema.valid();
}

void AbcMeshReader::readObjectData(Main *bmain, Scene *scene, float time)
{
	Mesh *mesh = BKE_mesh_add(bmain, m_data_name.c_str());

	const ISampleSelector sample_sel(time);
	const size_t poly_start = mesh->totpoly;

	bool is_constant = true;

	if (m_subd_schema.valid()) {
		is_constant = !has_animations(m_subd_schema, m_settings);

		const ISubDSchema::Sample sample = m_subd_schema.getValue(sample_sel);

		readVertexDataSample(mesh, sample.getPositions(), N3fArraySamplePtr());
		readPolyDataSample(mesh, sample.getFaceIndices(), sample.getFaceCounts(), N3fArraySamplePtr());
	}
	else {
		is_constant = !has_animations(m_schema, m_settings);

		const IPolyMeshSchema::Sample sample = m_schema.getValue(sample_sel);

		N3fArraySamplePtr vertex_normals, poly_normals;
		const IN3fGeomParam normals = m_schema.getNormalsParam();

		if (normals.valid()) {
			IN3fGeomParam::Sample normsamp = normals.getExpandedValue(sample_sel);

			if (normals.getScope() == Alembic::AbcGeom::kFacevaryingScope) {
				poly_normals = normsamp.getVals();
			}
			else if ((normals.getScope() == Alembic::AbcGeom::kVertexScope) ||
			         (normals.getScope() == Alembic::AbcGeom::kVaryingScope))
			{
				vertex_normals = normsamp.getVals();
			}
		}

		readVertexDataSample(mesh, sample.getPositions(), vertex_normals);
		readPolyDataSample(mesh, sample.getFaceIndices(), sample.getFaceCounts(), poly_normals);
	}

	BKE_mesh_validate(mesh, false, false);

	m_object = BKE_object_add(bmain, scene, OB_MESH, m_object_name.c_str());
	m_object->data = mesh;

	/* TODO: expose this as a setting to the user? */
	const bool assign_mat = true;

	if (assign_mat) {
		readFaceSetsSample(bmain, mesh, poly_start, sample_sel);
	}

	if (!is_constant) {
		addDefaultModifier(bmain);
	}
}

void AbcMeshReader::readVertexDataSample(Mesh *mesh,
                                         const P3fArraySamplePtr &positions,
                                         const N3fArraySamplePtr &normals)
{
	utils::mesh_add_verts(mesh, positions->size());
	read_mverts(mesh->mvert, positions, normals);
}

static void *add_customdata_cb(void *user_data, const char *name, int data_type)
{
	Mesh *mesh = static_cast<Mesh *>(user_data);
	CustomDataType cd_data_type = static_cast<CustomDataType>(data_type);
	void *cd_ptr = NULL;

	int index = -1;
	if (cd_data_type == CD_MLOOPUV) {
		index = ED_mesh_uv_texture_add(mesh, name, true);
		cd_ptr = CustomData_get_layer(&mesh->ldata, cd_data_type);
	}
	else if (cd_data_type == CD_MLOOPCOL) {
		index = ED_mesh_color_add(mesh, name, true);
		cd_ptr = CustomData_get_layer(&mesh->ldata, cd_data_type);
	}

	if (index == -1) {
		return NULL;
	}

	return cd_ptr;
}

#if 0
void print_scope(const std::string &prefix, const int scope, std::ostream &os)
{
	os << prefix << ": ";

	switch (scope) {
		case kVaryingScope:
			os << "Varying Scope";
			break;
		case kFacevaryingScope:
			os << "Face Varying Scope";
			break;
		case kVertexScope:
			os << "Vertex Scope";
			break;
	}

	os << '\n';
}
#endif

void AbcMeshReader::readPolyDataSample(Mesh *mesh,
                                       const Int32ArraySamplePtr &face_indices,
                                       const Int32ArraySamplePtr &face_counts,
                                       const N3fArraySamplePtr &normals)
{
	const size_t num_poly = face_counts->size();
	const size_t num_loops = face_indices->size();

	utils::mesh_add_mpolygons(mesh, num_poly);
	utils::mesh_add_mloops(mesh, num_loops);

	Alembic::AbcGeom::V2fArraySamplePtr uvs;
	Alembic::Abc::UInt32ArraySamplePtr uvs_indices;
	const IV2fGeomParam uv = (m_subd_schema.valid() ? m_subd_schema.getUVsParam()
	                                                : m_schema.getUVsParam());

	if (uv.valid()) {
		IV2fGeomParam::Sample uvsamp;
		uv.getIndexed(uvsamp, Alembic::Abc::ISampleSelector(0.0f));

		uvs = uvsamp.getVals();
		uvs_indices = uvsamp.getIndices();

		std::string name = Alembic::Abc::GetSourceName(uv.getMetaData());

		/* According to the convention, primary UVs should have had their name
		 * set using Alembic::Abc::SetSourceName, but you can't expect everyone
		 * to follow it! :) */
		if (name.empty()) {
			name = uv.getName();
		}

		ED_mesh_uv_texture_add(mesh, name.c_str(), true);
	}

	read_mpolys(mesh->mpoly, mesh->mloop, mesh->mloopuv, &mesh->ldata,
	            face_indices, face_counts, uvs, uvs_indices, normals);

	const ICompoundProperty &arb_geom_params = (m_schema.valid() ? m_schema.getArbGeomParams()
	                                                             : m_subd_schema.getArbGeomParams());

	CDStreamConfig config;
	config.mpoly = mesh->mpoly;
	config.mloop = mesh->mloop;
	config.totpoly = mesh->totpoly;
	config.totloop = mesh->totloop;
	config.user_data = mesh;
	config.add_customdata_cb = add_customdata_cb;

	read_custom_data(arb_geom_params, config, ISampleSelector(0.0f));
}

void AbcMeshReader::readFaceSetsSample(Main *bmain, Mesh *mesh, size_t poly_start,
                                       const ISampleSelector &sample_sel)
{
	std::vector<std::string> face_sets;

	if (m_subd_schema.valid()) {
		m_subd_schema.getFaceSetNames(face_sets);
	}
	else {
		m_schema.getFaceSetNames(face_sets);
	}

	if (face_sets.empty()) {
		return;
	}

	std::map<std::string, int> mat_map;
	int current_mat = 0;

	for (int i = 0; i < face_sets.size(); ++i) {
		const std::string &grp_name = face_sets[i];

		if (mat_map.find(grp_name) == mat_map.end()) {
			mat_map[grp_name] = 1 + current_mat++;
		}

		const int assigned_mat = mat_map[grp_name];

		const IFaceSet faceset = (m_subd_schema.valid() ? m_subd_schema.getFaceSet(grp_name)
		                                                : m_schema.getFaceSet(grp_name));

		if (!faceset.valid()) {
			continue;
		}

		const IFaceSetSchema face_schem = faceset.getSchema();
		const IFaceSetSchema::Sample face_sample = face_schem.getValue(sample_sel);
		const Int32ArraySamplePtr group_faces = face_sample.getFaces();
		const size_t num_group_faces = group_faces->size();

		for (size_t l = 0; l < num_group_faces; l++) {
			size_t pos = (*group_faces)[l] + poly_start;

			if (pos >= mesh->totpoly) {
				std::cerr << "Faceset overflow on " << faceset.getName() << '\n';
				break;
			}

			MPoly &poly = mesh->mpoly[pos];
			poly.mat_nr = assigned_mat - 1;
		}
	}

	utils::assign_materials(bmain, m_object, mat_map);
}

/* ************************************************************************** */

void read_mverts(MVert *mverts,
                 const Alembic::AbcGeom::P3fArraySamplePtr &positions,
                 const N3fArraySamplePtr &normals)
{
	for (int i = 0; i < positions->size(); ++i) {
		MVert &mvert = mverts[i];
		Imath::V3f pos_in = (*positions)[i];

		copy_yup_zup(mvert.co, pos_in.getValue());

		mvert.bweight = 0;

		if (normals) {
			Imath::V3f nor_in = (*normals)[i];

			short no[3];
			normal_float_to_short_v3(no, nor_in.getValue());

			copy_yup_zup(mvert.no, no);
		}
	}
}

void read_mpolys(MPoly *mpolys, MLoop *mloops, MLoopUV *mloopuvs, CustomData *ldata,
                 const Alembic::AbcGeom::Int32ArraySamplePtr &face_indices,
                 const Alembic::AbcGeom::Int32ArraySamplePtr &face_counts,
                 const Alembic::AbcGeom::V2fArraySamplePtr &uvs,
                 const Alembic::AbcGeom::UInt32ArraySamplePtr &uvs_indices,
                 const Alembic::AbcGeom::N3fArraySamplePtr &normals)
{
	float (*pnors)[3] = NULL;

	if (normals) {
		pnors = (float (*)[3])CustomData_get_layer(ldata, CD_NORMAL);

		if (!pnors) {
			pnors = (float (*)[3])CustomData_add_layer(ldata, CD_NORMAL, CD_CALLOC, NULL, normals->size());
		}
	}

	const bool do_normals = (normals && pnors);
	const bool do_uvs = (mloopuvs && uvs && uvs_indices);
	unsigned int loop_index = 0;
	unsigned int rev_loop_index = 0;
	unsigned int uv_index = 0;
	Imath::V3f nor;

	for (int i = 0; i < face_counts->size(); ++i) {
		const int face_size = (*face_counts)[i];

		MPoly &poly = mpolys[i];
		poly.loopstart = loop_index;
		poly.totloop = face_size;

		/* NOTE: Alembic data is stored in the reverse order. */
		rev_loop_index = loop_index + (face_size - 1);

		for (int f = 0; f < face_size; ++f, ++loop_index, --rev_loop_index) {
			MLoop &loop = mloops[rev_loop_index];
			loop.v = (*face_indices)[loop_index];

			if (do_normals) {
				poly.flag |= ME_SMOOTH;

				nor = (*normals)[loop_index];
				copy_yup_zup(pnors[rev_loop_index], nor.getValue());
			}

			if (do_uvs) {
				MLoopUV &loopuv = mloopuvs[rev_loop_index];

				uv_index = (*uvs_indices)[loop_index];
				loopuv.uv[0] = (*uvs)[uv_index][0];
				loopuv.uv[1] = (*uvs)[uv_index][1];
			}
		}
	}
}
