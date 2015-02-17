#include "platform.h"
#include "../tcutil.h"
#include "myconf.h"

#include <unistd.h>

bool closefd(int fd) {
    return (close(fd) != -1);
}

