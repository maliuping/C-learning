#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>
#include <string>
#include <vector>

const std::vector<std::string> scores = {"F","D","C","B","A","A++"};

void ScoresToLevel(int grade,std::string *lettergrade);


#endif