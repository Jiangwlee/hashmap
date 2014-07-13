#ifndef __TEST_H_
#define __TEST_H_

#include "hash_table.h"
#include <iostream>
#include <sstream>

using shm_stl::hash_table;
using namespace std;

template <typename _Key, typename _Value>
void test(const char *name) {
    hash_table<_Key, _Value> hash_tbl(64, 8);

    int j = 3;
    for (int i = 0; i < 100 && j > 0; ++i) {
        if (!hash_tbl.Insert(i, i * i))
            cout << "Insert <" << i << ", " << i * i << "> fail!" << endl;

        if (i == 32) {
            --j;
            i = 0;
        }
    }

    ostringstream os; 
    hash_tbl.Str(os);
    std::cout << os.str() << std::endl;

    int key = 18;
    int value = 0;
    if (hash_tbl.Find(key, &value))
        cout << "Find key : " << key << " in the hash table! Its value is " << value << "!" << endl;
    else
        cout << "Can't find key : " << key << " from the hash table!" << endl;

    return;
}

#endif
