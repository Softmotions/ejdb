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

void jb_idx_jbl_fill_ikey(JBIDX idx, JBL jbv, IWKV_val *ikey, char numbuf[static JBNUMBUF_SIZE]) {
  int64_t *llv = (void *) numbuf;
  jbl_type_t jbvt = jbl_type(jbv);
  ejdb_idx_mode_t itype = (idx->mode & ~(EJDB_IDX_UNIQUE));
  ikey->size = 0;
  ikey->data = 0;

  switch (itype) {
    case EJDB_IDX_STR:
      switch (jbvt) {
        case JBV_STR:
          ikey->size = jbl_size(jbv);
          ikey->data = jbl_get_str(jbv);
          break;
        case JBV_I64:
          ikey->size = (size_t) iwitoa(jbl_get_i64(jbv), numbuf, JBNUMBUF_SIZE);
          ikey->data = numbuf;
          break;
        case JBV_BOOL:
          if (jbl_get_i32(jbv)) {
            ikey->size = sizeof("true");
            ikey->data = "true";
          } else {
            ikey->size = sizeof("false");
            ikey->data = "false";
          }
          break;
        case JBV_F64:
          jb_idx_ftoa(jbl_get_f64(jbv), numbuf, &ikey->size);
          ikey->data = numbuf;
          break;
        default:
          break;
      }
      break;
    case EJDB_IDX_I64:
      ikey->size = sizeof(*llv);
      ikey->data = llv;
      switch (jbvt) {
        case JBV_I64:
        case JBV_F64:
        case JBV_BOOL:
          *llv = jbl_get_i64(jbv);
          break;
        case JBV_STR:
          *llv = iwatoi(jbl_get_str(jbv));
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
    case EJDB_IDX_F64:
      ikey->data = numbuf;
      switch (jbvt) {
        case JBV_F64:
        case JBV_I64:
        case JBV_BOOL:
          jb_idx_ftoa(jbl_get_f64(jbv), numbuf, &ikey->size);
          break;
        case JBV_STR:
          jb_idx_ftoa(iwatof(jbl_get_str(jbv)), numbuf, &ikey->size);
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
    default:
      break;
  }
}

void jb_idx_jqval_fill_ikey(JBIDX idx, const JQVAL *jqval, IWKV_val *ikey, char numbuf[static JBNUMBUF_SIZE]) {
  int64_t *llv = (void *) numbuf;
  ikey->size = 0;
  ikey->data = numbuf;
  ejdb_idx_mode_t itype = (idx->mode & ~(EJDB_IDX_UNIQUE));
  jqval_type_t jqvt = jqval->type;

  switch (itype) {
    case EJDB_IDX_STR:
      switch (jqvt) {
        case JQVAL_STR:
          ikey->size = strlen(jqval->vstr);
          ikey->data = (void *) jqval->vstr;
          break;
        case JQVAL_I64:
          ikey->size = (size_t) iwitoa(jqval->vi64, numbuf, JBNUMBUF_SIZE);
          ikey->data = numbuf;
          break;
        case JQVAL_BOOL:
          if (jqval->vbool) {
            ikey->size = sizeof("true");
            ikey->data = "true";
          } else {
            ikey->size = sizeof("false");
            ikey->data = "false";
          }
          break;
        case JQVAL_F64:
          jb_idx_ftoa(jqval->vf64, numbuf, &ikey->size);
          ikey->data = numbuf;
          break;
        default:
          break;
      }
      break;
    case EJDB_IDX_I64:
      ikey->size = sizeof(*llv);
      ikey->data = llv;
      switch (jqvt) {
        case JQVAL_I64:
          *llv = jqval->vi64;
          break;
        case JQVAL_F64:
          *llv = (int64_t) jqval->vf64;
          break;
        case JQVAL_BOOL:
          *llv = jqval->vbool;
          break;
        case JQVAL_STR:
          *llv = iwatoi(jqval->vstr);
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
    case EJDB_IDX_F64:
      ikey->data = numbuf;
      switch (jqvt) {
        case JQVAL_F64:
          jb_idx_ftoa(jqval->vf64, numbuf, &ikey->size);
          break;
        case JQVAL_I64:
          jb_idx_ftoa(jqval->vi64, numbuf, &ikey->size);
          break;
        case JQVAL_BOOL:
          jb_idx_ftoa(jqval->vbool, numbuf, &ikey->size);
          break;
        case JQVAL_STR:
          jb_idx_ftoa(iwatof(jqval->vstr), numbuf, &ikey->size);
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
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
