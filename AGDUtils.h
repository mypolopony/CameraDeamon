/* 
 * File:   AGDUtils.h
 * Author: agridata
 *
 * Created on March 13, 2017, 3:41 PM
 */
 
#ifndef AGDUTILS_H
#define AGDUTILS_H

#include <vector>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

namespace AGDUtils
{
    bool mkdirp(const char* path, mode_t mode);
    std::vector <std::string> split(const std::string &s, char delim);
    std::string grabTime(std::string format);
    int64_t grabSeconds();
    std::string pipe_to_string(const char *command);
}

#endif /* AGDUTILS_H */

