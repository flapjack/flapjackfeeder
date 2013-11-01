/*****************************************************************************
 *
 * FLAPJACKFEEDER.C
 * Copyright (c) 2013 Birger Schmidt (http://flapjack.io)
 *
 * Derived from NPCDMOD.C ...
 * Copyright (c) 2008-2010 Hendrik Baecker (http://www.pnp4nagios.org)
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
 * Last Modified: 10-30-2013
 *
 *****************************************************************************/

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

/* include some pnp stuff */
#include "../include/pnp.h"
#include "../include/npcdmod.h"

/* include redis stuff */
#include "../../hiredis/hiredis.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

void *npcdmod_module_handle = NULL;
char *perfdata_pipe = "/usr/local/nagios/var/perfdata";
char *redis_host = "127.0.0.1";
char *redis_port = "6379";
char *redis_connect_retry_interval = "15";
struct timeval timeout = { 1, 500000 }; // 1.5 seconds

redisContext *rediscontext;
redisReply *reply;

void npcdmod_file_roller();
int npcdmod_handle_data(int, void *);

int npcdmod_process_config_var(char *arg);
int npcdmod_process_module_args(char *args);

char servicestate[][10] = { "OK", "WARNING", "CRITICAL", "UNKNOWN", };
char hoststate[][12] = { "UP", "DOWN", "UNREACHABLE", };

/* this function gets called when the module is loaded by the event broker */
int nebmodule_init(int flags, char *args, nebmodule *handle) {
    char temp_buffer[1024];
    time_t current_time;

    /* save our handle */
    npcdmod_module_handle = handle;

    /* set some info - this is completely optional, as Nagios doesn't do anything with this data */
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_TITLE, "flapjackfeeder");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_AUTHOR, "Birger Schmidt, Hendrik Baecker");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_TITLE, "Copyright (c) 2013 Birger Schmidt, 2008-2009 Hendrik Baecker");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_VERSION, "0.0.3");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v2");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_DESC, "A simple performance data / check result extractor / pipe writer.");

    /* log module info to the Nagios log file */
    write_to_all_logs("flapjackfeeder: Copyright (c) 2013 Birger Schmidt, derived from npcdmod by Hendrik Baecker", NSLOG_INFO_MESSAGE);

    /* process arguments */
    if (npcdmod_process_module_args(args) == ERROR) {
        write_to_all_logs("flapjackfeeder: An error occurred while attempting to process module arguments.", NSLOG_INFO_MESSAGE);
        return -1;
    }

    /* Log some health data */
    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "flapjackfeeder: redis host '%s'.", redis_host);
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "flapjackfeeder: redis port '%s'.", redis_port);
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    /* open redis connection to push check results */
    rediscontext = redisConnectWithTimeout(redis_host, atoi(redis_port), timeout);
    if (rediscontext == NULL || rediscontext->err) {
        if (rediscontext) {
            snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                "Connection error: %s But I'll retry to connect regulary.\n",
                 rediscontext->errstr);
            redisFree(rediscontext);
        } else {
            snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                "Connection error: can't allocate redis context. I'll retry, but this can lead to permanent failure.\n");
        }
        temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
        write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
    }

    /* log a message to the Nagios log file that we're ready */
    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
            "flapjackfeeder: Ready to run to have some fun!\n");
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    /* register for a 15 seconds file move event */
    time(&current_time);
    schedule_new_event(EVENT_USER_FUNCTION,TRUE, current_time + atoi(redis_connect_retry_interval), TRUE,
    atoi(redis_connect_retry_interval), NULL, TRUE, (void *) npcdmod_file_roller, "", 0);

    /* register to be notified of certain events... */
    neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA, 
            npcdmod_module_handle, 0, npcdmod_handle_data);
    neb_register_callback(NEBCALLBACK_SERVICE_CHECK_DATA,
            npcdmod_module_handle, 0, npcdmod_handle_data);
    return 0;
}

/* this function gets called when the module is unloaded by the event broker */
int nebmodule_deinit(int flags, int reason) {
    char temp_buffer[1024];

    /* deregister for all events we previously registered for... */
    neb_deregister_callback(NEBCALLBACK_HOST_CHECK_DATA,npcdmod_handle_data);
    neb_deregister_callback(NEBCALLBACK_SERVICE_CHECK_DATA,npcdmod_handle_data);

    /* log a message to the Nagios log file */
    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
            "flapjackfeeder: Deinitializing flapjackfeeder nagios event broker module.\n");
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    return 0;
}

/* gets called every X seconds by an event in the scheduling queue */
void npcdmod_file_roller() {
    char temp_buffer[1024];
    int result = 0;
    time_t current_time;

    time(&current_time);

    /* open redis connection to push check results if needed */
    if (rediscontext == NULL || rediscontext->err) {
        rediscontext = redisConnectWithTimeout(redis_host, atoi(redis_port), timeout);
        if (rediscontext == NULL || rediscontext->err) {
            if (rediscontext) {
                snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                    "Connection error: %s But I'll retry to connect regulary.\n",
                     rediscontext->errstr);
                redisFree(rediscontext);
            } else {
                snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                    "Connection error: can't allocate redis context. I'll retry, but this can lead to permanent failure.\n");
            }
            temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
            write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
        }
    }

    return;
}

/* handle data from Nagios daemon */
int npcdmod_handle_data(int event_type, void *data) {
    nebstruct_host_check_data *hostchkdata = NULL;
    nebstruct_service_check_data *srvchkdata = NULL;

    host *host=NULL;
    service *service=NULL;

    char temp_buffer[1024];
    char perfdatafile_template[PERFDATA_BUFFER];
    int written;


    /* what type of event/data do we have? */
    switch (event_type) {

    case NEBCALLBACK_HOST_CHECK_DATA:
        /* an aggregated status data dump just started or ended... */
        if ((hostchkdata = (nebstruct_host_check_data *) data)) {

            host = find_host(hostchkdata->host_name);

            if (hostchkdata->type == NEBTYPE_HOSTCHECK_PROCESSED) {
                written = snprintf(perfdatafile_template, PERFDATA_BUFFER,
// {"entity":"localhost","check":"SSH","type":"service","state":"OK","summary":"SSH OK - OpenSSH_5.9p1 Debian-5ubuntu1 (protocol 2.0)","details":null,"time":"1383237623"}
                    "{"
                    "\"entity\":\"%s\","    // HOSTNAME
                    "\"check\":\"HOST\","   // SERVICENAME
                    "\"type\":\"host\","    // type
                    "\"state\":\"%s\","     // HOSTSTATE
                    //"\"time\":\"%f\","      // HOSTEXECUTIONTIME
                    //"\"time\":\"%f\","      // HOSTLATENCY
                    "\"summary\":\"%s\","   // HOSTOUTPUT
                    "\"details\":\"%s\","   // HOSTlongoutput
                    "\"time\":\"%d\""       // TIMET
                    "}",
                        hostchkdata->host_name,
                        hoststate[hostchkdata->state],
                        //hostchkdata->execution_time,
                        //hostchkdata->latency,
                        hostchkdata->output,
                        hostchkdata->perf_data,
                        (int)hostchkdata->timestamp.tv_sec);

                if (written >= PERFDATA_BUFFER) {
                    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                        "flapjackfeeder: Buffer size of %d in npcdmod.h is too small, ignoring data for %s\n", 
                        PERFDATA_BUFFER, hostchkdata->host_name);
                    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
                    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
                } else {
                    reply = redisCommand(rediscontext,"LPUSH events %s", perfdatafile_template);
                    if (reply != NULL) {
                        freeReplyObject(reply);
                    } else {
                        write_to_all_logs(perfdatafile_template, NSLOG_INFO_MESSAGE);
                    }
                }
            }
        }
        break;

    case NEBCALLBACK_SERVICE_CHECK_DATA:
        /* an aggregated status data dump just started or ended... */
        if ((srvchkdata = (nebstruct_service_check_data *) data)) {

            if (srvchkdata->type == NEBTYPE_SERVICECHECK_PROCESSED) {

                /* find the nagios service object for this service */
                service = find_service(srvchkdata->host_name, srvchkdata->service_description);

                written = snprintf(perfdatafile_template, PERFDATA_BUFFER,
                    "{"
                    "\"entity\":\"%s\","    // HOSTNAME
                    "\"check\":\"%s\","     // SERVICENAME
                    "\"type\":\"service\"," // type
                    "\"state\":\"%s\","     // HOSTSTATE
                    //"\"time\":\"%f\","      // HOSTEXECUTIONTIME
                    //"\"time\":\"%f\","      // HOSTLATENCY
                    "\"summary\":\"%s\","   // HOSTOUTPUT
                    "\"details\":\"%s\","   // HOSTlongoutput
                    "\"time\":\"%d\""       // TIMET
                    "}",
                        srvchkdata->host_name,
                        srvchkdata->service_description,
                        servicestate[srvchkdata->state],
                        //srvchkdata->execution_time,
                        //srvchkdata->latency,
                        srvchkdata->output,
                        srvchkdata->perf_data,
                        (int)srvchkdata->timestamp.tv_sec);

                if (written >= PERFDATA_BUFFER) {
                    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                        "flapjackfeeder: Buffer size of %d in npcdmod.h is too small, ignoring data for %s / %s\n", 
                        PERFDATA_BUFFER, srvchkdata->host_name, srvchkdata->service_description);
                    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
                    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
                } else {
                    reply = redisCommand(rediscontext,"LPUSH events %s", perfdatafile_template);
                    if (reply != NULL) {
                        freeReplyObject(reply);
                    } else {
                        write_to_all_logs(perfdatafile_template, NSLOG_INFO_MESSAGE);
                    }
                }
            }
        }
        break;

    default:
        break;
    }


    return 0;
}

/****************************************************************************/
/* CONFIG FUNCTIONS                                                         */
/****************************************************************************/

/* process arguments that were passed to the module at startup */
int npcdmod_process_module_args(char *args) {
    char *ptr = NULL;
    char **arglist = NULL;
    char **newarglist = NULL;
    int argcount = 0;
    int memblocks = 64;
    int arg = 0;

    if (args == NULL)
        return OK;

    /* get all the var/val argument pairs */

    /* allocate some memory */
    if ((arglist = (char **) malloc(memblocks * sizeof(char **))) == NULL)
        return ERROR;

    /* process all args */
    ptr = strtok(args, ",");
    while (ptr) {

        /* save the argument */
        arglist[argcount++] = strdup(ptr);

        /* allocate more memory if needed */
        if (!(argcount % memblocks)) {
            if ((newarglist = (char **) realloc(arglist, (argcount + memblocks)
                    * sizeof(char **))) == NULL) {
                for (arg = 0; arg < argcount; arg++)
                    free(arglist[argcount]);
                free(arglist);
                return ERROR;
            } else
                arglist = newarglist;
        }

        ptr = strtok(NULL, ",");
    }

    /* terminate the arg list */
    arglist[argcount] = '\x0';

    /* process each argument */
    for (arg = 0; arg < argcount; arg++) {
        if (npcdmod_process_config_var(arglist[arg]) == ERROR) {
            for (arg = 0; arg < argcount; arg++)
                free(arglist[arg]);
            free(arglist);
            return ERROR;
        }
    }

    /* free allocated memory */
    for (arg = 0; arg < argcount; arg++)
        free(arglist[arg]);
    free(arglist);

    return OK;
}

/* process a single module config variable */
int npcdmod_process_config_var(char *arg) {
    char temp_buffer[1024];
    char *var = NULL;
    char *val = NULL;

    /* split var/val */
    var = strtok(arg, "=");
    val = strtok(NULL, "\n");

    /* skip incomplete var/val pairs */
    if (var == NULL || val == NULL)
        return OK;

    /* strip var/val */
    strip(var);
    strip(val);

    /* process the variable... */
    if (!strcmp(var, "redis_host"))
        redis_host = strdup(val);

    else if (!strcmp(var, "redis_port"))
        redis_port = strdup(val);

    else if (!strcmp(var, "redis_connect_retry_interval"))
        redis_connect_retry_interval = strdup(val);

    else {
        snprintf(temp_buffer, sizeof(temp_buffer) - 1, "flapjackfeeder: I don't know what to do with '%s' as argument.", var);
        temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
        write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
        return ERROR;
    }

    return OK;
}
