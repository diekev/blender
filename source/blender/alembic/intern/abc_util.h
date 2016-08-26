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

#ifndef __ABC_UTIL_H__
#define __ABC_UTIL_H__

#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>

#include "abc_object.h"

#ifdef _MSC_VER
#  define ABC_INLINE static __forceinline
#else
#  define ABC_INLINE static inline
#endif

using Alembic::Abc::chrono_t;

class ImportSettings;

struct ID;
struct Object;

std::string get_id_name(ID *id);
std::string get_id_name(Object *ob);
std::string get_object_dag_path_name(Object *ob, Object *dupli_parent);

bool object_selected(Object *ob);
bool parent_selected(Object *ob);

Imath::M44d convert_matrix(float mat[4][4]);
void create_transform_matrix(ImportSettings *settings, float r_mat[4][4]);
void create_transform_matrix(ExportSettings *settings, Object *obj, float transform_mat[4][4]);

void split(const std::string &s, const char delim, std::vector<std::string> &tokens);

template<class TContainer>
bool begins_with(const TContainer &input, const TContainer &match)
{
	return input.size() >= match.size()
	        && std::equal(match.begin(), match.end(), input.begin());
}

void create_input_transform(ImportSettings *settings, const Alembic::AbcGeom::ISampleSelector &sample_sel,
                            const Alembic::AbcGeom::IXform &ixform, Object *ob,
                            float r_mat[4][4], float scale, bool has_alembic_parent = false);

template <typename Schema>
void get_min_max_time_ex(const Schema &schema, chrono_t &min, chrono_t &max)
{
	const Alembic::Abc::TimeSamplingPtr &time_samp = schema.getTimeSampling();

	if (!schema.isConstant()) {
		const size_t num_samps = schema.getNumSamples();

		if (num_samps > 0) {
			const chrono_t min_time = time_samp->getSampleTime(0);
			min = std::min(min, min_time);

			const chrono_t max_time = time_samp->getSampleTime(num_samps - 1);
			max = std::max(max, max_time);
		}
	}
}

template <typename Schema>
void get_min_max_time(const Alembic::AbcGeom::IObject &object, const Schema &schema, chrono_t &min, chrono_t &max)
{
	get_min_max_time_ex(schema, min, max);

	const Alembic::AbcGeom::IObject &parent = object.getParent();
	if (parent.valid() && Alembic::AbcGeom::IXform::matches(parent.getMetaData())) {
		Alembic::AbcGeom::IXform xform(parent, Alembic::AbcGeom::kWrapExisting);
		get_min_max_time_ex(xform.getSchema(), min, max);
	}
}

bool has_property(const Alembic::Abc::ICompoundProperty &prop, const std::string &name);

/* ************************** */

/* TODO(kevin): for now keeping these transformations hardcoded to make sure
 * everything works properly, and also because Alembic is almost exclusively
 * used in Y-up software, but eventually they'll be set by the user in the UI
 * like other importers/exporters do, to support other axis. */

/* Copy from Y-up to Z-up. */

#include "BLI_math.h"

ABC_INLINE void copy_yup_zup(ImportSettings *settings, float zup[3], const float yup[3])
{
	copy_v3_v3(zup, yup);

	if (settings->do_axis_transform) {
		mul_m3_v3(settings->rotation_matrix, zup);
	}
}

ABC_INLINE void copy_yup_zup(ImportSettings *settings, short zup[3], const short yup[3])
{
	copy_v3_v3_short(zup, yup);

	if (settings->do_axis_transform) {
		float tmp[3];
		normal_short_to_float_v3(tmp, yup);

		mul_m3_v3(settings->rotation_matrix, tmp);

		normal_float_to_short_v3(zup, tmp);
	}
}

/* Copy from Z-up to Y-up. */

ABC_INLINE void copy_zup_yup(ExportSettings *settings, float yup[3], const float zup[3])
{
	copy_v3_v3(yup, zup);

	if (settings->do_convert_axis) {
		mul_m3_v3(settings->convert_matrix, yup);
	}
}

ABC_INLINE void copy_zup_yup(ExportSettings *settings, short yup[3], const short zup[3])
{
	copy_v3_v3_short(yup, zup);

	if (settings->do_convert_axis) {
		float tmp[3];
		normal_short_to_float_v3(tmp, zup);

		mul_m3_v3(settings->convert_matrix, tmp);

		normal_float_to_short_v3(yup, tmp);
	}
}

#endif  /* __ABC_UTIL_H__ */
