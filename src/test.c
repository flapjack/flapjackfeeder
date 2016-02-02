#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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
                   char *state, char *output, char *long_output, int event_time) {

    char *escaped_host_name     = expand_escapes(host_name);
    char *escaped_service_name  = expand_escapes(service_name);
    char *escaped_state         = expand_escapes(state);
    char *escaped_output        = expand_escapes(output);
    char *escaped_long_output   = expand_escapes(long_output);

    int written = snprintf(buffer, buffer_size,
                            "{"
                                "\"entity\":\"%s\","    // HOSTNAME
                                "\"check\":\"%s\","     // SERVICENAME
                                "\"type\":\"service\"," // type
                                "\"state\":\"%s\","     // HOSTSTATE
                                "\"summary\":\"%s\","   // HOSTOUTPUT
                                "\"details\":\"%s\","   // HOSTlongoutput
                                "\"time\":%d"           // TIMET
                            "}",
                                escaped_host_name,
                                escaped_service_name,
                                escaped_state,
                                escaped_output,
                                escaped_long_output,
                                event_time);

    free(escaped_host_name);
    free(escaped_service_name);
    free(escaped_state);
    free(escaped_output);
    free(escaped_long_output);

    return(written);
}


int main() {

    const int buffer_size = 1024;

    char buffer[buffer_size];

    int written = generate_event(buffer, buffer_size, "abcdef", "ghijkl", NULL, "testing\"123", "456", 12345);

    if ( written < 0 ) {
        printf("failed\n");
        return(1);
    } else if ( written > 1024) {
        printf("over %d\n", written);
        return(2);
    } else {
        printf("%s\n", buffer);
        return(0);
    }

}
