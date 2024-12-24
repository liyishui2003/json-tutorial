# 记录自己做这个项目的过程
是的，在很久以前就开始想做这个东西，但是鸽了很久..终于又重新开始学了:p

## Part1
较为简单，略。

## Part2 
### task
```
1.重构合并 lept_parse_null()、lept_parse_false()、lept_parse_true() 为 lept_parse_literal()。
2.加入 维基百科双精度浮点数 的一些边界值至单元测试，如 min subnormal positive double、max double 等。
3.去掉 test_parse_invalid_value() 和 test_parse_root_not_singular() 中的 #if 0 ... #endif，执行测试，证实测试失败。按 JSON number 的语法在 lept_parse_number() 校验，不符合标准的情况返回 LEPT_PARSE_INVALID_VALUE 错误码。
4.去掉 test_parse_number_too_big 中的 #if 0 ... #endif，执行测试，证实测试失败。仔细阅读 strtod()，看看怎样从返回值得知数值是否过大，以返回 LEPT_PARSE_NUMBER_TOO_BIG 错误码。（提示：这里需要 #include 额外两个标准库头文件。）
一个一个来
```

#### 1.
我的写法：
```cpp
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

 
#### 2.
加入维基百科双精度浮点数的一些边界值至单元测试，如 min subnormal positive double、max double 等。
这个资料没看懂，所以也没测试。不过tutorial.md里说了加了也是能通过测试的，所以没做就原谅我吧(

 

 

#### 3.
这问是tutorial 2里最困难的一部分，主要考察怎么校验数字格式。我的做法很naive，面向测试样例编程，分多种情况一个个判过去。
```cpp
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

 

#### 4.
这题纯纯阅读理解，可惜我没理解出来。贴一个看完答案后自己写的能通过测试的代码：
```cpp
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

## part3
### task
```
1.编写 lept_get_boolean() 等访问函数的单元测试，然后实现。
2.实现除了 \u 以外的转义序列解析，令 test_parse_string() 中所有测试通过。
3.解决 test_parse_invalid_string_escape() 和 test_parse_invalid_string_char() 中的失败测试。
4.思考如何优化 test_parse_string() 的性能，那些优化方法有没有缺点。
```
#### 1.
这问让我们写单元测试，主要测能否正常get、set，仿照函数test_access_null()写了一份
```cpp
static void test_access_number() {
    lept_value v;
    lept_init(&v);
    lept_set_number(&v, 123.5);//测试的话主要是测set这个函数
    EXPECT_EQ_DOUBLE(123.5, lept_get_number(&v));
    lept_set_number(&v, 1000000007);
    EXPECT_EQ_DOUBLE(1000000007, lept_get_number(&v));
    lept_set_number(&v, -0.00005);
    EXPECT_EQ_DOUBLE(-0.00005, lept_get_number(&v));
    lept_free(&v);
}
```
思路简单，既然要测试，就将v设置成一个指定值，调用宏去判断是否符合预期，多写了几个边界数据
完成整份练习后看标程校验了一下，发现作者做得更好的一点是："故意先把值设为字符串，那么做可以测试设置其他类型时，有没有调用 lept_free() 去释放内存"。
简单学了如何检测内存泄漏：Windows下用CRT库，Linux下用valgrind工具。

#### 2.
简单枚举一下
```cpp
static int lept_parse_string(lept_context* c, lept_value* v) {
    size_t head = c->top, len;
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
            case '\0':
                c->top = head;
                return LEPT_PARSE_MISS_QUOTATION_MARK;
            case '\\':
                ch = *p++;
            //Updated begin
                switch (ch) {
                    case '\"' : PUTC(c, '\"');break;
                    case '\\' : PUTC(c, '\\');break;
                    case '/' : PUTC(c, '/');break;
                    case 'b' : PUTC(c, '\b');break;
                    case 'f': PUTC(c, '\f');break;
                    case 'n': PUTC(c, '\n');break;
                    case 'r': PUTC(c, '\r');break;
                    case 't': PUTC(c, '\t');break;
                }
                break;
            //Updated over ......
```
思路就是枚举所有的转义序列，因为'\\'能直接读取到，所以用switch语句先判断是否为'\\'，若是，再处理各种情况

#### 3.
观察失败测试因何失败。第一类失败测试形如"\"\\v\""、"\"\\x12\""，问题出在"\\"后字符非法，在上面新增的case语句后加一句默认值即可
```cpp
default: 
    return LEPT_PARSE_INVALID_STRING_ESCAPE;
```
第二类失败测试形如"\"\x01\""、"\"\x1F\""，超出了ascii码语法的码点(<0x20都是非法的)，因此再加个判断
```cpp
static int lept_parse_string(lept_context* c, lept_value* v) {
    //...
            case '\\':
                ch = *p++;
                switch (ch) {
                    case '\"' : PUTC(c, '\"');break;
                    case '\\' : PUTC(c, '\\');break;
                    case '/' : PUTC(c, '/');break;
                    case 'b' : PUTC(c, '\b');break;
                    case 'f': PUTC(c, '\f');break;
                    case 'n': PUTC(c, '\n');break;
                    case 'r': PUTC(c, '\r');break;
                    case 't': PUTC(c, '\t');break;
                    default: 
                        return LEPT_PARSE_INVALID_STRING_ESCAPE;
                }
                break;
            default:
                if (ch < 0x20) {  // 检测非法控制字符
                    c->top = head;               // 恢复栈
                    return LEPT_PARSE_INVALID_STRING_CHAR; // 返回错误
                }
                PUTC(c, ch);
        }
    }
}
```

#### 4.
这里实在是没什么idea了，作者提供的答案是用某个库去优化，以及最坏情况下要把字符串复制一遍，2倍常数。
如果能在转义字符前直接一遍扫过，则能优化一些空间。这个思路我在思考怎么判断转义时想过，但实在是太难写了，相当于手写一个大模拟来字符串匹配，还有太多的
边界情况，真正执行起来不见得快，不如直接用缓冲区复制。

#### 学习笔记
这部分讲到了内存管理、堆栈等，触动很大。接下来以罗列知识点的形式记录自己的收获：
- ![不同的布局](https://github.com/liyishui2003/json-tutorial/raw/master/tutorial03/images/union_layout.png)  
  神秘的布局，能从组成原理的角度节省内存，原来声明变量时如何设计，也是值得推敲的。
-  对内存管理有初步认识，通过lept_init()和lept_free()两个函数，前者用来初始化，清除可能的脏数据，确保结构体处于安全的初始状态；后者释放一个 lept_value 对象中可能动态分配的内存，防止内存泄漏。内存泄漏指的是"动态分配的内存未被释放，导致这些内存变得不可用，同时又无法被系统重新分配和使用的现象"，粗糙地说就是占着茅坑不拉屎还不走。往往出现在new或者malloc后，没有delete或者free。让我想起学操作系统时的"内部碎片"、"外部碎片"，内存泄漏对应的应该是"不可用碎片"。
-  在实现字符串时,作者用了堆栈来当缓冲区。这类模拟题写过很多了，无非就是手写栈来存储过程性的东西，选择栈是因为"先进后出"的需求。惊艳我的地方1.是扩展时每次以1.5倍而非2倍，并且很多STL源码都是这么设计的。2.是不明白为何"ret = c->stack + c->top;"此处可以直接拿stack和top相加?
  ```
答：c->stack 和 c->top 的单位是一致的：c->stack 是一个 char*，单位是字节。c->top 是一个 size_t，也表示字节数，此时指针运算直接以字节为单位。c语言是支持对指针运算的。
