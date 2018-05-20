#include "jbl.h"
#include "binn.h"
#include "ejdb2cfg.h"

struct _JBL {
  binn *bn;
};

iwrc jbl_create_object(JBL *jblp) {
  iwrc rc = 0;
  *jblp = malloc(sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = *jblp;
  jbl->bn = binn_object();
  if (!jbl->bn) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (rc) {
    free(jblp);
    *jblp = 0;
  }
  return rc;
}

iwrc jbl_create_array(JBL *jblp) {
  iwrc rc = 0;
  *jblp = malloc(sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = *jblp;
  jbl->bn = binn_list();
  if (!jbl->bn) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  if (rc) {
    free(jblp);
    *jblp = 0;
  }
  return rc;
}

iwrc jbl_from_buf(JBL *jblp, const void *buf, size_t bufsz) {
  iwrc rc = 0;
  if (bufsz < MIN_BINN_SIZE) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  int type, size = 0;
  if (!binn_is_valid_header(buf, &type, NULL, &size, NULL)) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  if (size > bufsz) {
    return JBL_ERROR_INVALID_BUFFER;
  }
  *jblp = malloc(sizeof(**jblp));
  if (!*jblp) {
    return iwrc_set_errno(IW_ERROR_ALLOC, errno);
  }
  JBL jbl = *jblp;
  jbl->bn = malloc(sizeof(*jbl->bn));
  if (!jbl->bn) {
    rc = iwrc_set_errno(IW_ERROR_ALLOC, errno);
    free(jbl);
    *jblp = 0;
    return rc;
  }
  // todo


  return rc;
}

void jbl_destroy(JBL *jblp) {
  JBL jbl = *jblp;
  if (!jbl) {
    return;
  }
  if (jbl->bn) {
    binn_free(jbl->bn);
  }
  free(jbl);
  *jblp = 0;
}

static const char *_jbl_ecodefn(locale_t locale, uint32_t ecode) {
  if (!(ecode > _JBL_ERROR_START && ecode < _JBL_ERROR_END)) {
    return 0;
  }
  switch (ecode) {
    case JBL_ERROR_INVALID_BUFFER:
      return "Invalid JBL buffer (JBL_ERROR_INVALID_BUFFER)";
  }
  return 0;
}

iwrc jbl_init() {
  static int _jbl_initialized = 0;
  if (!__sync_bool_compare_and_swap(&_jbl_initialized, 0, 1)) {
    return 0;
  }
  return iwlog_register_ecodefn(_jbl_ecodefn);
}
