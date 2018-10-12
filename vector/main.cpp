#include <stdio.h>
#include <iostream>
#include "sys.h"



int main() {

    /* std::vector<std::string> text;
    merbok::Store rvc;
    if (rvc.StoreToVector(text) == OK) {
        std::cout <<"==================="<<std::endl;
        std::cout <<"out!"<<std::endl;
        std::cout <<"==================="<<std::endl;
        for (decltype(text.size()) i=0;i!=number;i++) {

            std::cout << text[i] <<std::endl;
        }

    } */
    unsigned temp,j=0;
    std::vector<unsigned> scoures(11,0);

    while(std::cin >> temp) {
        if (temp <= 100) {
            scoures[temp/10]++;
            j++;
        }
        if (j == 11) {
            j = 0;
            break;
        }
    }
    std::cout <<"==================="<<std::endl;
    std::cout <<"out!"<<std::endl;
    std::cout <<"==================="<<std::endl;
    for (decltype(scoures.size()) i=0;i!=11;i++) {
            std::cout << scoures[i] <<std::endl;
        }

}