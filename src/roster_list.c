/*
 * roster_list.c
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


#include <string.h>
#include <glib.h>
#include <assert.h>

#include "roster_list.h"
#include "resource.h"
#include "contact.h"
#include "jid.h"
#include "tools/autocomplete.h"

// nicknames
static Autocomplete name_ac;

// barejids
static Autocomplete barejid_ac;

// fulljids
static Autocomplete fulljid_ac;

// groups
static Autocomplete groups_ac;

// contacts, indexed on barejid
static GHashTable *contacts;

// nickname to jid map
static GHashTable *name_to_barejid;

static gboolean _key_equals(void *key1, void *key2);
static gboolean _datetimes_equal(GDateTime *dt1, GDateTime *dt2);
static void _replace_name(const char * const current_name,
    const char * const new_name, const char * const barejid);
static void _add_name_and_barejid(const char * const name,
    const char * const barejid);
static gint _compare_contacts(PContact a, PContact b);

void
roster_clear(void)
{
    autocomplete_clear(name_ac);
    autocomplete_clear(barejid_ac);
    autocomplete_clear(fulljid_ac);
    autocomplete_clear(groups_ac);
    g_hash_table_destroy(contacts);
    contacts = g_hash_table_new_full(g_str_hash, (GEqualFunc)_key_equals, g_free,
        (GDestroyNotify)p_contact_free);
    g_hash_table_destroy(name_to_barejid);
    name_to_barejid = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
        g_free);
}

gboolean
roster_update_presence(const char * const barejid, Resource *resource,
    GDateTime *last_activity)
{
    assert(barejid != NULL);
    assert(resource != NULL);

    PContact contact = g_hash_table_lookup(contacts, barejid);
    if (contact == NULL) {
        return FALSE;
    }
    if (!_datetimes_equal(p_contact_last_activity(contact), last_activity)) {
        p_contact_set_last_activity(contact, last_activity);
    }
    p_contact_set_presence(contact, resource);
    Jid *jid = jid_create_from_bare_and_resource(barejid, resource->name);
    autocomplete_add(fulljid_ac, jid->fulljid);
    jid_destroy(jid);

    return TRUE;
}

PContact
roster_get_contact(const char * const barejid)
{
    return g_hash_table_lookup(contacts, barejid);
}

gboolean
roster_contact_offline(const char * const barejid,
    const char * const resource, const char * const status)
{
    PContact contact = g_hash_table_lookup(contacts, barejid);

    if (contact == NULL) {
        return FALSE;
    }
    if (resource == NULL) {
        return TRUE;
    } else {
        gboolean result = p_contact_remove_resource(contact, resource);
        if (result == TRUE) {
            Jid *jid = jid_create_from_bare_and_resource(barejid, resource);
            autocomplete_remove(fulljid_ac, jid->fulljid);
            jid_destroy(jid);
        }

        return result;
    }
}

void
roster_reset_search_attempts(void)
{
    autocomplete_reset(name_ac);
    autocomplete_reset(barejid_ac);
    autocomplete_reset(fulljid_ac);
    autocomplete_reset(groups_ac);
}

void
roster_init(void)
{
    name_ac = autocomplete_new();
    barejid_ac = autocomplete_new();
    fulljid_ac = autocomplete_new();
    groups_ac = autocomplete_new();
    contacts = g_hash_table_new_full(g_str_hash, (GEqualFunc)_key_equals, g_free,
        (GDestroyNotify)p_contact_free);
    name_to_barejid = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
        g_free);
}

void
roster_free(void)
{
    autocomplete_free(name_ac);
    autocomplete_free(barejid_ac);
    autocomplete_free(fulljid_ac);
    autocomplete_free(groups_ac);
}

void
roster_change_name(PContact contact, const char * const new_name)
{
    assert(contact != NULL);

    const char *current_name = NULL;
    const char *barejid = p_contact_barejid(contact);

    if (p_contact_name(contact) != NULL) {
        current_name = strdup(p_contact_name(contact));
    }

    p_contact_set_name(contact, new_name);
    _replace_name(current_name, new_name, barejid);
}

void
roster_remove(const char * const name, const char * const barejid)
{
    autocomplete_remove(barejid_ac, barejid);
    autocomplete_remove(name_ac, name);
    g_hash_table_remove(name_to_barejid, name);

    // remove each fulljid
    PContact contact = roster_get_contact(barejid);
    if (contact != NULL) {
        GList *resources = p_contact_get_available_resources(contact);
        while (resources != NULL) {
            GString *fulljid = g_string_new(strdup(barejid));
            g_string_append(fulljid, "/");
            g_string_append(fulljid, resources->data);
            autocomplete_remove(fulljid_ac, fulljid->str);
            g_string_free(fulljid, TRUE);
            resources = g_list_next(resources);
        }
    }

    // remove the contact
    g_hash_table_remove(contacts, barejid);
}

void
roster_update(const char * const barejid, const char * const name,
    GSList *groups, const char * const subscription, gboolean pending_out)
{
    PContact contact = g_hash_table_lookup(contacts, barejid);
    assert(contact != NULL);

    p_contact_set_subscription(contact, subscription);
    p_contact_set_pending_out(contact, pending_out);

    const char * const new_name = name;
    const char * current_name = NULL;
    if (p_contact_name(contact) != NULL) {
        current_name = strdup(p_contact_name(contact));
    }

    p_contact_set_name(contact, new_name);
    p_contact_set_groups(contact, groups);
    _replace_name(current_name, new_name, barejid);

    // add groups
    while (groups != NULL) {
        autocomplete_add(groups_ac, groups->data);
        groups = g_slist_next(groups);
    }
}

gboolean
roster_add(const char * const barejid, const char * const name, GSList *groups,
    const char * const subscription, gboolean pending_out)
{
    PContact contact = g_hash_table_lookup(contacts, barejid);
    if (contact != NULL) {
        return FALSE;
    }

    contact = p_contact_new(barejid, name, groups, subscription, NULL,
        pending_out);

    // add groups
    while (groups != NULL) {
        autocomplete_add(groups_ac, groups->data);
        groups = g_slist_next(groups);
    }

    g_hash_table_insert(contacts, strdup(barejid), contact);
    autocomplete_add(barejid_ac, barejid);
    _add_name_and_barejid(name, barejid);

    return TRUE;
}

char *
roster_barejid_from_name(const char * const name)
{
    return g_hash_table_lookup(name_to_barejid, name);
}

GSList *
roster_get_contacts(void)
{
    GSList *result = NULL;
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_hash_table_iter_init(&iter, contacts);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        result = g_slist_insert_sorted(result, value, (GCompareFunc)_compare_contacts);
    }

    // resturn all contact structs
    return result;
}

gboolean
roster_has_pending_subscriptions(void)
{
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_hash_table_iter_init(&iter, contacts);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        PContact contact = (PContact) value;
        if (p_contact_pending_out(contact)) {
            return TRUE;
        }
    }

    return FALSE;
}

char *
roster_find_contact(char *search_str)
{
    return autocomplete_complete(name_ac, search_str);
}

char *
roster_find_resource(char *search_str)
{
    return autocomplete_complete(fulljid_ac, search_str);
}

GSList *
roster_get_group(const char * const group)
{
    GSList *result = NULL;
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_hash_table_iter_init(&iter, contacts);
    while (g_hash_table_iter_next(&iter, &key, &value)) {
        GSList *groups = p_contact_groups(value);
        while (groups != NULL) {
            if (strcmp(groups->data, group) == 0) {
                result = g_slist_insert_sorted(result, value, (GCompareFunc)_compare_contacts);
                break;
            }
            groups = g_slist_next(groups);
        }
    }

    // resturn all contact structs
    return result;
}

GSList *
roster_get_groups(void)
{
    return autocomplete_get_list(groups_ac);
}

char *
roster_find_group(char *search_str)
{
    return autocomplete_complete(groups_ac, search_str);
}

char *
roster_find_jid(char *search_str)
{
    return autocomplete_complete(barejid_ac, search_str);
}

static
gboolean _key_equals(void *key1, void *key2)
{
    gchar *str1 = (gchar *) key1;
    gchar *str2 = (gchar *) key2;

    return (g_strcmp0(str1, str2) == 0);
}

static gboolean
_datetimes_equal(GDateTime *dt1, GDateTime *dt2)
{
    if ((dt1 == NULL) && (dt2 == NULL)) {
        return TRUE;
    } else if ((dt1 == NULL) && (dt2 != NULL)) {
        return FALSE;
    } else if ((dt1 != NULL) && (dt2 == NULL)) {
        return FALSE;
    } else {
        return g_date_time_equal(dt1, dt2);
    }
}

static void
_replace_name(const char * const current_name, const char * const new_name,
    const char * const barejid)
{
    // current handle exists already
    if (current_name != NULL) {
        autocomplete_remove(name_ac, current_name);
        g_hash_table_remove(name_to_barejid, current_name);
        _add_name_and_barejid(new_name, barejid);
    // no current handle
    } else if (new_name != NULL) {
        autocomplete_remove(name_ac, barejid);
        g_hash_table_remove(name_to_barejid, barejid);
        _add_name_and_barejid(new_name, barejid);
    }
}

static void
_add_name_and_barejid(const char * const name, const char * const barejid)
{
    if (name != NULL) {
        autocomplete_add(name_ac, name);
        g_hash_table_insert(name_to_barejid, strdup(name), strdup(barejid));
    } else {
        autocomplete_add(name_ac, barejid);
        g_hash_table_insert(name_to_barejid, strdup(barejid), strdup(barejid));
    }
}

static
gint _compare_contacts(PContact a, PContact b)
{
    const char * utf8_str_a = NULL;
    const char * utf8_str_b = NULL;

    if (p_contact_name(a) != NULL) {
        utf8_str_a = p_contact_name(a);
    } else {
        utf8_str_a = p_contact_barejid(a);
    }
    if (p_contact_name(b) != NULL) {
        utf8_str_b = p_contact_name(b);
    } else {
        utf8_str_b = p_contact_barejid(b);
    }

    gchar *key_a = g_utf8_collate_key(utf8_str_a, -1);
    gchar *key_b = g_utf8_collate_key(utf8_str_b, -1);

    gint result = g_strcmp0(key_a, key_b);

    g_free(key_a);
    g_free(key_b);

    return result;
}
