# 记录自己做这个项目的过程
是的，在很久以前就开始想做这个东西，但是鸽了很久..终于又重新开始学了:p

Part1
较为简单，略。

Part2 
task
```
1.重构合并 lept_parse_null()、lept_parse_false()、lept_parse_true() 为 lept_parse_literal()。
2.加入 维基百科双精度浮点数 的一些边界值至单元测试，如 min subnormal positive double、max double 等。
3.去掉 test_parse_invalid_value() 和 test_parse_root_not_singular() 中的 #if 0 ... #endif，执行测试，证实测试失败。按 JSON number 的语法在 lept_parse_number() 校验，不符合标准的情况返回 LEPT_PARSE_INVALID_VALUE 错误码。
4.去掉 test_parse_number_too_big 中的 #if 0 ... #endif，执行测试，证实测试失败。仔细阅读 strtod()，看看怎样从返回值得知数值是否过大，以返回 LEPT_PARSE_NUMBER_TOO_BIG 错误码。（提示：这里需要 #include 额外两个标准库头文件。）
一个一个来
```

1.我的写法：
```
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
```

 

2.加入 维基百科双精度浮点数 的一些边界值至单元测试，如 min subnormal positive double、max double 等。
这个资料没看懂，所以也没测试。不过tutorial.md里说了加了也是能通过测试的，所以没做就原谅我吧(

 

 

3.这问是tutorial 2里最困难的一部分，主要考察怎么校验数字格式。我的做法很naive，面向测试样例编程，分多种情况一个个判过去。
```
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
```

 

4.这题纯纯阅读理解，可惜我没理解出来。贴一个看完答案后自己写的能通过测试的代码：
```
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
```
最开始没有把 errno 设为0，这不符合校验的原则(初值应该设置为一个不是目标值的值)；其次没懂文档里“会返回HUGE_VAL”是什么意思，查完才知道在math.h库里，所有太大的数都会被设置成HUGE_VAL。
