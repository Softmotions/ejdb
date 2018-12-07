#include "ejdb2_internal.h"
#include <iowow/iwutils.h>

// ---------------------------------------------------------------------------

void jb_idx_ftoa(double val, char buf[static JBNUMBUF_SIZE], size_t *osz) {
  // TODO:
  int sz = snprintf(buf, JBNUMBUF_SIZE, "%.6f", val);
  while (sz > 0 && buf[sz - 1] == '0') buf[sz--] = '\0';
  if (buf[sz] == '.') buf[sz--] = '\0';
  *osz = sz;
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
      llv = IW_HTOILL(llv);
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

iwrc jb_idx_node_expr_matched(IWKV_cursor cur, JQP_QUERY *qp, JQP_EXPR *nexpr) {
  size_t sz;
  uint8_t skey[1024];
  uint8_t *kbuf = skey;
  iwrc rc = iwkv_cursor_copy_key(cur, kbuf, sizeof(skey), &sz);
  RCRET(rc);
  if (sz > sizeof(kbuf)) {
    kbuf = malloc(sz);
    if (!kbuf) {
      return iwrc_set_errno(IW_ERROR_ALLOC, errno);
    }
  }
  rc = iwkv_cursor_copy_key(cur, kbuf, sizeof(skey), &sz);
  RCGO(rc, finish);

  // TODO:


finish:
  if (kbuf != skey) {
    free(kbuf);
  }
  return rc;
}
