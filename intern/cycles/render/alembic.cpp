/*
 * Copyright 2011-2018 Blender Foundation
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

#include "alembic.h"

#include <algorithm>
#include <fnmatch.h>
#include <iterator>
#include <set>
#include <sstream>
#include <stack>
#include <stdio.h>
#include <vector>

#include "render/camera.h"
#include "render/curves.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"
#include "render/shader.h"

#include "util/util_foreach.h"
#include "util/util_transform.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

// class AlembicMotionTransform : public MotionTransform {
// public:
//    AlembicMotionTransform() {
//        pre = transform_identity();
//        mid = transform_identity();
//        post = transform_identity();
//    }
//    AlembicMotionTransform(const MotionTransform& a) {
//        pre = a.pre;
//        mid = a.mid;
//        post = a.post;
//    }
//    AlembicMotionTransform(const Transform& a) {
//        pre = a;
//        mid = a;
//        post = a;
//    }

//    AlembicMotionTransform operator*(const AlembicMotionTransform a) {
//        AlembicMotionTransform c;
//        c.pre = pre * a.pre;
//        c.mid = mid * a.mid;
//        c.post = post * a.post;
//        return c;
//    }
//};

NODE_DEFINE(AlembicObject)
{
  NodeType *type = NodeType::add("alembic_object", create);
  SOCKET_STRING(path, "Alembic Path", ustring());
  SOCKET_NODE(shader, "Shader", &Shader::node_type);

  return type;
}

AlembicObject::AlembicObject() : Node(node_type)
{
}

AlembicObject::~AlembicObject()
{
}

NODE_DEFINE(AlembicProcedural)
{
  NodeType *type = NodeType::add("alembic", create);

  SOCKET_BOOLEAN(use_motion_blur, "Use Motion Blur", false);

  SOCKET_STRING(filepath, "Filename", ustring());
  SOCKET_FLOAT(frame, "Frame", 1.0);
  SOCKET_FLOAT(frameRate, "Frame Rate", 24.0);

  SOCKET_NODE_ARRAY(objects, "Objects", &AlembicObject::node_type);

  return type;
}

AlembicProcedural::AlembicProcedural() : Procedural(node_type)
{
}

AlembicProcedural::~AlembicProcedural()
{
}

static Transform make_transform(const Abc::M44d &a)
{
  Transform trans;
  for (int j = 0; j < 4; j++)
    for (int i = 0; i < 4; i++)
      trans[j][i] = static_cast<float>(a[i][j]);
  return trans;
}

void AlembicProcedural::create(Scene *scene)
{
  Alembic::AbcCoreFactory::IFactory factory;
  factory.setPolicy(ErrorHandler::kQuietNoopPolicy);
  IArchive archive = factory.getArchive(filepath.c_str());

  /*
   * Traverse Alembic file hierarchy, avoiding recursion by
   * using an explicit stack
   */
  std::stack<std::pair<IObject, Transform>> objstack;
  objstack.push(std::pair<IObject, Transform>(archive.getTop(), transform_identity()));

  while (!objstack.empty()) {
    std::pair<IObject, Transform> obj = objstack.top();
    objstack.pop();

    string path = obj.first.getFullName();
    vector<pathShaderType>::const_iterator it;
    Transform currmatrix = obj.second;

    Shader *shader = NULL;

    for (int i = 0; i < objects.size(); i++) {
      if (fnmatch(objects[i]->path.c_str(), path.c_str(), 0) == 0) {
        shader = objects[i]->shader;
      }
    }

    if (IXform::matches(obj.first.getHeader())) {
      IXform xform(obj.first, Alembic::Abc::kWrapExisting);
      XformSample samp = xform.getSchema().getValue(
          ISampleSelector((Abc::chrono_t)(frame / frameRate)));
      Transform ax = make_transform(samp.getMatrix());
      //      AlembicMotionTransform ax;
      //      ax.mid = make_transform(samp.getMatrix());
      //      if(use_motion_blur) {
      //        float shutteropentime = (frame - scene->camera->shuttertime/2.0f) / frameRate;
      //        float shutterclosetime = (frame + scene->camera->shuttertime/2.0f) / frameRate;
      //        samp = xform.getSchema().getValue(ISampleSelector((Abc::chrono_t)
      //        shutteropentime)); ax.pre = make_transform(samp.getMatrix()); samp =
      //        xform.getSchema().getValue(ISampleSelector((Abc::chrono_t) shutterclosetime));
      //        ax.post = make_transform(samp.getMatrix());
      //      }
      currmatrix = currmatrix * ax;
    }
    else if (IPolyMesh::matches(obj.first.getHeader()) &&
             shader) { /* "shader" means matched some path pattern */
      IPolyMesh mesh(obj.first, Alembic::Abc::kWrapExisting);
      read_mesh(scene, shader, currmatrix, mesh);
    }
    else if (ICurves::matches(obj.first.getHeader()) && shader) {
      ICurves curves(obj.first, Alembic::Abc::kWrapExisting);
      read_curves(scene, shader, currmatrix, curves);
    }

    for (int i = 0; i < obj.first.getNumChildren(); i++)
      objstack.push(std::pair<IObject, Transform>(obj.first.getChild(i), currmatrix));
  }
}

M44d convert_yup_zup(M44d &mtx)
{
  Imath::Vec3<double> scale, shear, rot, trans;
  extractSHRT(mtx, scale, shear, rot, trans);
  M44d rotmat, scalemat, transmat;
  rotmat.setEulerAngles(Imath::Vec3<double>(rot.x, -rot.z, rot.y));
  scalemat.setScale(Imath::Vec3<double>(scale.x, scale.z, scale.y));
  transmat.setTranslation(Imath::Vec3<double>(trans.x, -trans.z, trans.y));
  return scalemat * rotmat * transmat;
}

void AlembicProcedural::read_curves(Scene *scene, Shader *shader, Transform xform, ICurves &curves)
{
  Hair *hair = new Hair();
  scene->geometry.push_back(hair);
  hair->used_shaders.push_back(shader);
  hair->use_motion_blur = use_motion_blur;

  /* create object*/
  Object *object = new Object();
  object->geometry = hair;
  object->tfm = xform;  // transform_scale(1.0, 1.0, -1.0) * xform.mid;
  //  object->motion = AlembicMotionTransform(transform_scale(1.0, 1.0, -1.0)) * xform;
  //  object->use_motion = use_motion_blur;
  scene->objects.push_back(object);

  float frameTime = frame / frameRate;

  ICurvesSchema::Sample samp = curves.getSchema().getValue(
      ISampleSelector((Abc::chrono_t)frameTime));

  hair->reserve_curves(samp.getNumCurves(), samp.getPositions()->size());

  Abc::Int32ArraySamplePtr curveNumVerts = samp.getCurvesNumVertices();
  int offset = 0;
  for (int i = 0; i < curveNumVerts->size(); i++) {
    int numVerts = curveNumVerts->get()[i];
    for (int j = 0; j < numVerts; j++) {
      Imath::Vec3<float> f = samp.getPositions()->get()[offset + j];
      hair->add_curve_key(make_float3(f[0], f[1], f[2]), 0.01f);
    }
    hair->add_curve(offset, 0);
    offset += numVerts;
  }

  if (use_motion_blur) {
    Attribute *attr = hair->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    float3 *fdata = attr->data_float3();
    float shuttertimes[2] = {-scene->camera->shuttertime / 2.0f,
                             scene->camera->shuttertime / 2.0f};
    AbcA::TimeSamplingPtr ts = curves.getSchema().getTimeSampling();
    for (int i = 0; i < 2; i++) {
      frameTime = (frame + shuttertimes[i]) / frameRate;
      std::pair<index_t, chrono_t> idx = ts->getNearIndex((Abc::chrono_t)frameTime,
                                                          curves.getSchema().getNumSamples());
      ICurvesSchema::Sample shuttersamp = curves.getSchema().getValue(idx.first);
      for (int i = 0; i < shuttersamp.getPositions()->size(); i++) {
        Imath::Vec3<float> f = shuttersamp.getPositions()->get()[i];
        float3 p = make_float3(f[0], f[1], f[2]);
        *fdata++ = p;
      }
    }
  }

#if 0
  IV2fGeomParam uvs = polymesh.getSchema().getUVsParam();
  if(uvs.valid()) {
    switch(uvs.getScope()) {
      case kVaryingScope:
      case kVertexScope:
      {
        IV2fGeomParam::Sample uvsample = uvs.getExpandedValue();
      }
        break;
      case kFacevaryingScope:
      {
        IV2fGeomParam::Sample uvsample = uvs.getIndexedValue();

        ustring name = ustring("UVMap");
        Attribute *attr = mesh->attributes.add(ATTR_STD_UV, name);
        float3 *fdata = attr->data_float3();

        /* loop over the triangles */
        index_offset = 0;
        const unsigned int *uvIndices = uvsample.getIndices()->get();
        const Imath::Vec2<float> *uvValues = uvsample.getVals()->get();

#  ifndef NDEBUG
        int num_uvValues = uvsample.getVals()->size();
#  endif

        for(size_t i = 0; i < numFaces; i++) {
          for(int j = 0; j < faceCounts[i]-2; j++) {
            int v0 = uvIndices[index_offset];
            int v1 = uvIndices[index_offset + j + 1];
            int v2 = uvIndices[index_offset + j + 2];

            assert(v0*2+1 < num_uvValues);
            assert(v1*2+1 < num_uvValues);
            assert(v2*2+1 < num_uvValues);

            fdata[0] = make_float3(uvValues[v0*2][0], uvValues[v0*2][1], 0.0);
            fdata[1] = make_float3(uvValues[v1*2][0], uvValues[v1*2][1], 0.0);
            fdata[2] = make_float3(uvValues[v2*2][0], uvValues[v2*2][1], 0.0);
            fdata += 3;
          }
          index_offset += faceCounts[i];
        }
      }
        break;
      default:
        break;
    }
  }

#endif

  /* we don't yet support arbitrary attributes, for now add vertex
   * coordinates as generated coordinates if requested */
  //  if(hair->need_attribute(scene, ATTR_STD_GENERATED)) {
  //    Attribute *attr = hair->attributes.add(ATTR_STD_GENERATED);
  //    memcpy(attr->data_float3(), hair->verts.data(), sizeof(float3)*hair->verts.size());
  //  }
}

void AlembicProcedural::read_mesh(Scene *scene,
                                  Shader *shader,
                                  Transform xform,
                                  IPolyMesh &polymesh)
{
  Mesh *mesh = new Mesh();
  scene->geometry.push_back(mesh);
  mesh->used_shaders.push_back(shader);
  mesh->use_motion_blur = use_motion_blur;

  /* create object*/
  Object *object = new Object();
  object->geometry = mesh;
  object->tfm = xform;
  //  object->tfm = transform_scale(1.0, 1.0, -1.0) * xform.mid;
  //  object->motion = AlembicMotionTransform(transform_scale(1.0, 1.0, -1.0)) * xform;
  //  object->use_motion = use_motion_blur;
  scene->objects.push_back(object);

  float frameTime = frame / frameRate;

  IPolyMeshSchema::Sample samp = polymesh.getSchema().getValue(
      ISampleSelector((Abc::chrono_t)frameTime));

  int num_triangles = 0;
  for (int i = 0; i < samp.getFaceCounts()->size(); i++) {
    num_triangles += samp.getFaceCounts()->get()[i] - 2;
  }

  mesh->reserve_mesh(samp.getPositions()->size(), num_triangles);
  for (int i = 0; i < samp.getPositions()->size(); i++) {
    Imath::Vec3<float> f = samp.getPositions()->get()[i];
    mesh->verts.push_back_reserved(make_float3(f[0], f[1], f[2]));
  }

  if (use_motion_blur) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
    float3 *fdata = attr->data_float3();
    float shuttertimes[2] = {-scene->camera->shuttertime / 2.0f,
                             scene->camera->shuttertime / 2.0f};
    AbcA::TimeSamplingPtr ts = polymesh.getSchema().getTimeSampling();
    for (int i = 0; i < 2; i++) {
      frameTime = (frame + shuttertimes[i]) / frameRate;
      std::pair<index_t, chrono_t> idx = ts->getNearIndex((Abc::chrono_t)frameTime,
                                                          polymesh.getSchema().getNumSamples());
      IPolyMeshSchema::Sample shuttersamp = polymesh.getSchema().getValue(idx.first);
      for (int i = 0; i < shuttersamp.getPositions()->size(); i++) {
        Imath::Vec3<float> f = shuttersamp.getPositions()->get()[i];
        float3 p = make_float3(f[0], f[1], f[2]);
        *fdata++ = p;
      }
    }
  }

  /* create triangles */
  int index_offset = 0;

  int numFaces = samp.getFaceCounts()->size();
  const int *faceIndices = samp.getFaceIndices()->get();
  const int *faceCounts = samp.getFaceCounts()->get();

  for (size_t i = 0; i < numFaces; i++) {
    for (int j = 0; j < faceCounts[i] - 2; j++) {
      int v0 = faceIndices[index_offset];
      int v1 = faceIndices[index_offset + j + 1];
      int v2 = faceIndices[index_offset + j + 2];

      assert(v0 < (int)mesh->verts.size());
      assert(v1 < (int)mesh->verts.size());
      assert(v2 < (int)mesh->verts.size());

      mesh->add_triangle(v0, v1, v2, 0, 1);
    }

    index_offset += samp.getFaceCounts()->get()[i];
  }

  IV2fGeomParam uvs = polymesh.getSchema().getUVsParam();
  if (uvs.valid()) {
    switch (uvs.getScope()) {
      case kVaryingScope:
      case kVertexScope: {
        IV2fGeomParam::Sample uvsample = uvs.getExpandedValue();
        break;
      }
      case kFacevaryingScope: {
        IV2fGeomParam::Sample uvsample = uvs.getIndexedValue();

        ustring name = ustring("UVMap");
        Attribute *attr = mesh->attributes.add(ATTR_STD_UV, name);
        float3 *fdata = attr->data_float3();

        /* loop over the triangles */
        index_offset = 0;
        const unsigned int *uvIndices = uvsample.getIndices()->get();
        const Imath::Vec2<float> *uvValues = uvsample.getVals()->get();

#ifndef NDEBUG
        int num_uvValues = uvsample.getVals()->size();
#endif

        for (size_t i = 0; i < numFaces; i++) {
          for (int j = 0; j < faceCounts[i] - 2; j++) {
            int v0 = uvIndices[index_offset];
            int v1 = uvIndices[index_offset + j + 1];
            int v2 = uvIndices[index_offset + j + 2];

            assert(v0 < num_uvValues);
            assert(v1 < num_uvValues);
            assert(v2 < num_uvValues);

            fdata[0] = make_float3(uvValues[v0][0], uvValues[v0][1], 0.0);
            fdata[1] = make_float3(uvValues[v1][0], uvValues[v1][1], 0.0);
            fdata[2] = make_float3(uvValues[v2][0], uvValues[v2][1], 0.0);
            fdata += 3;
          }
          index_offset += faceCounts[i];
        }
        break;
      }
      default: {
        break;
      }
    }
  }

  /* we don't yet support arbitrary attributes, for now add vertex
   * coordinates as generated coordinates if requested */
  if (mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED);
    memcpy(attr->data_float3(), mesh->verts.data(), sizeof(float3) * mesh->verts.size());
  }
}

CCL_NAMESPACE_END
