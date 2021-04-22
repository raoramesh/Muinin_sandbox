/*
 * get_sandbox_rc.c
 *
 * Util functions.
 *
 * Copyright 1999-2008 Gentoo Foundation
 * Licensed under the GPL-2
 *
 * Some parts might have Copyright:
 *   Copyright (C) 2002 Brad House <brad@mainstreetsoftworks.com>
 */

#include "headers.h"
#include "sbutil.h"

void get_sandbox_rc(char *path)
{
  const char *env_sandbox_bashrc = getenv(ENV_SANDBOX_BASHRC);
	save_errno();
	if (is_env_on(ENV_SANDBOX_TESTING)) {
		snprintf(path, SB_PATH_MAX, "%s/data/%s",
			getenv("abs_top_srcdir"), BASHRC_NAME);
  } else if (env_sandbox_bashrc) {
    strcpy(path, env_sandbox_bashrc);
  } else {
		snprintf(path, SB_PATH_MAX, "%s/%s",
			SANDBOX_BASHRC_PATH, BASHRC_NAME);
  }

	restore_errno();
}
