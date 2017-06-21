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
#include <time.h>
#include "AGDUtils.h"
#include <sys/stat.h>
#include <errno.h>

class popen;
using namespace std;

#define DEFAULT_MODE      S_IRWXU | S_IRGRP |  S_IXGRP | S_IROTH | S_IXOTH

namespace AGDUtils {
    
    /**
     * mkdirp
     *
     * Implemention of mkdir -p 
     */

    bool mkdirp(const char* path, mode_t mode = DEFAULT_MODE) {
      // Invalid string
      if(path[0] == '\0') {
        return false;
      }

      char* p = const_cast<char*>(path);    // const cast for hack

      while (*p != '\0') {                  // Find next slash mkdir() it and until we're at end of string
        p++;                                // Skip first character
        
        while(*p != '\0' && *p != '/') p++; // Find first slash or end
        
        char v = *p;                        // Remember value from p
        *p = '\0';                          // Write end of string at p
        if(mkdir(path, mode) != 0 && errno != EEXIST) { // Create folder from path to '\0' inserted at p
          *p = v;
          return false;
        }
        *p = v;                             // Restore path to it's former glory
      }
      return true;
    }
    
    
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
     * A call to strftime returns a timestamp
     */
    string grabTime(string format="%Y-%m-%d_%H-%M-%S") {
        time_t rawtime;
        struct tm * timeinfo;
        char buffer [80];

        time (&rawtime);
        timeinfo = localtime (&rawtime);

        strftime (buffer,80,format.c_str(),timeinfo);
        return string(buffer);
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