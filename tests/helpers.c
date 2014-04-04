#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdlib.h>
#include <cmocka.h>
#include <glib.h>
#include <stdio.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"
#include "config/preferences.h"

void create_config_dir(void **state)
{
    setenv("XDG_CONFIG_HOME", "./tests/files/xdg_config_home", 1);
    gchar *xdg_config = xdg_get_config_home();

    GString *profanity_dir = g_string_new(xdg_config);
    g_string_append(profanity_dir, "/profanity");

    if (!mkdir_recursive(profanity_dir->str)) {
        assert_true(FALSE);
    }

    g_free(xdg_config);
    g_string_free(profanity_dir, TRUE);
}

void remove_config_dir(void **state)
{
    rmdir("./tests/files/xdg_config_home/profanity");
    rmdir("./tests/files/xdg_config_home");
}

void create_data_dir(void **state)
{
    setenv("XDG_DATA_HOME", "./tests/files/xdg_data_home", 1);
    gchar *xdg_data = xdg_get_data_home();

    GString *profanity_dir = g_string_new(xdg_data);
    g_string_append(profanity_dir, "/profanity");

    if (!mkdir_recursive(profanity_dir->str)) {
        assert_true(FALSE);
    }

    g_free(xdg_data);
    g_string_free(profanity_dir, TRUE);
}

void remove_data_dir(void **state)
{
    rmdir("./tests/files/xdg_data_home/profanity");
    rmdir("./tests/files/xdg_data_home");
}

void load_preferences(void **state)
{
    create_config_dir(state);
    FILE *f = fopen("./tests/files/xdg_config_home/profanity/profrc", "ab+");
    if (f) {
        prefs_load();
    }
}

void close_preferences(void **state)
{
    prefs_close();
    remove("./tests/files/xdg_config_home/profanity/profrc");
    remove_config_dir(state);
    rmdir("./tests/files");
}

static GCompareFunc cmp_func;

void
glist_set_cmp(GCompareFunc func)
{
    cmp_func = func;
}

int
glist_contents_equal(const void *actual, const void *expected)
{
    GList *ac = (GList *)actual;
    GList *ex = (GList *)expected;

    GList *p = ex;
    printf("\nExpected\n");
    while(ex) {
        printf("\n\n%s\n", (char*)p->data);
        ex = g_list_next(ex);
    }
    printf("\n\n");
    p = ac;
    printf("\nActual\n");
    while(ac) {
        printf("\n\n%s\n", (char *)p->data);
        ac = g_list_next(ac);
    }
    printf("\n\n");

    if (g_list_length(ex) != g_list_length(ac)) {
        return 0;
    }

    GList *ex_curr = ex;
    while (ex_curr != NULL) {
        if (g_list_find_custom(ac, ex_curr->data, cmp_func) == NULL) {
            return 0;
        }
        ex_curr = g_list_next(ex_curr);
    }

    return 1;
}
