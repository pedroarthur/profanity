/*
 * connection.c
 *
 * Copyright (C) 2012 - 2014 James Booth <boothj5@gmail.com>
 *
 * This file is part of Profanity.
 *
 * Profanity is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Profanity is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Profanity.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <strophe.h>

#include "chat_session.h"
#include "common.h"
#include "config/preferences.h"
#include "jid.h"
#include "log.h"
#include "muc.h"
#include "profanity.h"
#include "server_events.h"
#include "xmpp/bookmark.h"
#include "xmpp/capabilities.h"
#include "xmpp/connection.h"
#include "xmpp/iq.h"
#include "xmpp/message.h"
#include "xmpp/presence.h"
#include "xmpp/roster.h"
#include "xmpp/stanza.h"
#include "xmpp/xmpp.h"

static struct _jabber_conn_t {
    xmpp_log_t *log;
    xmpp_ctx_t *ctx;
    xmpp_conn_t *conn;
    jabber_conn_status_t conn_status;
    char *presence_message;
    int priority;
    int tls_disabled;
    char *domain;
} jabber_conn;

static GHashTable *available_resources;

// for auto reconnect
static struct {
    char *name;
    char *passwd;
} saved_account;

static struct {
    char *name;
    char *jid;
    char *passwd;
    char *altdomain;
    int port;
} saved_details;

static GTimer *reconnect_timer;

static log_level_t _get_log_level(xmpp_log_level_t xmpp_level);
static xmpp_log_level_t _get_xmpp_log_level();
static void _xmpp_file_logger(void * const userdata,
    const xmpp_log_level_t level, const char * const area,
    const char * const msg);
static xmpp_log_t * _xmpp_get_file_logger();

static jabber_conn_status_t _jabber_connect(const char * const fulljid,
    const char * const passwd, const char * const altdomain, int port);
static void _jabber_reconnect(void);

static void _connection_handler(xmpp_conn_t * const conn,
    const xmpp_conn_event_t status, const int error,
    xmpp_stream_error_t * const stream_error, void * const userdata);

void _connection_free_saved_account(void);
void _connection_free_saved_details(void);
void _connection_free_session_data(void);

static void
_jabber_init(const int disable_tls)
{
    log_info("Initialising XMPP");
    jabber_conn.conn_status = JABBER_STARTED;
    jabber_conn.presence_message = NULL;
    jabber_conn.conn = NULL;
    jabber_conn.ctx = NULL;
    jabber_conn.tls_disabled = disable_tls;
    jabber_conn.domain = NULL;
    presence_sub_requests_init();
    caps_init();
    available_resources = g_hash_table_new_full(g_str_hash, g_str_equal, free,
        (GDestroyNotify)resource_destroy);
    xmpp_initialize();
}

static jabber_conn_status_t
_jabber_connect_with_account(const ProfAccount * const account)
{
    assert(account != NULL);

    log_info("Connecting using account: %s", account->name);

    // save account name and password for reconnect
    saved_account.name = strdup(account->name);
    saved_account.passwd = strdup(account->password);

    // connect with fulljid
    Jid *jidp = jid_create_from_bare_and_resource(account->jid, account->resource);
    jabber_conn_status_t result =
      _jabber_connect(jidp->fulljid, account->password, account->server, account->port);
    jid_destroy(jidp);

    return result;
}

static jabber_conn_status_t
_jabber_connect_with_details(const char * const jid,
    const char * const passwd, const char * const altdomain, const int port)
{
    assert(jid != NULL);
    assert(passwd != NULL);

    // save details for reconnect, remember name for account creating on success
    saved_details.name = strdup(jid);
    saved_details.passwd = strdup(passwd);
    if (altdomain != NULL) {
        saved_details.altdomain = strdup(altdomain);
    } else {
        saved_details.altdomain = NULL;
    }
    if (port != 0) {
        saved_details.port = port;
    } else {
        saved_details.port = 0;
    }

    // use 'profanity' when no resourcepart in provided jid
    Jid *jidp = jid_create(jid);
    if (jidp->resourcepart == NULL) {
        jid_destroy(jidp);
        jidp = jid_create_from_bare_and_resource(jid, "profanity");
        saved_details.jid = strdup(jidp->fulljid);
    } else {
        saved_details.jid = strdup(jid);
    }
    jid_destroy(jidp);

    // connect with fulljid
    log_info("Connecting without account, JID: %s", saved_details.jid);
    return _jabber_connect(saved_details.jid, passwd, saved_details.altdomain, saved_details.port);
}

static void
_jabber_disconnect(void)
{
    // if connected, send end stream and wait for response
    if (jabber_conn.conn_status == JABBER_CONNECTED) {
        log_info("Closing connection");
        jabber_conn.conn_status = JABBER_DISCONNECTING;
        xmpp_disconnect(jabber_conn.conn);

        while (jabber_get_connection_status() == JABBER_DISCONNECTING) {
            jabber_process_events();
        }
        _connection_free_saved_account();
        _connection_free_saved_details();
        _connection_free_session_data();
        if (jabber_conn.conn != NULL) {
            xmpp_conn_release(jabber_conn.conn);
            jabber_conn.conn = NULL;
        }
        if (jabber_conn.ctx != NULL) {
            xmpp_ctx_free(jabber_conn.ctx);
            jabber_conn.ctx = NULL;
        }
    }

    jabber_conn.conn_status = JABBER_STARTED;
    FREE_SET_NULL(jabber_conn.presence_message);
    FREE_SET_NULL(jabber_conn.domain);
}

static void
_jabber_shutdown(void)
{
    xmpp_shutdown();
}

static void
_jabber_process_events(void)
{
    // run xmpp event loop if connected, connecting or disconnecting
    if (jabber_conn.conn_status == JABBER_CONNECTED
            || jabber_conn.conn_status == JABBER_CONNECTING
            || jabber_conn.conn_status == JABBER_DISCONNECTING) {
        xmpp_run_once(jabber_conn.ctx, 10);

    // check timer and reconnect if disconnected and timer set
    } else if (prefs_get_reconnect() != 0) {
        if ((jabber_conn.conn_status == JABBER_DISCONNECTED) &&
            (reconnect_timer != NULL)) {
            if (g_timer_elapsed(reconnect_timer, NULL) > prefs_get_reconnect()) {
                _jabber_reconnect();
            }
        }
    }

}

static GList *
_jabber_get_available_resources(void)
{
    return g_hash_table_get_values(available_resources);
}

static jabber_conn_status_t
_jabber_get_connection_status(void)
{
    return (jabber_conn.conn_status);
}

xmpp_conn_t *
connection_get_conn(void)
{
    return jabber_conn.conn;
}

xmpp_ctx_t *
connection_get_ctx(void)
{
    return jabber_conn.ctx;
}

static const char *
_jabber_get_fulljid(void)
{
    return xmpp_conn_get_jid(jabber_conn.conn);
}

static const char *
_jabber_get_domain(void)
{
    return jabber_conn.domain;
}

static char *
_jabber_get_presence_message(void)
{
    return jabber_conn.presence_message;
}

static char *
_jabber_get_account_name(void)
{
    return saved_account.name;
}

void
connection_set_presence_message(const char * const message)
{
    FREE_SET_NULL(jabber_conn.presence_message);
    if (message != NULL) {
        jabber_conn.presence_message = strdup(message);
    }
}

void
connection_set_priority(const int priority)
{
    jabber_conn.priority = priority;
}

void
connection_add_available_resource(Resource *resource)
{
    g_hash_table_replace(available_resources, strdup(resource->name), resource);
}

void
connection_remove_available_resource(const char * const resource)
{
    g_hash_table_remove(available_resources, resource);
}

void
_connection_free_saved_account(void)
{
    FREE_SET_NULL(saved_account.name);
    FREE_SET_NULL(saved_account.passwd);
}

void
_connection_free_saved_details(void)
{
    FREE_SET_NULL(saved_details.name);
    FREE_SET_NULL(saved_details.jid);
    FREE_SET_NULL(saved_details.passwd);
    FREE_SET_NULL(saved_details.altdomain);
}

void
_connection_free_session_data(void)
{
    g_hash_table_remove_all(available_resources);
    chat_sessions_clear();
    presence_clear_sub_requests();
}

static jabber_conn_status_t
_jabber_connect(const char * const fulljid, const char * const passwd,
    const char * const altdomain, int port)
{
    assert(fulljid != NULL);
    assert(passwd != NULL);

    Jid *jid = jid_create(fulljid);

    if (jid == NULL) {
        log_error("Malformed JID not able to connect: %s", fulljid);
        jabber_conn.conn_status = JABBER_DISCONNECTED;
        return jabber_conn.conn_status;
    } else if (jid->fulljid == NULL) {
        log_error("Full JID required to connect, received: %s", fulljid);
        jabber_conn.conn_status = JABBER_DISCONNECTED;
        return jabber_conn.conn_status;
    }

    jid_destroy(jid);

    log_info("Connecting as %s", fulljid);
    jabber_conn.log = _xmpp_get_file_logger();

    if (jabber_conn.conn != NULL) {
        xmpp_conn_release(jabber_conn.conn);
    }
    if (jabber_conn.ctx != NULL) {
        xmpp_ctx_free(jabber_conn.ctx);
    }
    jabber_conn.ctx = xmpp_ctx_new(NULL, jabber_conn.log);
    if (jabber_conn.ctx == NULL) {
        log_warning("Failed to get libstrophe ctx during connect");
        return JABBER_DISCONNECTED;
    }
    jabber_conn.conn = xmpp_conn_new(jabber_conn.ctx);
    if (jabber_conn.conn == NULL) {
        log_warning("Failed to get libstrophe conn during connect");
        return JABBER_DISCONNECTED;
    }
    xmpp_conn_set_jid(jabber_conn.conn, fulljid);
    xmpp_conn_set_pass(jabber_conn.conn, passwd);
    if (jabber_conn.tls_disabled) {
        xmpp_conn_disable_tls(jabber_conn.conn);
    }

    int connect_status = xmpp_connect_client(jabber_conn.conn, altdomain, port,
        _connection_handler, jabber_conn.ctx);

    if (connect_status == 0)
        jabber_conn.conn_status = JABBER_CONNECTING;
    else
        jabber_conn.conn_status = JABBER_DISCONNECTED;

    return jabber_conn.conn_status;
}

static void
_jabber_reconnect(void)
{
    // reconnect with account.
    ProfAccount *account = accounts_get_account(saved_account.name);

    if (account == NULL) {
        log_error("Unable to reconnect, account no longer exists: %s", saved_account.name);
    } else {
        char *fulljid = create_fulljid(account->jid, account->resource);
        log_debug("Attempting reconnect with account %s", account->name);
        _jabber_connect(fulljid, saved_account.passwd, account->server, account->port);
        free(fulljid);
        g_timer_start(reconnect_timer);
    }
}

static void
_connection_handler(xmpp_conn_t * const conn,
    const xmpp_conn_event_t status, const int error,
    xmpp_stream_error_t * const stream_error, void * const userdata)
{
    // login success
    if (status == XMPP_CONN_CONNECT) {
        log_debug("Connection handler: XMPP_CONN_CONNECT");

        // logged in with account
        if (saved_account.name != NULL) {
            log_debug("Connection handler: logged in with account name: %s", saved_account.name);
            handle_login_account_success(saved_account.name);

        // logged in without account, use details to create new account
        } else {
            log_debug("Connection handler: logged in with jid: %s", saved_details.name);
            accounts_add(saved_details.name, saved_details.altdomain, saved_details.port);
            accounts_set_jid(saved_details.name, saved_details.jid);

            handle_login_account_success(saved_details.name);
            saved_account.name = strdup(saved_details.name);
            saved_account.passwd = strdup(saved_details.passwd);

            _connection_free_saved_details();
        }

        Jid *myJid = jid_create(jabber_get_fulljid());
        jabber_conn.domain = strdup(myJid->domainpart);
        jid_destroy(myJid);

        chat_sessions_init();

        roster_add_handlers();
        message_add_handlers();
        presence_add_handlers();
        iq_add_handlers();

        roster_request();
        bookmark_request();
        jabber_conn.conn_status = JABBER_CONNECTED;

        if (prefs_get_reconnect() != 0) {
            if (reconnect_timer != NULL) {
                g_timer_destroy(reconnect_timer);
                reconnect_timer = NULL;
            }
        }

    } else if (status == XMPP_CONN_DISCONNECT) {
        log_debug("Connection handler: XMPP_CONN_DISCONNECT");

        // lost connection for unkown reason
        if (jabber_conn.conn_status == JABBER_CONNECTED) {
            log_debug("Connection handler: Lost connection for unknown reason");
            handle_lost_connection();
            if (prefs_get_reconnect() != 0) {
                assert(reconnect_timer == NULL);
                reconnect_timer = g_timer_new();
                // free resources but leave saved_user untouched
                _connection_free_session_data();
            } else {
                _connection_free_saved_account();
                _connection_free_saved_details();
                _connection_free_session_data();
            }

        // login attempt failed
        } else if (jabber_conn.conn_status != JABBER_DISCONNECTING) {
            log_debug("Connection handler: Login failed");
            if (reconnect_timer == NULL) {
                log_debug("Connection handler: No reconnect timer");
                handle_failed_login();
                _connection_free_saved_account();
                _connection_free_saved_details();
                _connection_free_session_data();
            } else {
                log_debug("Connection handler: Restarting reconnect timer");
                if (prefs_get_reconnect() != 0) {
                    g_timer_start(reconnect_timer);
                }
                // free resources but leave saved_user untouched
                _connection_free_session_data();
            }
        }

        // close stream response from server after disconnect is handled too
        jabber_conn.conn_status = JABBER_DISCONNECTED;
    } else if (status == XMPP_CONN_FAIL) {
        log_debug("Connection handler: XMPP_CONN_FAIL");
    } else {
        log_error("Connection handler: Unknown status");
    }
}

static log_level_t
_get_log_level(const xmpp_log_level_t xmpp_level)
{
    if (xmpp_level == XMPP_LEVEL_DEBUG) {
        return PROF_LEVEL_DEBUG;
    } else if (xmpp_level == XMPP_LEVEL_INFO) {
        return PROF_LEVEL_INFO;
    } else if (xmpp_level == XMPP_LEVEL_WARN) {
        return PROF_LEVEL_WARN;
    } else {
        return PROF_LEVEL_ERROR;
    }
}

static xmpp_log_level_t
_get_xmpp_log_level()
{
    log_level_t prof_level = log_get_filter();

    if (prof_level == PROF_LEVEL_DEBUG) {
        return XMPP_LEVEL_DEBUG;
    } else if (prof_level == PROF_LEVEL_INFO) {
        return XMPP_LEVEL_INFO;
    } else if (prof_level == PROF_LEVEL_WARN) {
        return XMPP_LEVEL_WARN;
    } else {
        return XMPP_LEVEL_ERROR;
    }
}

static void
_xmpp_file_logger(void * const userdata, const xmpp_log_level_t level,
    const char * const area, const char * const msg)
{
    log_level_t prof_level = _get_log_level(level);
    log_msg(prof_level, area, msg);
}

static xmpp_log_t *
_xmpp_get_file_logger()
{
    xmpp_log_level_t level = _get_xmpp_log_level();
    xmpp_log_t *file_log = malloc(sizeof(xmpp_log_t));

    file_log->handler = _xmpp_file_logger;
    file_log->userdata = &level;

    return file_log;
}

void
jabber_init_module(void)
{
    jabber_init = _jabber_init;
    jabber_connect_with_account = _jabber_connect_with_account;
    jabber_connect_with_details = _jabber_connect_with_details;
    jabber_disconnect = _jabber_disconnect;
    jabber_shutdown = _jabber_shutdown;
    jabber_process_events = _jabber_process_events;
    jabber_get_available_resources = _jabber_get_available_resources;
    jabber_get_connection_status = _jabber_get_connection_status;
    jabber_get_fulljid = _jabber_get_fulljid;
    jabber_get_domain = _jabber_get_domain;
    jabber_get_presence_message = _jabber_get_presence_message;
    jabber_get_account_name = _jabber_get_account_name;
}
