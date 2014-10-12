#include "MemPool.h"

#include <iostream>

using namespace std;

namespace KxServer {

CMemPool::CMemPool(void):
m_AlocatedSize(0),				//�ѷ����С
m_MinAlocateSize(5),			//ÿ�����ٷ���32���ֽ�
m_WaterMark(MAX_WATER_MARK)		//���ˮλ
{
}

CMemPool::~CMemPool(void)
{
	//�ͷ��ɱ��ڴ�ط���������ڴ�
	MemMS::iterator iter;
	for (iter = m_Stub.begin(); iter != m_Stub.end(); ++iter)
	{
		set<void*>* pset = iter->second;
		set<void*>::iterator setiter = pset->begin();
		//�������е�Set
		for (; setiter != pset->end(); ++ setiter)
		{
			//�ͷ�Set�������ڴ�
			delete [] static_cast<char*>(*setiter);
		}
	}
}

void CMemPool::MemDumpInfo()
{
	cout<<"AlocatedSize "<<m_AlocatedSize/1024<<"KB"<<endl;
	for (MemMS::iterator iter = m_Stub.begin(); iter != m_Stub.end(); ++iter)
	{
		MemML::iterator itlist = m_Free.find(iter->first);
		if (itlist != m_Free.end())
		{
			int freecount = itlist->second->size();
			int stubcount = iter->second->size();
			cout<<"Block Size "<<iter->first<<" Free Count "<<
			freecount << " Free Size "<< freecount * iter->first <<
			" Stub Count "<<stubcount << " Stub Size "<< stubcount*iter->first
			<<endl;
		}
	}
}

// �����СΪsize���ڴ�,ʵ�ʷ����ڴ��СΪresultsize
void* CMemPool::MemAlocate(unsigned int size)
{
	size = MemFitSize(size);
	MemML::iterator freeiter = m_Free.find(size);
	
	//����Ҳ����ô�С�Ŀ�������
	if (m_Free.end() == freeiter)
	{
		//�����µĴ�С
		if (0 != MemExtendNewSize(size))
		{
			//�ڴ治��,����ʧ��
			return NULL;
		}
		freeiter = m_Free.find(size);
	}

	list<void*>* pfreelist = freeiter->second;
	list<void*>::iterator listiter = pfreelist->begin();
	
	//����Ѿ�û�п����ڴ���
	if (pfreelist->end() == listiter)
	{
		MemMS::iterator setiter = m_Stub.find(size);

		//��չ�µ��ڴ�
		if(0 != MemExtend(size, pfreelist, setiter->second))
		{
			return NULL;
		}

		listiter = pfreelist->begin();
		if (pfreelist->end() ==  listiter)
		{
			return NULL;
		}
	}

	void* pret = *listiter;
	//������ڴ�ӿ����������߳�
	pfreelist->pop_front();
	return pret;
}

// �����ڴ�,��MemPool������ڴ����,���յ��ڴ��СΪsize,�ɹ�����0
int CMemPool::MemRecycle(void* mem, unsigned int size)
{
	size = MemFitSize(size);

	MemMS::iterator stubiter = m_Stub.find(size);
	//�Ҳ���������������Ҵ�����
	if (m_Stub.end() == stubiter)
	{
		return -1;
	}

	//�Ҳ���������������Ҵ�����
	set<void*>* pstubset = stubiter->second;
	if(pstubset->end() == pstubset->find(mem))
	{
		return -1;
	}

	//�Ҳ�����Ӧ�Ŀ����б�
	MemML::iterator freeiter = m_Free.find(size);
	if (m_Free.end() == freeiter)
	{
		return -1;
	}

	list<void*>* pfreelist = freeiter->second;
	pfreelist->push_front(mem);

	//���ˮλ���ͷ�
	MemAutoRelease(size, pfreelist, pstubset);

	return 0;
}

//����Ҫ������ڴ���С
unsigned int CMemPool::MemFitSize(unsigned int size)
{
	//PS.���size����32-63 ���ص�size��64
	unsigned int i = m_MinAlocateSize;
	size = (size>>i);
	while(size)
	{
		size = (size>>1);
		++i;
	}
	return 1 << i;
}

//��ȡһ���Է������������
unsigned int CMemPool::MemFitCounts(unsigned int size)
{
	//��������
	if(size < MEM_SIZE_MIN)
	{
		//������
		return MEM_BASE_COUNT;
	}
	//�е�������
	else if (size < MEM_SIZE_MID)
	{
		return MEM_SIZE_BIG / size;
	}
	//��������
	else
	{
		return 1;
	}
}

//��չ�ڴ��,���ӵ�plist��ͷ��
int CMemPool::MemExtend(unsigned int size, std::list<void*>* plist, std::set<void*>* pset)
{
	unsigned int extendcounts = MemFitCounts(size);

	//���ڴ治��ʱ����Ҫ�ͷ�һЩ
	if (m_AlocatedSize + extendcounts * size >= MAX_POOL_SIZE)
	{
		//�ͷ�һЩ�����ڴ�
		if (0 != MemReleaseWithSize(m_AlocatedSize + size * extendcounts - MAX_POOL_SIZE))
		{
			return -1;
		}
	}

	//�����ڴ�
	for (unsigned int i = 0; i < extendcounts; ++i)
	{
		void* mem = new char[size];
		plist->push_front(mem);
		pset->insert(mem);
	}

	//�����ѷ���������
	m_AlocatedSize += extendcounts * size;

	return 0;
}

//��չ���ڴ��
int CMemPool::MemExtendNewSize(unsigned int size)
{
	std::list<void*>* plist = new std::list<void*>;
	std::set<void*>* pset = new std::set<void*>;

	m_Free.insert(MemML::value_type(size, plist));
	m_Stub.insert(MemMS::value_type(size, pset));

	return MemExtend(size, plist, pset);
}

//��Ҫ�ͷŶ��ٿ��ڴ�
unsigned int CMemPool::MemRelsaseCount(unsigned int size, unsigned int freecount, unsigned int stubcount)
{
	//1. ��ռ�ÿռ�̫С���ͷ�
	if (size * freecount < MEM_SIZE_MIN)
	{
		return 0;
	}

	//2. �ڴ��ϴ�
	if (size > MEM_SIZE_MID)
	{
		return (freecount > 1) ? (freecount / 2) : freecount;
	}

	//3. ���пռ����ˮλ�������������һ��
	if (freecount >= stubcount / 2
		|| freecount * size >= m_WaterMark)
	{
		return freecount / 2;
	}

	return 0;
}

int CMemPool::MemAutoRelease(unsigned size, std::list<void*>* plist, std::set<void*>* pset)
{
	//������пռ����򳬳�ˮλ,���ͷ�һЩ
	unsigned int releaseCount = MemRelsaseCount(size, plist->size(), pset->size());
	if (0 == releaseCount)
	{
		return 0;
	}

	for (unsigned int i = 0; i < releaseCount; ++i)
	{
		//�ͷ��ڴ�,���Ӵ��Set�Ϳ����б���ɾ��
		void* p = plist->front();
		pset->erase(p);
		delete [] static_cast<char*>(p);
		plist->pop_front();
	}

	m_AlocatedSize -= size * releaseCount;

	return 0;
}

int CMemPool::MemReleaseWithSize(unsigned int size)
{
	m_WaterMark = MEM_SIZE_BIG;
	unsigned int releaseSize = MAX_POOL_SIZE;
	unsigned int allocSize = m_AlocatedSize;

	//�����Ŀ�ʼ��������С���ڴ��
	while((allocSize - m_AlocatedSize) < size
		&& releaseSize >= MEM_SIZE_MIN)
	{
		releaseSize = releaseSize >> 1;
		MemMS::iterator itset = m_Stub.find(releaseSize);
		if (m_Stub.end() == itset)
		{
			continue;
		}

		MemML::iterator itlist = m_Free.find(releaseSize);
		if (m_Free.end() == itlist)
		{
			continue;
		}

		MemAutoRelease(releaseSize, itlist->second, itset->second);
	}

	m_WaterMark = MAX_WATER_MARK;
	if (allocSize - m_AlocatedSize < size)
	{
		return -1;
	}

	return 0;
}

CMemManager* CMemManager::m_Instance = NULL;

CMemManager::CMemManager()
{
    m_MemPool = new CMemPool();
}

CMemManager::~CMemManager()
{
    delete m_MemPool;
    m_MemPool = NULL;
}

CMemManager* CMemManager::GetInstance()
{
    if (NULL == m_Instance)
    {
        m_Instance = new CMemManager();
    }

    return m_Instance;
}

void CMemManager::Destroy()
{
    if (NULL != m_Instance)
    {
        delete m_Instance;
        m_Instance = NULL;
    }
}

void* CMemManager::MemAlocate(unsigned int size)
{
    return m_MemPool->MemAlocate(size);
}

int CMemManager::MemRecycle(void* mem, unsigned int size)
{
    return m_MemPool->MemRecycle(mem, size);
}

void CMemManager::MemDumpInfo()
{
    return m_MemPool->MemDumpInfo();
}


}