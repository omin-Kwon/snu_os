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


//------------Unstable Tree Functions----------------//
//Insert the new node to the unstable tree
void insert_unstable(struct node* root, ksm_entry* entry){
    if(root == 0){ //first node
        unstable_tree = (struct node*)kalloc();
        unstable_tree->entry = entry;
        unstable_tree->left_child = 0;
        unstable_tree->right_child = 0;
        unstable_tree->parent = 0;
        return;
    }
    if(entry->hash_value < root->entry->hash_value){
        if(root->left_child == 0){
            root->left_child = (struct node*)kalloc();
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
            root->right_child = (struct node*)kalloc();
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
      printf("hash value already exists\n");
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
    kfree((void*)root->entry);
    kfree((void*)root);
    root = 0;
    //printf("node has been deleted, root: %d\n", root);
    return;
}


//----------------Stable Tree Functions----------------//

//Insert the new node to the stable tree
void insert_stable(struct node* root, ksm_entry* entry){
    if(root == 0){ //first node
        stable_tree = (struct node*)kalloc();
        stable_tree->entry = entry;
        stable_tree->left_child = 0;
        stable_tree->right_child = 0;
        stable_tree->parent = 0;
        printf("root has been inserted\n");
        return;
    }
    if(entry->hash_value < root->entry->hash_value){
        if(root->left_child == 0){
            root->left_child = (struct node*)kalloc();
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
            root->right_child = (struct node*)kalloc();
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
void delete_stable(struct node* root, uint64 hash_value, int is_left, uint64 pid ){
    if(root == 0){
        return;
    }
    if(root->entry->hash_value == hash_value){
        //printf("Found the node to delete\n");
        for(int i = 0; i < root->entry->counts; i++){
            if(root->entry->reverse_mapping_list[i].pid == pid){
                root->entry->reverse_mapping_list[i] = root->entry->reverse_mapping_list[root->entry->counts - 1];
                root->entry->counts--;
                break;
            }
        }
        if(root->entry->counts > 0){
            return;
        }
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
            kfree((void*)root->entry);
            kfree((void*)root);
            return;
        }
        if(root->left_child == 0){
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
            kfree((void*)root->entry);
            kfree((void*)root);
            root = 0;
            return;
        }
        if(root->right_child == 0){
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
            kfree((void*)root->entry);
            kfree((void*)root);
            root = 0;
            return;
        }
        struct node* temp = root->right_child;
        while(temp->left_child != 0){
            temp = temp->left_child;
        }
        root->entry = temp->entry;
        delete_stable(root->right_child, temp->entry->hash_value, 0, pid);
    }
    if(hash_value < root->entry->hash_value){
        delete_stable(root->left_child, hash_value, 1, pid);
    }
    else{
        delete_stable(root->right_child, hash_value, 0, pid);
    }
}




//print

void print_tree(struct node* root){
    if(root == 0){
        return;
    }
    print_tree(root->left_child);
    printf("hash value: %d, counts: %d\n", root->entry->hash_value, root->entry->counts);
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
    // process list
    struct proc *p;
    for(int i = 0; i < NPROC; i++){
        p = &mycpu()->proc[i];
        //pid type is long int. print it
        //printf("pid: %d\n", p->pid);
        //pid check, continue when pid is 1 or 2 or process itself invoking the ksm system call 
        if(p->state == UNUSED || p->state == ZOMBIE || p->state > 5){
            //printf("pid: %d, state: %d\n", p->pid, p->state);
            continue;
        }
        if(p->pid == 1 || p->pid == 2 || p == myproc()){
            //printf("p is myproc : %d\n", p == myproc());
            continue;
        }
        //printf("pid: %d, state: %d\n", p->pid, p->state);
        // scanning all pages from the process's virtual memory
        // page table
        pagetable_t pagetable = p->pagetable;
        uint64 max_sz = p->sz;
        //printf("=============SCANNING PROCESS %d=============\n", p->pid);
        for(uint64 va = 0; va < max_sz; va += PGSIZE){
            pte_t *pte = walk(pagetable, va, 0); //dosen
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
                //write protection to the pte
                *pte &= ~PTE_W;
                //free the pa
                kfree((void*)pa);
                //printf("zero merged\n");
                merged++;
                continue;
              }
            }
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
              *pte &= ~PTE_W; // Write protection is removed
            }
            else{//second, find it in the unstable tree
              struct node* node = find_unstable(unstable_tree, h);
              //print the node information
              // first print the node's hash value
              if(node == 0){
                // it means that the page is not in the unstable tree
                // insert it to the unstable tree
                ksm_entry* entry = (ksm_entry*)kalloc();
                entry->hash_value = h;
                entry->pa = pa;
                entry->reverse_mapping_list[0].pid = p->pid;
                entry->reverse_mapping_list[0].va = va;
                entry->counts = 1;
                //printf("Inserting in unstable tree, hash value: %d\n", h);
                insert_unstable(unstable_tree, entry);
              }else{
                //printf("Found in unstable tree, hash value: %d\n", h);
                // it means that there is the page which has the same hash value, so that we can merge them and insert to the stable tree
                // we don't have to make new entry for this page, just add the reverse mapping to the existing entry
                node->entry->reverse_mapping_list[node->entry->counts].pid = p->pid;
                node->entry->reverse_mapping_list[node->entry->counts].va = va;
                node->entry->counts++;
                // delete the node from the unstable tree
                delete_unstable(unstable_tree, h, -1);
                //kfree the node page
                // insert the node to the stable tree
                insert_stable(stable_tree, node->entry);
                merged++;
                //free the pa
                kfree((void*)pa);
                //change the pte
                int perm = *pte & 0x3FF;
                *pte = PA2PTE(node->entry->pa) | perm;
                *pte &= ~PTE_W; // Write protection is removed
                kfree((void*)node);
              }
            }
        }
    }
    //delete the unstable tree
    //printf("====unstable tree====\n");
    //print_tree(unstable_tree);
    //printf("====stable tree====\n");
    //print_tree(stable_tree);
    free_all_unstable(unstable_tree);
    unstable_tree = 0;
    //printf("unstable tree has been deleted, unstable tree: %d\n", unstable_tree);

    if (copyout(myproc()->pagetable, arg1, (char *)&scanned, sizeof(scanned)) < 0)
        return -1;
    if (copyout(myproc()->pagetable, arg2, (char *)&merged, sizeof(merged)) < 0)
        return -1;
    return freemem;
}
