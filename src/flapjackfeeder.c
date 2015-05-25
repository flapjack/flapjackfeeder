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
#include "../../hiredis/hiredis.h"

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

void *npcdmod_module_handle = NULL;
char *redis_connect_retry_interval = "15";
struct timeval timeout = { 1, 500000 }; // 1.5 seconds

/* redis target structure */
typedef struct redistarget_struct {
    char         *redis_host;
    char         *redis_port;
    int          redis_connection_established;
    redisContext *redis_context;
    struct redistarget_struct *next;
} redistarget;

redistarget *currentredistarget = NULL;
//= {"127.0.0.1",6379,*redis_context,NULL};

redisContext *rediscontext;
redisReply *reply;
int redis_connection_established = 0;

void npcdmod_file_roller();
int npcdmod_handle_data(int, void *);

int npcdmod_process_config_var(char *arg);
int npcdmod_process_module_args(char *args);

char servicestate[][10] = { "OK", "WARNING", "CRITICAL", "UNKNOWN", };
char hoststate[][12] = { "OK", "CRITICAL", "CRITICAL", };

int count_escapes(const char *src);
char *expand_escapes(const char* src);

int generate_event(char *buffer, size_t buffer_size, char *host_name, char *service_name,
                   char *state, char *output, char *long_output, char *tags,
                   long initial_failure_delay, long repeat_failure_delay, 
                   int event_time);

/* this function gets called when the module is loaded by the event broker */
int nebmodule_init(int flags, char *args, nebmodule *handle) {
    char temp_buffer[1024];
    time_t current_time;

    /* save our handle */
    npcdmod_module_handle = handle;

    /* set some info - this is completely optional, as Nagios doesn't do anything with this data */
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_TITLE, "flapjackfeeder");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_AUTHOR, "Birger Schmidt");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_COPYRIGHT, "Copyright (c) 2013-2015 Birger Schmidt");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_VERSION, "0.0.4");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_LICENSE, "GPL v2");
    neb_set_module_info(npcdmod_module_handle, NEBMODULE_MODINFO_DESC, "A simple performance data / check result extractor / pipe writer.");

    /* log module info to the Nagios log file */
    write_to_all_logs("flapjackfeeder: Copyright (c) 2013-2015 Birger Schmidt, derived from npcdmod", NSLOG_INFO_MESSAGE);

    /* process arguments */
    if (npcdmod_process_module_args(args) == ERROR) {
        write_to_all_logs("flapjackfeeder: An error occurred while attempting to process module arguments.", NSLOG_INFO_MESSAGE);
        return -1;
    }

    /* Log some health data */
    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "flapjackfeeder: redis host '%s'.", currentredistarget->redis_host);
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    snprintf(temp_buffer, sizeof(temp_buffer) - 1, "flapjackfeeder: redis port '%s'.", currentredistarget->redis_port);
    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

    /* open redis connection to push check results */
    rediscontext = redisConnectWithTimeout(currentredistarget->redis_host, atoi(currentredistarget->redis_port), timeout);
    if (rediscontext == NULL || rediscontext->err) {
        if (rediscontext) {
            snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                "flapjackfeeder: Connection error: '%s'. But I'll retry to connect regulary.\n",
                 rediscontext->errstr);
            redisFree(rediscontext);
        } else {
            snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                "flapjackfeeder: Connection error: can't allocate redis context. I'll retry, but this can lead to permanent failure.\n");
        }
        temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
        write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
    } else {
        redis_connection_established = 1;
        /* log a message to the Nagios log file that we're ready */
        snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                "flapjackfeeder: Ready to run to have some fun!\n");
        temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
        write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
    }

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
        write_to_all_logs("flapjackfeeder: redis connection has to be (re)established.", NSLOG_INFO_MESSAGE);
        rediscontext = redisConnectWithTimeout(currentredistarget->redis_host, atoi(currentredistarget->redis_port), timeout);
        if (rediscontext == NULL || rediscontext->err) {
            if (rediscontext) {
                snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                    "flapjackfeeder: Connection error: '%s'. But I'll retry to connect regulary.\n",
                     rediscontext->errstr);
                redisFree(rediscontext);
            } else {
                snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                    "flapjackfeeder: Connection error: can't allocate redis context. I'll retry, but this can lead to permanent failure.\n");
            }
            temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
            write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
        } else {
            redis_connection_established = 1;
            write_to_all_logs("flapjackfeeder: redis connection established.", NSLOG_INFO_MESSAGE);
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
    char push_buffer[PERFDATA_BUFFER];
    int written;


    /* what type of event/data do we have? */
    switch (event_type) {

    case NEBCALLBACK_HOST_CHECK_DATA:
        /* an aggregated status data dump just started or ended... */
        if ((hostchkdata = (nebstruct_host_check_data *) data)) {

            host = find_host(hostchkdata->host_name);

            customvariablesmember *currentcustomvar = host->custom_variables;
            long initial_failure_delay = 0;
            long repeat_failure_delay  = 0;
            char *cur = temp_buffer, * const end = temp_buffer + sizeof temp_buffer;
            temp_buffer[0] = '\x0';
            while (currentcustomvar != NULL) {
                if (strcmp(currentcustomvar->variable_name, "TAG") == 0) {
                  cur += snprintf(cur, end - cur,
                      "\"%s\",",
                      currentcustomvar->variable_value
                      );
                }
                else if (strcmp(currentcustomvar->variable_name, "INITIAL_FAILURE_DELAY") == 0) {
                      initial_failure_delay = strtol(currentcustomvar->variable_value,NULL,10);
                }
                else if (strcmp(currentcustomvar->variable_name, "REPEAT_FAILURE_DELAY") == 0) {
                      repeat_failure_delay = strtol(currentcustomvar->variable_value,NULL,10);
                }
                currentcustomvar = currentcustomvar->next;
            }
            cur--;
            if (strcmp(cur, ",") == 0) {
                cur[0] = '\x0';
            }
            else {
                cur++;
            }

            my_free(currentcustomvar);

            if (hostchkdata->type == NEBTYPE_HOSTCHECK_PROCESSED) {

                int written = generate_event(push_buffer, PERFDATA_BUFFER,
                    hostchkdata->host_name,
                    "HOST",
                    hoststate[hostchkdata->state],
                    hostchkdata->output,
                    hostchkdata->long_output,
                    temp_buffer,
                    initial_failure_delay,
                    repeat_failure_delay,
                    (int)hostchkdata->timestamp.tv_sec);

                if (written >= PERFDATA_BUFFER) {
                    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                        "flapjackfeeder: Buffer size of %d in npcdmod.h is too small, ignoring data for %s\n",
                        PERFDATA_BUFFER, hostchkdata->host_name);
                    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
                    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
                } else if (redis_connection_established) {
                    reply = redisCommand(rediscontext,"LPUSH events %s", push_buffer);
                    if (reply != NULL) {
                        freeReplyObject(reply);
                    } else {
                        write_to_all_logs("flapjackfeeder: unable to write to redis, connection lost.", NSLOG_INFO_MESSAGE);
                        redis_connection_established = 0;
                        redisFree(rediscontext);
                    }
                } else {
                    write_to_all_logs("flapjackfeeder: lost check result due to redis connection fail.", NSLOG_INFO_MESSAGE);
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

                customvariablesmember *currentcustomvar = service->custom_variables;
                long initial_failure_delay = 0;
                long repeat_failure_delay  = 0;
                char *cur = temp_buffer, * const end = temp_buffer + sizeof temp_buffer;
                temp_buffer[0] = '\x0';
                while (currentcustomvar != NULL) {
                    if (strcmp(currentcustomvar->variable_name, "TAG") == 0) {
                      cur += snprintf(cur, end - cur,
                          "\"%s\",",
                          currentcustomvar->variable_value
                          );
                    }
                    else if (strcmp(currentcustomvar->variable_name, "INITIAL_FAILURE_DELAY") == 0) {
                          initial_failure_delay = strtol(currentcustomvar->variable_value,NULL,10);
                    }
                    else if (strcmp(currentcustomvar->variable_name, "REPEAT_FAILURE_DELAY") == 0) {
                          repeat_failure_delay = strtol(currentcustomvar->variable_value,NULL,10);
                    }
                    currentcustomvar = currentcustomvar->next;
                }
                cur--;
                if (strcmp(cur, ",") == 0) {
                    cur[0] = '\x0';
                }
                else {
                    cur++;
                }

                my_free(currentcustomvar);

                written = generate_event(push_buffer, PERFDATA_BUFFER,
                    srvchkdata->host_name,
                    srvchkdata->service_description,
                    servicestate[srvchkdata->state],
                    srvchkdata->output,
                    srvchkdata->long_output,
                    temp_buffer,
                    initial_failure_delay,
                    repeat_failure_delay,
                    (int)srvchkdata->timestamp.tv_sec);

                if (written >= PERFDATA_BUFFER) {
                    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                        "flapjackfeeder: Buffer size of %d in npcdmod.h is too small, ignoring data for %s / %s\n",
                        PERFDATA_BUFFER, srvchkdata->host_name, srvchkdata->service_description);
                    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
                    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
                } else if (redis_connection_established) {
                    reply = redisCommand(rediscontext,"LPUSH events %s", push_buffer);
                    if (reply != NULL) {
                        freeReplyObject(reply);
                    } else {
                        write_to_all_logs("flapjackfeeder: unable to write to redis, connection lost.", NSLOG_INFO_MESSAGE);
                        redis_connection_established = 0;
                        redisFree(rediscontext);
                    }
                } else {
                    write_to_all_logs("flapjackfeeder: lost check result due to redis connection fail.", NSLOG_INFO_MESSAGE);
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
                    my_free(arglist[argcount]);
                my_free(arglist);
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
                my_free(arglist[arg]);
            my_free(arglist);
            return ERROR;
        }
    }

	redistarget *new_redistarget = NULL;
    // fill redistarget with defaults (if parameters are missing from module config)
    if (currentredistarget == NULL) {
        /* allocate memory for a new redis target */
        if ((new_redistarget = malloc(sizeof(redistarget))) == NULL) {
            //logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for redis target\n");
            write_to_all_logs("Error: Could not allocate memory for redis target\n", NSLOG_INFO_MESSAGE);
        }
        new_redistarget->redis_host = "127.0.0.1";
        new_redistarget->redis_port = "6379";
        new_redistarget->redis_connection_established = 0;
    }
    if (new_redistarget != NULL) {
    	/* add the new redistarget to the head of the redistarget list */
    	new_redistarget->next = currentredistarget;
    	currentredistarget = new_redistarget;
    }
    if (currentredistarget->redis_host == NULL) {
        currentredistarget->redis_host = "127.0.0.1";
    }
    if (currentredistarget->redis_port == NULL) {
        currentredistarget->redis_port = "6379";
    }

    /* free allocated memory */
    for (arg = 0; arg < argcount; arg++)
        my_free(arglist[arg]);
    my_free(arglist);

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

	redistarget *new_redistarget = NULL;

    /* process the variable... */
    if (!strcmp(var, "redis_host")) {
        // fill redistarget structure
        if (currentredistarget == NULL || currentredistarget->redis_host != NULL) {
            /* allocate memory for a new redis target */
            if ((new_redistarget = malloc(sizeof(redistarget))) == NULL) {
                logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for redis target\n");
                write_to_all_logs("Error: Could not allocate memory for redis target\n", NSLOG_INFO_MESSAGE);
            }
            new_redistarget->redis_host = NULL;
            new_redistarget->redis_port = NULL;
            new_redistarget->redis_connection_established = 0;
        }
        if (currentredistarget != NULL && currentredistarget->redis_host == NULL) {
            currentredistarget->redis_host = strdup(val);
        }
        else {
            new_redistarget->redis_host = strdup(val);
        }
        if (new_redistarget != NULL) {
        	/* add the new redistarget to the head of the redistarget list */
        	new_redistarget->next = currentredistarget;
        	currentredistarget = new_redistarget;
        }
    }

    else if (!strcmp(var, "redis_port")) {
        // fill redistarget structure
        if (currentredistarget == NULL || currentredistarget->redis_port != NULL) {
            /* allocate memory for a new redis target */
            if ((new_redistarget = malloc(sizeof(redistarget))) == NULL) {
                //logit(NSLOG_CONFIG_ERROR, TRUE, "Error: Could not allocate memory for redis target\n");
                write_to_all_logs("Error: Could not allocate memory for redis target\n", NSLOG_INFO_MESSAGE);
            }
            new_redistarget->redis_host = NULL;
            new_redistarget->redis_port = NULL;
            new_redistarget->redis_connection_established = 0;
        }
        if (currentredistarget != NULL && currentredistarget->redis_port == NULL) {
            currentredistarget->redis_port = strdup(val);
        }
        else {
            new_redistarget->redis_port = strdup(val);
        }
        if (new_redistarget != NULL) {
        	/* add the new redistarget to the head of the redistarget list */
        	new_redistarget->next = currentredistarget;
        	currentredistarget = new_redistarget;
        }
    }

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

/* Counts escape sequences within a string

   Used for calculating the size of the destination string for
   expand_escapes, below.
*/
int count_escapes(const char *src) {
    int e = 0;

    char c = *(src++);

    while (c) {
        switch(c) {
            case '\\':
                e++;
                break;
            case '\"':
                e++;
                break;
        }
        c = *(src++);
    }

    return(e);
}

/* Expands escape sequences within a string
 *
 * src must be a string with a NUL terminator
 *
 * NUL characters are not expanded to \0 (otherwise how would we know when
 * the input string ends?)
 *
 * Adapted from http://stackoverflow.com/questions/3535023/convert-characters-in-a-c-string-to-their-escape-sequences
 */
char *expand_escapes(const char* src)
{
    char* dest;
    char* d;

    if ((src == NULL) || ( strlen(src) == 0)) {
        dest = malloc(sizeof(char));
        d = dest;
    } else {
        // escaped lengths must take NUL terminator into account
        int dest_len = strlen(src) + count_escapes(src) + 1;
        dest = malloc(dest_len * sizeof(char));
        d = dest;

        char c = *(src++);

        while (c) {
            switch(c) {
                case '\\':
                    *(d++) = '\\';
                    *(d++) = '\\';
                    break;
                case '\"':
                    *(d++) = '\\';
                    *(d++) = '\"';
                    break;
                default:
                    *(d++) = c;
            }
            c = *(src++);
        }
    }

    *d = '\0'; /* Ensure NUL terminator */

    return(dest);
}

int generate_event(char *buffer, size_t buffer_size, char *host_name, char *service_name,
                   char *state, char *output, char *long_output, char *tags,
                   long initial_failure_delay, long repeat_failure_delay, 
                   int event_time) {

    char *escaped_host_name    = expand_escapes(host_name);
    char *escaped_service_name = expand_escapes(service_name);
    char *escaped_state        = expand_escapes(state);
    char *escaped_output       = expand_escapes(output);
    char *escaped_long_output  = expand_escapes(long_output);

    int written = snprintf(buffer, buffer_size,
                            "{"
                                "\"entity\":\"%s\","                   // HOSTNAME
                                "\"check\":\"%s\","                    // SERVICENAME
                                "\"type\":\"service\","                // type
                                "\"state\":\"%s\","                    // HOSTSTATE
                                "\"summary\":\"%s\","                  // HOSTOUTPUT
                                "\"details\":\"%s\","                  // HOSTlongoutput
                                "\"tags\":[%s],"                       // tags
                                "\"initial_failure_delay\":%lu,"       // initial_failure_delay
                                "\"repeat_failure_delay\":%lu,"        // repeat_failure_delay
                                "\"time\":%d"                          // TIMET
                            "}",
                                escaped_host_name,
                                escaped_service_name,
                                escaped_state,
                                escaped_output,
                                escaped_long_output,
                                tags,
                                initial_failure_delay,
                                repeat_failure_delay,
                                event_time);

    my_free(escaped_host_name);
    my_free(escaped_service_name);
    my_free(escaped_state);
    my_free(escaped_output);
    my_free(escaped_long_output);

    return(written);
}
