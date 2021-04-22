/*
 * get_sandbox_conf.c
 *
 * Util functions.
 *
 * Copyright 1999-2008 Gentoo Foundation
 * Licensed under the GPL-2
 */

#include "headers.h"
#include "sbutil.h"

/* Where the file exists in our source tree */
#define LOCAL_SANDBOX_CONF_FILE "/etc/sandbox.conf"
char *SANDBOX_CONF_FILE = ETCDIR "/sandbox.conf";

void get_sandbox_conf(char *path)
{
	char *ret = SANDBOX_CONF_FILE;
  char *sandbox_conf_env = getenv(ENV_SANDBOX_CONF);
	save_errno();
  if (sandbox_conf_env) {
    strcpy(path, sandbox_conf_env);
  } else if (is_env_on(ENV_SANDBOX_TESTING)) {
		char *abs = sandbox_conf_env;
    if (!abs) {
      abs = getenv("abs_top_srcdir");
    }
		sprintf(path, "%s%s", abs, LOCAL_SANDBOX_CONF_FILE);
	} else {
    strcpy(path, ret);
  }

	restore_errno();
}
