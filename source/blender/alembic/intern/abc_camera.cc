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

#include "abc_camera.h"

#include "abc_archive.h"
#include "abc_transform.h"
#include "abc_util.h"

extern "C" {
#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_camera_types.h"
#include "DNA_cachefile_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RNA_access.h"

#include "BKE_camera.h"
#include "BKE_fcurve.h"
#include "BKE_library.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "BLI_math.h"
#include "BLI_string.h"

#include "ED_keyframing.h"
}

using Alembic::AbcGeom::ICamera;
using Alembic::AbcGeom::ICompoundProperty;
using Alembic::AbcGeom::IFloatProperty;
using Alembic::AbcGeom::ISampleSelector;

using Alembic::AbcGeom::OCamera;
using Alembic::AbcGeom::OFloatProperty;

using Alembic::AbcGeom::CameraSample;
using Alembic::AbcGeom::kWrapExisting;

/* ************************************************************************** */

AbcCameraWriter::AbcCameraWriter(Scene *scene,
                                 Object *ob,
                                 AbcTransformWriter *parent,
                                 uint32_t time_sampling,
                                 ExportSettings &settings)
    : AbcObjectWriter(scene, ob, time_sampling, settings, parent)
{
	OCamera camera(parent->alembicXform(), m_name, m_time_sampling);
	m_camera_schema = camera.getSchema();

	m_custom_data_container = m_camera_schema.getUserProperties();
	m_stereo_distance = OFloatProperty(m_custom_data_container, "stereoDistance", m_time_sampling);
	m_eye_separation = OFloatProperty(m_custom_data_container, "eyeSeparation", m_time_sampling);
}

void AbcCameraWriter::do_write()
{
	Camera *cam = static_cast<Camera *>(m_object->data);

	m_stereo_distance.set(cam->stereo.convergence_distance);
	m_eye_separation.set(cam->stereo.interocular_distance);

	const double apperture_x = cam->sensor_x / 10.0;
	const double apperture_y = cam->sensor_y / 10.0;
	const double film_aspect = apperture_x / apperture_y;

	m_camera_sample.setFocalLength(cam->lens);
	m_camera_sample.setHorizontalAperture(apperture_x);
	m_camera_sample.setVerticalAperture(apperture_y);
	m_camera_sample.setHorizontalFilmOffset(apperture_x * cam->shiftx);
	m_camera_sample.setVerticalFilmOffset(apperture_y * cam->shifty * film_aspect);
	m_camera_sample.setNearClippingPlane(cam->clipsta);
	m_camera_sample.setFarClippingPlane(cam->clipend);

	if (cam->dof_ob) {
		Imath::V3f v(m_object->loc[0] - cam->dof_ob->loc[0],
		        m_object->loc[1] - cam->dof_ob->loc[1],
		        m_object->loc[2] - cam->dof_ob->loc[2]);
		m_camera_sample.setFocusDistance(v.length());
	}
	else {
		m_camera_sample.setFocusDistance(cam->gpu_dof.focus_distance);
	}

	/* Blender camera does not have an fstop param, so try to find a custom prop
	 * instead. */
	m_camera_sample.setFStop(cam->gpu_dof.fstop);

	m_camera_sample.setLensSqueezeRatio(1.0);
	m_camera_schema.set(m_camera_sample);
}

/* ************************************************************************** */

AbcCameraReader::AbcCameraReader(const Alembic::Abc::IObject &object, ImportSettings &settings)
    : AbcObjectReader(object, settings)
{
	ICamera abc_cam(m_iobject, kWrapExisting);
	m_schema = abc_cam.getSchema();

	get_min_max_time(m_iobject, m_schema, m_min_time, m_max_time);
}

bool AbcCameraReader::valid() const
{
	return m_schema.valid();
}

void AbcCameraReader::readObjectData(Main *bmain, float time)
{
	Camera *bcam = static_cast<Camera *>(BKE_camera_add(bmain, m_data_name.c_str()));

	ISampleSelector sample_sel(time);
	CameraSample cam_sample;
	m_schema.get(cam_sample, sample_sel);

	ICompoundProperty customDataContainer = m_schema.getUserProperties();

	if (customDataContainer.valid() &&
	    customDataContainer.getPropertyHeader("stereoDistance") &&
	    customDataContainer.getPropertyHeader("eyeSeparation"))
	{
		IFloatProperty convergence_plane(customDataContainer, "stereoDistance");
		IFloatProperty eye_separation(customDataContainer, "eyeSeparation");

		bcam->stereo.interocular_distance = eye_separation.getValue(sample_sel);
		bcam->stereo.convergence_distance = convergence_plane.getValue(sample_sel);
	}

	const float lens = cam_sample.getFocalLength();
	const float apperture_x = cam_sample.getHorizontalAperture();
	const float apperture_y = cam_sample.getVerticalAperture();
	const float h_film_offset = cam_sample.getHorizontalFilmOffset();
	const float v_film_offset = cam_sample.getVerticalFilmOffset();
	const float film_aspect = apperture_x / apperture_y;

	bcam->lens = lens;
	bcam->sensor_x = apperture_x * 10;
	bcam->sensor_y = apperture_y * 10;
	bcam->shiftx = h_film_offset / apperture_x;
	bcam->shifty = v_film_offset / apperture_y / film_aspect;
	bcam->clipsta = max_ff(0.1f, cam_sample.getNearClippingPlane());
	bcam->clipend = cam_sample.getFarClippingPlane();
	bcam->gpu_dof.focus_distance = cam_sample.getFocusDistance();
	bcam->gpu_dof.fstop = cam_sample.getFStop();

	m_object = BKE_object_add_only_object(bmain, OB_CAMERA, m_object_name.c_str());
	m_object->data = bcam;
}

void AbcCameraReader::setupAnimationData(Scene *scene, float time)
{
	if (m_schema.isConstant()) {
		return;
	}

	Camera *bcam = static_cast<Camera *>(m_object->data);

	/* Verify that the FOV is animated. */

	bool fixed_fov = true;
	const float orig_fov = bcam->lens;

	const Alembic::Abc::TimeSamplingPtr &time_samp = m_schema.getTimeSampling();
	CameraSample cam_sample;

	for (size_t i = 0, e = m_schema.getNumSamples(); i < e; ++i) {
		const chrono_t samp_time = time_samp->getSampleTime(i);

		ISampleSelector sample_sel(samp_time);
		m_schema.get(cam_sample, sample_sel);

		if (orig_fov != cam_sample.getFocalLength()) {
			fixed_fov = false;
			break;
		}
	}

	if (fixed_fov) {
		return;
	}

	/* Setup data for keyframes. */
	PointerRNA cam_ptr;
	RNA_pointer_create(&bcam->id, &RNA_Camera, bcam, &cam_ptr);

	PropertyRNA *prop = RNA_struct_find_property(&cam_ptr, "lens");

	char *path = RNA_path_from_ID_to_property(&cam_ptr, prop);

	ToolSettings *ts = scene->toolsettings;

	ReportList reports;
	const bool success = insert_keyframe(&reports,
	                                     &bcam->id,
	                                     /* bAction *act= */ NULL,
	                                     /* const char group[]= */ NULL,
	                                     path,
	                                     /* index= */ -1,
	                                     time,
	                                     ts->keyframe_type,
	                                     0);

	MEM_freeN(path);

	if (!success) {
		/* Report error */
		std::cerr << "Alembic reader: cannot add keyframe to camera focal lens\n";
		return;
	}

	bool driven;
	FCurve *fcu = id_data_find_fcurve(&bcam->id, bcam, &RNA_Camera, "lens", 0, &driven);
	FModifier *fcm = add_fmodifier(&fcu->modifiers, FMODIFIER_TYPE_CACHE);

	if (fcm == NULL) {
		std::cerr << "Alembic reader: cannot add cache modifier to fcurve\n";
	}

	set_active_fmodifier(&fcu->modifiers, fcm);

	FMod_Cache *cache_mod = static_cast<FMod_Cache *>(fcm->data);

	cache_mod->cache_file = m_settings->cache_file;
	id_us_plus(&cache_mod->cache_file->id);

	BLI_strncpy(cache_mod->object_path, m_iobject.getFullName().c_str(), FILE_MAX);
	BLI_strncpy(cache_mod->property, "focal_length", 64);

	cache_mod->reader = reinterpret_cast<CacheReader *>(this);
	this->incref();

	reinterpret_cast<ArchiveReader *>(m_settings->cache_file->handle)->add_fcurve(cache_mod);
}
