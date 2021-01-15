#include "ejdb2_internal.h"
#include "convert.h"
#include <ejdb2/iowow/iwutils.h>

// ---------------------------------------------------------------------------

// fixme: code duplication below
void jbi_jbl_fill_ikey(JBIDX idx, JBL jbv, IWKV_val *ikey, char numbuf[static JBNUMBUF_SIZE]) {
  int64_t *llv = (void*) numbuf;
  jbl_type_t jbvt = jbl_type(jbv);
  ejdb_idx_mode_t itype = (idx->mode & ~(EJDB_IDX_UNIQUE));
  ikey->size = 0;
  ikey->data = 0;

  switch (itype) {
    case EJDB_IDX_STR:
      switch (jbvt) {
        case JBV_STR:
          ikey->size = jbl_size(jbv);
          ikey->data = (void*) jbl_get_str(jbv);
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
          jbi_ftoa(jbl_get_f64(jbv), numbuf, &ikey->size);
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
          jbi_ftoa(jbl_get_f64(jbv), numbuf, &ikey->size);
          break;
        case JBV_STR:
          jbi_ftoa(iwatof(jbl_get_str(jbv)), numbuf, &ikey->size);
          break;
        default:
          ikey->size = 0; // -V1048
          ikey->data = 0;
          break;
      }
      break;
    default:
      break;
  }
}

void jbi_jqval_fill_ikey(JBIDX idx, const JQVAL *jqval, IWKV_val *ikey, char numbuf[static JBNUMBUF_SIZE]) {
  int64_t *llv = (void*) numbuf;
  ikey->size = 0;
  ikey->data = numbuf;
  ejdb_idx_mode_t itype = (idx->mode & ~(EJDB_IDX_UNIQUE));
  jqval_type_t jqvt = jqval->type;

  switch (itype) {
    case EJDB_IDX_STR:
      switch (jqvt) {
        case JQVAL_STR:
          ikey->size = strlen(jqval->vstr);
          ikey->data = (void*) jqval->vstr;
          break;
        case JQVAL_I64:
          ikey->size = (size_t) iwitoa(jqval->vi64, numbuf, JBNUMBUF_SIZE);
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
          jbi_ftoa(jqval->vf64, numbuf, &ikey->size);
          break;
        default:
          break;
      }
      break;
    case EJDB_IDX_I64:
      ikey->size = sizeof(*llv);
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
          ikey->data = 0;
          break;
      }
      break;
    case EJDB_IDX_F64:
      switch (jqvt) {
        case JQVAL_F64:
          jbi_ftoa(jqval->vf64, numbuf, &ikey->size);
          break;
        case JQVAL_I64:
          jbi_ftoa(jqval->vi64, numbuf, &ikey->size);
          break;
        case JQVAL_BOOL:
          jbi_ftoa(jqval->vbool, numbuf, &ikey->size);
          break;
        case JQVAL_STR:
          jbi_ftoa(iwatof(jqval->vstr), numbuf, &ikey->size);
          break;
        default:
          ikey->data = 0;
          break;
      }
      break;
    default:
      break;
  }
}

void jbi_node_fill_ikey(JBIDX idx, JBL_NODE node, IWKV_val *ikey, char numbuf[static JBNUMBUF_SIZE]) {
  int64_t *llv = (void*) numbuf;
  ikey->size = 0;
  ikey->data = numbuf;
  ejdb_idx_mode_t itype = (idx->mode & ~(EJDB_IDX_UNIQUE));
  jbl_type_t jbvt = node->type;

  switch (itype) {
    case EJDB_IDX_STR:
      switch (jbvt) {
        case JBV_STR:
          ikey->size = (size_t) node->vsize;
          ikey->data = (void*) node->vptr;
          break;
        case JBV_I64:
          ikey->size = (size_t) iwitoa(node->vi64, numbuf, JBNUMBUF_SIZE);
          break;
        case JBV_BOOL:
          if (node->vbool) {
            ikey->size = sizeof("true");
            ikey->data = "true";
          } else {
            ikey->size = sizeof("false");
            ikey->data = "false";
          }
          break;
        case JBV_F64:
          jbi_ftoa(node->vf64, numbuf, &ikey->size);
          break;
        default:
          break;
      }
      break;
    case EJDB_IDX_I64:
      ikey->size = sizeof(*llv);
      switch (jbvt) {
        case JBV_I64:
          *llv = node->vi64;
          break;
        case JBV_F64:
          *llv = (int64_t) node->vf64;
          break;
        case JBV_BOOL:
          *llv = node->vbool;
          break;
        case JBV_STR:
          *llv = iwatoi(node->vptr);
          break;
        default:
          ikey->size = 0;
          ikey->data = 0;
          break;
      }
      break;
    case EJDB_IDX_F64:
      switch (jbvt) {
        case JBV_F64:
          jbi_ftoa(node->vf64, numbuf, &ikey->size);
          break;
        case JBV_I64:
          jbi_ftoa(node->vi64, numbuf, &ikey->size);
          break;
        case JBV_BOOL:
          jbi_ftoa(node->vbool, numbuf, &ikey->size);
          break;
        case JBV_STR:
          jbi_ftoa(iwatof(node->vptr), numbuf, &ikey->size);
          break;
        default:
          ikey->data = 0;
          break;
      }
      break;
    default:
      break;
  }
}

bool jbi_node_expr_matched(JQP_AUX *aux, JBIDX idx, IWKV_cursor cur, JQP_EXPR *expr, iwrc *rcp) {
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
