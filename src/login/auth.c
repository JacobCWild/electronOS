/* ===========================================================================
 * electronOS Login Manager — PAM Authentication
 * ===========================================================================
 * Uses Linux PAM to authenticate users. The PAM service name is
 * "electronos-login" (configured in /etc/pam.d/electronos-login).
 * =========================================================================== */

#include "auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>

#define PAM_SERVICE "electronos-login"

/* ---- PAM conversation callback ------------------------------------------ */

typedef struct {
    const char *password;
} auth_conv_data_t;

static int pam_conversation(int num_msg, const struct pam_message **msg,
                            struct pam_response **resp, void *appdata) {
    auth_conv_data_t *data = (auth_conv_data_t *)appdata;

    struct pam_response *responses = calloc(num_msg,
                                            sizeof(struct pam_response));
    if (!responses) return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; i++) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            /* Provide the password */
            responses[i].resp = strdup(data->password);
            if (!responses[i].resp) {
                /* Clean up already-allocated responses */
                for (int j = 0; j < i; j++) {
                    free(responses[j].resp);
                }
                free(responses);
                return PAM_BUF_ERR;
            }
            responses[i].resp_retcode = 0;
            break;

        case PAM_ERROR_MSG:
            fprintf(stderr, "PAM error: %s\n", msg[i]->msg);
            responses[i].resp = NULL;
            responses[i].resp_retcode = 0;
            break;

        case PAM_TEXT_INFO:
            responses[i].resp = NULL;
            responses[i].resp_retcode = 0;
            break;

        default:
            free(responses);
            return PAM_CONV_ERR;
        }
    }

    *resp = responses;
    return PAM_SUCCESS;
}

/* ---- Public API --------------------------------------------------------- */

auth_result_t auth_authenticate(const char *username, const char *password) {
    if (!username || !password) return AUTH_ERROR;

    pam_handle_t *pamh = NULL;
    auth_conv_data_t conv_data = { .password = password };

    struct pam_conv conv = {
        .conv = pam_conversation,
        .appdata_ptr = &conv_data,
    };

    int ret = pam_start(PAM_SERVICE, username, &conv, &pamh);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_start failed: %s\n", pam_strerror(pamh, ret));
        return AUTH_ERROR;
    }

    /* Authenticate */
    ret = pam_authenticate(pamh, 0);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_authenticate: %s\n", pam_strerror(pamh, ret));
        pam_end(pamh, ret);
        return AUTH_FAILURE;
    }

    /* Check account validity (expiration, etc.) */
    ret = pam_acct_mgmt(pamh, 0);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_acct_mgmt: %s\n", pam_strerror(pamh, ret));
        pam_end(pamh, ret);
        if (ret == PAM_USER_UNKNOWN || ret == PAM_ACCT_EXPIRED) {
            return AUTH_ACCOUNT_LOCKED;
        }
        return AUTH_FAILURE;
    }

    /* Open session */
    ret = pam_open_session(pamh, 0);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_open_session: %s\n", pam_strerror(pamh, ret));
    }

    pam_end(pamh, PAM_SUCCESS);
    return AUTH_SUCCESS;
}
