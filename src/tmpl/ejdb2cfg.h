#pragma once
#ifndef JBCFG_H
#define JBCFG_H

#if !defined(IW_32) && !defined(IW_64)
#error Unknown CPU bits
#endif

#define EJDB2_VERSION "@ejdb2_VERSION@"
#define EJDB2_VERSION_MAJOR @ejdb2_VERSION_MAJOR@
#define EJDB2_VERSION_MINOR @ejdb2_VERSION_MINOR@
#define EJDB2_VERSION_PATCH @ejdb2_VERSION_PATCH@

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#endif
