/* ===========================================================================
 * electronOS Login Manager — PAM Authentication
 * ===========================================================================
 * Uses Linux PAM to authenticate users. The PAM service name is
 * "electronos-login" (configured in /etc/pam.d/electronos-login).
 *
 * The PAM handle is kept alive for the duration of the user session
 * so that pam_close_session can be called properly on logout.
 * =========================================================================== */

#include "auth.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ELECTRONOS_PAM_SERVICE "electronos-login"

/* ---- PAM conversation callbacks ----------------------------------------- */

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

/*
 * No-op conversation used after authentication succeeds.
 * Replaces the stack-local password-bearing conversation so the PAM
 * handle never holds a dangling pointer once auth_authenticate returns.
 */
static int noop_conversation(int num_msg, const struct pam_message **msg,
                             struct pam_response **resp, void *appdata) {
    (void)appdata;
    struct pam_response *responses = calloc(num_msg,
                                            sizeof(struct pam_response));
    if (!responses) return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; i++) {
        if (msg[i]->msg_style == PAM_ERROR_MSG) {
            fprintf(stderr, "PAM error: %s\n", msg[i]->msg);
        }
        responses[i].resp = NULL;
        responses[i].resp_retcode = 0;
    }
    *resp = responses;
    return PAM_SUCCESS;
}

/* ---- Public API --------------------------------------------------------- */

auth_result_t auth_authenticate(const char *username, const char *password,
                                auth_session_t *session) {
    if (!username || !password || !session) return AUTH_ERROR;

    pam_handle_t *pamh = NULL;
    auth_conv_data_t conv_data = { .password = password };

    struct pam_conv conv = {
        .conv = pam_conversation,
        .appdata_ptr = &conv_data,
    };

    int ret = pam_start(ELECTRONOS_PAM_SERVICE, username, &conv, &pamh);
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

    /* Open session — the handle is kept alive until auth_close_session() */
    ret = pam_open_session(pamh, 0);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_open_session: %s\n", pam_strerror(pamh, ret));
        pam_end(pamh, ret);
        return AUTH_ERROR;
    }

    /*
     * Replace the stack-local conversation with a heap-allocated no-op.
     * This prevents a dangling pointer: conv_data and conv live on this
     * function's stack frame, but the PAM handle persists until
     * auth_close_session(). If any PAM module calls the conversation
     * during pam_close_session, it must find a valid function pointer.
     */
    struct pam_conv *safe_conv = malloc(sizeof(struct pam_conv));
    if (!safe_conv) {
        pam_close_session(pamh, 0);
        pam_end(pamh, PAM_BUF_ERR);
        return AUTH_ERROR;
    }
    safe_conv->conv = noop_conversation;
    safe_conv->appdata_ptr = NULL;
    pam_set_item(pamh, PAM_CONV, safe_conv);

    /* Return the PAM handle and conversation to the caller */
    session->pamh = pamh;
    session->conv = safe_conv;
    return AUTH_SUCCESS;
}

void auth_close_session(auth_session_t *session) {
    if (!session || !session->pamh) return;

    int ret = pam_close_session(session->pamh, 0);
    if (ret != PAM_SUCCESS) {
        fprintf(stderr, "pam_close_session: %s\n",
                pam_strerror(session->pamh, ret));
    }

    pam_end(session->pamh, PAM_SUCCESS);
    session->pamh = NULL;

    /* Free the heap-allocated no-op conversation */
    free(session->conv);
    session->conv = NULL;
}
