#ifdef _WIN32
#include "win32/platform.c"
#endif

#if defined(__unix) || defined(__APPLE__)
#include "nix/platform.c"
#endif




