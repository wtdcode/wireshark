/* dissect_opts.c
 * Routines for dissection options setting
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <string.h>

#include <errno.h>

#include <glib.h>

#include <epan/prefs.h>
#include <epan/timestamp.h>
#include <epan/addr_resolv.h>
#include <epan/disabled_protos.h>

#include "ui/decode_as_utils.h"

#if defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)
#include <epan/dissectors/read_keytab_file.h>
#endif

#include <wsutil/clopts_common.h>
#include <wsutil/cmdarg_err.h>
#include <wsutil/file_util.h>
#include <wsutil/ws_assert.h>

#include "ui/dissect_opts.h"

dissect_options global_dissect_options;

void
dissect_opts_init(void)
{
    global_dissect_options.time_format = TS_NOT_SET;
    global_dissect_options.time_precision = TS_PREC_NOT_SET;
    global_dissect_options.disable_protocol_slist = NULL;
    global_dissect_options.enable_protocol_slist = NULL;
    global_dissect_options.enable_heur_slist = NULL;
    global_dissect_options.disable_heur_slist = NULL;
}

gboolean
dissect_opts_handle_opt(int opt, char *optarg_str_p)
{
    char badopt;
    char *p, *dotp;
    ts_precision tsp;

    switch(opt) {
    case 'd':        /* Decode as rule */
        if (!decode_as_command_option(optarg_str_p))
             return FALSE;
        break;
    case 'K':        /* Kerberos keytab file */
#if defined(HAVE_HEIMDAL_KERBEROS) || defined(HAVE_MIT_KERBEROS)
        read_keytab_file(optarg_str_p);
#else
        cmdarg_err("-K specified, but Kerberos keytab file support isn't present");
        return FALSE;
#endif
        break;
    case 'n':        /* No name resolution */
        disable_name_resolution();
        break;
    case 'N':        /* Select what types of addresses/port #s to resolve */
        badopt = string_to_name_resolve(optarg_str_p, &gbl_resolv_flags);
        if (badopt != '\0') {
            cmdarg_err("-N specifies unknown resolving option '%c'; valid options are:",
                       badopt);
            cmdarg_err_cont("\t'd' to enable address resolution from captured DNS packets\n"
                            "\t'm' to enable MAC address resolution\n"
                            "\t'n' to enable network address resolution\n"
                            "\t'N' to enable using external resolvers (e.g., DNS)\n"
                            "\t    for network address resolution\n"
                            "\t't' to enable transport-layer port number resolution\n"
                            "\t'v' to enable VLAN IDs to names resolution");
            return FALSE;
        }
        break;
    case 't':        /* Time stamp type */
        tsp = TS_PREC_NOT_SET;
        dotp = strchr(optarg_str_p, '.');
        if (dotp != NULL) {
            /* Set dotp to NULL on errors. */
            p = dotp;
            switch(*++p) {
                case '\0':
                    tsp = TS_PREC_AUTO;
                    break;
                case '0':
                    tsp = TS_PREC_FIXED_SEC;
                    break;
                case '1':
                    tsp = TS_PREC_FIXED_DSEC;
                    break;
                case '2':
                    tsp = TS_PREC_FIXED_CSEC;
                    break;
                case '3':
                    tsp = TS_PREC_FIXED_MSEC;
                    break;
                case '6':
                    tsp = TS_PREC_FIXED_USEC;
                    break;
                case '9':
                    tsp = TS_PREC_FIXED_NSEC;
                    break;
                default:
                    dotp = NULL;
                    break;
            }
            /* If we saw a '.', reject if do not have only '.' or a single digit '.N'. */
            if (dotp && *p != '\0' && *++p != '\0')
                dotp = NULL;
            if (dotp) {
                /* Mask the '.' while checking format. */
                *dotp = '\0';
            } else {
                cmdarg_err("Invalid .N time stamp precision \"%s\"; N must be 0, 1, 2, 3, 6, 9 or absent", optarg_str_p);
                return FALSE;
            }
        }
        if (strcmp(optarg_str_p, "r") == 0)
            global_dissect_options.time_format = TS_RELATIVE;
        else if (strcmp(optarg_str_p, "a") == 0)
            global_dissect_options.time_format = TS_ABSOLUTE;
        else if (strcmp(optarg_str_p, "ad") == 0)
            global_dissect_options.time_format = TS_ABSOLUTE_WITH_YMD;
        else if (strcmp(optarg_str_p, "adoy") == 0)
            global_dissect_options.time_format = TS_ABSOLUTE_WITH_YDOY;
        else if (strcmp(optarg_str_p, "d") == 0)
            global_dissect_options.time_format = TS_DELTA;
        else if (strcmp(optarg_str_p, "dd") == 0)
            global_dissect_options.time_format = TS_DELTA_DIS;
        else if (strcmp(optarg_str_p, "e") == 0)
            global_dissect_options.time_format = TS_EPOCH;
        else if (strcmp(optarg_str_p, "u") == 0)
            global_dissect_options.time_format = TS_UTC;
        else if (strcmp(optarg_str_p, "ud") == 0)
            global_dissect_options.time_format = TS_UTC_WITH_YMD;
        else if (strcmp(optarg_str_p, "udoy") == 0)
            global_dissect_options.time_format = TS_UTC_WITH_YDOY;
        else if (optarg_str_p != dotp) {
            /* If (optarg_str_p == dotp), user only set precision. */
            if (dotp)
                *dotp = '.';
            cmdarg_err("Invalid time stamp type \"%s\"; it must be one of:", optarg_str_p);
            cmdarg_err_cont("\t\"a\"    for absolute\n"
                            "\t\"ad\"   for absolute with YYYY-MM-DD date\n"
                            "\t\"adoy\" for absolute with YYYY/DOY date\n"
                            "\t\"d\"    for delta\n"
                            "\t\"dd\"   for delta displayed\n"
                            "\t\"e\"    for epoch\n"
                            "\t\"r\"    for relative\n"
                            "\t\"u\"    for absolute UTC\n"
                            "\t\"ud\"   for absolute UTC with YYYY-MM-DD date\n"
                            "\t\"udoy\" for absolute UTC with YYYY/DOY date");
            return FALSE;
        }
        if (dotp) {
            *dotp = '.';
            global_dissect_options.time_precision = tsp;
        }
        break;
    case 'u':        /* Seconds type */
        if (strcmp(optarg_str_p, "s") == 0)
            timestamp_set_seconds_type(TS_SECONDS_DEFAULT);
        else if (strcmp(optarg_str_p, "hms") == 0)
            timestamp_set_seconds_type(TS_SECONDS_HOUR_MIN_SEC);
        else {
            cmdarg_err("Invalid seconds type \"%s\"; it must be one of:", optarg_str_p);
            cmdarg_err_cont("\t\"s\"   for seconds\n"
                            "\t\"hms\" for hours, minutes and seconds");
            return FALSE;
        }
        break;
    case LONGOPT_DISABLE_PROTOCOL: /* disable dissection of protocol */
        global_dissect_options.disable_protocol_slist = g_slist_append(global_dissect_options.disable_protocol_slist, optarg_str_p);
        break;
    case LONGOPT_ENABLE_HEURISTIC: /* enable heuristic dissection of protocol */
        global_dissect_options.enable_heur_slist = g_slist_append(global_dissect_options.enable_heur_slist, optarg_str_p);
        break;
    case LONGOPT_DISABLE_HEURISTIC: /* disable heuristic dissection of protocol */
        global_dissect_options.disable_heur_slist = g_slist_append(global_dissect_options.disable_heur_slist, optarg_str_p);
        break;
    case LONGOPT_ENABLE_PROTOCOL: /* enable dissection of protocol (that is disableed by default) */
        global_dissect_options.enable_protocol_slist = g_slist_append(global_dissect_options.enable_protocol_slist, optarg_str_p);
        break;
    default:
        /* the caller is responsible to send us only the right opt's */
        ws_assert_not_reached();
    }
    return TRUE;
}

gboolean
setup_enabled_and_disabled_protocols(void)
{
    gboolean success = TRUE;

    if (global_dissect_options.disable_protocol_slist) {
        GSList *proto_disable;

        for (proto_disable = global_dissect_options.disable_protocol_slist; proto_disable != NULL; proto_disable = g_slist_next(proto_disable))
            proto_disable_proto_by_name((char*)proto_disable->data);
    }

    if (global_dissect_options.enable_protocol_slist) {
        GSList *proto_enable;

        for (proto_enable = global_dissect_options.enable_protocol_slist; proto_enable != NULL; proto_enable = g_slist_next(proto_enable))
            proto_enable_proto_by_name((char*)proto_enable->data);
    }

    if (global_dissect_options.enable_heur_slist) {
        GSList *heur_enable;

        for (heur_enable = global_dissect_options.enable_heur_slist; heur_enable != NULL; heur_enable = g_slist_next(heur_enable)) {
            if (!proto_enable_heuristic_by_name((char*)heur_enable->data, TRUE)) {
                cmdarg_err("No such protocol %s, can't enable", (char*)heur_enable->data);
                success = FALSE;
            }
        }
    }

    if (global_dissect_options.disable_heur_slist) {
        GSList *heur_disable;

        for (heur_disable = global_dissect_options.disable_heur_slist; heur_disable != NULL; heur_disable = g_slist_next(heur_disable)) {
            if (!proto_enable_heuristic_by_name((char*)heur_disable->data, FALSE)) {
                cmdarg_err("No such protocol %s, can't disable", (char*)heur_disable->data);
                success = FALSE;
            }
        }
    }
    return success;
}
