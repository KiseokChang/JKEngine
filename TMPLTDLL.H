#ifndef _TMPLTDLL_H
#define _TMPLTDLL_H

#include "keydef.h"


template<class T>
class DLLFrame
{
public:
	DLLFrame();
 ~DLLFrame();

	int32 GetIndex(T *node);
	int32 GetIndex(T *node,uint16 compkind,T *start=0);

	T *GetNode(uint32 index,BOOL isdetach=FALSE);
	T *GetNode(T *node,uint16 compkind,BOOL isdetach=FALSE,T *start=0);

	T *GetNodeFirst(BOOL isdetach=FALSE);
	T *GetNodeLast(BOOL isdetach=FALSE);


	BOOL PutNode(T *node,uint32 index);

	BOOL PutNodeLast(T *node);
	BOOL PutNodeFirst(T *node);

	BOOL DeleteNode(uint32 index);
	BOOL DeleteNode(T *node,uint16 compkind,T *start=0);

	T * SetNodeFirst(uint32 index);
	T * SetNodeFirst(T *node,uint16 compkind);
	T * SetNodeLast(uint32 index);
	T * SetNodeLast(T *node,uint16 compkind);

	void ClearNode();

	uint32 AppendDLLFrame(DLLFrame& plus);

	DLLFrame<T>& operator = (const DLLFrame<T>& other);
	BOOL IsEqual(const DLLFrame<T>& other,uint16 compkind);

	uint32 NodeCount;
	T *HeadNode;
	T *TailNode;

protected:
	BOOL AttachPrev(T *node,T *other);
	BOOL AttachNext(T *node,T *other);
	T *Detach(T *node);
};

template <class T>
DLLFrame<T>::DLLFrame():
	NodeCount(0),HeadNode(0),TailNode(0)
{
}

template <class T>
DLLFrame<T>::~DLLFrame()
{
	ClearNode();
}

template <class T>
int32 DLLFrame<T>::GetIndex(T *node)
{
	if(node){
		int32 count=0;
		T *temp=HeadNode;
		while(temp){
			if(temp==node) return count;
			count++;
			temp=temp->next;
		}
	}
	return -1;
}

template <class T>
int32 DLLFrame<T>::GetIndex(T *node,uint16 compkind,T *start=0)
{
	if(node) {
		int32 count=0;
		T *temp=(start? start:HeadNode);
		while(temp) {
			if(temp->IsEqual(node,compkind)) return count;
			count++;
			temp=temp->next;
		}
	}
	return -1;
}

template <class T>
T *DLLFrame<T>::GetNode(uint32 index,BOOL isdetach)
{
	if(index<NodeCount){
		uint32 count=0;
		T *temp=HeadNode;
		while(temp&&count<index) {
			temp=temp->next;
			count++;
		}
		if(temp&&isdetach){
			Detach(temp);
		}
		return temp;
	}
	return 0;
}


template <class T>
T *DLLFrame<T>::GetNode(T *node,uint16 compkind,BOOL isdetach,T *start)
{
	if(node){
		T *temp=(start? start:HeadNode);
		while(temp) {
			if(temp->IsEqual(node,compkind)) {
				if(isdetach){
					Detach(temp);
				}
				return temp;
			}
			temp=temp->next;
		}
	}
	return 0;
}

template <class T>
BOOL DLLFrame<T>::DeleteNode(uint32 index)
{
	T *temp  = GetNode(index,TRUE);
	if(temp) {
		delete temp;
		return TRUE;
	}
	return FALSE;
}

template <class T>
BOOL DLLFrame<T>::DeleteNode(T *node,uint16 compkind,T *start)
{
	T *temp  = GetNode(node,compkind,TRUE,start);
	if(temp) {
		delete temp;
		return TRUE;
	}
	return FALSE;
}

template <class T>
BOOL DLLFrame<T>::PutNode(T *node,uint32 index)
{
	if(node) {
		node->prev=node->next=0;
		T *temp=GetNode(index);
		if(temp) {
			if(!temp->prev) HeadNode=node;
			AttachPrev(temp,node);
			NodeCount++;
			return TRUE;
		}
		return PutNodeLast(node);
	}
	return FALSE;
}

template <class T>
BOOL DLLFrame<T>::PutNodeLast(T *node)
{
	if(node) {
		node->prev=node->next=0;
		if(TailNode){
			AttachNext(TailNode,node);
			TailNode=node;
		}
		else{
			HeadNode=node;
			TailNode=node;
		}
		NodeCount++;
		return TRUE;
	}
	return FALSE;
}

template <class T>
BOOL DLLFrame<T>::PutNodeFirst(T *node)
{
	if(node) {
		node->prev=node->next=0;
		if(HeadNode){
			AttachPrev(HeadNode,node);
			HeadNode=node;
		}
		else{
			HeadNode=node;
			TailNode=node;
		}
		NodeCount++;
		return TRUE;
	}
	return FALSE;
}

template <class T>
T *DLLFrame<T>::GetNodeFirst(BOOL isdetach)
{
	T *node = HeadNode;
	if(node){
		if(isdetach){
			Detach(node);
		}
	}
	return node;
}

template <class T>
T * DLLFrame<T>::GetNodeLast(BOOL isdetach)
{
	T *node = TailNode;
	if(node){
		if(isdetach){
			Detach(node);
		}
	}
	return node;
}

template <class T>
T * DLLFrame<T>::SetNodeFirst(uint32 index)
{
	T *node = GetNode(index,TRUE);
	if(node) PutNodeFirst(node);
	return node;
}

template <class T>
T * DLLFrame<T>::SetNodeFirst(T *node,uint16 compkind)
{
	T *temp = GetNode(node,compkind);
	if(temp) PutNodeFirst(temp);
	return temp;
}

template <class T>
T * DLLFrame<T>::SetNodeLast(uint32 index)
{
	T *node = GetNode(index,TRUE);
	if(node) PutNodeLast(node);
	return node;
}

template <class T>
T * DLLFrame<T>::SetNodeLast(T *node,uint16 compkind)
{
	T *temp = GetNode(node,compkind);
	if(temp) PutNodeLast(temp);
	return temp;
}

template <class T>
void DLLFrame<T>::ClearNode()
{
	T *node = TailNode,*temp;
	while(node) {
		temp=node;
		node=node->prev;
		delete temp;
	}
	NodeCount=0;
	TailNode=HeadNode=0;
}

template <class T>
uint32 DLLFrame<T>::AppendDLLFrame(DLLFrame<T>& plus)
{
	if(plus.HeadNode){
		if(TailNode){
			TailNode->next=plus.HeadNode;
			plus.HeadNode->prev=TailNode;
			TailNode=plus.TailNode;
			NodeCount+=plus.NodeCount;
		}
		else{
			HeadNode=plus.HeadNode;
			TailNode=plus.TailNode;
			NodeCount=plus.NodeCount;
		}
		plus.HeadNode=plus.TailNode=0;
		plus.NodeCount=0;
	}
	return NodeCount;
}

template <class T>
DLLFrame<T>& DLLFrame<T>::operator = (const DLLFrame<T>& other)
{
	ClearNode();
	T *node = other.HeadNode;
	while(node) {
		PutNodeLast(node->Duplicate());
		node=node->next;
	}
	return *this;
}

template <class T>
BOOL DLLFrame<T>::IsEqual(const DLLFrame<T>& other,uint16 compkind)
{
	if(NodeCount!=other.NodeCount) return FALSE;
	T *thisnode=HeadNode,*othernode=other.HeadNode;
	while(thisnode&&othernode){
		if(!thisnode->IsEqual(othernode,compkind)) return FALSE;
		thisnode=thisnode->next;
		othernode=othernode->next;
	}
	return (!thisnode&&!othernode)? TRUE:FALSE;
}

//=================================================================
template <class T>
BOOL DLLFrame<T>::AttachPrev(T *node,T *other)
{
	if(node&&other){
		other->prev=node->prev;
		other->next=node;
		if(node->prev) node->prev->next=other;
		node->prev=other;
		return TRUE;
	}
	return FALSE;
}
template <class T>
BOOL DLLFrame<T>::AttachNext(T *node,T *other)
{
	if(node&&other){
		other->next=node->next;
		other->prev=node;
		if(node->next) node->next->prev=other;
		node->next=other;
		return TRUE;
	}
	return FALSE;
}
template <class T>
T *DLLFrame<T>::Detach(T *node)
{
	if(node){
		if(node->prev) node->prev->next=node->next;
		else HeadNode=node->next;
		if(node->next) node->next->prev=node->prev;
		else TailNode=node->prev;
		NodeCount--;
	}
	return node;
}

#endif