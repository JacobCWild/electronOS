/* ===========================================================================
 * electronOS Login Manager — Authentication Header
 * ===========================================================================
 * Provides PAM-based authentication with proper session lifecycle:
 *   1. auth_authenticate()   — verify credentials, open PAM session
 *   2. auth_close_session()  — close PAM session on logout
 * =========================================================================== */

#ifndef ELECTRONOS_LOGIN_AUTH_H
#define ELECTRONOS_LOGIN_AUTH_H

#include <security/pam_appl.h>

typedef enum {
    AUTH_SUCCESS = 0,
    AUTH_FAILURE,
    AUTH_ACCOUNT_LOCKED,
    AUTH_ERROR,
} auth_result_t;

/* Opaque session handle returned by auth_authenticate on success. */
typedef struct {
    pam_handle_t *pamh;
} auth_session_t;

/*
 * Authenticate a user via PAM and open a session.
 * On AUTH_SUCCESS, *session is populated and MUST be closed later
 * with auth_close_session(). On failure, *session is untouched.
 */
auth_result_t auth_authenticate(const char *username, const char *password,
                                auth_session_t *session);

/*
 * Close a PAM session previously opened by auth_authenticate().
 * Must be called when the user logs out.
 */
void auth_close_session(auth_session_t *session);

#endif /* ELECTRONOS_LOGIN_AUTH_H */
