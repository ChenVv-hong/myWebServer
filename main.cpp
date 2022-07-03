#include <iostream>

class ListNode{
public:
	ListNode(){

	}

	int val;
	ListNode *next;
};

void p(ListNode *a){
	a->next = nullptr;
	a = nullptr;
}

int main() {
	ListNode *a = new ListNode();
	a->val = 0;
	a->next = new ListNode();
	a->next->val = 10;
	a->next->next = nullptr;
	p(a);
	if(a->next == nullptr) std::cout << "1\n";
	else std::cout << "2\n";
	return 0;
}
