/*
* @Author: mypolopony
* @Date:   2019-01-21 01:45:53
* @Last Modified by:   mypolopony
* @Last Modified time: 2019-01-21 01:57:48
*/

#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

int main()
{
    // ifstream is RAII, i.e. no need to call close. Load the file!
    ifstream cFile ("config.txt");

    if (cFile.is_open())
    {
        string line;

        // For each line
        while(getline(cFile, line)){

            // Get rid of the garbage
            if(line[0] == '#' || line.empty())
                continue;

            // Parse
            auto delimiterPos = line.find("=");
            auto name = line.substr(0, delimiterPos);
            auto value = line.substr(delimiterPos + 1);

            // Print it pretty
            // The \ns vs endls are really a stupid struggle here
            cout << "Key\n---\n";
            cout << name;
            cout << "\n\nValue:\n-----\n";
            cout << value;
        }
        
    }
    else {
        cerr << "Couldn't open config file for reading.\n";
    }
}