// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2024 Kontron Europe GmbH
 *
 * Author: Heiko Thiery <heiko.thiery@kontron.com>
 * Created: May 18, 2024
 */

#define _GNU_SOURCE
#define _XOPEN_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "script.h"

#define DEFAULT_SCRIPT "/etc/mspm0flash/ctrl"
#define ENV_VAR_NAME "MSPM0FLASH_CTRL"

enum script_param_t {
	PARAM_INIT,
	PARAM_EXIT,
};

static int execute_control_script(enum script_param_t param)
{
	static const char* param_str[] = {
		[PARAM_INIT] = "init",
		[PARAM_EXIT] = "exit",
	};

	const char *script;
	int rc;
	char *cmd;

	script = getenv(ENV_VAR_NAME) ? getenv(ENV_VAR_NAME) : DEFAULT_SCRIPT;
	(void)!asprintf(&cmd, "%s %s", script, param_str[param]);

	rc = system(cmd);
	if (rc != 0) {
		printf("ERROR: script returned %d (parameter %s)\n",
			WEXITSTATUS(rc), param_str[param]);
		free(cmd);
		return 1;
	}
	free(cmd);

	return 0;
}

int script_init(void)
{
	int rc;

	rc = execute_control_script(PARAM_INIT);
	if (rc) {
		return rc;
	}
	usleep(250000);

	return 0;
}

void script_exit(void)
{
	execute_control_script(PARAM_EXIT);
}
