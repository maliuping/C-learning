#include <cctype>
#include "sys.h"

int main() {

    std::string line1("some,mlp,nkl!");
    unsigned int count = 0;

    count = Characterscount(line1);
    std::cout<< "there are " << count <<" characters in "<< line1 <<std::endl;

    std::cout<< ChangeWordToBig(line1) <<std::endl;
    std::cout<< ChangeChacterWithX(line1) <<std::endl;

    return 0;
}