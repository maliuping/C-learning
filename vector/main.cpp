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

    std::vector<unsigned> scores(11,0);
    merbok::Store sco;
    if(sco.ScoresCount(scores) == OK) {
        std::cout <<"==================="<<std::endl;
        std::cout <<"out!"<<std::endl;
        std::cout <<"==================="<<std::endl;
        for (decltype(scores.size()) i=0;i!=11;i++) {
            std::cout << scores[i] <<std::endl;
        }

    }


}