#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <ctype.h>
#include <gc/gc.h>
#include "stream.h"

struct Word;

typedef union {
        unsigned int umask;
        struct Word* word;
} address_t;

typedef union {
        struct Word* word;
        int num;
        const char* str;
} decrement_t;

struct Word {
        address_t address;
        decrement_t decrement;
};

#define WordAtomP(w) ((w)->address.umask & 1)
#define WordAtomType(w) ((w)->address.umask & 0xfffffff0)

#define WordNilP(w) ((w) == NULL || ((w)->address.word == NULL && (w)->decrement.word == NULL))

#define WORDATOM_WORD 0
#define WORDATOM_NUM 0x10
#define WORDATOM_STR 0x20

#define ATOM_STR(w) ((WordAtomType(w)==WORDATOM_STR) ? ((w)->decrement.str) : NULL)
#define ATOM_NUM(w) ((WordAtomType(w)==WORDATOM_NUM) ? ((w)->decrement.num) : 0)
#define ATOM_WORD(w) ((WordAtomType(w)==WORDATOM_WORD) ? ((w)->decrement.word) : NULL)

#define SetWord(w, ca, cd) do { (w)->address.word = (ca); (w)->decrement.word = (cd); } while (0)
#define SetCAR(w, ca) do { (w)->address.word = (ca); } while (0)
#define SetCDR(w, cd) do { (w)->decrement.word = (cd); } while (0)
#define SetAtomWord(w, ww) do { (w)->address.umask = 1+WORDATOM_WORD; (w)->decrement.word = (ww); } while(0)
#define SetAtomNum(w, n) do { (w)->address.umask = 1+WORDATOM_NUM; (w)->decrement.num = (n); } while(0)
#define SetAtomStr(w, s) do { (w)->address.umask = 1+WORDATOM_STR; (w)->decrement.str = (s); } while(0)

#define CAR(w) ((w)->address.word)
#define CDR(w) ((w)->decrement.word)

/*
  COND:
  input: list of expressions and associated values
  output: first value whose expression evaluates to true
 */

struct interpreter {
        struct instream stream;
        struct Word t;
        struct Word nil;
        struct Word* env;
};

const char* intern(const char* str) {
        return g_intern_string(str);
}

gboolean init_interpreter(struct interpreter* I, const char* filename) {
        memset(I, 0, sizeof(struct interpreter));
        init_file_stream(&I->stream, filename);
        /* set env to () */
        I->t.address.umask = 1;
        I->t.decrement.num = 1;
        I->nil.address.word = NULL;
        I->nil.decrement.word = NULL;
        I->env = NULL;
        return TRUE;
}

int get_char(struct interpreter* I) {
        return I->stream.get_char(I->stream.hstream);
}

void put_char(struct interpreter* I, int ch) {
        I->stream.put_char(ch, I->stream.hstream);
}

void print_atom(struct interpreter* I, struct Word* word) {
        if (WordNilP(word)) {
                puts("nil");
        }
        else {
                switch (WordAtomType(word)) {
                case WORDATOM_WORD:
                        printf("%p", ATOM_WORD(word));
                        break;
                case WORDATOM_NUM:
                        printf("%d", ATOM_NUM(word));
                        break;
                case WORDATOM_STR:
                        printf("%s", ATOM_STR(word));
                        break;
                default:
                        puts("***");
                        break;
                }
        }
}

void print_sexpr(struct interpreter* I, struct Word* word) {
        printf("(");

        if (WordAtomP(CAR(word)) && WordAtomP(CDR(word))) {
                print_atom(I, CAR(word));
                printf(" . ");
                print_atom(I, CDR(word));
        }
        else {
                while (1) {
                        if (WordNilP(CAR(word))) {
                                printf("nil");
                        }
                        else if (WordAtomP(CAR(word))) {
                                print_atom(I, CAR(word));
                        }
                        else {
                                print_sexpr(I, CAR(word));
                        }

                        if (!WordNilP(CDR(word))) {
                                printf(" ");
                        }
                        word = CDR(word);
                        if (WordNilP(word)) {
                                break;
                        }
                }
        }
        printf(")");
}

struct Word* eq(struct interpreter* I, struct Word* x, struct Word* y) {
        return (WordAtomP(x) && WordAtomP(y) && (x->decrement.word == y->decrement.word)) ? &I->t : NULL;
}

struct Word* eval(struct interpreter* I, struct Word* e);
struct Word* cond(struct interpreter* I, struct Word* e);


struct Word* cond(struct interpreter* I, struct Word* e) {
        while (e) {
                struct Word* cond0 = CAR(e);
                struct Word* e0 = CAR(cond0);
                struct Word* s1 = CDR(cond0);

                if (e0) {
                        e0 = eval(I, e0);
                        if (e0)
                                return s1;
                }
                e = CDR(e);
        }
        return &I->nil;
}

struct Word* eval(struct interpreter* I, struct Word* e) {
        static const char* squote = NULL;
        static const char* scar = NULL;
        static const char* scdr = NULL;
        static const char* satom = NULL;
        static const char* scond = NULL;
        static const char* scons = NULL;
        static const char* eq = NULL;

        if (squote == NULL) {
                squote = intern("quote");
                scar = intern("car");
                scdr = intern("cdr");
                satom = intern("atom");
                scond = intern("cond");
                scons = intern("cons");
                eq = intern("eq");
        }

        if (WordNilP(e)) {
                return &I->nil;
        } else if (WordAtomP(e)) {
                return e;
        }
        else {
                // todo: apply
                struct Word* car = eval(I, CAR(e));
                struct Word* cdr = CDR(e);
                const char* astr = ATOM_STR(car);
                if (astr == squote) {
                        return cdr;
                }
                else if (astr == satom) {
                        struct Word* ret = eval(I, CAR(cdr));
                        if (WordAtomP(ret))
                                return ret;
                        else
                                return &I->nil;
                }
                else if (astr == scar) {
                        return CAR(eval(I, CAR(cdr)));
                }
                else if (astr == scdr) {
                        return CDR(eval(I, CAR(cdr)));
                }
                else if (astr == scond) {
                        // todo: evcon()
                        return cond(I, cdr);
                }
                else if (astr == scons) {
                        return NULL;
                }
                else {
                        printf("Eval error:");
                        print_sexpr(I, e);
                        return &I->nil;
                }
        }
}


struct Word* read(struct interpreter* I);

struct Word* read_atom(struct interpreter* I);
struct Word* read_list(struct interpreter* I);

struct Word* read(struct interpreter* I) {
        int ch;
        struct Word* ret;
        ch = get_char(I);
        while (ch) {
                if (isspace(ch)) {
                }
                else if (ch == '(') {
                        put_char(I, ch);
                        ret = read_list(I);
                        break;
                }
                else {
                        put_char(I, ch);
                        ret = read_atom(I);
                        break;
                }

                ch = get_char(I);
        }
        return ret;
}

struct Word* read_atom(struct interpreter* I) {
        int ch;
        char buf[128];

        ch = get_char(I);
        if (isdigit(ch)) {
                /* parse number */
                int i = 0;
                while (isdigit(ch)) {
                        buf[i++] = ch;
                        ch = get_char(I);
                }
                buf[i] = 0;
                put_char(I, ch); // rewind
                struct Word* atom = malloc(sizeof(struct Word));
                SetAtomNum(atom, atoi(buf));
                return atom;
        }
        else if (isalpha(ch)) {
                /* parse symbol */
                int i = 0;
                while (isalnum(ch)) {
                        buf[i++] = ch;
                        ch = get_char(I);
                }
                buf[i] = 0;
                put_char(I, ch); // rewind
                struct Word* atom = malloc(sizeof(struct Word));
                SetAtomStr(atom, intern(buf));
                return atom;
        }
        else if (ch == '"') {
                /* parse string */
                return NULL;
        }
        else {
                if (isgraph(ch))
                        printf("Unknown token: %c\n", ch);
                return NULL;
        }
}

struct Word* read_list(struct interpreter* I) {
        int ch;
        ch = get_char(I);
        if (ch != '(')
                return NULL;

        struct Word* head = malloc(sizeof(struct Word));
        SetWord(head, NULL, NULL);

        struct Word* prev = NULL;
        struct Word* curr = head;

        ch = get_char(I);
        while (ch) {
                if (isspace(ch)) {
                }
                else if (ch == ')') {
                        break;
                }
                else if (ch == '(') {
                        prev = curr;
                        put_char(I, ch);
                        SetCAR(prev, read_list(I));
                        curr = malloc(sizeof(struct Word));
                        SetWord(curr, NULL, NULL);
                        SetCDR(prev, curr);
                }
                else {
                        prev = curr;
                        put_char(I, ch);
                        SetCAR(prev, read_atom(I));
                        curr = malloc(sizeof(struct Word));
                        SetWord(curr, NULL, NULL);
                        SetCDR(prev, curr);
                }

                ch = get_char(I);
        }
        return head;
}

void loop(struct interpreter* I, struct Word* word) {
        const char* STOP = intern("stop");
        const char* QUOTE = intern("quote");
        while (1) {
                if (WordNilP(word)) {
                        print_atom(I, word);
                        puts("");
                        word = read(I);
                }
                else if (WordAtomP(word)) {
                        if (ATOM_STR(word) == STOP)
                                return;
                        print_atom(I, word);
                        puts("");
                        word = read(I);
                }
                else if (WordAtomP(CAR(word)) && ATOM_STR(CAR(word)) == QUOTE) {
                        print_sexpr(I, word);
                        puts("");
                        word = read(I);
                }
                if (word == NULL)
                        return;
                word = eval(I, word);
        }
}

int main(int argc, char* argv[]) {
        struct interpreter i;
        struct interpreter* I = &i;
        init_interpreter(I, argv[1]);

        struct Word* lst = read(I);
        printf("\npost-read\n");
        print_sexpr(I, lst);
        printf("\npre-eval\n");

        lst = eval(I, lst);
        printf("\npost-eval\n");
        print_sexpr(I, lst);

        printf("\nloop\n");
        loop(I, lst);

        return 0;
}
