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
}
