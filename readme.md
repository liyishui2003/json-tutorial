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
```


## Part4 
### task
```
1.实现 lept_parse_hex4()，不合法的十六进位数返回 LEPT_PARSE_INVALID_UNICODE_HEX。
2.按第 3 节谈到的 UTF-8 编码原理，实现 lept_encode_utf8()。这函数假设码点在正确范围 U+0000 ~ U+10FFFF（用断言检测）。
3.加入对代理对的处理，不正确的代理对范围要返回 LEPT_PARSE_INVALID_UNICODE_SURROGATE 错误。
```

#### 1.
我的写法较标答繁琐，先判后四位是否是十六进制位数再转换
```cpp
static const char* lept_parse_hex4(const char* p, unsigned* u) {
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
```

#### 2.
只要把UTF-8的编码翻译出来就好，写代码时根据字节数声明了对应变量，这么做是为了方便自己调试。
```cpp
static void lept_encode_utf8(lept_context* c, unsigned u) {
    //把码点编码成 UTF-8，写进缓冲区
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
```
但此处有个困惑，为什么这里可以直接写入unsigned？PUT函数接受的不是char吗？以及lept_context_push()函数好像也没实际写入数据，只做了扩容，到底在哪完成的写入？
答：
```
来看PUTC的宏定义：#define PUTC(c, ch) do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
这里用了"解引用"将函数传回的指针所指向的位置赋值成ch，也就是说lept_context_push()函数里并没有写入，只是扩容并返回指针。
可以直接写入unsigned是因为C语言有隐式转换：将unsighed类型直接转为char。
如果unsigned类型的值在char类型能表示的范围(有符号char是-128到127，无符号char是0到255)内，那会被正确转换。
但可能会丢失超出char表示范围的高位数据(比如对于无符号char，如果unsigned值大于255，会只保留低8位)。
```

#### 3.
代理对可以在完成一次lept_parse_hex4后检测，如果解析出来的数在[0xD800,0xDBFF]范围内，说明后面应该跟着\\uXXXX。
逐个判断即可。
```cpp
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
```

#### 学习笔记
了解了码点、UTF-8的编码原理，印象最深的是代理对和字节序。
代理对原理:
```
正常来说如果用一个数字表示一个码点的话，能表示的数目就是O(n)的。
但需要用少量的数字范围表示更多的码点，且同时保留低位和ASCII码一致时，就可以选择开辟部分区间作为特别的"入口"。
从某些特定的数字(入口)出发，后面再跟着一个数字，就能用原先的范围表示o(n^2)的数据。
非常巧妙的代理对。
```
字节序原理：
```
UTF-8之所以通用的另一个原因是UTF - 8 不存在字节序的概念。
它不像UTF-16/32将一个字符的编码均匀分布在固定长度的多个字节中，且字节顺序会变化。
组成原理里有大端、小端的概念：这好比编程语言里面的阅读顺序，我们读现代文是从左往右，读古文时是从右往左。
那编译时放在大端的到底是低位还是高位？还好UTF-8没有这种困扰，它的编码是从高位到低位依次分布在字节中的。
```

## Part5
这次和之前不同的是在开始写代码前记录了读tutorial.md的笔记：

-
比较了数组和链表，数组支持快速访问和高缓存一致性，但无法快速插入；
链表只能O(n)访问和容易缓存不命中，但可以快速插入。

-
```
typedef struct lept_value lept_value;

struct lept_value {
    union {
        struct { lept_value* e; size_t size; }a; /* array */
        struct { char* s; size_t len; }s;
        double n;
    }u;
    lept_type type;
};
```
union 的特点是其内部所有成员共用同一块内存空间
在同一时刻只能有一个成员被使用
因为它们共享内存，对其中一个成员赋值会覆盖其他成员对应内存的数据
这也很合理，显然我们只会用到union的某一部分，json对象不可能既是字符又是数字

-
自身类型指针：指向自身结构体的指针，该指针可以用来实现
遍历数组、动态分配或释放数组内存等操作
前向声明：真正定义类型前，先通知编译器它存在，稍后再完整定义。
为什么需要前向声明：当结构体包含自身指针时，编译器又是按顺序读代码的，
就会尝试去找自身指针的完整定义——但此时还没定义好，因为它内部有一个指向自身类型的指针，完整定义还在后面
。就需要前向声明安抚一下编译器别找了，后面有，这里先放着。

-
作者反复强调了：C 语言的数组大小应该使用 size_t 类型。但受语言标准
的影响，打印size_t各有写法，所以if和endif这套可以加上条件：
```cpp
#if defined(_MSC_VER)
...
#else
...
#endif
```
