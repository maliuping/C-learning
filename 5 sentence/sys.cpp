#include "sys.h"

    /**
     * exercise
     * P159:5.5
     */
void ScoresToLevel(int grade,std::string *lettergrade) {

    if (grade <= 100 && grade >=0) {
            if (grade < 60) {
                *lettergrade = scores[0];
            } else {
                *lettergrade = scores[(grade-50)/10]; // get level

                if ((grade != 100) && ((grade % 10) < 3)) { // less than 3, add-
                    *lettergrade += "-";
                } else if ((grade != 100) && ((grade % 10) > 7)) { // more than 7, add+
                    *lettergrade += "+";
                }
            }
            std::cout << "对应等级:" << *lettergrade << std::endl;
        } else { // Beyond the scope
            std::cout << "error: input beyond the scope!" << std::endl;
        }

}