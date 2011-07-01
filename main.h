/* 
 * File:   main.h
 * Author: jafar
 *
 * Created on June 30, 2011, 11:21 AM
 */

#ifndef MAIN_H
#define	MAIN_H

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	__cplusplus
}
#endif

#include "purple.h"

#include <glib.h>

#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <sys/stat.h>
#include <dirent.h>

#include <pthread.h>

#include <mysql.h>

#define CUSTOM_USER_DIRECTORY  "/dev/null"
#define CUSTOM_PLUGIN_PATH     ""
#define PLUGIN_SAVE_PREF       "/purple/user/plugins/saved"
#define UI_ID                  "user"

/**
 * The following eventloop functions are used in both pidgin and purple-text. If your
 * application uses glib mainloop, you can safely use this verbatim.
 */
#define PURPLE_GLIB_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define PURPLE_GLIB_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

typedef struct _PurpleGLibIOClosure {
    PurpleInputFunction function;
    guint result;
    gpointer data;
} PurpleGLibIOClosure;

typedef struct {
    PurpleAccountRequestType type;
    PurpleAccount *account;
    void *ui_handle;
    char *user;
    gpointer userdata;
    PurpleAccountRequestAuthorizationCb auth_cb;
    PurpleAccountRequestAuthorizationCb deny_cb;
    guint ref;
} PurpleAccountRequestInfo;

#endif	/* MAIN_H */

