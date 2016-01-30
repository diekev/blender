/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __LEVELSET_H__
#define __LEVELSET_H__

#ifdef __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wfloat-conversion"
#	pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>
#include <openvdb/tools/RayIntersector.h>

#ifdef __GNUC__
#	pragma GCC diagnostic pop
#endif

#include "kernel_types.h"
#include "util_map.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class LevelSet;
class Progress;
class Scene;

void OpenVDB_initialize();
void OpenVDB_file_info(const char* filename);
LevelSet *OpenVDB_file_read(const char* filename, Scene* scene);
void OpenVDB_use_level_mesh(Scene* scene);
void OpenVDB_file_read_to_levelset(const char* filename, Scene* scene, LevelSet* levelset, int shader );

#if defined(_MSC_VER) && (defined(CYCLES_TR1_UNORDERED_MAP) || defined(CYCLES_STD_UNORDERED_MAP) || defined(CYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE))
struct pthread_hash {
	size_t operator()(const pthread_t& val) const
	{
		/* not really sure how to hash a pthread_t, since it could be implemented as a struct */
		size_t res;
		memcpy(&res, &val, sizeof(size_t) > sizeof(pthread_t)? sizeof(pthread_t): sizeof(size_t));
		return res;
	}
};

struct pthread_equal_to : std::binary_function<pthread_t, pthread_t, bool> {
	bool operator()(const pthread_t& x, const pthread_t& y) const
	{
		return pthread_equal(x, y);
	}
};
#endif

class LevelSet {
public:
	LevelSet( );
       LevelSet(openvdb::FloatGrid::Ptr gridPtr, int shader_);
	   LevelSet(const LevelSet& levelset);
       ~LevelSet();

       void tag_update(Scene *scene);

	   void initialize(openvdb::FloatGrid::Ptr& gridPtr, int shader_);
       bool intersect(const Ray* ray, Intersection *isect);

	   openvdb::FloatGrid::Ptr grid;
       int shader;

	   typedef openvdb::tools::LinearSearchImpl<openvdb::FloatGrid, 1, double> LinearSearchImpl;
	   typedef openvdb::math::Ray<double> vdb_ray_t;
       typedef openvdb::tools::LevelSetRayIntersector<openvdb::FloatGrid,
	                                                  LinearSearchImpl,
	                                                  openvdb::FloatTree::RootNodeType::ChildNodeType::LEVEL,
	                                                  vdb_ray_t> isect_t;

#if defined(_MSC_VER) && (defined(CYCLES_TR1_UNORDERED_MAP) || defined(CYCLES_STD_UNORDERED_MAP) || defined(CYCLES_STD_UNORDERED_MAP_IN_TR1_NAMESPACE))
	   typedef unordered_map<pthread_t, isect_t *,
	   pthread_hash, pthread_equal_to > isect_map_t;
#else
	   typedef unordered_map<pthread_t, isect_t * > isect_map_t;
#endif
	   isect_map_t isect_map;
};

class LevelSetManager {
public:
       bool need_update;

       LevelSetManager();
       ~LevelSetManager();

       void device_update(Device *device, DeviceScene *dscene, Scene *scene, Progress& progress);
       void device_free(Device *device, DeviceScene *dscene);

       void tag_update(Scene *scene);
};

CCL_NAMESPACE_END

#endif /* __LEVELSET_H__ */
