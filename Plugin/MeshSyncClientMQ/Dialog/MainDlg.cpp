#define _CRT_SECURE_NO_WARNINGS
#include "stdafx.h"
#include <string>
#include <memory>
#include "MainDlg.h"

CAppModule _Module;
CMessageLoop theLoop;

std::string& GetServer(MeshSyncClientPlugin *plugin);
uint16_t& GetPort(MeshSyncClientPlugin *plugin);
float& GetScaleFactor(MeshSyncClientPlugin *plugin);
bool& GetAutoSync(MeshSyncClientPlugin *plugin);
bool& GetSyncVertexColor(MeshSyncClientPlugin *plugin);
bool& GetSyncCamera(MeshSyncClientPlugin *plugin);
std::string& GetCameraPath(MeshSyncClientPlugin *plugin);
bool& GetBakeSkin(MeshSyncClientPlugin *plugin);
bool& GetBakeCloth(MeshSyncClientPlugin *plugin);
void SendAll(MeshSyncClientPlugin *plugin);
void SendCamera(MeshSyncClientPlugin *plugin);
void Import(MeshSyncClientPlugin *plugin);
void CloseWindow(MeshSyncClientPlugin *plugin);


CMainDlg::CMainDlg(MeshSyncClientPlugin *plugin)
    : m_plugin(plugin)
{
}

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg)
{
    return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnIdle()
{
    UIUpdateChildWindows();
    return FALSE;
}

LRESULT CMainDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    // center the dialog on the screen
    CenterWindow();

    // register object for message filtering and idle updates
    CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != NULL);
    pLoop->AddMessageFilter(this);
    pLoop->AddIdleHandler(this);

    UIAddChildWindowContainer(m_hWnd);

    m_edit_server.Attach(GetDlgItem(IDC_EDIT_SERVER));
    m_edit_port.Attach(GetDlgItem(IDC_EDIT_PORT));
    m_edit_scale.Attach(GetDlgItem(IDC_EDIT_SCALEFACTOR));
    m_check_vcolor.Attach(GetDlgItem(IDC_CHECK_VCOLOR));
    m_check_camera.Attach(GetDlgItem(IDC_CHECK_CAMERA));
    m_edit_camera_path.Attach(GetDlgItem(IDC_EDIT_CAMERA_PATH));
    m_check_autosync.Attach(GetDlgItem(IDC_CHECK_AUTOSYNC));
    m_check_bake_skin.Attach(GetDlgItem(IDC_CHECK_BAKE_SKIN));
    m_check_bake_cloth.Attach(GetDlgItem(IDC_CHECK_BAKE_CLOTH));

    char buf[256];
    m_edit_server.SetWindowText(GetServer(m_plugin).c_str());
    sprintf(buf, "%d", (int)GetPort(m_plugin));
    m_edit_port.SetWindowText(buf);
    sprintf(buf, "%.3f", GetScaleFactor(m_plugin));
    m_edit_scale.SetWindowText(buf);
    m_check_vcolor.SetCheck(GetSyncVertexColor(m_plugin));
    m_check_camera.SetCheck(GetSyncCamera(m_plugin));
    m_edit_camera_path.SetWindowText(GetCameraPath(m_plugin).c_str());
    m_check_autosync.SetCheck(GetAutoSync(m_plugin));
    m_check_bake_skin.SetCheck(GetBakeSkin(m_plugin));
    m_check_bake_cloth.SetCheck(GetBakeCloth(m_plugin));

    return TRUE;
}

LRESULT CMainDlg::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
    // unregister message filtering and idle updates
    CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != NULL);
    pLoop->RemoveMessageFilter(this);
    pLoop->RemoveIdleHandler(this);

    return 0;
}

LRESULT CMainDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    CloseDialog(wID);
    return 0;
}

void CMainDlg::CloseDialog(int nVal)
{
    ShowWindow(SW_HIDE);
    CloseWindow(m_plugin);
}


LRESULT CMainDlg::OnEnChangeEditServer(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
    char buf[1024];
    ::GetWindowTextA(hWndCtl, buf, sizeof(buf));
    GetServer(m_plugin) = buf;
    return 0;
}


LRESULT CMainDlg::OnEnChangeEditPort(WORD /*wNotifyCode*/, WORD /*wID*/, HWND hWndCtl, BOOL& /*bHandled*/)
{
    char buf[1024];
    ::GetWindowTextA(hWndCtl, buf, sizeof(buf));
    GetPort(m_plugin) = std::atoi(buf);
    return 0;
}

LRESULT CMainDlg::OnEnChangeScaleFactor(WORD, WORD, HWND hWndCtl, BOOL &)
{
    char buf[1024];
    ::GetWindowTextA(hWndCtl, buf, sizeof(buf));
    auto scale = std::atof(buf);
    if (scale != 0.0) {
        GetScaleFactor(m_plugin) = (float)scale;
        SendAll(m_plugin);
    }
    return 0;
}

LRESULT CMainDlg::OnBnClickedCheckVcolor(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    GetSyncVertexColor(m_plugin) = m_check_vcolor.GetCheck() != 0;
    SendAll(m_plugin);
    return 0;
}

LRESULT CMainDlg::OnBnClickedCheckCamera(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    GetSyncCamera(m_plugin) = m_check_camera.GetCheck() != 0;
    if (m_check_camera.GetCheck() != 0) {
        SendCamera(m_plugin);
    }
    return 0;
}

LRESULT CMainDlg::OnEnChangeCameraPath(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    GetCameraPath(m_plugin) = m_check_camera.GetCheck() != 0;
    SendCamera(m_plugin);
    return 0;
}

LRESULT CMainDlg::OnBnClickedCheckAutosync(WORD /*wNotifyCode*/, WORD wID, HWND hWndCtl, BOOL& /*bHandled*/)
{
    GetAutoSync(m_plugin) = m_check_autosync.GetCheck() != 0;
    return 0;
}


LRESULT CMainDlg::OnBnClickedButtonSync(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    SendAll(m_plugin);
    return 0;
}

LRESULT CMainDlg::OnBnClickedCheckBakeSkin(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    GetBakeSkin(m_plugin) = m_check_bake_skin.GetCheck() != 0;
    return 0;
}
LRESULT CMainDlg::OnBnClickedCheckBakeCloth(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    GetBakeCloth(m_plugin) = m_check_bake_cloth.GetCheck() != 0;
    return 0;
}

LRESULT CMainDlg::OnBnClickedButtonImport(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
    Import(m_plugin);
    return 0;
}


//---------------------------------------------------------------------------
//  DllMain
//---------------------------------------------------------------------------
BOOL APIENTRY DllMain(HINSTANCE hInstance,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    if (ul_reason_for_call == DLL_PROCESS_ATTACH)
    {
        HRESULT hRes = ::CoInitialize(NULL);
        // If you are running on NT 4.0 or higher you can use the following call instead to 
        // make the EXE free threaded. This means that calls come in on a random RPC thread.
        //	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
        ATLASSERT(SUCCEEDED(hRes));

        // this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
        ::DefWindowProc(NULL, 0, 0, 0L);

        AtlInitCommonControls(ICC_COOL_CLASSES | ICC_BAR_CLASSES);	// add flags to support other controls

        hRes = _Module.Init(NULL, hInstance);
        ATLASSERT(SUCCEEDED(hRes));

        _Module.AddMessageLoop(&theLoop);

        return SUCCEEDED(hRes);
    }

    if (ul_reason_for_call == DLL_PROCESS_DETACH)
    {
        _Module.RemoveMessageLoop();

        _Module.Term();
        ::CoUninitialize();
    }

    return TRUE;
}


static std::unique_ptr<CMainDlg> g_dlg;

void InitializeSettingDialog(MeshSyncClientPlugin *plugin, HWND parent)
{
    g_dlg.reset(new CMainDlg(plugin));
    g_dlg->Create(parent);
    //g_dlg->SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void FinalizeSettingDialog()
{
    if (g_dlg) {
        g_dlg->DestroyWindow();
        g_dlg.reset();
    }
}

void ShowSettingDialog(bool show)
{
    if (g_dlg) {
        g_dlg->ShowWindow(show ? SW_SHOW : SW_HIDE);
    }
}

bool IsSettingDialogActive()
{
    return g_dlg && g_dlg->IsWindowVisible();
}
