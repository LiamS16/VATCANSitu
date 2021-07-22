#include "pch.h"
#include "SituPlugin.h"
#include "CSiTRadar.h"
#include "constants.h"
#include "ACTag.h"

const int TAG_ITEM_IFR_REL = 5000;
const int TAG_FUNC_IFR_REL_REQ = 5001;
const int TAG_FUNC_IFR_RELEASED = 5002;

bool held = false;
size_t jurisdictionIndex = 0;

HHOOK appHook;

int ParseKeyBoardPress(LPARAM lParam) {

        return -1;

}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {

    // initialize a keyboard event to send back to ES
    INPUT ip{};
    ip.type = INPUT_KEYBOARD;
    ip.ki.time = 0;
    ip.ki.dwExtraInfo = 0;
    ip.ki.dwFlags = KEYEVENTF_SCANCODE;
    ip.ki.wVk = 0;

    if (
        wParam == VK_F1 ||
        wParam == VK_F3 ||
        wParam == VK_F9 ||
        wParam == VK_RETURN
        ) {

        if (!(lParam & 0x40000000)) { // if bit 30 is 0 this will evaluate true means key was previously up
            return -1;
        }
        else { // if key was previously down
            if (!(lParam & 0x80000000)) { // if bit 31 is 0 this will evaluate true, which means key is being pressed
                if (!(lParam & 0x0000ffff)) {  // if no repeats
                    return -1;
                }
                else {
                    held = true;
                    // Long Press Keyboard Commands will send the function direct to ES



                    // *** END LONG PRESS COMMANDS ***
                    return 0;
                }
            }
            else {
                if (held == false) {
                    // START OF SHORT PRESS KEYBOARD COMMANDS ***
                    switch (wParam) {
                    case VK_F1: {

                        // Toggle on hand-off mode
                        CSiTRadar::menuState.handoffMode = TRUE;

                        // ASEL the next aircraft in the handoff priority list
                        if (!CSiTRadar::menuState.jurisdictionalAC.empty()) {
                            CSiTRadar::m_pRadScr->GetPlugIn()->SetASELAircraft(CSiTRadar::m_pRadScr->GetPlugIn()->FlightPlanSelect(CSiTRadar::menuState.jurisdictionalAC.at(jurisdictionIndex).c_str()));
                            
                            // loop through the jurisdictional aircraft
                            if (jurisdictionIndex < CSiTRadar::menuState.jurisdictionalAC.size()-1) {
                                jurisdictionIndex++;
                            }
                            else { 
                                jurisdictionIndex = 0; 
                                CSiTRadar::menuState.handoffMode = FALSE;
                            }
                        }

                        // For the active aircraft toggle on the box drawing


                        ip.ki.wScan = 0x4E; //  scancode for numpad plus http://www.philipstorr.id.au/pcbook/book3/scancode.htm

                        SendInput(1, &ip, sizeof(INPUT));
                        ip.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
                        SendInput(1, &ip, sizeof(INPUT));

                        held = false;
                        return -1;
                    }

                    case VK_F3:
                    {
                        CSiTRadar::menuState.ptlAll = !CSiTRadar::menuState.ptlAll;
                        CSiTRadar::m_pRadScr->RequestRefresh();

                        held = false;
                        return -1;
                    }
                    case VK_F9:
                    {

                        if (CSiTRadar::menuState.filterBypassAll == FALSE) {
                            CSiTRadar::menuState.filterBypassAll = TRUE;

                            for (auto& p : CSiTRadar::mAcData) {
                                CSiTRadar::tempTagData[p.first] = p.second.tagType;
                                // Do not open uncorrelated tags
                                if (p.second.tagType == 0) {
                                    p.second.tagType = 1;
                                }
                            }

                        }
                        else if (CSiTRadar::menuState.filterBypassAll == TRUE) {

                            for (auto& p : CSiTRadar::tempTagData) {
                                // prevents closing of tags that became under your jurisdiction during quicklook
                                if (!CSiTRadar::m_pRadScr->GetPlugIn()->FlightPlanSelect(p.first.c_str()).GetTrackingControllerIsMe()) {
                                    CSiTRadar::mAcData[p.first].tagType = p.second;
                                }
                            }

                            CSiTRadar::tempTagData.clear();
                            CSiTRadar::menuState.filterBypassAll = FALSE;
                        }

                        CSiTRadar::m_pRadScr->RequestRefresh();

                        held = false;
                        return -1;
                    }
                    // *** END OF SHORT KEYBOARD PRESS COMMANDS ***
                    }          
                }
                held = false;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

SituPlugin::SituPlugin()
	: EuroScopePlugIn::CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
		"VATCANSitu",
		"0.4.4.6",
		"Ron Yan",
		"Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)")
{
    RegisterTagItemType("IFR Release", TAG_ITEM_IFR_REL);
    RegisterTagItemFunction("Request IFR Release", TAG_FUNC_IFR_REL_REQ);
    RegisterTagItemFunction("Grant IFR Release", TAG_FUNC_IFR_RELEASED);

    DWORD appProc = GetCurrentThreadId();
    appHook = SetWindowsHookEx(WH_KEYBOARD, KeyboardProc, NULL, appProc);
}

SituPlugin::~SituPlugin()
{
    UnhookWindowsHookEx(appHook);
}

EuroScopePlugIn::CRadarScreen* SituPlugin::OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated)
{
    return new CSiTRadar;
}

void SituPlugin::OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan,
    EuroScopePlugIn::CRadarTarget RadarTarget,
    int ItemCode,
    int TagData,
    char sItemString[16],
    int* pColorCode,
    COLORREF* pRGB,
    double* pFontSize) {

    if (ItemCode == TAG_ITEM_IFR_REL) {

        strcpy_s(sItemString, 16, "\u00AC");
         *pColorCode = TAG_COLOR_RGB_DEFINED;
         COLORREF c = C_PPS_ORANGE;
         *pRGB = c;

        if (strncmp(FlightPlan.GetControllerAssignedData().GetScratchPadString(), "RREQ", 4) == 0) {
            COLORREF c = C_PPS_ORANGE;
            strcpy_s(sItemString, 16, "\u00A4");
            *pRGB = c;
        }
        if (strncmp(FlightPlan.GetControllerAssignedData().GetScratchPadString(), "RREL", 4) == 0) {
            strcpy_s(sItemString, 16, "\u00A4");
            COLORREF c = RGB(9, 171, 0);
            *pRGB = c;
        }
    }

}

inline void SituPlugin::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
    CFlightPlan fp;
    fp = FlightPlanSelectASEL();
    string spString = fp.GetControllerAssignedData().GetScratchPadString();

    if (FunctionId == TAG_FUNC_IFR_REL_REQ) {
        if (strncmp(spString.c_str(), "RREQ", 4) == 0) {
            fp.GetControllerAssignedData().SetScratchPadString("");
            if (spString.size() > 4) { fp.GetControllerAssignedData().SetScratchPadString(spString.substr(5).c_str()); }
        }
        else if (strncmp(spString.c_str(), "RREL", 4) == 0) {
            fp.GetControllerAssignedData().SetScratchPadString("");
            if (spString.size() > 4) { fp.GetControllerAssignedData().SetScratchPadString(spString.substr(5).c_str()); }
        }
        else {
            fp.GetControllerAssignedData().SetScratchPadString(("RREQ " + spString).c_str());
        }

    }
    if (FunctionId == TAG_FUNC_IFR_RELEASED) {

        // Only allow if APP, DEP or CTR
        if (ControllerMyself().GetFacility() >= 5) {

            if (strncmp(fp.GetControllerAssignedData().GetScratchPadString(), "RREQ", 4) == 0) {

                if (spString.size() > 4) {
                    fp.GetControllerAssignedData().SetScratchPadString(("RREL " + spString.substr(5)).c_str());
                }
                else {
                    fp.GetControllerAssignedData().SetScratchPadString("RREL");
                }
            }
        }
    }
}