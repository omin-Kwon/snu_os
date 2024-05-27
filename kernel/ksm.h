//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Spring 2024)
//
//  Project #4: KSM (Kernel Samepage Merging)
//
//  May 7, 2024
//
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

#ifdef SNU

extern int freemem;
typedef struct{ // 8bytes * 2 = 16bytes
    uint64 pid;
    uint64 va;
}reverse_mapping;

typedef struct{
    //hash_value
    uint64 hash_value; //8bytes
    uint64 pa; //8bytes
    //reverse_mapping
    reverse_mapping reverse_mapping_list[16]; //16bytes * 16 = 256bytes
    //counts of mapped pages
    uint64 counts; //8bytes
}ksm_entry; // 8 + 256 + 8 + 8 = 280bytes

struct node {
    struct node * left_child; //8bytes
    struct node * right_child; //8bytes
    struct node * parent; //8bytes
    ksm_entry *entry; //8bytes
}; // 8 + 8 + 8 + 8 = 32bytes


typedef struct{
    ksm_entry entry_list[10]; //280bytes * 10 = 2800bytes = 2.8KB
    int fill[10]; //4bytes * 10 = 40bytes
    int counts; //4bytes counts to check the number of allocated entries
}ksm_entry_page;

typedef struct{
    struct node node_list[100]; //32bytes * 100 = 3200bytes = 3.2KB
    int fill[100]; //4bytes * 100 = 400bytes
    int counts; //4bytes counts to check the number of allocated entries
}node_page;



typedef struct{
    ksm_entry_page* ksm_entry_page_list[400];//8B*400 = 3.2KB => 400 * 10 = 4000
    node_page* node_page_list[40]; //8B*40 = 0.32KB => 40 * 100 = 4000  
    int ksm_entry_page_num; //4bytes <= 400
    int node_page_num; //4bytes <= 40
}ksm_page_list;



//physical address for ZEROPAGE in kernel page table
//kernel page table
extern pagetable_t kernel_pagetable;
extern uint64* zero_page;
extern uint64  zero_page_hash;
extern uint64 xxh64(void *input, unsigned int len);

extern ksm_page_list* ksm_page_info;



//------------Unstable Tree Functions----------------//
//Insert the new node to the unstable tree
void insert_unstable(struct node* root, ksm_entry* entry);

//Find the node in the unstable tree which has the same hash value with the given hash value
struct node* find_unstable(struct node* root, uint64 hash_value);

//Delete the node in the unstable tree.
void delete_unstable(struct node* root, uint64 hash_value, int is_left);
void delete_stable(struct node* root, uint64 hash_value, int is_left, uint64 pid, uint64 va, int all);


void free_all_unstable(struct node* root);


//----------------Stable Tree Functions----------------//

//Insert the new node to the stable tree
void insert_stable(struct node* root, ksm_entry* entry);

//Find the node in the stable tree which has the same hash value with the given hash value
struct node* find_stable(struct node* root, uint64 hash_value);

//Delete the node in the stable tree.



//print

void print_tree(struct node* root);


#endif
