/**
 * @file main.c
 * @author Xiaolin He
 * @brief Application to configure TSN function based on sysrepo datastore.
 *
 * Copyright 2019 NXP
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <cjson/cJSON.h>

#include "common.h"
#include "main.h"
#include "qbv.h"
#include "qbu.h"

static uint8_t exit_application;

static int module_change_cb(sr_session_ctx_t *session, const char *module_name,
		sr_notif_event_t event, void *private_ctx)
{
	return SR_ERR_OK;
}

static void sigint_handler(int signum)
{
	exit_application = 1;
}

int main(int argc, char **argv)
{
	int rc = SR_ERR_OK;
	sr_conn_ctx_t *connection;
	sr_session_ctx_t *session;
	sr_subscription_ctx_t *subscription;
	char path[XPATH_MAX_LEN];
	sr_subscr_options_t opts;

	exit_application = 0;

	/* Init tsn mutex */
	init_tsn_mutex();

	/* Connect to sysrepo */
	rc = sr_connect("netconf-tsn", SR_CONN_DEFAULT, &connection);
	if (rc != SR_ERR_OK) {
		fprintf(stderr, "Error by sr_connect: %s\n", sr_strerror(rc));
		goto cleanup;
	}

	/* Start session */
	rc = sr_session_start(connection, SR_DS_STARTUP, SR_SESS_DEFAULT,
			      &session);
	if (rc != SR_ERR_OK) {
		fprintf(stderr, "Error by sr_session_start: %s\n",
			sr_strerror(rc));
		goto cleanup;
	}

	/* Subscribe to ietf-interfaces module */
	opts = SR_SUBSCR_APPLY_ONLY | SR_SUBSCR_DEFAULT | SR_SUBSCR_CTX_REUSE;
	rc = sr_module_change_subscribe(session, "ietf-interfaces",
					module_change_cb, NULL, 0, opts,
					&subscription);
	if (rc != SR_ERR_OK) {
		fprintf(stderr, "Error by sr_module_change_subscribe: %s\n",
			sr_strerror(rc));
		goto cleanup;
	}

	/* Subscribe to QBV subtree */
	opts = SR_SUBSCR_DEFAULT | SR_SUBSCR_CTX_REUSE | SR_SUBSCR_EV_ENABLED;
	snprintf(path, XPATH_MAX_LEN, IF_XPATH);
	strncat(path, QBV_GATE_PARA_XPATH, XPATH_MAX_LEN);
	rc = sr_subtree_change_subscribe(session, path, qbv_subtree_change_cb,
					 NULL, 0, opts, &subscription);
	if (rc != SR_ERR_OK) {
		fprintf(stderr, "Error by sr_module_change_subscribe: %s\n",
			sr_strerror(rc));
		goto cleanup;
	}

	/* Subscribe to QBV subtree */
	snprintf(path, XPATH_MAX_LEN, IF_XPATH);
	strncat(path, QBV_MAX_SDU_XPATH, XPATH_MAX_LEN);
	opts = SR_SUBSCR_DEFAULT | SR_SUBSCR_CTX_REUSE | SR_SUBSCR_EV_ENABLED;
	rc = sr_subtree_change_subscribe(session, path, qbv_subtree_change_cb,
					 NULL, 0, opts, &subscription);
	if (rc != SR_ERR_OK) {
		fprintf(stderr, "Error by sr_module_change_subscribe: %s\n",
			sr_strerror(rc));
		goto cleanup;
	}

	/* Subscribe to QBU subtree */
	snprintf(path, XPATH_MAX_LEN, IF_XPATH);
	strncat(path, QBU_XPATH, XPATH_MAX_LEN);
	opts = SR_SUBSCR_DEFAULT | SR_SUBSCR_CTX_REUSE | SR_SUBSCR_EV_ENABLED;
	rc = sr_subtree_change_subscribe(session, path, qbu_subtree_change_cb,
					 NULL, 0, opts, &subscription);
	if (rc != SR_ERR_OK) {
		fprintf(stderr, "Error by sr_module_change_subscribe: %s\n",
			sr_strerror(rc));
		goto cleanup;
	}

	/* Loop until ctrl-c is pressed / SIGINT is received */
	signal(SIGINT, sigint_handler);
	signal(SIGPIPE, SIG_IGN);
	while (!exit_application)
		sleep(1000);  /* Or do some more useful work... */

cleanup:
	destroy_tsn_mutex();
	if (subscription)
		sr_unsubscribe(session, subscription);
	if (session)
		sr_session_stop(session);
	if (connection)
		sr_disconnect(connection);
	return rc;
}