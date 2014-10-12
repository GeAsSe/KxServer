#include <iostream>
#include <vector>
#include <algorithm>
#include "KXServer.h"

using namespace std;
using namespace KxServer;

//���ݴ�������
class CMyModule : public IBaseModule
{
	//����Э�鳤�ȣ�ÿ����ǰ��һ��int��ʾ������
    virtual int RequestLen(char* buffer, unsigned int len)
    {
        //����Э�鳤��
        if (len < sizeof(int))
        {
            return sizeof(int);
        }
        else
        {
			//�ײ����ư���󳤶�Ϊ MAX_PKGLEN 1<<16
            return *(int*)(buffer);
        }
    }

	void Process(char* buffer, unsigned int len, ICommunication *target)
	{
		//TODO: ���յ��������ݺ�Ĵ���
		char* str = buffer + 4;
		cout << "Server Recv Size "<< len << endl
			<< "Data " << str << endl;

		//ԭ�����ظ��ͻ���
		target->Send(buffer, len);
	}
};

class CMyModule2 : public IBaseModule
{
    virtual int RequestLen(char* buffer, unsigned int len)
    {
        //����Э�鳤��
        if (len < sizeof(int))
        {
            return sizeof(int);
        }
        else
        {
            return *(int*)(buffer);
        }
    }

    void Process(char* buffer, unsigned int len, ICommunication *target)
    {
		//�ͻ����յ���������Ӧ����ӡ���ݳ���
		char* str = buffer + 4;
		cout<<"Client Recv : " << str << endl;
    }
};

class CTestServer : public CBaseServer
{
public:
	bool ServerInit()
	{
		cout << "Server Start" << endl;
		m_Poller = new CSelectPoller();

		m_Listener = new CTCPListener(5555, NULL);
        m_Listener->SetClientModule(new CMyModule());
		m_Poller->AddPollObject(m_Listener, POLLTYPE_IN);
        
		m_Connecter = new CTCPConnector("127.0.0.1", 5555, m_Poller);
		m_Connecter->SetModule(new CMyModule2());
		m_Poller->AddPollObject(m_Connecter, POLLTYPE_IN);

		char buff[128];
		memset(buff, 0, sizeof(buff));
		char* text = "goooooooooogle";
		int len =  4 + strlen(text) + 1;
		*(int*)(buff) = len;
		strcpy(buff + 4, text);

		m_Connecter->Send(buff, len);

		return true;
	}

	void ServerUninit()
	{
		//PS.�������ͷ���CMyModule��CMyModule2
		delete m_Listener;
		delete m_Connecter;
		delete m_Poller;
	}

private:
	CTCPListener* m_Listener;
	CTCPConnector* m_Connecter;
};

int main()
{
	CTestServer* server = new CTestServer();
	server->ServerStart();
	delete server;

	return 0;
}
