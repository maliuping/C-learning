#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

#define  number  2

enum Status{
    OK = 0,
    ERROR,
};


namespace merbok {
    class Store {
    public:
        /**
         * exercise
         * P91:3.14; 3.15
         */
        Status StoreToVector(std::vector <std::string> &text);
    };
}
#endif