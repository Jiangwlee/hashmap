#ifndef __HASH_TABLE_H_
#define __HASH_TABLE_H_

#include <sys/types.h>
#include <bits/stl_function.h>
#include <iostream>
#include <sstream>
#include "hash_fun.h"
#include "common.h"

using std::ostream;
    
__SHM_STL_BEGIN

const u_int32_t DEFAULT_ENTRIES = 4096;
const u_int32_t DEFAULT_BUCKET_NUM = 512;

/* typedef */
typedef u_int32_t sig_t;
typedef u_int32_t uint32;
typedef int32_t   int32;

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
        void SetValue(const _Value & value) {m_value = value;}
        void SetIndex(uint32 idx) {m_index = idx;}

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

template <typename _Node, typename _Key, typename _KeyEqual>
class Bucket {
    public:
        Bucket () : m_size(0), m_head(NULL) {}

        // Put a node at the head of this bucket
        void Put(_Node * node) {
            node->SetNext(m_head);
            m_head = node;
            ++m_size;
        }

        // Lookup a node by signature and key
        _Node * Lookup(const sig_t &sig, const _Key &key) {
            // Search in this bucket
            _Node * current = m_head;
            _Node * prev = current;
            while (current) {
                if (sig == current->Signature() && m_equal_to(key, current->Key())) {
                    break;
                }

                prev = current;
                current = current->Next();
            }

            // If we find this key in bucket, move it to the front of bucket
            if (current != NULL) {
                if (current != m_head) {
                    prev->SetNext(current->Next());
                    current->SetNext(m_head);
                    m_head = current;
                }

#ifdef DEBUG
                std::ostringstream log;
                Str(log);
                std::cout << log.str() << std::endl;
#endif
            }

            return current;
        }

        // Remove a node from this bucket
        _Node * Remove(const sig_t &sig, const _Key &key) {
            _Node * node = Lookup(sig, key);

            // If we find this node, now it is in the front of our node list,
            // we just remove it from the head
            if (node) {
                m_head = node->Next();
                node->SetNext(NULL);
                --m_size;
#ifdef DEBUG
                std::ostringstream log;
                log << "The bucket after a node remove!" << std::endl;
                Str(log);
                std::cout << log.str() << std::endl;
#endif
            }

            return node;
        }

        uint32  Size(void) const {return m_size;}
        _Node * Head(void) const {return m_head;}

        void Str(ostream &os) {
            os << "\nBucket Size : " << m_size << std::endl;
            _Node * curr = m_head;
            while (curr) {
                curr->Str(os);
                curr = curr->Next();
            }
        }

    public:
        uint32 m_size; // the size of this bucket
        _Node *m_head; // the pointer of the first node in this bucket
        _KeyEqual m_equal_to;
}; 

template <typename _Node>
struct PrintNode {
    void operator() (const _Node & node) {
        std::cout << "[" << node.Index() << "] --> ";
    }
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

    public:
        hash_table(uint32 entries = DEFAULT_ENTRIES, uint32 buckets = DEFAULT_BUCKET_NUM) : m_total_entries(entries), 
                   m_bucket_num(buckets), m_free_entries(entries), m_bucket_mask(0), m_free_node_list(NULL),      
                   m_node_array(NULL), m_bucket_array(NULL) {
#ifdef DEBUG
                           std::cout << "Initialize hash_table" << std::endl;
#endif
                           Initialize();
                       }

        ~hash_table(void) {
            if (m_bucket_array)
                delete [] m_bucket_array;

            if (m_node_array)
                delete [] m_node_array;
        }

        bool Initialize(void) {
            // Adjust bucket number if necessary
            if (!is_power_of_2(m_bucket_num))
                m_bucket_num = convert_to_power_of_2(m_bucket_num);

            m_bucket_mask = m_bucket_num - 1;

            // Allocate memory for hash_table
            m_bucket_array = new bucket_type[m_bucket_num];
            if (m_bucket_array == NULL)
                return false;

            m_node_array = new node_type[m_total_entries];
            if (m_node_array == NULL) {
                delete [] m_bucket_array;
                return false;
            }

            InitializeFreeList();
        }

        bool Insert(const key_type & key, const value_type & value) {
            // Check if this key is already in hash table
            if (Find(key))
                return false;
                        
            // Get a new node from free list
            node_type * node = GetNode();
            if (node == NULL)
                return false;

            // Compute signature and fill the node
            sig_t sig = m_hash_func(key);
            node->Fill(key, value, sig);

            // Put node to bucket
            bucket_type * bucket = GetBucketBySig(sig); 
            bucket->Put(node);

#ifdef DEBUG
            PrintNode<node_type> action;
            std::cout << "\n\nFree List is : " << std::endl;
            TravelNodeList(m_free_node_list, action);
            std::cout << "\nCurrent Bucket is : " << std::endl;
            TravelNodeList(bucket->Head(), action);
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
            bucket_type * bucket = GetBucketBySig(sig);

            // Remove this node from bucket
            if (bucket) { 
                node_type * node = bucket->Remove(sig, key);
                if (node) {
                    // Put this node to free node list
                    PutNode(node);
#ifdef DEBUG
                    PrintNode<node_type> action;
                    std::cout << "\nFree List is : " << std::endl;
                    TravelNodeList(m_free_node_list, action);
                    std::cout << "\nCurrent Buckete is : " << std::endl;
                    TravelNodeList(bucket->Head(), action);
#endif
                    return true;
                }
            }

#ifdef DEBUG
            PrintNode<node_type> action;
            std::cout << "\nFree List is : " << std::endl;
            TravelNodeList(m_free_node_list, action);
            std::cout << "\nCurrent Buckete is : " << std::endl;
            TravelNodeList(bucket->Head(), action);
#endif

            return false;
        }

        void Str(ostream & os) {
            os << "\nHash Table Information : " << std::endl;
            os << "** Total Entries : " << m_total_entries << std::endl;
            os << "** Free  Entries : " << m_free_entries << std::endl;
            os << "** Total Buckets : " << m_bucket_num << std::endl;
            os << "** Bucket Mask   : 0x" << std::hex << m_bucket_mask << std::dec << std::endl;

            for (int i = 0; i < m_bucket_num; ++i) {
                os << std::endl;
                os << "Bucket[" << i << "]" << std::endl;
                m_bucket_array[i].Str(os); 
            }
        }

    private:
        void InitializeFreeList(void) {
            m_free_node_list = &m_node_array[0];
            int32 i = 0;
            for ( ; i < m_total_entries - 1; ++i) {
                m_node_array[i].SetNext(&m_node_array[i + 1]);
                m_node_array[i].SetIndex(i);
            }

            // Set the index of last node
            m_node_array[i].SetIndex(i);

#ifdef DEBUG
            PrintNode<node_type> action;
            TravelNodeList(m_free_node_list, action);
#endif
        }

        template <typename _Action>
        void TravelNodeList(node_type * head, _Action action) const {
            if (head == NULL)
                return;

            node_type * current_node = head;
            while (current_node != NULL) {
                action(*current_node);
                current_node = current_node->Next();
            }
        }

        bucket_type * GetBucketBySig(sig_t sig) {
            bucket_type * ret = &m_bucket_array[sig & m_bucket_mask];
        }

        // Get a free node
        node_type * GetNode(void) {
            if (m_free_node_list == NULL)
                return NULL;

            node_type * head = m_free_node_list;
            if (head->Next() == NULL) {
                // If the head is the last free node
                m_free_node_list = NULL;
            } else {
                // If the head is not the last free node, m_free_node_list points to the next free node
                m_free_node_list = head->Next(); 
            }

            --m_free_entries;
            return head;
        }

        // Return a node to free list
        void PutNode(node_type * node) {
            if (node == NULL)
                return;

            if (m_free_node_list == NULL) {
                // If m_free_node_list is NULL, put this node as the head
                m_free_node_list = node;
            } else {
                // If m_free_node_list is not NULL, put this node at the head position
                node->SetNext(m_free_node_list); 
                m_free_node_list = node;
            }

            ++m_free_entries;
        }

        node_type * LookupNodeByKey(const key_type & key) {
            node_type * ret = NULL;

            // Compute signature and get bucket
            sig_t sig = m_hash_func(key);
            bucket_type * bucket = GetBucketBySig(sig);

            // Search in this bucket
            if (bucket)
                return bucket->Lookup(sig, key); 
            else
                return NULL;
        }
        
    private:
        uint32       m_total_entries;
        uint32       m_bucket_num;           // the number of buckets, should be power of 2
        uint32       m_free_entries;
        uint32       m_bucket_mask;
        node_type   *m_free_node_list;      // the head of free node list
        node_type   *m_node_array;
        bucket_type *m_bucket_array;
        hasher       m_hash_func;
};

__SHM_STL_END

#endif
