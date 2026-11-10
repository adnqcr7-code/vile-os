/* Standalone PAM check — proves the lock screen's real unlock mechanism
 * works in this VM before it is wired into the compositor.
 * Usage: ./pam_test <user> <password>   (exit 0 = password correct)
 * This is a TEST tool only; it is not shipped in the compositor. */
#define _POSIX_C_SOURCE 200809L
#include <security/pam_appl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *password;

static int conv_cb(int num_msg, const struct pam_message **msg,
		struct pam_response **resp, void *appdata_ptr) {
	struct pam_response *r = calloc(num_msg, sizeof(*r));
	if (!r) {
		return PAM_BUF_ERR;
	}
	for (int i = 0; i < num_msg; i++) {
		if (msg[i]->msg_style == PAM_PROMPT_ECHO_OFF) {
			r[i].resp = strdup(password);
			r[i].resp_retcode = 0;
		}
	}
	*resp = r;
	return PAM_SUCCESS;
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <user> <password>\n", argv[0]);
		return 2;
	}
	password = argv[2];
	struct pam_conv conv = { conv_cb, NULL };
	pam_handle_t *pamh = NULL;
	int ret = pam_start("vile", argv[1], &conv, &pamh);
	if (ret != PAM_SUCCESS) {
		printf("pam_start failed: %s\n", pam_strerror(pamh, ret));
		return 2;
	}
	ret = pam_authenticate(pamh, 0);
	if (ret != PAM_SUCCESS) {
		printf("AUTH FAIL for %s: %s\n", argv[1], pam_strerror(pamh, ret));
		pam_end(pamh, ret);
		return 1;
	}
	ret = pam_acct_mgmt(pamh, 0);
	if (ret != PAM_SUCCESS) {
		printf("ACCT FAIL for %s: %s\n", argv[1], pam_strerror(pamh, ret));
		pam_end(pamh, ret);
		return 1;
	}
	printf("AUTH OK for %s\n", argv[1]);
	pam_end(pamh, PAM_SUCCESS);
	return 0;
}
