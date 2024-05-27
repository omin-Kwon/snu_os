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

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#include "proc.h"
#include "ksm.h"


// There are two trees which are used to store the ksm_entry
// First tree is stable tree which is used to store the previously merged pages
// When we scan the physical frame, first we check the stable tree to see if there is a page that has the same hash value
// If it exists, we merge this page to the page that has the same hash value in the stable tree
// If not, we insert the page to the unstable tree
// Second tree is unstable tree which is used to store the pages that are not merged yet
// Unstable tree has to exist because we need to check the pages in further scanning step has the same hash value with the pages in the unstable tree
// If there is a page that has the same hash value with the page in the unstable tree, we merge the two pages and insert the merged page to the stable tree, delete the page from the unstable tree
// If there is no page that has the same hash value with the page in the unstable tree, we insert the page to the unstable tree
// When we scan the physical frame, we first check the stable tree, then check the unstable tree


//stable tree root
struct node* stable_tree = 0;
//unstable tree root
struct node* unstable_tree = 0;

extern struct proc proc[NPROC];


//10 KSM entries are using same physical page of KSM Metadata. There are 20 pages of KSM Metadata.
//100 nodes are using same physical page of KSM Metadata. There are 2 pages of KSM Metadata.


// allocate the new page for the KSM entry
ksm_entry* ksm_entry_alloc(){
    int ksm_entry_page_num = ksm_page_info->ksm_entry_page_num;
    //if the ksm_entry_page_num is 0, allocate the new ksm_entry_page
    if(ksm_entry_page_num == 0){
        ksm_entry_page* ksm_entry_page = kalloc();
        //initialize the ksm_entry_page
        ksm_entry_page->counts = 0;
        ksm_page_info->ksm_entry_page_list[ksm_entry_page_num] = ksm_entry_page;
        ksm_entry_page_num++;
        ksm_page_info->ksm_entry_page_num = ksm_entry_page_num;
        //allocate the ksm_entry inside the ksm_entry_page
        ksm_entry* entry = &ksm_entry_page->entry_list[0];
        ksm_entry_page->counts++;
        ksm_entry_page->fill[0] = 1;
        //printf("ksm_page_num: %d, ksm_entry: %p, counts: %d\n", ksm_entry_page_num, entry, ksm_entry_page->counts);
        return entry;
    }
    else{//if the ksm_entry_page_num >=1, find the ksm_entry_page which has the empty slot
        for(int i = 0; i < ksm_entry_page_num; i++){
            ksm_entry_page* ksm_entry_page = ksm_page_info->ksm_entry_page_list[i];
            for(int j = 0; j <10; j++){
                //printf("ksm_entry_page->fill[%d]: %d\n", j, ksm_entry_page->fill[j]);
                if(ksm_entry_page->fill[j] !=1){
                    ksm_entry_page->fill[j] = 1;
                    ksm_entry_page->counts++;
                    //printf("ksm_page_num: %d, ksm_entry: %p, counts: %d\n", ksm_entry_page_num, &ksm_entry_page->entry_list[j], ksm_entry_page->counts);
                    return &ksm_entry_page->entry_list[j];
                }
            }
        }
        //if there is no empty slot in the ksm_entry_page, allocate the new ksm_entry_page
        ksm_entry_page* ksm_entry_page = kalloc();
        //initialize the ksm_entry_page
        ksm_entry_page->counts = 0;
        ksm_page_info->ksm_entry_page_list[ksm_entry_page_num] = ksm_entry_page;
        ksm_entry_page_num++;
        ksm_page_info->ksm_entry_page_num = ksm_entry_page_num;
        //allocate the ksm_entry inside the ksm_entry_page
        ksm_entry* entry = &ksm_entry_page->entry_list[0];
        ksm_entry_page->counts++;
        return entry;
    }
}


// allocate the new page for the node
struct node* node_alloc(){
    int node_page_num = ksm_page_info->node_page_num;
    //if the node_page_num is 0, allocate the new node_page
    if(node_page_num == 0){
        node_page* node_page = kalloc();
        //initialize the node_page
        node_page->counts = 0;
        ksm_page_info->node_page_list[node_page_num] = node_page;
        node_page_num++;
        ksm_page_info->node_page_num = node_page_num;
        //allocate the node inside the node_page
        struct node* node = &node_page->node_list[0];
        node_page->counts++;
        node_page->fill[0] = 1;
        //printf("node_page_num: %d,  node: %p, counts: %d\n",ksm_page_info->node_page_num ,node, node_page->counts);
        return node;
    }
    else{//if the node_page_num >=1, find the node_page which has the empty slot
        for(int i = 0; i < node_page_num; i++){
            node_page* node_page = ksm_page_info->node_page_list[i];
            for(int j = 0; j < 100; j++){
                //printf("node_page->fill[%d]: %d\n", j, node_page->fill[j]);
                if(node_page->fill[j] !=1){
                    node_page->fill[j] = 1;
                    struct node* node = &node_page->node_list[j];
                    node_page->counts++;
                    //printf("node_page_num: %d,  node: %p, counts: %d\n",ksm_page_info->node_page_num ,node, node_page->counts);
                    return node;
                }
            }
        }
        //if there is no empty slot in the node_page, allocate the new node_page
        node_page* node_page = kalloc();
        //initialize the node_page
        node_page->counts = 0;
        ksm_page_info->node_page_list[node_page_num] = node_page;
        node_page_num++;
        ksm_page_info->node_page_num = node_page_num;
        //allocate the node inside the node_page
        struct node* node = &node_page->node_list[0];
        node_page->counts++;
        return node;
    }
}



void ksm_entry_dealloc(ksm_entry* entry){
    //find the ksm_entry_page which has the entry
    for(int i = 0; i < ksm_page_info->ksm_entry_page_num; i++){
        ksm_entry_page* ksm_entry_page = ksm_page_info->ksm_entry_page_list[i];
        for(int j =0; j <10; j++){
            if(&ksm_entry_page->entry_list[j] == entry){
                //if the entry is the last entry in the ksm_entry_page, just decrease the counts
                ksm_entry_page->counts--;
                ksm_entry_page->fill[j] = 0;
                //printf("ksm_entry_page->counts: %d\n", ksm_entry_page->counts);
                //if counts is 0, free the ksm_entry_page
                if(ksm_entry_page->counts == 0){
                    kfree((void*)ksm_entry_page);
                    //printf("free the ksm_entry_page: %p\n", ksm_entry_page);
                    ksm_page_info->ksm_entry_page_num--;
                    //if the ksm_entry_page is the last ksm_entry_page, just decrease the ksm_entry_page_num
                    if(i == ksm_page_info->ksm_entry_page_num){
                        return;
                    }
                    //if the ksm_entry_page is not the last ksm_entry_page, replace the ksm_entry_page with the last ksm_entry_page
                    ksm_page_info->ksm_entry_page_list[i] = ksm_page_info->ksm_entry_page_list[ksm_page_info->ksm_entry_page_num];
                    return;
                }
            }
        }
    }
}


void node_dealloc(struct node* node){
    //find the node_page which has the node
    for(int i = 0; i < ksm_page_info->node_page_num; i++){
        node_page* node_page = ksm_page_info->node_page_list[i];
        for(int j = 0; j < 100; j++){
            if(&node_page->node_list[j] == node){
                //if the node is the last node in the node_page, just decrease the counts
                node_page->counts--;
                node_page->fill[j]=0;
                //initialize the node to 0
                node_page->node_list[j].entry = 0;
                node_page->node_list[j].left_child = 0;
                node_page->node_list[j].right_child = 0;
                node_page->node_list[j].parent = 0;
                //printf("node_page->counts: %d\n", node_page->counts);
                //if counts is 0, free the node_page
                if(node_page->counts == 0){
                    //printf("free the node_page: %p\n", node_page);
                    kfree((void*)node_page);
                    ksm_page_info->node_page_num--;
                    //if the node_page is the last node_page, just decrease the node_page_num
                    if(i == ksm_page_info->node_page_num){
                        return;
                    }
                    //if the node_page is not the last node_page, replace the node_page with the last node_page
                    ksm_page_info->node_page_list[i] = ksm_page_info->node_page_list[ksm_page_info->node_page_num];
                    return;
                }
            }
        }
    }
}





//------------Unstable Tree Functions----------------//
//Insert the new node to the unstable tree
void insert_unstable(struct node* root, ksm_entry* entry){
    if(root == 0){ //first node
        unstable_tree = node_alloc();
        unstable_tree->entry = entry;
        unstable_tree->left_child = 0;
        unstable_tree->right_child = 0;
        unstable_tree->parent = 0;
        return;
    }
    if(entry->hash_value < root->entry->hash_value){
        if(root->left_child == 0){
            root->left_child = node_alloc();
            root->left_child->entry = entry;
            root->left_child->left_child = 0;
            root->left_child->right_child = 0;
            root->left_child->parent = root;
            //printf("left child has been inserted\n");
            return;
        }else{
            insert_unstable(root->left_child, entry);
        }
    }
    else if(entry->hash_value > root->entry->hash_value){
        if(root->right_child == 0){
            root->right_child = node_alloc();
            root->right_child->entry = entry;
            root->right_child->left_child = 0;
            root->right_child->right_child = 0;
            root->right_child->parent = root;
            //printf("right child has been inserted\n");
            return;
        }else{
          insert_unstable(root->right_child, entry);
        }
    }
    else{
      //printf("hash value already exists\n");
    }
}

//Find the node in the unstable tree which has the same hash value with the given hash value
struct node* find_unstable(struct node* root, uint64 hash_value){
    if(root == 0){
        return 0;
    }
    if(root->entry->hash_value == hash_value){
        return root;
    }
    if(hash_value < root->entry->hash_value){
        return find_unstable(root->left_child, hash_value);
    }
    else{
        return find_unstable(root->right_child, hash_value);
    }
}

//Delete the node in the unstable tree.
void delete_unstable(struct node* root, uint64 hash_value, int is_left){
    if(root == 0){
        return;
    }
    if(root->entry->hash_value == hash_value){
        //printf("Found the node to delete\n");
        if(root->left_child == 0 && root->right_child == 0){
            //printf("no child\n");
            if(is_left == 1){
                root->parent->left_child = 0;
            }
            else if(is_left == 0){
                root->parent->right_child = 0;
            }
            else{
                unstable_tree = 0;
            }
            return;
        }
        if(root->left_child == 0){
            //printf("left child is null\n");
            struct node* temp = root->right_child;
            //printf("is_left: %d\n", is_left);
            if(is_left == 1){
                root->parent->left_child = temp;
            }
            else if(is_left == 0){
                root->parent->right_child = temp;
            }
            else{
                unstable_tree = temp;
            }
            root = 0;
            return;
        }
        if(root->right_child == 0){
            //printf("right child is null\n");
            struct node* temp = root->left_child;
            if(is_left == 1){
                root->parent->left_child = temp;
            }
            else if(is_left == 0){
                root->parent->right_child = temp;
            }
            else{
                unstable_tree = temp;
            }
            root = 0;
            return;
        }
        struct node* temp = root->right_child;
        while(temp->left_child != 0){
            temp = temp->left_child;
        }
        root->entry = temp->entry;
        delete_unstable(root->right_child, temp->entry->hash_value, 0);
    }
    if(hash_value < root->entry->hash_value){
        delete_unstable(root->left_child, hash_value, 1);
    }
    else{
        delete_unstable(root->right_child, hash_value, 0);
    }
}



void free_all_unstable(struct node* root){
    if(root == 0){
        return;
    }
    free_all_unstable(root->left_child);
    free_all_unstable(root->right_child);
    //printf("free the root->entry : %p\n", root->entry);
    //printf("dealloc the entry: %p, the node : %p\n", root->entry, root);
    ksm_entry_dealloc(root->entry);
    node_dealloc(root);
    root = 0;
    //printf("node has been deleted, root: %d\n", root);
    return;
}


//----------------Stable Tree Functions----------------//

//Insert the new node to the stable tree
void insert_stable(struct node* root, ksm_entry* entry){
    if(root == 0){ //first node
        stable_tree = node_alloc();
        stable_tree->entry = entry;
        stable_tree->left_child = 0;
        stable_tree->right_child = 0;
        stable_tree->parent = 0;
        //printf("stable_tree after alloc\n");
        //printf("stable_tree: %p\n", stable_tree);
        //print_tree(stable_tree);
        //printf("unstable_tree after alloc\n");
        //printf("unstable_tree: %p\n", unstable_tree);
        //print_tree(unstable_tree);
        return;
    }
    if(entry->hash_value < root->entry->hash_value){
        if(root->left_child == 0){
            root->left_child = node_alloc();
            root->left_child->entry = entry;
            root->left_child->left_child = 0;
            root->left_child->right_child = 0;
            root->left_child->parent = root;
            return;
        }else{
          insert_stable(root->left_child, entry);
        }
    }
    else{
        if(root->right_child == 0){
            root->right_child = node_alloc();
            root->right_child->entry = entry;
            root->right_child->left_child = 0;
            root->right_child->right_child = 0;
            root->right_child->parent = root;
            return;
        }else{
          insert_stable(root->right_child, entry);
        }
    }
}

//Find the node in the stable tree which has the same hash value with the given hash value
struct node* find_stable(struct node* root, uint64 hash_value){
    if(root == 0){
        return 0;
    }
    if(root->entry->hash_value == hash_value){
        return root;
    }
    if(hash_value < root->entry->hash_value){
        return find_stable(root->left_child, hash_value);
    }
    else{
        return find_stable(root->right_child, hash_value);
    }
}


//Delete the node in the stable tree.
void delete_stable(struct node* root, uint64 hash_value, int is_left, uint64 pid, uint64 va, int all){
    if(root == 0){
        return;
    }
    if(root->entry->hash_value == hash_value){
        //if all is true, just delete the node
        if(all ==1){
            root->entry->counts = 0;
        }
        //if all is not true, delete the node which has the same pid and va
        else{
            for(int i = 0; i < root->entry->counts; i++){
                if(root->entry->reverse_mapping_list[i].pid == pid && root->entry->reverse_mapping_list[i].va == va){
                    root->entry->reverse_mapping_list[i] = root->entry->reverse_mapping_list[root->entry->counts - 1];
                    root->entry->counts--;
                    break;
                }
            }
        }
        //if counts is bigger than 1, just return because it means that there are more than 1 pages that has the same hash value
        if(root->entry->counts > 1){
            //printf("counts: %d\n", root->entry->counts);
            return;
        }
        //else, if counts is 1, add the write permission to the page and delete the node in stable tree
        else if(root->entry->counts == 1){
            struct proc *p;
            uint64 target_pid = root->entry->reverse_mapping_list[0].pid;
            for(int i = 0; i < NPROC; i++){
                p = &proc[i];
                if(p->pid == target_pid){
                    break;
                }
            }
            pagetable_t pagetable = p->pagetable;
            uint64 va = root->entry->reverse_mapping_list[0].va;
            //printf("va: %p\n", va);
            pte_t *pte = walk(pagetable, va, 0);
            int perm = *pte & 0x3FF;
            int reserved = perm & 1<<8;
            if(reserved != 0){
                *pte |= PTE_W;
                *pte &= ~(1<<8);
            }
            //printf("write permission has been added to pa: %p, hash value: %p, pte: %p\n", root->entry->pa, root->entry->hash_value, *pte);
        }
        //if deleted node has no child, just delete the node
        if(root->left_child == 0 && root->right_child == 0){
            if(is_left == 1){
                root->parent->left_child = 0;
            }
            else if(is_left == 0){
                root->parent->right_child = 0;
            }
            else{
                stable_tree = 0;
            }
            ksm_entry_dealloc(root->entry);
            node_dealloc(root);
            return;
        }
        //if deleted node has one child, just delete the node and connect the parent node to the child node
        else if(root->left_child == 0){
            struct node* temp = root->right_child;
            //printf("is_left: %d\n", is_left);
            if(is_left == 1){
                root->parent->left_child = temp;
            }
            else if(is_left == 0){
                root->parent->right_child = temp;
            }
            else{
                stable_tree = temp;
            }
            ksm_entry_dealloc(root->entry);
            node_dealloc(root);
            root = 0;
            return;
        }
        else if(root->right_child == 0){
            struct node* temp = root->left_child;
            if(is_left == 1){
                root->parent->left_child = temp;
            }
            else if(is_left == 0){
                root->parent->right_child = temp;
            }
            else{
                stable_tree = temp;
            }
            ksm_entry_dealloc(root->entry);
            node_dealloc(root);
            root = 0;
            return;
        }
        //if deleted node has two children, find the smallest node in the right child and replace the deleted node with the smallest node
        else{
            struct node* temp = root->right_child;
            while(temp->left_child != 0){
                temp = temp->left_child;
            }
            root->entry = temp->entry;
            //delete the temp node in the right child
            delete_stable(root->right_child, temp->entry->hash_value, 0, pid, va, 1);
        }
    }
    if(hash_value < root->entry->hash_value){
        delete_stable(root->left_child, hash_value, 1, pid, va,0);
    }
    else{
        delete_stable(root->right_child, hash_value, 0, pid, va,0);
    }
}




//print

void print_tree(struct node* root){
    if(root == 0){
        return;
    }
    print_tree(root->left_child);
    printf("hash value: %p, counts: %d\n", root->entry->hash_value, root->entry->counts);
    print_tree(root->right_child);
}





uint64
sys_ksm(void)
{
    int arg1, arg2;

    // read the addresses from a0, a1
    argint(0, &arg1);
    argint(1, &arg2);
    // scanning all pages from the process's virtual memory
    int scanned = 0;
    int merged = 0;
    //uint64 zero_page_hash = xxh64((void*)zero_page, PGSIZE);
    //printf("zero_page_hash: %d\n", zero_page_hash);
    //printf("zero page: %p\n", zero_page);
    // process list
    struct proc *p;
    for(int i = 0; i < NPROC; i++){
        p = &proc[i];
        //pid type is long int. print it
        //pid check, continue when pid is 1 or 2 or process itself invoking the ksm system call 
        if(p->state == UNUSED || p->state == ZOMBIE || p->state > 5){
            //printf("pid: %d, state: %d\n", p->pid, p->state);
            continue;
        }
        if(p->pid == 1 || p->pid == 2 || p == myproc()){
            //printf("p is myproc : %d\n", p == myproc());
            continue;
        }
        //printf("i: %d, pid: %d\n", i, p->pid);
        //printf("pid: %d, state: %d\n", p->pid, p->state);
        // scanning all pages from the process's virtual memory
        // page table
        pagetable_t pagetable = p->pagetable;
        uint64 max_sz = p->sz;
        //printf("=============SCANNING PROCESS %d=============\n", p->pid);
        for(uint64 va = 0; va < max_sz; va += PGSIZE){
            pte_t *pte = walk(pagetable, va, 0); 
            if(pte == 0) //no leaf page table entry for this virtual address
                continue;
            if((*pte & PTE_V) == 0) // saying that the page is not valid -> should be panic.
                continue;
            // if((*pte & PTE_U) == 0) // guard page should be included to ksm
            //     continue;
            // if((*pte & PTE_W) == 0){ // saying that the page is not writable -> it's because it's previously merged frames.
            //     scanned++;
            //     printf("special case\n");
            //     continue;
            // }
            //extract the pa from pte
            uint64 pa = PTE2PA(*pte);
            scanned++;
            // hash the page
            uint64 h = xxh64((void *)pa, PGSIZE);
            //printf("hash value: %d\n", h);
            // Zero page check
            if(h == zero_page_hash){
              if(pa == (uint64)zero_page){ //if physical address is already zero page
                //printf("already zero page\n");
                continue;
              }
              else{
                 //map the zero page to the physical frame of the zero page
                int perm = *pte & 0x3FF;
                *pte = PA2PTE(zero_page) | perm;
                //store the old write permission bit to reserved bits because we need to restore this bit when we perform the COW
                int reserved = *pte & PTE_W;
                //reserved bit is 8th bit
                reserved = reserved >> 2;
                //printf("reserved: %d\n", reserved);
                *pte |= (reserved << 8);
                //write protection to the pte
                *pte &= ~PTE_W;
                //free the pa
                kfree((void*)pa);
                //printf("merged to zero page : %p\n", zero_page);
                merged++;
                continue;
              }
            }
            //printf("pa: %p, write permission: %d, hash value: %p\n", pa, *pte & PTE_W, xxh64((void*)pa, PGSIZE));
            //First, find it in the stable tree
            struct node* node = find_stable(stable_tree, h);
            if(node != 0){
              // it means that the page is in the stable tree
              // we don't have to make new entry for this page, just add the reverse mapping to the existing entry
              //printf("Found in Stable Tree, hash value: %d\n", h);
              //Check if this page is already in the reverse mapping list
                int check = 0;
                for(int i = 0; i < node->entry->counts; i++){
                    if(node->entry->reverse_mapping_list[i].pid == p->pid && node->entry->reverse_mapping_list[i].va == va){
                    // it means that this page is already in the reverse mapping list
                    // so we don't have to do anything
                    check = 1;
                    break;
                    }
                }
                if(check == 1){
                    continue;
                }
                node->entry->reverse_mapping_list[node->entry->counts].pid = p->pid;
                node->entry->reverse_mapping_list[node->entry->counts].va = va;
                node->entry->counts++;
                merged++;
                //change the mapping of the page to the new physical frame which we find in the stable tree
                //free the pa
                kfree((void*)pa);
                //change pte
                int perm = *pte & 0x3FF;
                *pte = PA2PTE(node->entry->pa) | perm;
                //store the old write permission bit to reserved bits because we need to restore this bit when we perform the COW
                int reserved = *pte & PTE_W;
                //reserved bit is 8th bit
                reserved = reserved >> 2;
                //printf("reserved: %d\n", reserved);
                *pte |= (reserved << 8);
                *pte &= ~PTE_W; // Write protection.
            }
            else{//second, find it in the unstable tree
              struct node* node = find_unstable(unstable_tree, h);
              //print the node information
              // first print the node's hash value
              if(node == 0){
                // it means that the page is not in the unstable tree
                // insert it to the unstable tree
                //printf("Not Found in Unstable Tree, hash value: %d\n", h);
                ksm_entry* entry = ksm_entry_alloc();
                entry->hash_value = h;
                entry->pa = pa;
                entry->reverse_mapping_list[0].pid = p->pid;
                entry->reverse_mapping_list[0].va = va;
                entry->counts = 1;
                //printf("Inserting in unstable tree, hash value: %d\n", h);
                insert_unstable(unstable_tree, entry);
              }else{
                //printf("hi\n");
                //printf("Found in unstable tree, hash value: %d\n", h);
                // it means that there is the page which has the same hash value, so that we can merge them and insert to the stable tree
                // we don't have to make new entry for this page, just add the reverse mapping to the existing entry
                node->entry->reverse_mapping_list[node->entry->counts].pid = p->pid;
                node->entry->reverse_mapping_list[node->entry->counts].va = va;
                node->entry->counts++;
                //find out other page's pte
                //original va
                uint64 original_va = node->entry->reverse_mapping_list[0].va;
                //original pid
                uint64 original_pid = node->entry->reverse_mapping_list[0].pid;
                struct proc *p2;
                for(int i = 0; i < NPROC; i++){
                    p2 = &proc[i];
                    if(p2->pid == original_pid){
                        break;
                    }
                }
                pagetable_t pagetable2 = p2->pagetable;
                pte_t *pte2 = walk(pagetable2, original_va, 0);
                //store the old write permission bit to reserved bits because we need to restore this bit when we perform the COW
                int reserved_2 = *pte2 & PTE_W;
                //reserved_2 bit is 8th bit
                reserved_2 = reserved_2 >> 2;
                //printf("reserved_2: %d\n", reserved_2);
                *pte2 |= (reserved_2 << 8);
                //change the pte2's write permission
                *pte2 &= ~PTE_W;
                // delete the node from the unstable tree
                delete_unstable(unstable_tree, h, -1);

                //kfree the node page
                // insert the node to the stable tree
                //printf("pte2's pa: %p\n", PTE2PA(*pte2));
                //temp the entry
                ksm_entry* temp = node->entry;
                node_dealloc(node);
                insert_stable(stable_tree, temp);
                //free the node from unstable tree
                merged++;
                //free the pa
                kfree((void*)pa);
                //change the pte
                int perm = *pte & 0x3FF;
                *pte = PA2PTE(node->entry->pa) | perm;
                //store the old write permission bit to reserved bits because we need to restore this bit when we perform the COW
                int reserved = *pte & PTE_W;
                //reserved bit is 8th bit
                reserved = reserved >> 2;
                //printf("reserved: %d\n", reserved);
                *pte |= (reserved << 8);
                *pte &= ~PTE_W; // Write protection.
              }
            }
        }
    }
    //delete the unstable tree
    free_all_unstable(unstable_tree);
    unstable_tree = 0;
    //printf("unstable tree has been deleted, unstable tree: %d\n", unstable_tree);

    if (copyout(myproc()->pagetable, arg1, (char *)&scanned, sizeof(scanned)) < 0)
        return -1;
    if (copyout(myproc()->pagetable, arg2, (char *)&merged, sizeof(merged)) < 0)
        return -1;
    return freemem;
}
