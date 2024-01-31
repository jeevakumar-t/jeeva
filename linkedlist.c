#include <stdio.h>
#include <stdlib.h>

struct node {
    int info;
    struct node* next;
};

struct node* head, * old, * n, * p1, * p2;

int traverse();
struct node** swap(struct node** h, struct node* p1, struct node* p2);
int swappascend(int num);
int insert();

int main() {
    head = (struct node*)malloc(sizeof(struct node));
    scanf("%d", &head->info);
    head->next = NULL;
    old = head;
    int num = 10;
    for (int i = 1; i < num; i++) {
        n = (struct node*)malloc(sizeof(struct node));
        scanf("%d", &n->info);
        n->next = NULL;
        old->next = n;
        old = old->next;
    }

    swappascend(num);  // Call the sorting function
    traverse();
}

int traverse() {
    struct node* p = head;
    printf("The list is \n");
    while (p != NULL) {
        printf("%d-> ", p->info);
        p = p->next;
    }
    printf("\n");
}

struct node** swap(struct node** h, struct node* p1, struct node* p2) {
    p1->next = p2->next;
    p2->next = p1;
    *h = p2;
    return h;
}

int swappascend(int num) {
    struct node** h;
    int i, j, swapped;

    for (i = 0; i < num - 1; i++) {
        h = &head;
        swapped = 0;

        for (j = 0; j < num - i - 1; j++) {
            struct node* p1 = *h;
            struct node* p2 = p1->next;

            if (p1->info > p2->info) {
                h = swap(h, p1, p2);
                swapped = 1;
            }

            h = &(*h)->next;
        }

        if (swapped == 0)
            break;
    }
}

int insert() {
    int i, pos, nd;
    printf("\n Enter the position for new node : ");
    scanf("%d", &pos);
    printf("\n Enter the new data for new node : ");
    scanf("%d", &nd);

    struct node* p = head;
    struct node* n = (struct node*)malloc(sizeof(struct node));
    n->info = nd;
    for (i = 0; i < pos - 1; i++) {
        p = p->next;
    }
    n->next = p->next;
    p->next = n;
    traverse();
}

