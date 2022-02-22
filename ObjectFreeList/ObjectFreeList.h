#ifndef __OBJECT_FREE_LIST__

class CAllocList;

#define __OBJECT_FREE_LIST__

#define OBJECT_FREE_LIST_SAFE


#define allocObject() _allocObject(__FILEW__, __LINE__)
#define freeObject(x) _freeObject(x, __FILEW__, __LINE__)
#define toNode(ptr) ((stAllocNode<T>*)((unsigned __int64)ptr & 0x000007FFFFFFFFFF))
#define toPtr(cnt, pNode) ((void*)((unsigned __int64)pNode | (cnt << 43)))

template<typename T>
struct stAllocNode {
	stAllocNode() {

		nextPtr = nullptr;
		used = false;

		#if defined(OBJECT_FREE_LIST_SAFE)
			underFlowCheck = (void*)0xF9F9F9F9F9F9F9F9;
			overFlowCheck = (void*)0xF9F9F9F9F9F9F9F9;
		#endif
	}

	#if defined(OBJECT_FREE_LIST_SAFE)
		// f9�� �ʱ�ȭ�ؼ� ����÷ο� üũ�մϴ�.
		void* underFlowCheck;
	#endif

	// alloc �Լ����� ������ ���� ������
	T data;
	
	#if defined(OBJECT_FREE_LIST_SAFE)
		// f9�� �ʱ�ȭ�ؼ� �����÷ο� üũ�մϴ�.
		void* overFlowCheck;
	#endif

	// �Ҵ��� ���� ���
	void* nextPtr;

	// ��尡 ��������� Ȯ��
	bool used;

	// �ҽ� ���� �̸�
	const wchar_t* allocSourceFileName;
	const wchar_t* freeSourceFileName;

	// �ҽ� ����
	int allocLine;
	int freeLine;
};

template<typename T>
class CObjectFreeList
{
public:

	CObjectFreeList(HANDLE heap, bool runConstructor, bool runDestructor, int _capacity = 0);
	~CObjectFreeList();

	T* _allocObject(const wchar_t*, int);

	int _freeObject(T* data, const wchar_t*, int);

	inline unsigned int getCapacity() { return _capacity; }
	inline unsigned int getUsedCount() { return _usedCnt; }

private:

	// �޸� �Ҵ�, ������ ���� ��
	HANDLE _heap;

	// ��� ������ ��带 ����Ʈ�� ���·� �����մϴ�.
	// �Ҵ��ϸ� �����մϴ�.
	void* _freePtr;

	// ��ü ��� ����
	unsigned int _capacity;

	// ���� �Ҵ�� ��� ����
	unsigned int _usedCnt;

	// ������ ������(stNode->data)�� �� ���� ���ϸ� ��� ������(stNode)�� �ȴ� !
	unsigned __int64 _dataPtrToNodePtr;

	// �޸� ������
	// �ܼ� ����Ʈ
	struct stSimpleListNode {
		stAllocNode<T>* ptr;
		stSimpleListNode* next;
	};

	// freeList �Ҹ��ڿ��� �޸� ���������� ����մϴ�.
	// new�� �����͵�
	stSimpleListNode* _totalAllocList;
		
	// ����Ʈ ���� Ƚ��
	// ABA ������ ȸ���ϱ� ���� ����մϴ�.
	unsigned __int64 _nodeChangeCnt = 0;

	// �Ҵ� ��, ������ ���� ���θ� ��Ÿ���ϴ�.
	bool _runConstructor;

	// ���� ��, �Ҹ��� ���� ���θ� ��Ÿ���ϴ�.
	bool _runDestructor;

};

template <typename T>
CObjectFreeList<T>::CObjectFreeList(HANDLE heap, bool runConstructor, bool runDestructor, int size) {

	_totalAllocList = nullptr;
	_freePtr = nullptr;

	_capacity = size;
	_usedCnt = 0;

	_heap = heap;
	_runConstructor = runConstructor;
	_runDestructor = runDestructor;
	
	// ���� ���� ����� data���� �Ÿ� ���
	stAllocNode<T> tempNode;
	_dataPtrToNodePtr = (unsigned __int64)&tempNode - (unsigned __int64)&tempNode.data;

	if (size == 0) {
		return;
	}

	for(int nodeCnt = 0; nodeCnt < size; ++nodeCnt){

		// �̸� �������� ������ŭ ��带 ����� ����

		stAllocNode<T>* newNode = (stAllocNode<T>*)HeapAlloc(_heap, 0, sizeof(stAllocNode<T>));
		new (newNode) stAllocNode<T>;
		newNode->nextPtr = _freePtr;
		_freePtr = newNode;

		{
			// ��ü alloc list�� �߰�
			// �Ҹ��ڿ��� �ϰ������� �޸� �����ϱ� ����
			stSimpleListNode* totalAllocNode = (stSimpleListNode*)HeapAlloc(_heap, 0, sizeof(stSimpleListNode));

			totalAllocNode->ptr = newNode;
			totalAllocNode->next = _totalAllocList;

			_totalAllocList = totalAllocNode;

		}

	}


}

template <typename T>
CObjectFreeList<T>::~CObjectFreeList() {

	while(_totalAllocList != nullptr){
		stAllocNode<T>* freeNode = _totalAllocList->ptr;
		HeapFree(_heap, 0, freeNode);
		_totalAllocList = _totalAllocList->next;
	}

}

template<typename T>
T* CObjectFreeList<T>::_allocObject(const wchar_t* fileName, int line) {

	stAllocNode<T>* allocNode = nullptr;
	stAllocNode<T>* nextNode = nullptr;
	stAllocNode<T>* freeNode = nullptr;

	void* freePtr = nullptr;
	void* nextPtr = nullptr;

	unsigned __int64 nodeChangeCnt = 0;
	
	_nodeChangeCnt += 1;

	do {
		// ���� ������ ����
		freePtr = _freePtr;
		nodeChangeCnt = _nodeChangeCnt;

		freeNode = toNode(freePtr);

		allocNode = freeNode;

		if (allocNode == nullptr) {

			// �߰� �Ҵ�
			allocNode = (stAllocNode<T>*)HeapAlloc(_heap, 0, sizeof(stAllocNode<T>));
			new (allocNode) stAllocNode<T>;
		
			// ��ü alloc list�� �߰�
			// �Ҹ��ڿ��� �ϰ������� �޸� �����ϱ� ����	
		
			stSimpleListNode* totalAllocNode = (stSimpleListNode*)HeapAlloc(_heap, 0, sizeof(stSimpleListNode));
			stSimpleListNode* totalAllocList = nullptr;

			do {

				totalAllocList = _totalAllocList;

				totalAllocNode->ptr = allocNode;
				totalAllocNode->next = totalAllocList;

				_totalAllocList = totalAllocNode;

			} while( InterlockedCompareExchange64((LONG64*)&_totalAllocList, (LONG64)totalAllocNode, (LONG64)totalAllocList) != (LONG64)totalAllocList );

			_capacity += 1;

			break;

		}

		nextPtr = allocNode->nextPtr;

	} while(InterlockedCompareExchange64((LONG64*)&_freePtr, (LONG64)nextPtr, (LONG64)freePtr) != (LONG64)freePtr);
	
	// ��带 ��������� üũ��
	allocNode->used = true;

	// �Ҵ� ��û�� �ҽ����ϰ� �ҽ������� �����
	allocNode->allocSourceFileName = fileName;
	allocNode->allocLine = line;
	
	InterlockedIncrement(&_usedCnt);
	
	T* data = &allocNode->data;

	// ������ ����
	if(_runConstructor == true){
		new (data) T();
	}

	return &(allocNode->data);
}

template <typename T>
int CObjectFreeList<T>::_freeObject(T* data, const wchar_t* fileName, int line) {

	stAllocNode<T>* usedNode = (stAllocNode<T>*)(((char*)data) + _dataPtrToNodePtr);
	
	#if defined(OBJECT_FREE_LIST_SAFE)
		// �ߺ� free üũ
		if(usedNode->used == false){
			CDump::crash();
		}

		// �����÷ο� üũ
		if((unsigned __int64)usedNode->overFlowCheck != 0xF9F9F9F9F9F9F9F9){
			CDump::crash();
		}

		// ����÷ο� üũ
		if((unsigned __int64)usedNode->underFlowCheck != 0xF9F9F9F9F9F9F9F9){
			CDump::crash();
		}
	#endif

	// ����� ����� �÷��׸� ����
	usedNode->used = false;

	// �Ҹ��� ����
	if(_runDestructor == true){
		data->~T();
	}

	stAllocNode<T>* freeNode;
	stAllocNode<T>* nextNode;

	void* freePtr;
	void* nextPtr;

	unsigned __int64 nodeChangeCnt = 0;

	_nodeChangeCnt += 1;

	do {

		// ���� ������ ����
		freePtr = _freePtr;
		nodeChangeCnt = _nodeChangeCnt;

		freeNode = toNode(freePtr);
	
		// free node�� �����
		usedNode->nextPtr = freePtr;

		nextNode = usedNode;
		nextPtr = toPtr(nodeChangeCnt, nextNode);

	} while(InterlockedCompareExchange64((LONG64*)&_freePtr, (LONG64)nextPtr, (LONG64)freePtr) != (LONG64)freePtr);
	
	InterlockedDecrement(&_usedCnt);

	return 0;

}

#endif