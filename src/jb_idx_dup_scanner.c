#include "ejdb2_internal.h"

static iwrc jb_idx_consume_eq(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  bool matched;
  IWKV_cursor cur;
  int64_t step = 1;
  char buf[JBNUMBUF_SIZE];
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;
  IWKV_cursor_op cursor_reverse_step = IWKV_CURSOR_NEXT;
  midx->cursor_step = IWKV_CURSOR_PREV;

  IWKV_val key = {.data = buf};
  jb_idx_jqval_fill_key(jqval, &key);
  key.compound = INT64_MIN;
  if (!key.size) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }
  rc = iwkv_cursor_open(idx->idb, &cur, IWKV_CURSOR_GE, &key);
  if (rc == IWKV_ERROR_NOTFOUND) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  } else RCRET(rc);

  do {
    if (step > 0) --step;
    else if (step < 0) ++step;
    if (!step) {
      int64_t id;
      rc = iwkv_cursor_is_matched_key(cur, &key, &matched, &id);
      RCGO(rc, finish);
      if (!matched) {
        break;
      }
      step = 1;
      rc = consumer(ctx, 0, id, &step, &matched, 0);
      RCGO(rc, finish);
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? midx->cursor_step : cursor_reverse_step)));

finish:
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static int _cmp_jqval(const void *v1, const void *v2) {
  iwrc rc;
  const JQVAL *jqv1 = v1;
  const JQVAL *jqv2 = v2;
  return jql_cmp_jqval_pair(jqv1, jqv2, &rc);
}

static iwrc jb_idx_consume_in_node(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  int i;
  int64_t id;
  bool matched;
  IWKV_cursor cur = 0;
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;

  char jqvarrbuf[512];
  char buf[JBNUMBUF_SIZE];
  IWKV_val key = {.data = buf, .compound = INT64_MIN};

  int64_t step = 1;
  iwrc rc = 0;

  JBL_NODE nv = jqval->vnode->child;
  for (i = 0; nv; nv = nv->next) {
    if (nv->type >= JBV_I64 && nv->type <= JBV_STR) ++i;
  }
  if (i == 0) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }

  JQVAL *jqvarr = (i * sizeof(*jqvarr)) <= sizeof(jqvarrbuf)
                  ? jqvarrbuf : malloc(i * sizeof(*jqvarr));
  if (!jqvarr) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  for (i = 0, nv = jqval->vnode->child; nv; nv = nv->next) {
    if (nv->type >= JBV_I64 && nv->type <= JBV_STR) {
      JQVAL jqv;
      jql_node_to_jqval(nv, &jqv);
      memcpy(&jqvarr[i++], &jqv, sizeof(jqv));
    }
  }
  // Sort jqvarr according to index order, lowest first (asc)
  qsort(jqvarr, i, sizeof(jqvarr[0]), _cmp_jqval);

  for (int c = 0; c < i; ++c) {
    JQVAL *jqv = &jqvarr[i];
    jb_idx_jqval_fill_key(jqv, &key);
    if (cur) {
      iwkv_cursor_close(&cur);
    }
    rc = iwkv_cursor_open(idx->idb, &cur, IWKV_CURSOR_GE, &key);
    if (rc == IWKV_ERROR_NOTFOUND) {
      rc = 0;
      goto finish;
    } else RCGO(rc, finish);
    do {
      if (step > 0) --step;
      else if (step < 0) ++step;
      if (!step) {
        rc = iwkv_cursor_is_matched_key(cur, &key, &matched, &id);
        RCGO(rc, finish);
        if (!matched) {
          break;
        }
        step = 1;
        rc = consumer(ctx, 0, id, &step, &matched, 0);
        RCGO(rc, finish);
      }
    } while (step && !(rc = iwkv_cursor_to(cur, IWKV_CURSOR_PREV))); // !!! only one direction
  }

finish:
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  if ((char *) jqvarr != jqvarrbuf) {
    free(jqvarr);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

static iwrc jb_idx_consume_scan(struct _JBEXEC *ctx, JQVAL *jqval, JB_SCAN_CONSUMER consumer) {
  size_t sz;
  bool matched;
  IWKV_cursor cur;
  int64_t step = 1;
  char buf[JBNUMBUF_SIZE];
  struct _JBMIDX *midx = &ctx->midx;
  JBIDX idx = midx->idx;

  IWKV_val key = {.data = buf};
  jb_idx_jqval_fill_key(jqval, &key);
  if (!key.size) {
    return consumer(ctx, 0, 0, 0, 0, 0);
  }
  key.compound = (midx->cursor_step == IWKV_CURSOR_PREV) ? INT64_MIN : INT64_MAX;

  iwrc rc = iwkv_cursor_open(idx->idb, &cur, midx->cursor_init, &key);
  if (rc == IWKV_ERROR_NOTFOUND && (midx->expr1->op->value == JQP_OP_LT || midx->expr1->op->value == JQP_OP_LTE)) {
    iwkv_cursor_close(&cur);
    key.compound = INT64_MAX;
    midx->cursor_init = IWKV_CURSOR_BEFORE_FIRST;
    midx->cursor_step = IWKV_CURSOR_NEXT;
    rc = iwkv_cursor_open(idx->idb, &cur, midx->cursor_init, 0);
    RCGO(rc, finish);
    if (!midx->expr2) { // Fail fast
      midx->expr2 = midx->expr1;
    }
  } else RCRET(rc);

  if (midx->cursor_init < IWKV_CURSOR_NEXT) { // IWKV_CURSOR_BEFORE_FIRST || IWKV_CURSOR_AFTER_LAST
    rc = iwkv_cursor_to(cur, midx->cursor_step);
    RCGO(rc, finish);
  }

  IWKV_cursor_op cursor_reverse_step = (midx->cursor_step == IWKV_CURSOR_PREV)
                                       ? IWKV_CURSOR_NEXT : IWKV_CURSOR_PREV;
  do {
    if (step > 0) --step;
    else if (step < 0) ++step;
    if (!step) {
      int64_t id;
      rc = iwkv_cursor_copy_key(cur, 0, 0, &sz, &id);
      RCGO(rc, finish);
      if (midx->expr2
          && !midx->expr2->prematched
          && !jb_idx_node_expr_matched(ctx->ux->q->qp->aux, midx->idx, cur, midx->expr2, &rc)) {
        break;
      }
      RCGO(rc, finish);
      step = 1;
      matched = false;
      rc = consumer(ctx, 0, id, &step, &matched, 0);
      RCGO(rc, finish);
      if (!midx->expr1->prematched && matched) {
        // Further scan will always match main index expression
        midx->expr1->prematched = true;
      }
    }
  } while (step && !(rc = iwkv_cursor_to(cur, step > 0 ? midx->cursor_step : cursor_reverse_step)));

finish:
  if (rc == IWKV_ERROR_NOTFOUND) rc = 0;
  if (cur) {
    iwkv_cursor_close(&cur);
  }
  return consumer(ctx, 0, 0, 0, 0, rc);
}

iwrc jb_idx_dup_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  JQP_QUERY *qp = ctx->ux->q->qp;
  struct _JBMIDX *midx = &ctx->midx;
  JQVAL *jqval = jql_unit_to_jqval(qp->aux, midx->expr1->right, &rc);
  RCRET(rc);
  switch (midx->expr1->op->value) {
    case JQP_OP_EQ:
      return jb_idx_consume_eq(ctx, jqval, consumer);
    case JQP_OP_IN:
      if (jqval->type == JQVAL_JBLNODE) {
        return jb_idx_consume_in_node(ctx, jqval, consumer);
      } else {
        iwlog_ecode_error3(IW_ERROR_ASSERTION);
        return IW_ERROR_ASSERTION;
      }
      break;
    default:
      break;
  }

  if (midx->expr1->op->value == JQP_OP_GT && jqval->type == JQVAL_I64) {
    JQVAL mjqv;
    memcpy(&mjqv, jqval, sizeof(*jqval));
    mjqv.vi64 = mjqv.vi64 + 1; // Because for index scan we use `IWKV_CURSOR_GE`
    return jb_idx_consume_scan(ctx, &mjqv, consumer);
  } else {
    return jb_idx_consume_scan(ctx, jqval, consumer);
  }
}
