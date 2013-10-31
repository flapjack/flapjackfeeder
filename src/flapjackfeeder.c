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

/* specify event broker API version (required) */
NEB_API_VERSION(CURRENT_NEB_API_VERSION);

extern int process_performance_data;

FILE *fp = NULL;

void *npcdmod_module_handle = NULL;
char *perfdata_pipe = "/usr/local/nagios/var/perfdata";
char *perfdata_spool_filename = "perfdata";
char *spool_dir = NULL;
char *perfdata_pipe_processing_interval = "15";

void npcdmod_file_roller();
int npcdmod_handle_data(int, void *);

int npcdmod_process_config_var(char *arg);
int npcdmod_process_module_args(char *args);

/* this function gets called when the module is loaded by the event broker */
int nebmodule_init(int flags, char *args, nebmodule *handle) {
	char temp_buffer[1024];
	time_t current_time;
	//unsigned long interval;

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

	if (process_performance_data == FALSE) {
		write_to_all_logs("flapjackfeeder: I can not work with disabled performance data in nagios.cfg.", NSLOG_INFO_MESSAGE);
		write_to_all_logs("flapjackfeeder: Please enable it with 'process_performance_data=1' in nagios.cfg", NSLOG_INFO_MESSAGE);
		return -1;
	}

	/* process arguments */
	if (npcdmod_process_module_args(args) == ERROR) {
		write_to_all_logs("flapjackfeeder: An error occurred while attempting to process module arguments.", NSLOG_INFO_MESSAGE);
		return -1;
	}

	/* de-initialize if there is no perfdata file nor spool dir */
	if (perfdata_pipe == NULL) {
		write_to_all_logs("flapjackfeeder: You have to provide the perfdata_pipe option.", NSLOG_INFO_MESSAGE);
		return -1;
	}

	/* Log some health data */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1, "flapjackfeeder: perfdata file '%s'.", perfdata_pipe);
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

	/* open perfdata_pipe to write perfdata in it */
	if ((fp = fopen(perfdata_pipe, "a")) == NULL) {
		snprintf(temp_buffer, sizeof(temp_buffer) - 1,
				"flapjackfeeder: Could not open file. %s", strerror(errno));
		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
		write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
		return -1;
	}

	/* log a message to the Nagios log file that we're ready */
	snprintf(temp_buffer, sizeof(temp_buffer) - 1,
			"flapjackfeeder: Ready to run to have some fun!\n");
	temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
	write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);

	/* register for a 15 seconds file move event */
/*
	time(&current_time);
	//interval = 15;
	schedule_new_event(EVENT_USER_FUNCTION,TRUE, current_time + atoi(perfdata_pipe_processing_interval), TRUE,
	atoi(perfdata_pipe_processing_interval), NULL, TRUE, (void *) npcdmod_file_roller, "", 0);
*/

	/* register to be notified of certain events... */
	neb_register_callback(NEBCALLBACK_HOST_CHECK_DATA, npcdmod_module_handle,
			0, npcdmod_handle_data);
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
	char spool_file[1024];
	int result = 0;
	time_t current_time;

	time(&current_time);

//	sprintf(spool_file, "%s/%s.%d", spool_dir, perfdata_spool_filename, (int)current_time);
//	spool_file[sizeof(spool_file) - 1] = '\x0';

	/* flush pipe */
	fflush(fp);

//	/* close actual file */
//	fclose(fp);
//
//	/* move the original file */
//	result = my_rename(perfdata_pipe, spool_file);
//
//	/* open a new file */
//	if ((fp = fopen(perfdata_pipe, "a")) == NULL) {
//		snprintf(temp_buffer, sizeof(temp_buffer) - 1,
//				"flapjackfeeder: Could not reopen file. %s", strerror(errno));
//		temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
//		write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
//	}

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

// we want to process all hosts to get alerting right
//                        if(host->process_performance_data == 0) {
//                            break;
//                        }

			/* Do some Debuglog */
			/*
			snprintf(temp_buffer, sizeof(temp_buffer) - 1,  "flapjackfeeder: DEBUG >>> %s\n",
			host->host_check_command);

			temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
			write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
			 */

			if (hostchkdata->type == NEBTYPE_HOSTCHECK_PROCESSED
				&& hostchkdata->perf_data != NULL) {
				written = snprintf(perfdatafile_template, PERFDATA_BUFFER,
					"[HOSTPERFDATA]\t"
					"%d\t"  // TIMET
					"%s\t"  // HOSTNAME
					"HOST\t"
					"%d\t"  // HOSTSTATE
                    "%f\t"  // HOSTEXECUTIONTIME
                    "%f\t"  // HOSTLATENCY
                    "%s\t"  // HOSTOUTPUT
					"%s\t", // HOSTPERFDATA
                        (int)hostchkdata->timestamp.tv_sec,
						hostchkdata->host_name,
						hostchkdata->state,
						hostchkdata->execution_time,
						hostchkdata->latency,
                        hostchkdata->output,
						hostchkdata->perf_data);

                if (written >= PERFDATA_BUFFER) {
                    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                        "flapjackfeeder: Buffer size of %d in npcdmod.h is too small, ignoring data for %s\n", PERFDATA_BUFFER, hostchkdata->host_name);
                    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
                    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
                } else {
                    fputs(perfdatafile_template, fp);
                    /* I'm not sure if this fflush is a bad thing, but hope this will do no harm as we write to a pipe */
	                fflush(fp);
                }
			}
		}
		break;

	case NEBCALLBACK_SERVICE_CHECK_DATA:
		/* an aggregated status data dump just started or ended... */
		if ((srvchkdata = (nebstruct_service_check_data *) data)) {

			if (srvchkdata->type == NEBTYPE_SERVICECHECK_PROCESSED
					&& srvchkdata->perf_data != NULL) {

				/* find the nagios service object for this service */
				service = find_service(srvchkdata->host_name, srvchkdata->service_description);

// we want to process all services to get alerting right
//                                if(service->process_performance_data == 0) {
//                                    break;
//                                }

				/* Do some Debuglog */
				/*
				snprintf(temp_buffer, sizeof(temp_buffer) - 1,  "flapjackfeeder: DEBUG >>> %s\n",
						service->service_check_command);

				temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
				write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
				*/

				written = snprintf(perfdatafile_template, PERFDATA_BUFFER,
					"[SERVICEPERFDATA]\t"
					"%d\t"  // TIMET
					"%s\t"  // HOSTNAME
					"%s\t"  // SERVICEDESC
					"%d\t"  // SERVICESTATE
                    "%f\t"  // SERVICEEXECUTIONTIME
                    "%f\t"  // SERVICELATENCY
                    "%s\t"  // SERVICEOUTPUT
					"%s\t", // SERVICEPERFDATA
                        (int)srvchkdata->timestamp.tv_sec,
						srvchkdata->host_name,
                        srvchkdata->service_description,
						srvchkdata->state,
						srvchkdata->execution_time,
						srvchkdata->latency,
                        srvchkdata->output,
						srvchkdata->perf_data);

                if (written >= PERFDATA_BUFFER) {
                    snprintf(temp_buffer, sizeof(temp_buffer) - 1,
                        "flapjackfeeder: Buffer size of %d in npcdmod.h is too small, ignoring data for %s / %s\n", PERFDATA_BUFFER, srvchkdata->host_name, srvchkdata->service_description);
                    temp_buffer[sizeof(temp_buffer) - 1] = '\x0';
                    write_to_all_logs(temp_buffer, NSLOG_INFO_MESSAGE);
                } else {
                    fputs(perfdatafile_template, fp);
                    /* I'm not sure if this fflush is a bad thing, but hope this will do no harm as we write to a pipe */
	                fflush(fp);
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
	if (!strcmp(var, "perfdata_pipe"))
		perfdata_pipe = strdup(val);

	else if (!strcmp(var, "perfdata_pipe_processing_interval"))
		perfdata_pipe_processing_interval = strdup(val);

	else
		return ERROR;

	return OK;
}
