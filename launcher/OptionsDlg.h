#pragma once

#include <launcher_common/VideoDeviceInfoProvider.h>
#include <common/config/GameConfig.h>
#include <wxx_wincore.h>
#include <wxx_dialog.h>
#include <wxx_controls.h>

class OptionsDlg : public CDialog
{
public:
	OptionsDlg();

protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    BOOL OnCommand(WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotify(WPARAM wparam, LPARAM lparam) override;

private:
    void InitToolTip();
    void UpdateAdapterCombo();
    void UpdateResolutionCombo();
    void UpdateColorDepthRadioButtons();
    void UpdateMsaaCombo();
    void UpdateAnisotropyCheckbox();
    void OnBnClickedOk();
    void OnBnClickedExeBrowse();
    void OnBnClickedResetTrackerBtn();
    void OnForcePortClick();
    void OnAdapterChange();
    void OnResolutionChange();
    void OnColorDepthChange();
    void OnWindowModeChange();

private:
    VideoDeviceInfoProvider m_video_info;
    std::vector<unsigned> m_multi_sample_types;
    CToolTip m_tool_tip;
    GameConfig m_conf;
    CComboBox m_adapter_combo;
    CComboBox m_res_combo;
    CComboBox m_msaa_combo;
    CComboBox m_lang_combo;
};
