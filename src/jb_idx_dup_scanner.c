#include "ejdb2_internal.h"

static iwrc jb_idx_consume_eq(struct _JBEXEC *ctx, JQVAL *rv, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

static iwrc jb_idx_consume_in_node(struct _JBEXEC *ctx, JQVAL *rv, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

static iwrc jb_idx_consume_scan(struct _JBEXEC *ctx, JQVAL *rval, JB_SCAN_CONSUMER consumer) {
  return IW_ERROR_NOT_IMPLEMENTED;
}

iwrc jb_idx_dup_scanner(struct _JBEXEC *ctx, JB_SCAN_CONSUMER consumer) {
  iwrc rc;
  JQP_QUERY *qp = ctx->ux->q->qp;
  struct _JBMIDX *midx = &ctx->midx;
  JQVAL *rval = jql_unit_to_jqval(qp->aux, midx->expr1->right, &rc);
  RCRET(rc);
  switch (midx->expr1->op->value) {
    case JQP_OP_EQ:
      return jb_idx_consume_eq(ctx, rval, consumer);
    case JQP_OP_IN:
      if (rval->type == JQVAL_JBLNODE) {
        return jb_idx_consume_in_node(ctx, rval, consumer);
      } else {
        iwlog_ecode_error3(IW_ERROR_ASSERTION);
        return IW_ERROR_ASSERTION;
      }
      break;
    default:
      break;
  }
  return jb_idx_consume_scan(ctx, rval, consumer);
}
