#include "ejdb2_internal.h"

iwrc jb_exec_idx_select(JBEXEC *ctx) {
  struct JQP_AUX *aux = ctx->ux->q->qp->aux;
  ctx->iop_init = IWKV_CURSOR_BEFORE_FIRST;
  ctx->iop_step = IWKV_CURSOR_NEXT;
  ctx->iop_reverse_step = IWKV_CURSOR_PREV;


  // TODO:
  //

  if (aux->orderby_num && ctx->iop_init != IWKV_CURSOR_EQ) {
    ctx->sorting = true;
  }

  return 0;
}
