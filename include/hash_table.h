#ifndef __HASH_TABLE_H_
#define __HASH_TABLE_H_

#include <sys/types.h>
#include <bits/stl_function.h>
#include <memory.h>
#include <iostream>
#include <sstream>
#include "hash_fun.h"
#include "common.h"
#include "bucket.h"

using std::ostream;
    
__SHM_STL_BEGIN

template <typename _Value>
struct Assignment {
    void operator() (_Value &old_value, const _Value &new_value) {
        old_value = new_value;
    }
};

const u_int32_t DEFAULT_ENTRIES = 4096;

template <typename _Key, typename _Value>
class Node {
    public:
        Node () : m_sig(0), m_next(NULL) {}
        
        void Fill(_Key k, _Value v, sig_t s) {
            m_key = k;
            m_value = v;
            m_sig = s;
        }

        void SetNext(Node * next) {m_next = next;}
        void SetIndex(uint32 idx) {m_index = idx;}

        template <typename _Modifier>
        void Update(_Value & new_value, _Modifier &update) {
            update(m_value, new_value);
        }

        _Key Key(void) const {return m_key;}
        _Value Value(void) const {return m_value;}
        sig_t Signature(void) const {return m_sig;}
        Node * Next(void) const {return m_next;}
        uint32 Index(void) const {return m_index;}

        void Str(ostream &os) {
            os << "[ <" << m_key << ", " << m_value << ">, " << m_sig << " ] --> " << std::endl; 
        }

    private:
        _Key   m_key;
        _Value m_value;
        sig_t  m_sig;   // the sinature - hash value
        Node * m_next;  // the pointer of next node
        uint32 m_index; // the index of this node in node list, it should never be changed after initialization
};

/*
 * @brief : FreeNodePool manages free nodes used by hashmap. A hashmap should always
 *          get a free node from FreeNodePool and return it back when it decides to
 *          erase a node from hashmap.
 *
 *          FreeNodePool can resize itself if all free nodes are exhausted. It creates
 *          a new free node list whose size is double of previous free node list. The
 *          max resize count is 32 by default. It means you can create 32 free node
 *          lists including the first one at most.
 *
 *          All free nodes in free node lists are chained together and be accessed
 *          from m_node_pool_head.
 *
 *          This class provides following methods to programmers:
 *          1. GetNode - Get a free node from FreeNodePool
 *          2. PutNode - Put a node to FreeNodePool
 *          3. PutNodeList - Put a list of nodes to FreeNodePool
 *
 *          Important:
 *          1. Programmers should not free any node outside of FreeNodePool
 *
 *          Following is a chart to illustrate this class:
 *
 *          m_free_list_array --> +-----------------------------------------------+
 *                                |     |     |     |     |     |     |     |     | 
 *                                +-----------------------------------------------+
 *                 [the first list]  |     |   [create the second free list if the first is exhausted]
 *                                   V     +-----> +-------------------------------+
 *          m_node_pool_head -->  +-----+          |   |   |   |   |   |   |   |   |
 *                                |     |          +-------------------------------+
 *                                +-----+            ^
 *                                |     |            |
 *                                +-----+            |
 *                                |     |            |
 *                                +-----+            |
 *                                |     |            | [chain the new created free nodes to m_node_pool_head]
 *                                +-----+            |
 *                                   |_______________|
 *
 * */
template <typename _Node>
class NodePool {
    public:
        typedef _Node node_type;
        static const uint32 MAX_RESIZE_COUNT = 5;
        static const uint32 DEFAULT_LIST_SIZE = 16;  // The default size of the first free list

        NodePool(uint32 size) : m_capacity(0), m_free_entries(0), m_free_list_num(0), 
                                m_next_free_list_size(size), m_node_pool_head(NULL) {
                                    // Initilize the m_free_list_array
                                    memset(&m_free_list_array[0], 0, sizeof(m_free_list_array));
                                    
                                    // Create the first free list
                                    Resize();
                                }

        ~NodePool() {
            for (int i = 0; i < m_free_list_num; ++i) {
                node_type * node_list = m_free_list_array[i];
                delete [] node_list;
                m_free_list_array[i] = NULL;
            }

            m_capacity = 0;
            m_free_entries = 0;
            m_free_list_num = 0;
            m_next_free_list_size = DEFAULT_LIST_SIZE;
            m_node_pool_head = NULL;
        }

        uint32 Capacity(void) const {return m_capacity;}
        uint32 FreeEntries(void) const {return m_free_entries;}

        // Get a free node
        node_type * GetNode(void) {
            if (m_node_pool_head == NULL)
                Resize();

            if (m_node_pool_head == NULL)
                return NULL;

            node_type * head = m_node_pool_head;
            if (head->Next() == NULL) {
                // If the head is the last free node
                m_node_pool_head = NULL;
            } else {
                // If the head is not the last free node, m_free_node_list points to the next free node
                m_node_pool_head = head->Next(); 
            }

            --m_free_entries;
            return head;
        }

        // Return a node to free list
        void PutNode(node_type * node) {
            if (node == NULL)
                return;

            // Put this node at the front of m_free_node_list
            node->SetNext(m_node_pool_head); 
            m_node_pool_head = node;

            ++m_free_entries;
        }

        // Return nodes in a bucket to free list
        void PutNodeList(node_type *start, node_type *end, uint32 size) {
            // If start or end is NULL, do nothing
            if (!start || !end)
                return;

            // Put the node list decribed by start and end at the front of m_node_pool_head
            end->SetNext(m_node_pool_head);
            m_node_pool_head = start;
            m_free_entries += size;
        }


        void Print(void) {
            std::ostringstream os;
            Str(os);
            std::cout << os.str() << std::endl;

            os << "\nFree Node Pool : " << std::endl;
            node_type * start = m_node_pool_head;
            PrintNode<node_type> action;
            while (start) {
                action(*start, os);
                start = start->Next();
            }

            std::cout << os.str() << std::endl;
        }

    private:
        // Create a new free node list and chain it to free node pool
        void Resize(void) {
            // Have reached the maxinum size
            if (m_free_list_num >= MAX_RESIZE_COUNT)
                return;

            // Create a new free list
            uint32 size = m_next_free_list_size;
            node_type * new_list = new node_type[size];
            if (new_list == NULL)
                return;

            // Now we have created the new free list successfully, add it to m_free_list_array
            // and free node pool
            InitializeFreeNodeList(new_list, size, m_capacity);
            m_free_list_array[m_free_list_num] = new_list;
            node_type * end_of_list = &new_list[size - 1];
            PutNodeList(new_list, end_of_list, size); // PutNodeList will calculate m_free_entries

            // Calculate new capacity, free_list_num and next_free_list_size 
            m_capacity += size;
            m_free_list_num++;
            m_next_free_list_size = size << 1;

#ifdef DEBUG
            std::cout << "Just Resize Node Pool! ...... " << std::endl;
            Print();
#endif
        }

        void InitializeFreeNodeList(node_type *list, uint32 size, uint32 index_start) {
            uint32 i = 0;
            for ( ; i < size - 1; ++i) {
                list[i].SetNext(&list[i + 1]);
                list[i].SetIndex(index_start++);
            }

            // Set the index of last node
            list[i].SetIndex(index_start);
        }

        void Str(std::ostream &os) {
            os << "Node Pool Status : " << std::endl;
            os << "Capacity      : " << m_capacity << std::endl;
            os << "Free entries  : " << m_free_entries << std::endl;
            os << "Free list num : " << m_free_list_num << std::endl;
        }

    private:
        uint32     m_capacity;            // the capacity of this free node pool
        uint32     m_free_entries;        // the count of available free nodes in this pool
        uint32     m_free_list_num;       // how many free lists we have now
        uint32     m_next_free_list_size; // the size of next free list
        node_type *m_node_pool_head;      // the head of free node pool
        node_type *m_free_list_array[MAX_RESIZE_COUNT]; // free lists
};

template <typename _Key, typename _Value, typename _HashFunc = hash<_Key>, typename _EqualKey = std::equal_to<_Key> >
class hash_table {
    public:
        typedef Node<_Key, _Value> node_type;
        typedef _Key key_type;
        typedef _Value value_type;
        typedef _HashFunc hasher;
        typedef _EqualKey key_equal;
        typedef Bucket<node_type, key_type, key_equal>  bucket_type;
        typedef NodePool<node_type> node_pool_type;
        typedef BucketMgr<bucket_type> bucket_mgr;

    public:
        hash_table(uint32 entries = DEFAULT_ENTRIES, uint32 buckets = DEFAULT_BUCKET_NUM) : 
                   m_buckets(buckets), m_node_pool(entries) {}

        ~hash_table(void) {}

        bool Insert(const key_type & key, const value_type & value) {
            // Check if this key is already in hash table
            if (Find(key))
                return false;
                        
            // Get a new node from free list
            node_type * node = m_node_pool.GetNode();
            if (node == NULL)
                return false;

            // Compute signature and fill the node
            sig_t sig = m_hash_func(key);
            node->Fill(key, value, sig);

            // Put node to bucket
            bucket_type * bucket = m_buckets.GetBucketBySig(sig); 
            bucket->Put(node);

#ifdef DEBUG
            m_node_pool.Print();
            PrintBucketList(*bucket);
#endif

            return true;
        }

        /*
         * @brief
         *  This method takes two parameters :
         *  key is an input parameter for hash table lookup
         *  ret is an output parameter to take the value if the key is in the hash table
         * */
        bool Find(const key_type & key, value_type * ret = NULL) {
            node_type * node = LookupNodeByKey(key);
            if (node) {
                if (ret) {
                    *ret = node->Value();
                }
                return true;
            } else {
                return false;
            }
        }

        bool Erase(const key_type &key, value_type * ret = NULL) {
            // Compute signature and get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = m_buckets.GetBucketBySig(sig);

            // Remove this node from bucket
            if (bucket) { 
                node_type * node = bucket->Remove(sig, key);
                if (node) {
                    // Put this node to free node list
                    m_node_pool.PutNode(node);
#ifdef DEBUG
                    m_node_pool.Print();
                    PrintBucketList(*bucket);
#endif
                    return true;
                }
            }

#ifdef DEBUG
            m_node_pool.Print();
            PrintBucketList(*bucket);
#endif

            return false;
        }

        // Update the value
        template <typename _Modifier>
        bool Update(const key_type & key, value_type new_value, _Modifier &update) {
            node_type * node = LookupNodeByKey(key);
            if (node) {
                node->Update(new_value, update); 
            } else {
                return false;
            }
        }

        // Clear this hash table
        void Clear(void) {
            for (int i = 0; i < m_buckets.Size(); ++i) {
                PutBucketToFreeList(m_buckets.GetBucketByIndex(i));
            }
#ifdef DEBUG
            m_node_pool.Print();
#endif
        }

        void Str(ostream & os) const {
            os << "\nHash Table Information : " << std::endl;
            os << "** Total Entries : " << m_node_pool.Capacity() << std::endl;
            os << "** Free  Entries : " << m_node_pool.FreeEntries() << std::endl;
            m_buckets.Str(os);
        }

    private:
        template <typename _Action>
        void TravelNodeList(node_type * head, _Action action, ostream &os) const {
            if (head == NULL)
                return;

            node_type * current_node = head;
            while (current_node != NULL) {
                action(*current_node, os);
                current_node = current_node->Next();
            }
        }


        // Return nodes in a bucket to free list
        void PutBucketToFreeList(bucket_type *bucket) {
            if (bucket == NULL)
                return;

            node_type * start = bucket->Head();
            node_type * end   = bucket->Tail();

            // Put nodes to m_node_pool
            m_node_pool.PutNodeList(start, end, bucket->Size());

            // Now it is time to clear bucket
            bucket->Clear();

#ifdef DEBUG
            PrintBucketList(*bucket);
            std::ostringstream log;
            bucket->Str(log);
            std::cout << log.str() << std::endl;
#endif
        }

        node_type * LookupNodeByKey(const key_type & key) {
            node_type * ret = NULL;

            // Compute signature and get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = m_buckets.GetBucketBySig(sig);

            // Search in this bucket
            if (bucket)
                return bucket->Lookup(sig, key); 
            else
                return NULL;
        }

        void PrintBucketList(const bucket_type &bucket) const {
            std::ostringstream os;
            PrintNode<node_type> action;
            os << "\nCurrent Bucket is : " << std::endl;
            TravelNodeList(bucket.Head(), action, os);
            std::cout << os.str() << std::endl;
        }
        
    private:
        hasher         m_hash_func;
        node_pool_type m_node_pool;
        bucket_mgr     m_buckets;
};

__SHM_STL_END

#endif
