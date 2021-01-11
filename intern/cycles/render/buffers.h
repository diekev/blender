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

#ifndef __BUFFERS_H__
#define __BUFFERS_H__

#include "device/device_memory.h"

#include "render/film.h"

#include "kernel/kernel_types.h"

#include "util/util_api.h"
#include "util/util_half.h"
#include "util/util_string.h"
#include "util/util_thread.h"
#include "util/util_types.h"

struct TileInfo;

CCL_NAMESPACE_BEGIN

class Device;
class DeviceTask;
struct DeviceDrawParams;
struct float4;

/* Buffer Parameters
 * Size of render buffer and how it fits in the full image (border render). */

class BufferParams {
  /* width/height of the physical buffer */
  GET_SET(int, width)
  GET_SET(int, height)

  /* offset into and width/height of the full buffer */
  GET_SET(int, full_x)
  GET_SET(int, full_y)
  GET_SET(int, full_width)
  GET_SET(int, full_height)

  /* passes */
  GET_SET(vector<Pass>, passes)
  GET_SET(bool, denoising_data_pass)
  /* If only some light path types should be target, an additional pass is needed. */
  GET_SET(bool, denoising_clean_pass)
  /* When we're prefiltering the passes during rendering, we need to keep both the
   * original and the prefiltered data around because neighboring tiles might still
   * need the original data. */
  GET_SET(bool, denoising_prefiltered_pass)

 public:
  /* functions */
  BufferParams();

  void get_offset_stride(int &offset, int &stride);
  bool modified(const BufferParams &params);
  int get_passes_size();
  int get_denoising_offset();
  int get_denoising_prefiltered_offset();
};

/* Render Buffers */

class RenderBuffers {
  /* buffer parameters */
  GET_SET(BufferParams, params)

  /* float buffer */
  GET(device_vector<float>, buffer)
  GET_SET(bool, map_neighbor_copied)
  GET_SET(double, render_time)
 public:
  explicit RenderBuffers(Device *device);
  ~RenderBuffers();

  void reset(BufferParams &params);
  void zero();

  bool copy_from_device();
  bool get_pass_rect(
      const string &name, float exposure, int sample, int components, float *pixels);
  bool get_denoising_pass_rect(
      int offset, float exposure, int sample, int components, float *pixels);
  bool set_pass_rect(PassType type, int components, float *pixels, int samples);
};

/* Display Buffer
 *
 * The buffer used for drawing during render, filled by converting the render
 * buffers to byte of half float storage */

class DisplayBuffer {
  /* buffer parameters */
  GET_SET(BufferParams, params)
  /* dimensions for how much of the buffer is actually ready for display.
   * with progressive render we can be using only a subset of the buffer.
   * if these are zero, it means nothing can be drawn yet */
  GET_SET(int, draw_width)
  GET_SET(int, draw_height)
  /* draw alpha channel? */
  GET_SET(bool, transparent)
  /* use half float? */
  GET_SET(bool, half_float)
  /* byte buffer for converted result */
  GET(device_pixels<uchar4>, rgba_byte)
  GET(device_pixels<half4>, rgba_half)

 public:
  DisplayBuffer(Device *device, bool linear = false);
  ~DisplayBuffer();

  void reset(BufferParams &params);

  void draw_set(int width, int height);
  void draw(Device *device, const DeviceDrawParams &draw_params);
  bool draw_ready();
};

/* Render Tile
 * Rendering task on a buffer */

class RenderTile {
 public:
  typedef enum { PATH_TRACE = (1 << 0), BAKE = (1 << 1), DENOISE = (1 << 2) } Task;
  typedef enum { NO_STEALING = 0, CAN_BE_STOLEN = 1, WAS_STOLEN = 2 } StealingState;

  GET_SET(Task, task)
  GET_SET(int, x)
  GET_SET(int, y)
  GET_SET(int, w)
  GET_SET(int, h)
  GET_SET(int, start_sample)
  GET_SET(int, num_samples)
  GET_SET(int, sample)
  GET_SET(int, resolution)
  GET_SET(int, offset)
  GET_SET(int, stride)
  GET_SET(int, tile_index)

  GET_SET(device_ptr, buffer)
  GET_SET(int, device_size)

  GET_SET(StealingState, stealing_state)

  GET_SET(RenderBuffers *, buffers)

 public:
  RenderTile();

  int4 bounds() const
  {
    return make_int4(x,      /* xmin */
                     y,      /* ymin */
                     x + w,  /* xmax */
                     y + h); /* ymax */
  }

  /* Create a WorkTile from this RenderTile. */
  WorkTile work_tile() const;

  /* Create a RenderTile from a DeviceTask, the set_sample parameter controls
   * whether to inherit the current sample from the task. */
  static RenderTile from_device_task(const DeviceTask &task, bool set_sample);
};

/* Render Tile Neighbors
 * Set of neighboring tiles used for denoising. Tile order:
 *  0 1 2
 *  3 4 5
 *  6 7 8 */

class RenderTileNeighbors {
 public:
  static const int SIZE = 9;
  static const int CENTER = 4;

  /* cannot use a template in a macro */
  using RenderTileArray = std::array<RenderTile, SIZE>;
  GET(RenderTileArray, tiles)
  GET_SET(RenderTile, target)

 public:
  RenderTileNeighbors(const RenderTile &center)
  {
    tiles[CENTER] = center;
  }

  int4 bounds() const
  {
    return make_int4(tiles[3].get_x(),                     /* xmin */
                     tiles[1].get_y(),                     /* ymin */
                     tiles[5].get_x() + tiles[5].get_w(),  /* xmax */
                     tiles[7].get_y() + tiles[7].get_h()); /* ymax */
  }

  void set_bounds_from_center()
  {
    tiles[3].set_x(tiles[CENTER].get_x());
    tiles[1].set_y(tiles[CENTER].get_y());
    tiles[5].set_x(tiles[CENTER].get_x() + tiles[CENTER].get_w());
    tiles[7].set_y(tiles[CENTER].get_y() + tiles[CENTER].get_h());
  }

  void fill_tile_info(TileInfo *tile_info);
};

CCL_NAMESPACE_END

#endif /* __BUFFERS_H__ */
