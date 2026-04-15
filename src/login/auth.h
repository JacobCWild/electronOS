/* ===========================================================================
 * electronOS Login Manager — Authentication Header
 * =========================================================================== */

#ifndef ELECTRONOS_LOGIN_AUTH_H
#define ELECTRONOS_LOGIN_AUTH_H

typedef enum {
    AUTH_SUCCESS = 0,
    AUTH_FAILURE,
    AUTH_ACCOUNT_LOCKED,
    AUTH_ERROR,
} auth_result_t;

/*
 * Authenticate a user via PAM.
 * Returns AUTH_SUCCESS if the username/password combination is valid.
 */
auth_result_t auth_authenticate(const char *username, const char *password);

#endif /* ELECTRONOS_LOGIN_AUTH_H */
