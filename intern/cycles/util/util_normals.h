/*
 * Copyright 2020 Blender Foundation
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

#ifndef __UTIL_NORMALS_H__
#define __UTIL_NORMALS_H__

#include "util/util_math.h"
#include "util/util_types.h"

CCL_NAMESPACE_BEGIN

ccl_device_inline ushort quantize_float_15_bits(float f)
{
  return static_cast<unsigned short>(f * (32768.0f - 1.0f)) & 0x7fff;
}

ccl_device_inline float dequantize_float_15_bits(unsigned short i)
{
  return static_cast<float>(i) * (1.0f / (32768.0f - 1.0f));
}

ccl_device_inline float3 project_float3_octohedra(float3 f)
{
  f /= (fabsf(f.x) + fabsf(f.y) + fabsf(f.z));

  float fy = f.y * 0.5f + 0.5f;
  float fx = f.x * 0.5f + fy;
  fy = f.x * -0.5f + fy;

  float fz = clamp(f.z * __FLT_MAX__, 0.0f, 1.0f);
  return make_float3(fx, fy, fz);
}

ccl_device_inline float3 unproject_float3_octohedra(float3 n)
{
  float3 r;
  r.x = (n.x - n.y);
  r.y = (n.x + n.y) - 1.0f;
  r.z = n.z * 2.0f - 1.0f;
  r.z = r.z * (1.0f - fabsf(r.x) - fabsf(r.y));
  return normalize(r);
}

ccl_device_inline uint compress_float3(float3 v)
{
  auto qx = static_cast<unsigned int>(quantize_float_15_bits(v.x));
  auto qy = static_cast<unsigned int>(quantize_float_15_bits(v.y));
  auto qz = (v.z > 0.0f) ? 0u : 1u;

  return (qx << 17 | qy << 2 | qz);
}

ccl_device_inline auto encode_normal(float3 v)
{
  return compress_float3(project_float3_octohedra(v));
}

ccl_device_inline float3 decode_normal(uint q)
{
  auto ix = (q >> 17) & 0x7fff;
  auto iy = (q >>  2) & 0x7fff;
  auto iz = q & 0x3;

  auto x = dequantize_float_15_bits(static_cast<ushort>(ix));
  auto y = dequantize_float_15_bits(static_cast<ushort>(iy));
  auto z = (iz) ? 0.0f : 1.0f;

  return unproject_float3_octohedra(make_float3(x, y, z));
}

CCL_NAMESPACE_END

#endif /* __UTIL_NORMALS_H__ */
