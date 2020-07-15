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

#ifndef __ALEMBIC_H__
#define __ALEMBIC_H__

#include "graph/node.h"
#include "render/procedural.h"
#include "util/util_vector.h"

//#ifdef WITH_ALEMBIC

#include <Alembic/AbcCoreFactory/All.h>
#include <Alembic/AbcGeom/All.h>

using namespace Alembic::AbcGeom;

CCL_NAMESPACE_BEGIN

class Shader;

class AlembicObject : public Node {
 public:
  NODE_DECLARE

  AlembicObject();
  ~AlembicObject();

  ustring path;
  Shader *shader;
};

class AlembicProcedural : public Procedural {
 public:
  NODE_DECLARE

  typedef std::pair<const ustring, Shader *> pathShaderType;

  AlembicProcedural();
  ~AlembicProcedural();
  void create(Scene *scene);
  //    void set_transform(MotionTransform& tfm) {
  //        parentTransform = tfm;
  //    }

  bool use_motion_blur;
  ustring filepath;
  float frame;
  float frameRate;
  array<AlembicObject *> objects;
  // MotionTransform parentTransform;

 private:
  void read_mesh(Scene *scene, Shader *shader, Transform xform, IPolyMesh &mesh);
  void read_curves(Scene *scene, Shader *shader, Transform xform, ICurves &curves);
};

CCL_NAMESPACE_END

#endif /* __ALEMBIC_H__ */
