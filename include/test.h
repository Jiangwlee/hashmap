#ifndef __TEST_H_
#define __TEST_H_

#include "hash_table.h"
#include <iostream>
#include <sstream>

#define OCCUPY_HASHMAP

using shm_stl::hash_table;
using namespace std;

struct MyAssign {
    void operator() (int & old_v, int &new_v) {
        old_v = new_v;
    } 
};


template <typename _Key, typename _Value>
void test(const char *name) {
    hash_table<_Key, _Value> hash_tbl(2, 8);


#ifdef OCCUPY_HASHMAP
    for (int i = 0; i < 1000; ++i) {
        if (!hash_tbl.Insert(i, i * i))
            cout << "Insert <" << i << ", " << i * i << "> fail!" << endl;
    }
#else

    hash_tbl.Insert(1, 11);
    hash_tbl.Insert(17, 117);
    hash_tbl.Insert(33, 133);
#endif

    ostringstream os; 
    hash_tbl.Str(os);
    std::cout << os.str() << std::endl;

    int key = 18;
    int value = 0;
    if (hash_tbl.Find(key, &value)) {
        cout << "Find key : " << key << " in the hash table! Its value is " << value << "!" << endl;
        //shm_stl::Assignment<_Value> assignment;
        MyAssign assignment;
        hash_tbl.Update(key, value + 100, assignment);
        hash_tbl.Find(key, &value);
        cout << "Update key : " << key << " in the hash table! Its new value is " << value << "!" << endl;
    } else {
        cout << "Can't find key : " << key << " from the hash table!" << endl;
    }
    
#ifdef OCCUPY_HASHMAP
//    for (int i = 100; i >= 0; --i) {
//        if (!hash_tbl.Erase(i))
//            cout << "Erase <" << i << "> fail!" << endl;
//    }
    hash_tbl.Clear();
#else
    hash_tbl.Erase(1);
    hash_tbl.Erase(17);
    hash_tbl.Erase(33);
#endif

    return;
}

#endif
