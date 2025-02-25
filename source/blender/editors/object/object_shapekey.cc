/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 */

#include <cmath>
#include <cstring>

#ifndef WIN32
#  include <unistd.h>
#else
#  include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_context.h"
#include "BKE_crazyspace.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"
#include "DEG_depsgraph_query.h"

#include "BLI_sys_types.h" /* for intptr_t support */

#include "ED_mesh.hh"
#include "ED_object.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.hh"
#include "WM_types.hh"

#include "object_intern.h"

/* -------------------------------------------------------------------- */
/** \name Add Shape Key Function
 * \{ */

static void ED_object_shape_key_add(bContext *C, Object *ob, const bool from_mix)
{
  Main *bmain = CTX_data_main(C);
  KeyBlock *kb;
  if ((kb = BKE_object_shapekey_insert(bmain, ob, nullptr, from_mix))) {
    Key *key = BKE_key_from_object(ob);
    /* for absolute shape keys, new keys may not be added last */
    ob->shapenr = BLI_findindex(&key->block, kb) + 1;

    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Remove Shape Key Function
 * \{ */

static bool object_shapekey_remove(Main *bmain, Object *ob)
{
  KeyBlock *kb;
  Key *key = BKE_key_from_object(ob);

  if (key == nullptr) {
    return false;
  }

  kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));
  if (kb) {
    return BKE_object_shapekey_remove(bmain, ob, kb);
  }

  return false;
}

static bool object_shape_key_mirror(
    bContext *C, Object *ob, int *r_totmirr, int *r_totfail, bool use_topology)
{
  KeyBlock *kb;
  Key *key;
  int totmirr = 0, totfail = 0;

  *r_totmirr = *r_totfail = 0;

  key = BKE_key_from_object(ob);
  if (key == nullptr) {
    return false;
  }

  kb = static_cast<KeyBlock *>(BLI_findlink(&key->block, ob->shapenr - 1));

  if (kb) {
    char *tag_elem = static_cast<char *>(
        MEM_callocN(sizeof(char) * kb->totelem, "shape_key_mirror"));

    if (ob->type == OB_MESH) {
      Mesh *me = static_cast<Mesh *>(ob->data);
      int i1, i2;
      float *fp1, *fp2;
      float tvec[3];

      ED_mesh_mirror_spatial_table_begin(ob, nullptr, nullptr);

      for (i1 = 0; i1 < me->totvert; i1++) {
        i2 = mesh_get_x_mirror_vert(ob, nullptr, i1, use_topology);
        if (i2 == i1) {
          fp1 = ((float *)kb->data) + i1 * 3;
          fp1[0] = -fp1[0];
          tag_elem[i1] = 1;
          totmirr++;
        }
        else if (i2 != -1) {
          if (tag_elem[i1] == 0 && tag_elem[i2] == 0) {
            fp1 = ((float *)kb->data) + i1 * 3;
            fp2 = ((float *)kb->data) + i2 * 3;

            copy_v3_v3(tvec, fp1);
            copy_v3_v3(fp1, fp2);
            copy_v3_v3(fp2, tvec);

            /* flip x axis */
            fp1[0] = -fp1[0];
            fp2[0] = -fp2[0];
            totmirr++;
          }
          tag_elem[i1] = tag_elem[i2] = 1;
        }
        else {
          totfail++;
        }
      }

      ED_mesh_mirror_spatial_table_end(ob);
    }
    else if (ob->type == OB_LATTICE) {
      Lattice *lt = static_cast<Lattice *>(ob->data);
      int i1, i2;
      float *fp1, *fp2;
      int u, v, w;
      /* half but found up odd value */
      const int pntsu_half = (lt->pntsu / 2) + (lt->pntsu % 2);

      /* Currently edit-mode isn't supported by mesh so ignore here for now too. */
#if 0
      if (lt->editlatt) {
        lt = lt->editlatt->latt;
      }
#endif

      for (w = 0; w < lt->pntsw; w++) {
        for (v = 0; v < lt->pntsv; v++) {
          for (u = 0; u < pntsu_half; u++) {
            int u_inv = (lt->pntsu - 1) - u;
            float tvec[3];
            if (u == u_inv) {
              i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
              fp1 = ((float *)kb->data) + i1 * 3;
              fp1[0] = -fp1[0];
              totmirr++;
            }
            else {
              i1 = BKE_lattice_index_from_uvw(lt, u, v, w);
              i2 = BKE_lattice_index_from_uvw(lt, u_inv, v, w);

              fp1 = ((float *)kb->data) + i1 * 3;
              fp2 = ((float *)kb->data) + i2 * 3;

              copy_v3_v3(tvec, fp1);
              copy_v3_v3(fp1, fp2);
              copy_v3_v3(fp2, tvec);
              fp1[0] = -fp1[0];
              fp2[0] = -fp2[0];
              totmirr++;
            }
          }
        }
      }
    }

    MEM_freeN(tag_elem);
  }

  *r_totmirr = totmirr;
  *r_totfail = totfail;

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shared Poll Functions
 * \{ */

static bool shape_key_poll(bContext *C)
{
  Object *ob = ED_object_context(C);
  ID *data = static_cast<ID *>((ob) ? ob->data : nullptr);

  return (ob != nullptr && !ID_IS_LINKED(ob) && !ID_IS_OVERRIDE_LIBRARY(ob) && data != nullptr &&
          !ID_IS_LINKED(data) && !ID_IS_OVERRIDE_LIBRARY(data));
}

static bool shape_key_mode_poll(bContext *C)
{
  Object *ob = ED_object_context(C);

  return (shape_key_poll(C) && ob->mode != OB_MODE_EDIT);
}

static bool shape_key_mode_exists_poll(bContext *C)
{
  Object *ob = ED_object_context(C);

  return (shape_key_mode_poll(C) &&
          /* check a keyblock exists */
          (BKE_keyblock_from_object(ob) != nullptr));
}

static bool shape_key_move_poll(bContext *C)
{
  /* Same as shape_key_mode_exists_poll above, but ensure we have at least two shapes! */
  Object *ob = ED_object_context(C);
  Key *key = BKE_key_from_object(ob);

  return (shape_key_mode_poll(C) && key != nullptr && key->totkey > 1);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Add Operator
 * \{ */

static int shape_key_add_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  const bool from_mix = RNA_boolean_get(op->ptr, "from_mix");

  ED_object_shape_key_add(C, ob, from_mix);

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  DEG_relations_tag_update(CTX_data_main(C));

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Shape Key";
  ot->idname = "OBJECT_OT_shape_key_add";
  ot->description = "Add shape key to the object";

  /* api callbacks */
  ot->poll = shape_key_mode_poll;
  ot->exec = shape_key_add_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "from_mix",
                  true,
                  "From Mix",
                  "Create the new shape key from the existing mix of keys");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Remove Operator
 * \{ */

static int shape_key_remove_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = ED_object_context(C);
  bool changed = false;

  if (RNA_boolean_get(op->ptr, "all")) {
    if (RNA_boolean_get(op->ptr, "apply_mix")) {
      float *arr = BKE_key_evaluate_object_ex(
          ob, nullptr, nullptr, 0, static_cast<ID *>(ob->data));
      MEM_freeN(arr);
    }
    changed = BKE_object_shapekey_free(bmain, ob);
  }
  else {
    changed = object_shapekey_remove(bmain, ob);
  }

  if (changed) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_relations_tag_update(CTX_data_main(C));
    WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_CANCELLED;
}

static bool shape_key_remove_poll_property(const bContext * /*C*/,
                                           wmOperator *op,
                                           const PropertyRNA *prop)
{
  const char *prop_id = RNA_property_identifier(prop);
  const bool do_all = RNA_enum_get(op->ptr, "all");

  /* Only show seed for randomize action! */
  if (STREQ(prop_id, "apply_mix") && !do_all) {
    return false;
  }
  return true;
}

static char *shape_key_remove_get_description(bContext * /*C*/,
                                              wmOperatorType * /*ot*/,
                                              PointerRNA *ptr)
{
  const bool do_apply_mix = RNA_boolean_get(ptr, "apply_mix");

  if (do_apply_mix) {
    return BLI_strdup(
        TIP_("Apply current visible shape to the object data, and delete all shape keys"));
  }

  return nullptr;
}

void OBJECT_OT_shape_key_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Shape Key";
  ot->idname = "OBJECT_OT_shape_key_remove";
  ot->description = "Remove shape key from the object";

  /* api callbacks */
  ot->poll = shape_key_mode_exists_poll;
  ot->exec = shape_key_remove_exec;
  ot->poll_property = shape_key_remove_poll_property;
  ot->get_description = shape_key_remove_get_description;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "all", false, "All", "Remove all shape keys");
  RNA_def_boolean(ot->srna,
                  "apply_mix",
                  false,
                  "Apply Mix",
                  "Apply current mix of shape keys to the geometry before removing them");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Clear Operator
 * \{ */

static int shape_key_clear_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = ED_object_context(C);
  Key *key = BKE_key_from_object(ob);
  KeyBlock *kb = BKE_keyblock_from_object(ob);

  if (!key || !kb) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    kb->curval = 0.0f;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_clear(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Clear Shape Keys";
  ot->description = "Clear weights for all shape keys";
  ot->idname = "OBJECT_OT_shape_key_clear";

  /* api callbacks */
  ot->poll = shape_key_poll;
  ot->exec = shape_key_clear_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* starting point and step size could be optional */
static int shape_key_retime_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = ED_object_context(C);
  Key *key = BKE_key_from_object(ob);
  KeyBlock *kb = BKE_keyblock_from_object(ob);
  float cfra = 0.0f;

  if (!key || !kb) {
    return OPERATOR_CANCELLED;
  }

  LISTBASE_FOREACH (KeyBlock *, kb, &key->block) {
    kb->pos = cfra;
    cfra += 0.1f;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_retime(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Re-Time Shape Keys";
  ot->description = "Resets the timing for absolute shape keys";
  ot->idname = "OBJECT_OT_shape_key_retime";

  /* api callbacks */
  ot->poll = shape_key_poll;
  ot->exec = shape_key_retime_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Mirror Operator
 * \{ */

static int shape_key_mirror_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);
  int totmirr = 0, totfail = 0;
  bool use_topology = RNA_boolean_get(op->ptr, "use_topology");

  if (!object_shape_key_mirror(C, ob, &totmirr, &totfail, use_topology)) {
    return OPERATOR_CANCELLED;
  }

  ED_mesh_report_mirror(op, totmirr, totfail);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_mirror(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mirror Shape Key";
  ot->idname = "OBJECT_OT_shape_key_mirror";
  ot->description = "Mirror the current shape key along the local X axis";

  /* api callbacks */
  ot->poll = shape_key_mode_poll;
  ot->exec = shape_key_mirror_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(
      ot->srna,
      "use_topology",
      false,
      "Topology Mirror",
      "Use topology based mirroring (for when both sides of mesh have matching, unique topology)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Shape Key Move (Re-Order) Operator
 * \{ */

enum {
  KB_MOVE_TOP = -2,
  KB_MOVE_UP = -1,
  KB_MOVE_DOWN = 1,
  KB_MOVE_BOTTOM = 2,
};

static int shape_key_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = ED_object_context(C);

  Key *key = BKE_key_from_object(ob);
  const int type = RNA_enum_get(op->ptr, "type");
  const int totkey = key->totkey;
  const int act_index = ob->shapenr - 1;
  int new_index;

  switch (type) {
    case KB_MOVE_TOP:
      /* Replace the ref key only if we're at the top already (only for relative keys) */
      new_index = (ELEM(act_index, 0, 1) || key->type == KEY_NORMAL) ? 0 : 1;
      break;
    case KB_MOVE_BOTTOM:
      new_index = totkey - 1;
      break;
    case KB_MOVE_UP:
    case KB_MOVE_DOWN:
    default:
      new_index = (totkey + act_index + type) % totkey;
      break;
  }

  if (!BKE_keyblock_move(ob, act_index, new_index)) {
    return OPERATOR_CANCELLED;
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

void OBJECT_OT_shape_key_move(wmOperatorType *ot)
{
  static const EnumPropertyItem slot_move[] = {
      {KB_MOVE_TOP, "TOP", 0, "Top", "Top of the list"},
      {KB_MOVE_UP, "UP", 0, "Up", ""},
      {KB_MOVE_DOWN, "DOWN", 0, "Down", ""},
      {KB_MOVE_BOTTOM, "BOTTOM", 0, "Bottom", "Bottom of the list"},
      {0, nullptr, 0, nullptr, nullptr}};

  /* identifiers */
  ot->name = "Move Shape Key";
  ot->idname = "OBJECT_OT_shape_key_move";
  ot->description = "Move the active shape key up/down in the list";

  /* api callbacks */
  ot->poll = shape_key_move_poll;
  ot->exec = shape_key_move_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_enum(ot->srna, "type", slot_move, 0, "Type", "");
}

/** \} */
