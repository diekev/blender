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

#include "render/particles.h"
#include "device/device.h"
#include "render/scene.h"
#include "render/stats.h"

#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_map.h"
#include "util/util_progress.h"
#include "util/util_vector.h"

CCL_NAMESPACE_BEGIN

/* Particle System */

NODE_DEFINE(ParticleSystem)
{
  NodeType *type = NodeType::add("particle_system", create);
  return type;
}

ParticleSystem::ParticleSystem() : Node(node_type)
{
}

ParticleSystem::~ParticleSystem()
{
}

void ParticleSystem::tag_update(Scene *scene)
{
  scene->get_particle_system_manager()->need_update = true;
}

/* Particle System Manager */

ParticleSystemManager::ParticleSystemManager()
{
  need_update = true;
}

ParticleSystemManager::~ParticleSystemManager()
{
}

void ParticleSystemManager::device_update_particles(Device *,
                                                    DeviceScene *dscene,
                                                    Scene *scene,
                                                    Progress &progress)
{
  /* count particles.
   * adds one dummy particle at the beginning to avoid invalid lookups,
   * in case a shader uses particle info without actual particle data. */
  int num_particles = 1;
  foreach (ParticleSystem *psys, scene->get_particle_systems())
    num_particles += psys->get_particles().size();

  KernelParticle *kparticles = dscene->particles.alloc(num_particles);

  /* dummy particle */
  memset(kparticles, 0, sizeof(KernelParticle));

  int i = 1;
  foreach (ParticleSystem *psys, scene->get_particle_systems()) {
    foreach (const Particle &pa, psys->get_particles()) {
      /* pack in texture */
      kparticles[i].index = pa.index;
      kparticles[i].age = pa.age;
      kparticles[i].lifetime = pa.lifetime;
      kparticles[i].size = pa.size;
      kparticles[i].rotation = pa.rotation;
      kparticles[i].location = float3_to_float4(pa.location);
      kparticles[i].velocity = float3_to_float4(pa.velocity);
      kparticles[i].angular_velocity = float3_to_float4(pa.angular_velocity);

      i++;

      if (progress.get_cancel())
        return;
    }
  }

  dscene->particles.copy_to_device();
}

void ParticleSystemManager::device_update(Device *device,
                                          DeviceScene *dscene,
                                          Scene *scene,
                                          Progress &progress)
{
  if (!need_update)
    return;

  scoped_callback_timer timer([scene](double time) {
    if (scene->get_update_stats()) {
      scene->get_update_stats()->particles.times.add_entry({"device_update", time});
    }
  });

  VLOG(1) << "Total " << scene->get_particle_systems().size() << " particle systems.";

  device_free(device, dscene);

  progress.set_status("Updating Particle Systems", "Copying Particles to device");
  device_update_particles(device, dscene, scene, progress);

  if (progress.get_cancel())
    return;

  need_update = false;
}

void ParticleSystemManager::device_free(Device *, DeviceScene *dscene)
{
  dscene->particles.free();
}

void ParticleSystemManager::tag_update(Scene * /*scene*/)
{
  need_update = true;
}

CCL_NAMESPACE_END
