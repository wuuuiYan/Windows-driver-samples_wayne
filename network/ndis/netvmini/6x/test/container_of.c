// From:Linux内核代码大佬们如何观看的？ - 叨陪鲤的回答 - 知乎
// https://www.zhihu.com/question/439569498/answer/2143592573

#include <stdio.h>

struct test {
	char i;
	int j;
	char k;
};

// 计算结构体成员 member 相对于其所属的结构体类型 type 的变量的地址偏移量，原因见 main() 函数
#define offsetof(type, member) ((size_t) &((type *)0)->member)

/*
	该宏定义来自 Linux 内核，宏的主体使用了 GCC 扩展的语法 ({ ... })，允许在一个宏中执行语句并返回一个值。
	第二行定义了一个与 type 结构体内的 member 成员类型相同的指针 __mptr，并将它初始化为传入的成员指针 ptr。
	如果开发者使用时输入的参数有问题：ptr与member类型不匹配，编译时便会有warnning，
	但是如果去掉该行，那个就没有了，而这个警告恰恰是必须的
*/
#define container_of(ptr, type, member) ({ \
	const typeof(((type *)0)->member) *__mptr = (ptr); \
	(type *)((char *)__mptr - offsetof(type, member)); \
})

int main() {
	struct test temp;

	printf("&temp = %p\n", &temp);
	printf("&temp.k = %p\n", &temp.k);

	//
	// 求结构体成员 k 相对于结构体变量 temp 的地址偏移量
	//
	printf("&((struct test *)0)->k = %d\n", ((int)&((struct test *)0)->k));
	// 成员访问操作符(->)的优先级高于取地址操作符(&)

	char *p = &temp.k;
	printf("%d\n", offsetof(struct test, k));
	printf("%p\n", container_of(p, struct test, k)); // 传入结构体变量的成员名及其地址，返回结构体变量的起始地址

	// printf("%d", sizeof(struct test)); // 12
	// printf("Hello, World!\n");
	return 0;
}