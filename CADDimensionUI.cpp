#include "CADDimension.h"
#include <custcont.h>

namespace
{
    const int kEditIDs[9] = {
        IDC_P1_X_EDIT, IDC_P1_Y_EDIT, IDC_P1_Z_EDIT,
        IDC_P2_X_EDIT, IDC_P2_Y_EDIT, IDC_P2_Z_EDIT,
        IDC_PD_X_EDIT, IDC_PD_Y_EDIT, IDC_PD_Z_EDIT
    };

    const int kSpinIDs[9] = {
        IDC_P1_X_SPIN, IDC_P1_Y_SPIN, IDC_P1_Z_SPIN,
        IDC_P2_X_SPIN, IDC_P2_Y_SPIN, IDC_P2_Z_SPIN,
        IDC_PD_X_SPIN, IDC_PD_Y_SPIN, IDC_PD_Z_SPIN
    };
}

//--------------------------------------------------------------------------
// ParamBlock2 没有 Point3 原生 spinner；用 ParamMap2UserDlgProc 管理
// 9 个分量控件，读写 point1/point2/dimPoint。
//--------------------------------------------------------------------------
class CADDimDlgProc : public ParamMap2UserDlgProc
{
    HWND             m_hWnd = nullptr;
    IParamBlock2*    m_pb = nullptr;
    ISpinnerControl* m_spin[9] = {};
    bool             m_updating = false;
    bool             m_valid = false;

    void Release()
    {
        for (int i = 0; i < 9; ++i) {
            if (m_spin[i]) {
                ReleaseISpinner(m_spin[i]);
                m_spin[i] = nullptr;
            }
        }
        m_hWnd = nullptr;
        m_valid = false;
    }

    void Init(HWND hWnd)
    {
        m_valid = true;
        for (int i = 0; i < 9; ++i) {
            m_spin[i] = GetISpinner(GetDlgItem(hWnd, kSpinIDs[i]));
            if (!m_spin[i]) {
                m_valid = false;
                continue;
            }
            m_spin[i]->SetLimits(-1.0e6f, 1.0e6f, FALSE);
            m_spin[i]->SetScale(0.1f);
            m_spin[i]->LinkToEdit(GetDlgItem(hWnd, kEditIDs[i]), EDITTYPE_FLOAT);
        }
    }

    void Refresh(TimeValue t)
    {
        if (!m_pb || !m_hWnd || !m_valid) return;

        Interval iv;
        iv.SetInfinite();
        Point3 p1, p2, pd;
        m_pb->GetValue(p_point1, t, p1, iv);
        m_pb->GetValue(p_point2, t, p2, iv);
        m_pb->GetValue(p_dimpoint, t, pd, iv);

        m_updating = true;
        m_spin[0]->SetValue(p1.x, FALSE);
        m_spin[1]->SetValue(p1.y, FALSE);
        m_spin[2]->SetValue(p1.z, FALSE);
        m_spin[3]->SetValue(p2.x, FALSE);
        m_spin[4]->SetValue(p2.y, FALSE);
        m_spin[5]->SetValue(p2.z, FALSE);
        m_spin[6]->SetValue(pd.x, FALSE);
        m_spin[7]->SetValue(pd.y, FALSE);
        m_spin[8]->SetValue(pd.z, FALSE);
        m_updating = false;
    }

    void Commit(TimeValue t)
    {
        if (!m_pb || m_updating || !m_valid) return;

        const Point3 p1(m_spin[0]->GetFVal(), m_spin[1]->GetFVal(), m_spin[2]->GetFVal());
        const Point3 p2(m_spin[3]->GetFVal(), m_spin[4]->GetFVal(), m_spin[5]->GetFVal());
        const Point3 pd(m_spin[6]->GetFVal(), m_spin[7]->GetFVal(), m_spin[8]->GetFVal());

        m_updating = true;
        m_pb->SetValue(p_point1, t, p1);
        m_pb->SetValue(p_point2, t, p2);
        m_pb->SetValue(p_dimpoint, t, pd);
        m_updating = false;
    }

public:
    INT_PTR DlgProc(TimeValue t, IParamMap2* /*map*/, HWND hWnd,
                    UINT msg, WPARAM wParam, LPARAM /*lParam*/) override
    {
        switch (msg) {
        case WM_INITDIALOG:
            m_hWnd = hWnd;
            Init(hWnd);
            Refresh(t);
            return FALSE;

        case WM_DESTROY:
            Release();
            return FALSE;

        case CC_SPINNER_CHANGE: {
            const int id = LOWORD(wParam);
            for (int i = 0; i < 9; ++i) {
                if (id == kSpinIDs[i]) {
                    Commit(t);
                    break;
                }
            }
            return FALSE;
        }

        default:
            return FALSE;
        }
    }

    void DeleteThis() override
    {
        Release();
        delete this;
    }

    void SetParamBlock(IParamBlock2* pb) override
    {
        m_pb = pb;
        if (m_hWnd) Refresh(0);
    }

    void SetThing(ReferenceTarget* /*m*/) override {}

    void Update(TimeValue t) override
    {
        Refresh(t);
    }
};

ParamMap2UserDlgProc* CreateCADDimensionDlgProc()
{
    return new CADDimDlgProc();
}
