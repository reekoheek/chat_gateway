/* 
 * File:   main.c
 * Author: jafar
 *
 * Created on June 29, 2011, 3:51 PM
 */

#include "main.h"

PurpleAccount *g_account;

MYSQL *g_conn;

char *MYSQL_HOSTNAME, *MYSQL_USERNAME, *MYSQL_PASSWORD, *MYSQL_DATABASE;
char *PURP_ACCOUNT, *PURP_PROTOCOL, *PURP_PASSWORD;

MYSQL* data_get_connection(void) {
    if (g_conn == NULL) {
        g_conn = mysql_init(NULL);
        if (!mysql_real_connect(g_conn, MYSQL_HOSTNAME,
                MYSQL_USERNAME, MYSQL_PASSWORD, MYSQL_DATABASE, 0, NULL, 0)) {
            fprintf(stderr, "%s\n", mysql_error(g_conn));
            exit(1);
        }
    }
    return g_conn;
}

MYSQL_RES* data_query(const char *sql) {
    MYSQL *conn = data_get_connection();
    if (mysql_query(conn, sql)) {
        fprintf(stderr, "%s\n", mysql_error(conn));
        exit(1);
    }

    return mysql_use_result(conn);
}

char *data_row_value(MYSQL_ROW row, const char *field_name, MYSQL_RES *result) {
    MYSQL_FIELD *field;

    int index = 0;

    mysql_field_seek(result, 0);
    while ((field = mysql_fetch_field(result)) != NULL) {
        if (strcmp(field->name, field_name) == 0) {
            return row[index];
        }
        index++;
    }
    return NULL;
}

static void purple_glib_io_destroy(gpointer data) {
    g_free(data);
}

static gboolean purple_glib_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data) {
    PurpleGLibIOClosure *closure = data;
    PurpleInputCondition purple_cond = 0;

    if (condition & PURPLE_GLIB_READ_COND)
        purple_cond |= PURPLE_INPUT_READ;
    if (condition & PURPLE_GLIB_WRITE_COND)
        purple_cond |= PURPLE_INPUT_WRITE;

    closure->function(closure->data, g_io_channel_unix_get_fd(source),
            purple_cond);

    return TRUE;
}

static guint glib_input_add(gint fd, PurpleInputCondition condition, PurpleInputFunction function,
        gpointer data) {
    PurpleGLibIOClosure *closure = g_new0(PurpleGLibIOClosure, 1);
    GIOChannel *channel;
    GIOCondition cond = 0;

    closure->function = function;
    closure->data = data;

    if (condition & PURPLE_INPUT_READ)
        cond |= PURPLE_GLIB_READ_COND;
    if (condition & PURPLE_INPUT_WRITE)
        cond |= PURPLE_GLIB_WRITE_COND;

    channel = g_io_channel_unix_new(fd);
    closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
            purple_glib_io_invoke, closure, purple_glib_io_destroy);

    g_io_channel_unref(channel);
    return closure->result;
}

static PurpleEventLoopUiOps glib_eventloops = {
    g_timeout_add,
    g_source_remove,
    glib_input_add,
    g_source_remove,
    NULL,
#if GLIB_CHECK_VERSION(2,14,0)
    g_timeout_add_seconds,
#else
    NULL,
#endif

    /* padding */
    NULL,
    NULL,
    NULL
};

/*** End of the eventloop functions. ***/


static void network_disconnected(void) {

    printf("This machine has been disconnected from the internet\n");

    exit(0);

}

static void report_disconnect_reason(PurpleConnection *gc, PurpleConnectionError reason, const char *text) {

    PurpleAccount *account = purple_connection_get_account(gc);
    printf("Connection disconnected: \"%s\" (%s)\n  >Error: %d\n  >Reason: %s\n", purple_account_get_username(account), purple_account_get_protocol_id(account), reason, text);

    exit(0);
}

static void *request_authorize(PurpleAccount *account, const char *remote_user, const char *id, const char *alias, const char *message, gboolean on_list, PurpleAccountRequestAuthorizationCb authorize_cb, PurpleAccountRequestAuthorizationCb deny_cb, void *user_data) {

    printf("Buddy authorization request from \"%s\" (%s): %s\n", remote_user, purple_account_get_protocol_id(account), message);

    PurpleBuddy * b = purple_buddy_new(account, remote_user, remote_user);
    purple_blist_add_buddy(b, NULL, NULL, NULL);
    purple_account_add_buddy(account, b);

    authorize_cb(user_data);
    //deny_cb(user_data);

}

static PurpleAccountUiOps account_uiops = {
    NULL, /* notify_added          */
    NULL, /* status_changed        */
    NULL, /* request_add           */
    request_authorize, /* request_authorize     */
    NULL, /* close_account_request */
    NULL,
    NULL,
    NULL,
    NULL
};

static PurpleConnectionUiOps connection_uiops = {
    NULL, /* connect_progress         */
    NULL, /* connected                */
    NULL, /* disconnected             */
    NULL, /* notice                   */
    NULL, /* report_disconnect        */
    NULL, /* network_connected        */
    network_disconnected, /* network_disconnected     */
    report_disconnect_reason, /* report_disconnect_reason */
    NULL,
    NULL,
    NULL
};

static void ui_init(void) {
    /**
     * This should initialize the UI components for all the modules.
     */

    purple_connections_set_ui_ops(&connection_uiops);
    purple_accounts_set_ui_ops(&account_uiops);

}

static PurpleCoreUiOps core_uiops = {
    NULL,
    NULL,
    ui_init,
    NULL,

    /* padding */
    NULL,
    NULL,
    NULL,
    NULL
};

static void init_libpurple(void) {
    /* Set a custom user directory (optional) */
    purple_util_set_user_dir(CUSTOM_USER_DIRECTORY);

    /* We do not want any debugging for now to keep the noise to a minimum. */
    purple_debug_set_enabled(FALSE);

    /* Set the core-uiops, which is used to
     * 	- initialize the ui specific preferences.
     * 	- initialize the debug ui.
     * 	- initialize the ui components for all the modules.
     * 	- uninitialize the ui components for all the modules when the core terminates.
     */
    purple_core_set_ui_ops(&core_uiops);

    /* Set the uiops for the eventloop. If your client is glib-based, you can safely
     * copy this verbatim. */
    purple_eventloop_set_ui_ops(&glib_eventloops);

    /* Set path to search for plugins. The core (libpurple) takes care of loading the
     * core-plugins, which includes the protocol-plugins. So it is not essential to add
     * any path here, but it might be desired, especially for ui-specific plugins. */
    purple_plugins_add_search_path(CUSTOM_PLUGIN_PATH);

    /* Now that all the essential stuff has been set, let's try to init the core. It's
     * necessary to provide a non-NULL name for the current ui to the core. This name
     * is used by stuff that depends on this ui, for example the ui-specific plugins. */
    if (!purple_core_init(UI_ID)) {
        /* Initializing the core failed. Terminate. */
        fprintf(stderr,
                "libpurple initialization failed. Dumping core.\n"
                "Please report this!\n");
        abort();
    }

    /* Create and load the buddylist. */
    purple_set_blist(purple_blist_new());
    purple_blist_load();

    /* Load the preferences. */
    purple_prefs_load();

    /* Load the desired plugins. The client should save the list of loaded plugins in
     * the preferences using purple_plugins_save_loaded(PLUGIN_SAVE_PREF) */
    purple_plugins_load_saved(PLUGIN_SAVE_PREF);

    /* Load the pounces. */
    purple_pounces_load();
}

static void signed_on(PurpleConnection *gc) {

    PurpleAccount *account = purple_connection_get_account(gc);
    printf("Account connected: \"%s\" (%s)\n", purple_account_get_username(account), purple_account_get_protocol_id(account));

}

static void received_im_msg(PurpleAccount *account, char *sender, char *message,
        PurpleConversation *conv, PurpleMessageFlags flags) {

    if (conv == NULL) {
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, account, sender);
    }

    PurpleBuddy * b = purple_buddy_new(account, sender, sender);
    purple_blist_add_buddy(b, NULL, NULL, NULL);
    purple_account_add_buddy(account, b);

    MYSQL *conn = data_get_connection();
    char query[1000], *end;

    end = stpcpy(query, "INSERT INTO inbox VALUES(");
    *end++ = '\'';
    end += mysql_real_escape_string(conn, end, account->username, strlen(account->username));
    *end++ = '\'';
    *end++ = ',';
    *end++ = '\'';
    end += mysql_real_escape_string(conn, end, sender, strlen(sender));
    *end++ = '\'';
    *end++ = ',';
    *end++ = '\'';
    end += mysql_real_escape_string(conn, end, message, strlen(message));
    *end++ = '\'';
    *end++ = ',';
    *end++ = 'N';
    *end++ = 'O';
    *end++ = 'W';
    *end++ = '(';
    *end++ = ')';
    *end++ = ')';
    int i;
    for (i = 0; i < (unsigned int) (end - query); i++) {
        printf("%c", query[i]);
    }
    printf("\n");

    if (mysql_real_query(conn, query, (unsigned int) (end - query))) {
        fprintf(stderr, "Failed to insert row, Error: %s\n",
                mysql_error(conn));
    }
}

static int account_authorization_requested(PurpleAccount *account, const char *user) {
    printf("User \"%s\" (%s) has sent a buddy request\n", user, purple_account_get_protocol_id(account));

    PurpleBuddy * b = purple_buddy_new(account, user, user);
    purple_blist_add_buddy(b, NULL, NULL, NULL);
    purple_account_add_buddy(account, b);

    return 1; //authorize buddy request automatically (-1 denies it)
}

static void connect_to_signals(void) {

    static int handle;

    purple_signal_connect(purple_connections_get_handle(), "signed-on", &handle,
            PURPLE_CALLBACK(signed_on), NULL);

    purple_signal_connect(purple_conversations_get_handle(), "received-im-msg", &handle,
            PURPLE_CALLBACK(received_im_msg), NULL);

    purple_signal_connect(purple_accounts_get_handle(), "account-authorization-requested", &handle,
            PURPLE_CALLBACK(account_authorization_requested), NULL);

}

void read_outbox(void) {
    char sql[1024] = "";
    char username[1024] = "";
    
    if (purple_account_is_connected(g_account)) {        
        strcpy(username, "");
        mysql_escape_string(username, g_account->username, strlen(g_account->username));
        
        strcpy(sql, "");
        sprintf(sql, "SELECT * FROM outbox WHERE account = '%s'", username);
        //printf("%s\n", sql);
        MYSQL_RES *result = (MYSQL_RES *) data_query(sql);

        MYSQL_ROW row;
        while ((row = mysql_fetch_row(result)) != NULL) {
            char *to = (char *) data_row_value(row, "to", result);
            char *body = (char *) data_row_value(row, "body", result);

            PurpleConversation *conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, g_account, to);
            PurpleConvIm * im = purple_conversation_get_im_data(conv);
            purple_conv_im_send(im, body);

            printf("(%s) to(%s) (%s): %s\n", purple_utf8_strftime("%H:%M:%S", NULL), to, purple_conversation_get_name(conv), body);
        }

        mysql_free_result(result);
        
        strcpy(sql, "");
        sprintf(sql, "DELETE FROM outbox WHERE account = '%s'", username);
        //printf("%s\n", sql);
        data_query(sql);
    }
}

void *listener_listen() {
    while (1) {
        read_outbox();
        sleep(1);
    }
}

void read_config(int argc, char*argv[]) {
    GKeyFile *kf;
    GKeyFileFlags flags;
    GError *error = NULL;

    flags = G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS;
    kf = g_key_file_new();

    if (argc == 1) {
        if (!g_key_file_load_from_file(kf, "./main.conf", flags, &error)) {
            g_error("%s", error->message);
            exit(-1);
        }
    } else {
        if (!g_key_file_load_from_file(kf, argv[1], flags, &error)) {
            g_error("%s", error->message);
            exit(-1);
        }
    }

    MYSQL_HOSTNAME = g_key_file_get_string(kf, "mysql", "hostname", NULL);
    MYSQL_USERNAME = g_key_file_get_string(kf, "mysql", "username", NULL);
    MYSQL_PASSWORD = g_key_file_get_string(kf, "mysql", "password", NULL);
    MYSQL_DATABASE = g_key_file_get_string(kf, "mysql", "database", NULL);

    PURP_PROTOCOL = g_key_file_get_string(kf, "purple", "protocol", NULL);
    PURP_ACCOUNT = g_key_file_get_string(kf, "purple", "account", NULL);
    PURP_PASSWORD = g_key_file_get_string(kf, "purple", "password", NULL);

    g_key_file_free(kf);
}

int main(int argc, char *argv[]) {

    read_config(argc, argv);

    GMainLoop *loop = g_main_loop_new(NULL, FALSE);

    /* libpurple's built-in DNS resolution forks processes to perform
     * blocking lookups without blocking the main process.  It does not
     * handle SIGCHLD itself, so if the UI does not you quickly get an army
     * of zombie subprocesses marching around.
     */
    signal(SIGCHLD, SIG_IGN);

    init_libpurple();

    printf("libpurple initialized. Running version %s.\n", purple_core_get_version()); //I like to see the version number

    connect_to_signals();

    g_account = purple_account_new(PURP_ACCOUNT, PURP_PROTOCOL); //this could be prpl-aim, prpl-yahoo, prpl-msn, prpl-icq, etc.
    purple_account_set_password(g_account, PURP_PASSWORD);

    purple_accounts_add(g_account);
    purple_account_set_enabled(g_account, UI_ID, TRUE);

    pthread_t thread;

    pthread_create(&thread, NULL, listener_listen, (void*) NULL);

    g_main_loop_run(loop);

    return 0;

}