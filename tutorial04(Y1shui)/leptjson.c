#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */

#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)

typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;

static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)
            c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}

static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}

static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;
    if (*p == '-') p++;
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    errno = 0;
    v->u.n = strtod(c->json, NULL);
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
        return LEPT_PARSE_NUMBER_TOO_BIG;
    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

static const char* lept_parse_hex4(const char* p, unsigned* u) {
    /* \TODO */
    //如果 `\u` 后不是 4 位十六进位数字，则返回 `LEPT_PARSE_INVALID_UNICODE_HEX` 错误。
    char* to = p;
    int cntOfNumber = 0;
    to++;
    if (((*to) <= 'F' && (*to) >= 'A') || ((*to) <= '9' && (*to) >= '0') ||
        ((*to) <= 'f' && (*to) >= 'a')) {
        cntOfNumber++;
        while (((*to) <= 'F' && (*to) >= 'A') || ((*to) <= '9' && (*to) >= '0') ||
        ((*to) <= 'f' && (*to) >= 'a')){
            to++;
            cntOfNumber++;
            if (cntOfNumber == 4) break;
        }
    }
    if (cntOfNumber < 4) return NULL;
    *u = 0;
    for (int i = 0;i < 4;i++) {
        *u <<= 4;
        if ((*p) <= 'F' && (*p) >= 'A') {
            *u += ((*p) - 'A') + 10;
        }
        else if ((*p) <= 'f' && (*p) >= 'a') {
            *u += ((*p) - 'a') + 10;
        }
        else if ((*p) <= '9' && (*p) >= '0') {
            *u += ((*p) - '0');
        }
        else {
            return NULL;
        }
        p++;
    }
    return p;
}

//为什么这里可以直接写入unsigned？
//PUT函数接受的不是char吗？
static void lept_encode_utf8(lept_context* c, unsigned u) {
    //把码点编码成 UTF-8，写进缓冲区
    //翻译编码原则，声明成多个变量有助于调试时直接看值是多少
    assert(u >= 0x0000 && u <= 0x10FFFF);
    if (u >= 0x0000 && u <= 0x007F) {
        unsigned bit1;
        bit1 = u & 0xFF;
        PUTC(c,bit1);
    }
    else if (u >= 0x0080 && u <= 0x07FF) {
        unsigned bit1,bit2;
        bit1 = (0xC0 | ((u >> 6) & 0xFF) );
        bit2 = (0x80 | (u & 0x3F));
        PUTC(c,bit1);
        PUTC(c, bit2);
    }
    else if (u >= 0x0800 && u <= 0xFFFF) {
        unsigned bit1, bit2, bit3;
        bit1 = (0xE0 | ((u >> 12) & 0xFF));
        bit2 = (0x80 | ((u >> 6) & 0x3F));
        bit3 = (0x80 | (u & 0x3F));
        PUTC(c, bit1);
        PUTC(c, bit2);
        PUTC(c, bit3);
    }
    else {
        unsigned bit1, bit2, bit3,bit4;
        bit1 = (0xF0 | ((u >> 18) & 0xFF));
        bit2 = (0x80 | ((u >> 12) & 0x3F));
        bit3 = (0x80 | ((u >> 6) & 0x3F));
        bit4 = (0x80 | (u  & 0x3F));
        PUTC(c, bit1);
        PUTC(c, bit2);
        PUTC(c, bit3);
        PUTC(c, bit4);
    }
}

#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

static int lept_parse_string(lept_context* c, lept_value* v) {
    size_t head = c->top, len;
    unsigned u;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                len = c->top - head;
                lept_set_string(v, (const char*)lept_context_pop(c, len), len);
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break;
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':
                    //codepoint = 0x10000 + (H − 0xD800) × 0x400 + (L − 0xDC00)
                        if (!(p = lept_parse_hex4(p, &u)))
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) {
                            if (*p != '\\') return LEPT_PARSE_INVALID_UNICODE_SURROGATE;
                            p++;
                            if (*p != 'u') return LEPT_PARSE_INVALID_UNICODE_SURROGATE;
                            p++;
                            unsigned uLow;
                            if (!(p = lept_parse_hex4(p, &uLow)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (uLow > 0xDFFF || uLow < 0xDC00) return LEPT_PARSE_INVALID_UNICODE_SURROGATE;
                            unsigned codePoint;
                            codePoint = 0x10000 + (u - 0xD800) * 0x400 + (uLow - 0xDC00);
                            lept_encode_utf8(c, codePoint);
                        }
                        else lept_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}

static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:   return lept_parse_number(c, v);
        case '"':  return lept_parse_string(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);
    lept_parse_whitespace(&c);
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') {
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

void lept_free(lept_value* v) {
    assert(v != NULL);
    if (v->type == LEPT_STRING)
        free(v->u.s.s);
    v->type = LEPT_NULL;
}

lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}

const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}
