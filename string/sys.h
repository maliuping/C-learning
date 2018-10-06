#ifndef _SYS_H
#define _SYS_H

#include "stdio.h"
#include <iostream>
#include <map>
#include <string>


struct Sales_data {
    double price;
    std::string booknum;
    unsigned units_sold;
};


std::string ReadOneLine ();
void ReadOneWord (std::string word);
unsigned int Characterscount(std::string line);
std::string ChangeWordToBig(std::string line);
#endif