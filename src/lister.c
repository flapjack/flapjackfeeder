/*****************************************************************************
 *
 * FLAPJACKFEEDER.C
 * Copyright (c) 2013-2015 Birger Schmidt (http://flapjack.io)
 *
 * Derived from NPCDMOD.C ...
 * Copyright (c) 2008-2010 PNP4Nagios Project (http://www.pnp4nagios.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *****************************************************************************/

#ifdef HAVE_NAEMON_H
/* we compile for the naemon core ( -DHAVE_NAEMON_H was given as compile option ) */
#include "../naemon/naemon.h"
#include "string.h"
#else
/* we compile for the legacy nagios 3 / icinga 1 core */

/* include (minimum required) event broker header files */
#include "../include/nebmodules.h"
#include "../include/nebcallbacks.h"

/* include other event broker header files that we need for our work */
#include "../include/nebstructs.h"
#include "../include/broker.h"

/* include some Nagios stuff as well */
#include "../include/config.h"
#include "../include/common.h"
#include "../include/nagios.h"
#endif

/* include some pnp stuff */
#include "../include/pnp.h"
#include "../include/npcdmod.h"

/* include redis stuff */
#include "../hiredis/hiredis.h"

#ifndef VERSION
#define VERSION "unknown"
#endif

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

void *lister_module_handle = NULL;

int lister_handle_data(int, void *);

/* this function gets called when the module is loaded by the event broker */
int nebmodule_init(int flags, char *args, nebmodule *handle) {
    /* save our handle */
    npcdmod_module_handle = handle;

    /* set some info - this is completely optional, as Nagios doesn't do anything with this data */
    neb_set_module_info(lister_module_handle, NEBMODULE_MODINFO_TITLE, "lister");
    neb_set_module_info(lister_module_handle, NEBMODULE_MODINFO_AUTHOR, "Ali Graham");
    neb_set_module_info(lister_module_handle, NEBMODULE_MODINFO_COPYRIGHT, "Copyright (c) 2015 Ali Graham");
    neb_set_module_info(lister_module_handle, NEBMODULE_MODINFO_VERSION, VERSION);
    neb_set_module_info(lister_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v2");
    neb_set_module_info(lister_module_handle, NEBMODULE_MODINFO_DESC, "List enabled NEB modules.");

    /* log module info to the Nagios log file */
    write_to_all_logs("lister: Copyright (c) 2015 Ali Graham", NSLOG_INFO_MESSAGE);
    write_to_all_logs("lister: This is version '" VERSION "' running.", NSLOG_INFO_MESSAGE);

    /* register to be notified of certain events... */
    neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA,
            lister_module_handle, 0, lister_handle_data);
    neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA,
            lister_module_handle, 0, lister_handle_data);
    return 0;
}

/* this function gets called when the module is unloaded by the event broker */
int nebmodule_deinit(int flags, int reason) {
    char temp_buffer[1024];

    /* deregister for all events we previously registered for... */
    neb_deregister_callback(NEBCALLBACK_HOST_CHECK_DATA, lister_handle_data);
    neb_deregister_callback(NEBCALLBACK_SERVICE_CHECK_DATA, lister_handle_data);

    // log a message to the Nagios log file
    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
            "lister: Deinitializing lister nagios event broker module.\n");
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    return 0;
}

/* handle data from Nagios daemon */
int lister_handle_data(int event_type, void *data) {

    return 0;
}
