/*
 * connect() wrapper.
 *
 * Copyright 1999-2008 Gentoo Foundation
 * Licensed under the GPL-2
 */

#define _WRAPPER_ARGS_PROTO int fd, const struct sockaddr *sa, socklen_t salen
#define WRAPPER_ARGS_PROTO _WRAPPER_ARGS_PROTO
#define WRAPPER_ARGS_PROTO_FULL _WRAPPER_ARGS_PROTO
#define WRAPPER_ARGS fd, sa, salen
#define WRAPPER_ARGS_FULL WRAPPER_ARGS

static int __wrapper___connect(int fd, const struct sockaddr *sa, socklen_t salen) {
  if (sa && sa->sa_family == AF_UNIX) {
    const struct sockaddr_un *sau = (const struct sockaddr_un *)sa;
    fprintf(stderr, "connect_unix %s\n", sau->sun_path);
    return SB_SAFE_CONNECT_INT(fd, sa, salen, sau->sun_path);
  } else {
    return SB_SAFE_CONNECT_INT(fd, sa, salen, NULL);
  }
}

#define WRAPPER_SAFE() __wrapper___connect(fd, sa, salen)
#include "__wrapper_simple.c"
