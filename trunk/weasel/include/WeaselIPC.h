#pragma once
#include <WeaselCommon.h>
#include <windows.h>
#include <boost/function.hpp>

#define WEASEL_IPC_WINDOW L"WeaselIPCWindow_1.0"
#define WEASEL_IPC_SHARED_MEMORY "WeaselIPCSharedMemory_1.0"

#define WEASEL_IPC_METADATA_SIZE 1024
#define WEASEL_IPC_BUFFER_SIZE (4 * 1024)
#define WEASEL_IPC_BUFFER_LENGTH (WEASEL_IPC_BUFFER_SIZE / sizeof(WCHAR))
#define WEASEL_IPC_SHARED_MEMORY_SIZE (WEASEL_IPC_METADATA_SIZE + WEASEL_IPC_BUFFER_SIZE)

enum WEASEL_IPC_COMMAND
{	
	WEASEL_IPC_ECHO = (WM_APP + 1),
	WEASEL_IPC_START_SESSION,
	WEASEL_IPC_END_SESSION,
	WEASEL_IPC_PROCESS_KEY_EVENT,
	WEASEL_IPC_SHUTDOWN_SERVER,
	WEASEL_IPC_FOCUS_IN,
	WEASEL_IPC_FOCUS_OUT,
	WEASEL_IPC_UPDATE_INPUT_POS,
	WEASEL_IPC_LAST_COMMAND
};

namespace weasel
{

	struct IPCMetadata
	{
		enum { WINDOW_CLASS_LENGTH = 64 };
		UINT32 server_hwnd;
		WCHAR server_window_class[WINDOW_CLASS_LENGTH];
	};

	struct KeyEvent
	{
		UINT keycode : 16;
		UINT mask : 16;
		KeyEvent() : keycode(0), mask(0) {}
		KeyEvent(UINT _keycode, UINT _mask) : keycode(_keycode), mask(_mask) {}
		KeyEvent(UINT x)
		{
			*reinterpret_cast<UINT*>(this) = x;
		}
		operator UINT32 const() const
		{
			return *reinterpret_cast<UINT32 const*>(this);
		}
	};

	// ̎��Ո��֮���
	struct RequestHandler
	{
		RequestHandler() {}
		virtual ~RequestHandler() {}
		virtual void Initialize() {}
		virtual void Finalize() {}
		virtual UINT FindSession(UINT session_id) { return 0; }
		virtual UINT AddSession(LPWSTR buffer) { return 0; }
		virtual UINT RemoveSession(UINT session_id) { return 0; }
		virtual BOOL ProcessKeyEvent(KeyEvent keyEvent, UINT session_id, LPWSTR buffer) { return FALSE; }
		virtual void FocusIn(UINT session_id) {}
		virtual void FocusOut(UINT session_id) {}
		virtual void UpdateInputPosition(RECT const& rc, UINT session_id) {}
	};
	
	// ̎��server�˻ؑ�֮���
	typedef boost::function<bool (LPWSTR buffer, UINT length)> ResponseHandler;
	
	// �¼�̎����
	typedef boost::function<bool ()> CommandHandler;

	// ���ӷ����M��֮���
	typedef CommandHandler ServerLauncher;


	// IPC���F���

	class ClientImpl;
	class ServerImpl;

	// IPC�ӿ��

	class Client
	{
	public:

		Client();
		virtual ~Client();
		
		// ���ӵ����񣬱�Ҫʱ�����������
		bool Connect(ServerLauncher launcher = 0);
		// �Ͽ�����
		void Disconnect();
		// ��ֹ����
		void ShutdownServer();
		// �l���Ԓ
		void StartSession();
		// �Y����Ԓ
		void EndSession();
		// ��������
		bool Echo();
		// �������������Ϣ
		bool ProcessKeyEvent(KeyEvent const& keyEvent);
		// ��������λ��
		void UpdateInputPosition(RECT const& rc);
		// ���봰�ڻ�ý���
		void FocusIn();
		// ���봰��ʧȥ����
		void FocusOut();
		// ��ȡserver���ص�����
		bool GetResponseData(ResponseHandler handler);

	private:
		ClientImpl* m_pImpl;
	};

	class Server
	{
	public:
		Server();
		virtual ~Server();

		// ��ʼ������
		int Start();
		// ��������
		int Stop();
		// ��Ϣѭ��
		int Run();

		void SetRequestHandler(RequestHandler* pHandler);
		void AddMenuHandler(UINT uID, CommandHandler handler);
		HWND GetHWnd();

	private:
		ServerImpl* m_pImpl;
	};

}