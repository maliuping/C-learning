#include <cctype>
#include "sys.h"

int main() {

    /* int grade = 0;
    std::string lettergrade;

    std::cout << "请输入分数" << std::endl;
    while ( std::cin >> grade) {
        ScoresToLevel(grade,&lettergrade);
    } */
    int a_cnt = 0,e_cnt = 0,i_cnt = 0,o_cnt = 0,u_cnt = 0;
    char text;

    while (std::cin >> text) {
        switch (text) {
            case 'a':
            ++a_cnt;
            break;

            case 'e':
            ++e_cnt;
            break;

            case 'i':
            ++i_cnt;
            break;

            case 'o':
            ++o_cnt;
            break;

            case 'u':
            ++u_cnt;
            break;

            default:
            break;
        }
    }

    std::cout<<"元音字母a的个数为："<<a_cnt<<std::endl;
    std::cout<<"元音字母e的个数为："<<e_cnt<<std::endl;
    std::cout<<"元音字母i的个数为："<<i_cnt<<std::endl;
    std::cout<<"元音字母o的个数为："<<o_cnt<<std::endl;
    std::cout<<"元音字母u的个数为："<<u_cnt<<std::endl;


}