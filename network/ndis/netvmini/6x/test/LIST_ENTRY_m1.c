#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct _my_LIST_ENTRY {
	struct _my_LIST_ENTRY *Flink;
	struct _my_LIST_ENTRY *Blink;
} my_LIST_ENTRY, *Pmy_LIST_ENTRY;

typedef struct _test {
	my_LIST_ENTRY Entry;
	int k;
} test, *ptest;

my_LIST_ENTRY my_InitializeListHead() {
	puts("my_InitializeListHead() begin...");

	my_LIST_ENTRY head;
	head.Flink = &head;
	head.Blink = &head;

	puts("my_InitializeListHead() end...");
	return head;
}

bool my_IsListEmpty(Pmy_LIST_ENTRY phead) {
	puts("my_IsListEmpty() begin...");

	puts("my_IsListEmpty() end...");
	return phead->Flink == phead && phead->Blink == phead;
}

int main() {
	// 如果按值传递链表头，那么初始化函数执行完毕之后其内存地址就会被释放
	// 但由于该链表的初始化操作令其成员指针指向自身，因此函数按值传递后、复制给 head 的成员指针指向不存在的内存空间
	// 所以后续在判断链表是否为空时，会出现不空的结论
	my_LIST_ENTRY head = my_InitializeListHead();
	puts("--------------");

	if (my_IsListEmpty(&head)) {
		puts("List is empty!");
	} else {
		puts("List is not empty!");
	}
	puts("--------------");

	// free(phead);

	// printf("Hello, World!\n");
	return 0;
}