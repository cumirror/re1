#include "regexp.h"
#include <stdlib.h>

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

struct Dfa {
    short mark;/* marked or not */
    short flag;/* accepr or not */
    struct Dfa *trans[256];
    struct Dfa *next;
    int n;
    int *nstates;
};


static struct Dfa *head;
static struct Dfa *tail;

static struct Dfa *newDfaNode(Prog *p, int *nstates, int n)
{
    struct Dfa *dfa = (struct Dfa *)mal(sizeof(struct Dfa));
    int *ns = (int *)mal(sizeof(int)*n);

    dfa->next = NULL;
    dfa->n = n;
    dfa->nstates = ns;
    memcpy(ns, nstates, sizeof(int)*n);

    if (head == NULL) {
        head = dfa;
        tail = dfa;
    } else {
        tail->next = dfa;
        tail = dfa;
    }

    if ((p->len - 1) == nstates[n-1])
        dfa->flag = 1;

    return dfa;
}

static struct Dfa *DfaSearch(int *nstates, int n)
{
    struct Dfa *dfa = head;

    while(dfa) {
        if (dfa->n == n && memcmp(dfa->nstates, nstates, sizeof(int)*n) == 0)
            break;
        dfa = dfa->next;
    }

    return dfa;
}

static struct Dfa *findDfaNoMark()
{
    struct Dfa *dfa = head;

    while(dfa) {
        if (dfa->mark == 0)
            break;
        dfa = dfa->next;
    }

    return dfa;
}

int getAcceptAlphabet(Prog *p, char *alphabet)
{
    Inst *pc, *e;
    int count = 0;

    pc = p->start;
    e = p->start + p->len;

    for(; pc < e; pc++) {
        if (pc->opcode == Char && alphabet[pc->c] == 0) {
            alphabet[pc->c] = 1;
            count++;
        }
    }

    return count;
}

#define REGEX_ALPHABET_NUM 128
void miniSizeDfa(Prog *p, struct Dfa *head)
{

#if 0
    char alph[REGEX_ALPHABET_NUM] = {0};
    int alphSize;
    alphSize = getAcceptAlphabet(p, alph);
    printf("Accept alphabet num is %d\n\t", alphSize);
    for(i = 0; i < REGEX_ALPHABET_NUM; i++) {
        if (alph[i] == 1)
            printf("%c ", i);
    }
    printf("\n");
#endif

}
void dumpDfa()
{
    struct Dfa *dfa = head;
    //int i;

    while(dfa) {
        printf("\t%p | ", dfa);
        printf("%c | ", dfa->flag == 1 ? 'F' : 'M');
        printIntArray(dfa->nstates, dfa->n);
        printf("| ");
#if 0
        for(i = 'a'; i <= 'z'; i++) {
            if (dfa->trans[i])
                printf(" %c -> %p ", i, dfa->trans[i]);
        }
#endif
        printf("\n");
        dfa = dfa->next;
    }
}

void nfa2dfa(Prog *p)
{
    int *nstates = (int*)mal((p->len)* sizeof(int));
    int i, n;
    struct Dfa *dfa, *nextdfa;

    printf("nfa states num %d transto dfa:\n", p->len);

    n = e_closure(p->start, nstates, 1);
    dfa = newDfaNode(p, nstates, n);/* s0 */

    while((dfa = findDfaNoMark())) {
        dfa->mark = 1;
        for(i = 'a'; i <= 'z'; i++) {
            if ((n = move(p->start, dfa->nstates, dfa->n, i, nstates)) == 0)
                continue;

            n = e_closure(p->start, nstates, n);
            if ((nextdfa = DfaSearch(nstates, n)) == NULL)
                nextdfa = newDfaNode(p, nstates, n);
            dfa->trans[i] = nextdfa;
        }
    }

    dumpDfa();
    miniSizeDfa(p, head);

}
