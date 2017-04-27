/* 
 * File:   AGDUtils.cpp
 * Author: agridata
 * 
 * Created on March 13, 2017, 1:33 PM
 */

#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AGDUtils.h"


class popen;
using namespace std;

namespace AGDUtils {

    /**
     * split
     *
     * Splits a string based on delim
     */
    vector <string> split(const string &s, char delim) {
        stringstream ss(s);
        string item;
        vector <string> tokens;
        while (getline(ss, item, delim)) {
            tokens.push_back(item);
        }
        return tokens;
    }

    /**
     * grabTime
     *
     * A call to date (via bash) returns a timestamp
     */
    string grabTime() {
        // Many ways to get the string, but this is the easiest
        string time = pipe_to_string("date --rfc-3339=ns | sed 's/ /T/; s/(.......).*-/1-/g'");

        // Fix colon issue if necessary
        // replace(time.begin(), time.end(), ':', '_');
        return (time);
    }
    
    /** grabSeconds
     * 
     * grabTime is a way to get a string from the shell, but this is the more canonical
     * way to grab seconds since 1970, returning an int64_t
     
     */
    int64_t grabSeconds() {
        time_t now;
        time(&now);
        int64_t itime = *((int64_t*)&now);
        
        return (itime);
    }

    /**
     * pipe_to_string
     *
     * Grabs the results of a bash command as a string
     */
    string pipe_to_string(const char *command) {
        char buffer[128];
        FILE* pipe = popen(command, "r");
        if (pipe) {
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != NULL) {
                }
            }
            pclose(pipe);
            buffer[strlen(buffer) - 1] = '\0';
        }
        return (string) buffer;
    }
}