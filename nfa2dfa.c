#include "regexp.h"
#include <stdlib.h>
#include <string.h>

int compare(const void *a, const void *b)
{
    return *(int *)a - *(int *)b;
}

int search(int key, int *base, int num)
{
    int i;

    for (i = 0; i < num; i++)
        if (base[i] == key)
            return i;

    return -1;
}

void printIntArray(int *array, int n)
{
    int i;
    for(i = 0; i < n; i++) {
        printf("%d ", array[i]);
    }
}

int e_closure(Inst *start, int *closure, int n)
{
    int i;
    int ss;

    for(i = 0; i < n; i++) {
        Inst *s = start + closure[i];
        switch(s->opcode) {
            case Split:
                ss = (int)(s->x-start);
                if (search(ss, closure, n) < 0)
                    closure[n++] = ss;
                ss = (int)(s->y-start);
                if (search(ss, closure, n) < 0)
                    closure[n++] = ss;
                break;
            case Jmp:
                ss = (int)(s->x-start);
                if (search(ss, closure, n) < 0)
                    closure[n++] = ss;
                break;
            case Save:
                ss = (int)(s-start+1);
                if (search(ss, closure, n) < 0)
                    closure[n++] = ss;
                break;
        }
    }

    qsort(closure, n, sizeof(int), compare);
    return n;
}

int move(Inst *start, int *closure, int n, char a, int *trans)
{
    int i;
    int ss;
    int count = 0;

    for(i = 0; i < n; i++) {
        Inst *s = start + closure[i];
        ss = (int)(s-start+1);
        if ((s->opcode == Char && s->c == a) || s->opcode == Any)
            if (search(ss, trans, count) < 0)
                trans[count++] = ss;
    }

    qsort(trans, count, sizeof(int), compare);
    return count;
}

struct Node {
    short mark;                 /* marked or not */
    short flag;                 /* accepr or not */
    struct Node *trans[256];
    struct Node *next;
    int index;                  /* index used for dfa compress */
    int n;                      /* num of states */
    int *nstates;
};

#define MAX_ALPHABET_NUM 128
struct Dfa {
    struct Node *head;
    struct Node *tail;
    unsigned short num;
    unsigned short accept;
    char alphabet[MAX_ALPHABET_NUM];
};

static struct Dfa dfa;

void init()
{
    memset(&dfa, 0, sizeof(struct Dfa));
}

static struct Node *newNode(Prog *p, int *nstates, int n)
{
    struct Node *node = (struct Node *)mal(sizeof(struct Node));
    int *ns = (int *)mal(sizeof(int)*n);

    node->next = NULL;
    node->n = n;
    node->nstates = ns;
    memcpy(ns, nstates, sizeof(int)*n);
    node->index = dfa.num;

    dfa.num += 1;
    if (dfa.head == NULL) {
        dfa.head = node;
        dfa.tail = node;
    } else {
        dfa.tail->next = node;
        dfa.tail = node;
    }

    if ((p->len - 1) == nstates[n-1])
        node->flag = 1;

    return node;
}

static struct Node *NodeSearch(int *nstates, int n)
{
    struct Node *node = dfa.head;

    while(node) {
        if (node->n == n && memcmp(node->nstates, nstates, sizeof(int)*n) == 0)
            break;
        node = node->next;
    }

    return node;
}

static struct Node *findNodeNoMark()
{
    struct Node *node = dfa.head;

    while(node) {
        if (node->mark == 0)
            break;
        node = node->next;
    }

    return node;
}

static int getAlphabet(Prog *p)
{
    Inst *pc, *e;
    int count = 0;
    char *alphabet = dfa.alphabet;

    pc = p->start;
    e = p->start + p->len;

    for(; pc < e; pc++) {
        if (pc->opcode == Char && alphabet[pc->c] == 0) {
            alphabet[pc->c] = 1;
            count++;
        }
    }

    count++;/* 1 for others */
    dfa.accept = count;
    return count;
}

static void miniSize()
{
   /* TODO */ ;

}

static void dump()
{
    struct Node *node = dfa.head;
    //int i;

    printf("Dfa:\n\tNode num %d\n", dfa.num);
    while(node) {
        printf("\t%p | ", node);
        printf("%c | ", node->flag == 1 ? 'F' : 'M');
        printIntArray(node->nstates, node->n);
        printf("| \n");
#if 0
        for(i = 'a'; i <= 'z'; i++) {
            if (node->trans[i])
                printf(" %c -> %p \n", i, node->trans[i]);
        }
        printf("\n");
#endif
        node = node->next;
    }
}

static void dfaCompress();
static void dumpDfaCompress();
void nfa2dfa(Prog *p)
{
    int *nstates = (int*)mal((p->len)* sizeof(int));
    int i, n;
    struct Node *node, *nextnode;

    printf("nfa states num %d transto dfa:\n", p->len);

    init();
    n = e_closure(p->start, nstates, 1);
    node = newNode(p, nstates, n);/* s0 */

    while((node = findNodeNoMark())) {
        node->mark = 1;
        for(i = 'a'; i <= 'z'; i++) {
            if ((n = move(p->start, node->nstates, node->n, i, nstates)) == 0)
                continue;

            n = e_closure(p->start, nstates, n);
            if ((nextnode = NodeSearch(nstates, n)) == NULL)
                nextnode = newNode(p, nstates, n);
            node->trans[i] = nextnode;
        }
    }

    getAlphabet(p);
    dump();
    miniSize();

    dfaCompress();
    dumpDfaCompress();
}

typedef unsigned int DfaIndex;
struct NodeCompress {
    short flag;
    DfaIndex next[0];
};

struct DfaCompress {
    unsigned short num;
    unsigned short accept;
    char acceptAlphabet[MAX_ALPHABET_NUM];
    char alphabet[MAX_ALPHABET_NUM];
    struct NodeCompress *states;
};

#define dfaNodeTrans(node, size, offset) \
    ((struct NodeCompress *)((char*)(node) + (size)*(offset)))

static struct DfaCompress dfaFinal;

static void dfaNodesCompress(struct NodeCompress *s1, struct Node *s2, int size, char *a)
{
    int i;
    struct Node *node = s2;
    struct NodeCompress *ns;

    while(node) {
        ns = dfaNodeTrans(s1, size, node->index);
        ns->flag = node->flag;
        for(i = 0; i <= MAX_ALPHABET_NUM; i++) {
            if (node->trans[i])
                ns->next[(int)a[i]] = node->trans[i]->index;
        }
        node = node->next;
    }

    return;
}

static void dumpDfaCompress()
{
    int i, j, size;
    struct NodeCompress *node;
    struct NodeCompress *ns;

    size = sizeof(struct NodeCompress) + sizeof(DfaIndex) * dfaFinal.accept;

    printf("\n");
    printf("Dfa compress:\n");
    for(i = 'a'; i <= 'z'; i++) {
        printf("%2c ", i);
    }
    printf("\n");
    for(i = 'a'; i <= 'z'; i++) {
        printf("%2d ", dfaFinal.alphabet[i]);
    }
    printf("\n        ");
    for(i = 0; i <= dfaFinal.accept; i++) {
        printf("%2c ", dfaFinal.acceptAlphabet[i]);
    }
    printf("\n");
    ns = dfaFinal.states;
    for(i = 0; i < dfaFinal.num; i++) {
        node = dfaNodeTrans(ns, size, i);
        printf("node %d: ", i);
        for(j = 0; j < dfaFinal.accept; j++) {
            printf("%2d ", node->next[j]);
        }
        printf("\n");
    }
}

void dfaCompress()
{
    int stateSize, i, count;
    struct NodeCompress *nd;
    char *acceptAlphabet = dfaFinal.acceptAlphabet;
    char *alphabet = dfaFinal.alphabet;

    dfaFinal.num = dfa.num;
    dfaFinal.accept = dfa.accept;
    memcpy(dfaFinal.alphabet, dfa.alphabet, MAX_ALPHABET_NUM);

    acceptAlphabet[0] = '*';
    for(i = 0, count = 0; i < MAX_ALPHABET_NUM; i++) {
        if (alphabet[i]) {
            count++;
            acceptAlphabet[count] = i;
            alphabet[i] = count;
        }
    }

    stateSize = sizeof(struct NodeCompress) + sizeof(DfaIndex) * dfaFinal.accept;

    nd = (struct NodeCompress *)mal(stateSize * dfaFinal.num);

    dfaNodesCompress(nd, dfa.head, stateSize, dfaFinal.alphabet);

    dfaFinal.states = nd;
}


