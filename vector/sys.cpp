#include "sys.h"

namespace merbok {
    /**
     * exercise
     * P91:3.14; 3.15
     */
    Status merbok::Store::StoreToVector(std::vector <std::string> &text) {
        std::string line;
        while(std::cin >> line) {
            text.push_back(line);
            if (text.size() == number) {
                return OK;
            }
        }
    }

    /**
     * exercise
     * P93:scores
     */
    Status merbok::Store::ScoresCount(std::vector<unsigned> &scores) {
        unsigned temp,j=0;

        while(std::cin >> temp) {
            if (temp <= 100) {
                scores[temp/10]++;
                j++;
            }
            if (j == 11) {
                std::cout <<"j==11"<<std::endl;
                j = 0;
                return OK;
            }
        }

    }

}
