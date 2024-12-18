#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <stdlib.h>  /* NULL, strtod() */
#include <stdio.h>
#include <errno.h>
#include <math.h>

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)

typedef struct {
    const char* json;
}lept_context;

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c,lept_value* v,int op) {
    char word[3][5] = {
        "null","false","true"
    };
    int lenOfType[3] = { 4,5,4 };
    EXPECT(c, word[op][0]);
    int checkFlag = 1;
    for (int i = 0;i < lenOfType[op];i++) {
        if (c->json[i] != word[op][i+1]) checkFlag = 0;
    }
    if(!checkFlag) return LEPT_PARSE_INVALID_VALUE;
    c->json += lenOfType[op]-1;
    if (op == 0) v->type = LEPT_NULL;
    if (op == 1) v->type = LEPT_FALSE;
    if (op == 2) v->type = LEPT_TRUE;
    return LEPT_PARSE_OK;
}
/*按 JSON number 的语法在 lept_parse_number() 校验，
不符合标准的情况返回 `LEPT_PARSE_INVALID_VALUE` 错误码。*/
static int lept_parse_number(lept_context* c, lept_value* v) {
    // ①check First letter must <='9'&&>='0' || =='-'
    char* p = c->json;
    if (*p < '0' || *p>'9') {
        if (*p != '-') return LEPT_PARSE_INVALID_VALUE;
    }
    //②after zero should be '.' , 'E' , 'e' or nothing 
    if (*p == '0') {
        p++;
        if (*p != '.' && *p != 'E' && *p != 'e' && *p != '\0')
        {
            return LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    //③ at least one digit after '.'
    p = c->json;
    char* point = NULL;
    while (*p != '\0') {
        if (*p == '.' && point == NULL) {
            point = p;
            break;
        }
        p++;
    }
    if (point != NULL) {
        point++;
        if (*point >= '0' && *point <= '9') {//do nothing
        }
        else return LEPT_PARSE_INVALID_VALUE;
    }
    char* end;
    /* \TODO validate number */
    errno = 0;
    v->n = strtod(c->json, &end);
    if (c->json == end)
        return LEPT_PARSE_INVALID_VALUE;
    //④ check number too big
    if (errno == ERANGE && (v->n == HUGE_VAL || v->n == -HUGE_VAL ||
        v->n == HUGE_VALF || v->n == -HUGE_VALF ||
        v->n == HUGE_VALL || v->n == -HUGE_VALL)){
        return LEPT_PARSE_NUMBER_TOO_BIG;
    }
    c->json = end;
    v->type = LEPT_NUMBER;
    return LEPT_PARSE_OK;
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, 2);
        case 'f':  return lept_parse_literal(c, v, 1);
        case 'n':  return lept_parse_literal(c, v, 0);
        default:   return lept_parse_number(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    v->type = LEPT_NULL;
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    return ret;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->n;
}
