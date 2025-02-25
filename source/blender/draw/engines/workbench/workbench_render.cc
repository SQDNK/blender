/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 *
 * Render functions for final render output.
 */

#include "BLI_rect.h"

#include "DNA_node_types.h"

#include "BKE_report.h"

#include "DRW_render.h"

#include "ED_view3d.hh"

#include "GPU_context.h"
#include "GPU_shader.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_imbuf_types.h"

#include "RE_pipeline.h"

#include "workbench_private.h"

static void workbench_render_cache(void *vedata,
                                   Object *ob,
                                   RenderEngine * /*engine*/,
                                   Depsgraph * /*depsgraph*/)
{
  workbench_cache_populate(vedata, ob);
}

static void workbench_render_matrices_init(RenderEngine *engine, Depsgraph *depsgraph)
{
  /* TODO(sergey): Shall render hold pointer to an evaluated camera instead? */
  Object *ob_camera_eval = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));

  /* Set the perspective, view and window matrix. */
  float winmat[4][4], viewmat[4][4], viewinv[4][4];

  RE_GetCameraWindow(engine->re, ob_camera_eval, winmat);
  RE_GetCameraModelMatrix(engine->re, ob_camera_eval, viewinv);

  invert_m4_m4(viewmat, viewinv);

  DRWView *view = DRW_view_create(viewmat, winmat, nullptr, nullptr, nullptr);
  DRW_view_default_set(view);
  DRW_view_set_active(view);
}

static bool workbench_render_framebuffers_init()
{
  /* For image render, allocate own buffers because we don't have a viewport. */
  const float *viewport_size = DRW_viewport_size_get();
  const int size[2] = {int(viewport_size[0]), int(viewport_size[1])};

  DefaultTextureList *dtxl = DRW_viewport_texture_list_get();

  /* When doing a multi view rendering the first view will allocate the buffers
   * the other views will reuse these buffers */
  if (dtxl->color == nullptr) {
    eGPUTextureUsage usage = GPU_TEXTURE_USAGE_SHADER_READ | GPU_TEXTURE_USAGE_ATTACHMENT;
    BLI_assert(dtxl->depth == nullptr);
    dtxl->color = GPU_texture_create_2d(
        "txl.color", UNPACK2(size), 1, GPU_RGBA16F, usage, nullptr);
    dtxl->depth = GPU_texture_create_2d(
        "txl.depth", UNPACK2(size), 1, GPU_DEPTH24_STENCIL8, usage, nullptr);
  }

  if (!(dtxl->depth && dtxl->color)) {
    return false;
  }

  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();

  GPU_framebuffer_ensure_config(
      &dfbl->default_fb,
      {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  GPU_framebuffer_ensure_config(&dfbl->depth_only_fb,
                                {GPU_ATTACHMENT_TEXTURE(dtxl->depth), GPU_ATTACHMENT_NONE});

  GPU_framebuffer_ensure_config(&dfbl->color_only_fb,
                                {GPU_ATTACHMENT_NONE, GPU_ATTACHMENT_TEXTURE(dtxl->color)});

  bool ok = true;
  ok = ok && GPU_framebuffer_check_valid(dfbl->default_fb, nullptr);
  ok = ok && GPU_framebuffer_check_valid(dfbl->color_only_fb, nullptr);
  ok = ok && GPU_framebuffer_check_valid(dfbl->depth_only_fb, nullptr);

  return ok;
}

static void workbench_render_result_z(RenderLayer *rl, const char *viewname, const rcti *rect)
{
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  ViewLayer *view_layer = draw_ctx->view_layer;

  if ((view_layer->passflag & SCE_PASS_Z) != 0) {
    RenderPass *rp = RE_pass_find_by_name(rl, RE_PASSNAME_Z, viewname);
    float *rp_buffer_data = rp->ibuf->float_buffer.data;

    GPU_framebuffer_bind(dfbl->default_fb);
    GPU_framebuffer_read_depth(dfbl->default_fb,
                               rect->xmin,
                               rect->ymin,
                               BLI_rcti_size_x(rect),
                               BLI_rcti_size_y(rect),
                               GPU_DATA_FLOAT,
                               rp_buffer_data);

    float winmat[4][4];
    DRW_view_winmat_get(nullptr, winmat, false);

    int pix_num = BLI_rcti_size_x(rect) * BLI_rcti_size_y(rect);

    /* Convert GPU depth [0..1] to view Z [near..far] */
    if (DRW_view_is_persp_get(nullptr)) {
      for (int i = 0; i < pix_num; i++) {
        if (rp_buffer_data[i] == 1.0f) {
          rp_buffer_data[i] = 1e10f; /* Background */
        }
        else {
          rp_buffer_data[i] = rp_buffer_data[i] * 2.0f - 1.0f;
          rp_buffer_data[i] = winmat[3][2] / (rp_buffer_data[i] + winmat[2][2]);
        }
      }
    }
    else {
      /* Keep in mind, near and far distance are negatives. */
      float near = DRW_view_near_distance_get(nullptr);
      float far = DRW_view_far_distance_get(nullptr);
      float range = fabsf(far - near);

      for (int i = 0; i < pix_num; i++) {
        if (rp_buffer_data[i] == 1.0f) {
          rp_buffer_data[i] = 1e10f; /* Background */
        }
        else {
          rp_buffer_data[i] = rp_buffer_data[i] * range - near;
        }
      }
    }
  }
}

void workbench_render(void *ved, RenderEngine *engine, RenderLayer *render_layer, const rcti *rect)
{
  WORKBENCH_Data *data = static_cast<WORKBENCH_Data *>(ved);
  DefaultFramebufferList *dfbl = DRW_viewport_framebuffer_list_get();
  const DRWContextState *draw_ctx = DRW_context_state_get();
  Depsgraph *depsgraph = draw_ctx->depsgraph;
  workbench_render_matrices_init(engine, depsgraph);

  if (!workbench_render_framebuffers_init()) {
    RE_engine_report(engine, RPT_ERROR, "Failed to allocate OpenGL buffers");
    return;
  }

  workbench_private_data_alloc(data->stl);
  data->stl->wpd->cam_original_ob = DEG_get_evaluated_object(depsgraph, RE_GetCamera(engine->re));
  workbench_engine_init(data);

  workbench_cache_init(data);
  DRW_render_object_iter(data, engine, depsgraph, workbench_render_cache);
  workbench_cache_finish(data);

  DRW_render_instance_buffer_finish();

  /* Also we weed to have a correct FBO bound for #DRW_curves_update */
  GPU_framebuffer_bind(dfbl->default_fb);
  DRW_curves_update();

  GPU_framebuffer_bind(dfbl->default_fb);
  GPU_framebuffer_clear_depth(dfbl->default_fb, 1.0f);

  WORKBENCH_PrivateData *wpd = data->stl->wpd;
  while (wpd->taa_sample < max_ii(1, wpd->taa_sample_len)) {
    if (RE_engine_test_break(engine)) {
      break;
    }
    workbench_update_world_ubo(wpd);
    workbench_draw_sample(data);
  }

  workbench_draw_finish(data);

  /* Perform render step between samples to allow
   * flushing of freed GPUBackend resources. */
  GPU_render_step();

  /* Write render output. */
  const char *viewname = RE_GetActiveRenderView(engine->re);
  RenderPass *rp = RE_pass_find_by_name(render_layer, RE_PASSNAME_COMBINED, viewname);

  GPU_framebuffer_bind(dfbl->default_fb);
  GPU_framebuffer_read_color(dfbl->default_fb,
                             rect->xmin,
                             rect->ymin,
                             BLI_rcti_size_x(rect),
                             BLI_rcti_size_y(rect),
                             4,
                             0,
                             GPU_DATA_FLOAT,
                             rp->ibuf->float_buffer.data);

  workbench_render_result_z(render_layer, viewname, rect);
}

void workbench_render_update_passes(RenderEngine *engine, Scene *scene, ViewLayer *view_layer)
{
  RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_COMBINED, 4, "RGBA", SOCK_RGBA);
  if ((view_layer->passflag & SCE_PASS_Z) != 0) {
    RE_engine_register_pass(engine, scene, view_layer, RE_PASSNAME_Z, 1, "Z", SOCK_FLOAT);
  }
}
