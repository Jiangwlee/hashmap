#include <iostream>
#include "test.h"

using namespace std;

int main(void) {
    char name[] = "test";
    test<int, int>(name);
    return 0;
}
