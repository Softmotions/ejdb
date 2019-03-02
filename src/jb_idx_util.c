#include "ejdb2_internal.h"
#include <iowow/iwutils.h>

// ---------------------------------------------------------------------------

void jb_idx_ftoa(long double val, char buf[static JBNUMBUF_SIZE], size_t *osz) {
  // TODO:
  int sz = snprintf(buf, JBNUMBUF_SIZE, "%.6Lf", val);
  while (sz > 0 && buf[sz - 1] == '0') buf[sz--] = '\0';
  if (buf[sz] == '.') buf[sz--] = '\0';
  *osz = (size_t) sz;
}

/**
 * @warning key->data Must point to valid memory buffer `JBNUMBUF_SIZE` in size
 */
void jb_idx_jqval_fill_key(const JQVAL *rval, IWKV_val *key) {
  key->size = 0;
  switch (rval->type) {
    case JQVAL_STR:
      key->data = (void *) rval->vstr;
      key->size = strlen(rval->vstr);
      break;
    case JQVAL_I64: {
      int64_t llv = rval->vi64;
      memcpy(key->data, &llv, sizeof(llv));
      key->size = sizeof(llv);
      break;
    }
    case JQVAL_F64: {
      size_t sz;
      jb_idx_ftoa(rval->vf64, key->data, &sz);
      key->size = sz;
      break;
    }
    default:
      break;
  }
}

bool jb_idx_node_expr_matched(JQP_AUX *aux, JBIDX idx, IWKV_cursor cur, JQP_EXPR *expr, iwrc *rcp) {
  size_t sz;
  char skey[1024];
  char *kbuf = skey;
  bool ret = false;
  iwrc rc = 0;

  if (!(idx->mode & (EJDB_IDX_STR | EJDB_IDX_I64 | EJDB_IDX_F64))) {
    return false;
  }
  JQVAL lv, *rv = jql_unit_to_jqval(aux, expr->right, &rc);
  RCGO(rc, finish);

  rc = iwkv_cursor_copy_key(cur, kbuf, sizeof(skey) - 1, &sz, 0);
  RCGO(rc, finish);
  if (sz > sizeof(skey) - 1) {
    kbuf = malloc(sz);
    if (!kbuf) {
      rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
      goto finish;
    }
    rc = iwkv_cursor_copy_key(cur, kbuf, sizeof(skey) - 1, &sz, 0);
    RCGO(rc, finish);
  }
  if (idx->mode & EJDB_IDX_STR) {
    kbuf[sz] = '\0';
    lv.type = JQVAL_STR;
    lv.vstr = kbuf;
  } else if (idx->mode & EJDB_IDX_I64) {
    memcpy(&lv.vi64, kbuf, sizeof(lv.vi64));
    lv.type = JQVAL_I64;
  } else if (idx->mode & EJDB_IDX_F64) {
    kbuf[sz] = '\0';
    lv.type = JQVAL_F64;
    lv.vf64 = (double) iwatof(kbuf);
  }

  ret = jql_match_jqval_pair(aux, &lv, expr->op, rv, &rc);

finish:
  if (kbuf != skey) {
    free(kbuf);
  }
  *rcp = rc;
  return ret;
}
