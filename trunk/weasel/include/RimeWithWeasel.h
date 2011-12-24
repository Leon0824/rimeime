#pragma once
#include <WeaselIPC.h>
#include <WeaselUI.h>

class RimeWithWeaselHandler :
	public weasel::RequestHandler
{
public:
	RimeWithWeaselHandler(weasel::UI *ui);
	virtual ~RimeWithWeaselHandler();
	virtual void Initialize();
	virtual void Finalize();
	virtual UINT FindSession(UINT session_id);
	virtual UINT AddSession(LPWSTR buffer);
	virtual UINT RemoveSession(UINT session_id);
	virtual BOOL ProcessKeyEvent(weasel::KeyEvent keyEvent, UINT session_id, LPWSTR buffer);
	virtual void FocusIn(UINT session_id);
	virtual void FocusOut(UINT session_id);
	virtual void UpdateInputPosition(RECT const& rc, UINT session_id);

private:
	void _UpdateUI(UINT session_id);
	bool _Respond(UINT session_id, LPWSTR buffer);
	void _UpdateUIStyle();
	
	weasel::UI *m_ui;  // reference
	UINT active_session;
};
