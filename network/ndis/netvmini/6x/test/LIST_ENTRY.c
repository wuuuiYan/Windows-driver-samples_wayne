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

Pmy_LIST_ENTRY my_InitializeListHead() {
	puts("my_InitializeListHead() begin...");

	Pmy_LIST_ENTRY phead = (Pmy_LIST_ENTRY)malloc(sizeof(my_LIST_ENTRY));
	phead->Flink = phead;
	phead->Blink = phead;

	puts("my_InitializeListHead() end...");
	return phead;
}

bool my_IsListEmpty(Pmy_LIST_ENTRY phead) {
	puts("my_IsListEmpty() begin...");

	puts("my_IsListEmpty() end...");
	return phead->Flink == phead && phead->Blink == phead;
}

Pmy_LIST_ENTRY my_FindListTail(Pmy_LIST_ENTRY phead) {
	puts("my_FindListTail() begin...");

	Pmy_LIST_ENTRY p = phead;
	while (p->Flink != phead) {
		puts("while()...");
		p = p->Flink;
		// printf("%d ", ((ptest)p)->k);
	}
	puts("");

	puts("my_FindListTail() end...");
	return p;
}

void my_InsertTailList(Pmy_LIST_ENTRY phead, Pmy_LIST_ENTRY pblock) {
	puts("my_InsertTailList() begin...");

	Pmy_LIST_ENTRY ptail = my_FindListTail(phead);

	ptail->Flink = pblock;
	phead->Blink = pblock;

	pblock->Flink = phead;
	pblock->Blink = ptail;

	puts("my_InsertTailList() end...");
}

void my_TraverseList(Pmy_LIST_ENTRY phead) {
	puts("my_TraverseList() begin()...");

	Pmy_LIST_ENTRY p = phead;
	while (p->Flink != phead) {
		p = p->Flink;
		printf("%d ", ((ptest)p)->k);
	}
	puts("");

	puts("my_TraverseList() end()...");
}

int main() {

	Pmy_LIST_ENTRY phead = my_InitializeListHead(); // 链表头不存放数据
	puts("--------------");

	if (my_IsListEmpty(phead)) {
		puts("List is empty!");
	} else {
		puts("List is not empty!");
	}
	puts("--------------");

	// Pmy_LIST_ENTRY ptail = my_FindListTail(phead);

	// 不能使用循环替代下述代码，因为其中涉及到内存空间的有效范围
	test my_data_1;
	my_data_1.k = 1;
	my_InsertTailList(phead, &my_data_1.Entry);

	test my_data_2;
	my_data_2.k = 2;
	my_InsertTailList(phead, &my_data_2.Entry);
	puts("--------------");

	if (my_IsListEmpty(phead)) {
		puts("List is empty!");
	} else {
		puts("List is not empty!");
	}
	puts("--------------");

	my_TraverseList(phead);

	free(phead); // 防止内存泄漏

	// printf("Hello, World!\n");
	return 0;
}