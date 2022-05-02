#include "ConfigFan.h"
#include "MonHelper.h"
#include "alienfan-SDK.h"

#define DRAG_ZONE 2

extern ConfigFan* fan_conf;
extern MonHelper* mon;
extern HWND fanWindow, tipWindow;
extern AlienFan_SDK::Control* acpi;

extern HWND CreateToolTip(HWND hwndParent, HWND oldTip);
extern HWND toolTip;

fan_point* lastFanPoint = NULL;

void SetTooltip(HWND tt, int x, int y) {
    TOOLINFO ti{ 0 };
    ti.cbSize = sizeof(ti);
    if (tt) {
        SendMessage(tt, TTM_ENUMTOOLS, 0, (LPARAM)&ti);
        string toolTip = "Temp: " + to_string(x) + ", Boost: " + to_string(y);
        ti.lpszText = (LPTSTR)toolTip.c_str();
        SendMessage(tt, TTM_SETTOOLINFO, 0, (LPARAM)&ti);
    }
}

void DrawFan(int oper = 0, int xx = -1, int yy = -1)
{
    if (fanWindow) {
        RECT clirect, graphZone;
        GetClientRect(fanWindow, &clirect);
        graphZone = clirect;
        clirect.right -= 1;
        clirect.bottom -= 1;

        switch (oper) {
        case 2:// tooltip, no redraw
            SetTooltip(toolTip, 100 * (xx - clirect.left) / (clirect.right - clirect.left),
                100 * (clirect.bottom - yy) / (clirect.bottom - clirect.top));
            return;
        case 1:// show tooltip
            SetTooltip(toolTip, 100 * (xx - clirect.left) / (clirect.right - clirect.left),
                100 * (clirect.bottom - yy) / (clirect.bottom - clirect.top));
            break;
        }

        HDC hdc_r = GetDC(fanWindow);

        // Double buff...
        HDC hdc = CreateCompatibleDC(hdc_r);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc_r, graphZone.right - graphZone.left, graphZone.bottom - graphZone.top);

        SetBkMode(hdc, TRANSPARENT);

        HGDIOBJ hOld = SelectObject(hdc, hbmMem);

        SetDCPenColor(hdc, RGB(127, 127, 127));
        SelectObject(hdc, GetStockObject(DC_PEN));
        for (int x = 0; x < 11; x++)
            for (int y = 0; y < 11; y++) {
                int cx = x * (clirect.right - clirect.left) / 10 + clirect.left,
                    cy = y * (clirect.bottom - clirect.top) / 10 + clirect.top;
                MoveToEx(hdc, cx, clirect.top, NULL);
                LineTo(hdc, cx, clirect.bottom);
                MoveToEx(hdc, clirect.left, cy, NULL);
                LineTo(hdc, clirect.right, cy);
            }

        if (fan_conf->lastSelectedFan != -1) {
            // curve...
            temp_block* sen = fan_conf->FindSensor(fan_conf->lastSelectedSensor);
            fan_block* fan = NULL;
            for (auto senI = fan_conf->lastProf->fanControls.begin();
                senI < fan_conf->lastProf->fanControls.end(); senI++)
                if (fan = fan_conf->FindFanBlock(&(*senI), fan_conf->lastSelectedFan)) {
                    // draw fan curve
                    if (sen && sen->sensorIndex == senI->sensorIndex)
                        SetDCPenColor(hdc, RGB(0, 255, 0));
                    else
                        SetDCPenColor(hdc, RGB(255, 255, 0));
                    SelectObject(hdc, GetStockObject(DC_PEN));
                    // First point
                    MoveToEx(hdc, clirect.left, clirect.bottom, NULL);
                    for (int i = 0; i < fan->points.size(); i++) {
                        int cx = fan->points[i].temp * (clirect.right - clirect.left) / 100 + clirect.left,
                            cy = (100 - fan->points[i].boost) * (clirect.bottom - clirect.top) / 100 + clirect.top;
                        LineTo(hdc, cx, cy);
                        Ellipse(hdc, cx - 2, cy - 2, cx + 2, cy + 2);
                    }
                    // Yellow dots
                    if (sen && senI->sensorIndex != sen->sensorIndex) {
                        SetDCPenColor(hdc, RGB(255, 255, 0));
                        SetDCBrushColor(hdc, RGB(255, 255, 0));
                        SelectObject(hdc, GetStockObject(DC_PEN));
                        SelectObject(hdc, GetStockObject(DC_BRUSH));
                        POINT mark;
                        mark.x = acpi->GetTempValue(senI->sensorIndex) * (clirect.right - clirect.left) / 100 + clirect.left;
                        mark.y = (100 - acpi->GetFanValue(fan_conf->lastSelectedFan)) * (clirect.bottom - clirect.top) / 100 + clirect.top;
                        Ellipse(hdc, mark.x - 3, mark.y - 3, mark.x + 3, mark.y + 3);
                    }
                }
            int percent;
            // Red dot
            SetDCPenColor(hdc, RGB(255, 0, 0));
            SetDCBrushColor(hdc, RGB(255, 0, 0));
            SelectObject(hdc, GetStockObject(DC_PEN));
            SelectObject(hdc, GetStockObject(DC_BRUSH));
            POINT mark;
            mark.x = acpi->GetTempValue(fan_conf->lastSelectedSensor) * (clirect.right - clirect.left) / 100 + clirect.left;
            mark.y = (100 - acpi->GetFanValue(fan_conf->lastSelectedFan)) * (clirect.bottom - clirect.top) / 100 + clirect.top;
            Ellipse(hdc, mark.x - 3, mark.y - 3, mark.x + 3, mark.y + 3);
            // RPM
            string rpmText = "Fan curve (scale: " + to_string(fan_conf->boosts[fan_conf->lastSelectedFan].maxBoost) + ", boost: " + to_string(acpi->GetFanValue(fan_conf->lastSelectedFan)) +
                ", " + to_string((percent = acpi->GetFanPercent(fan_conf->lastSelectedFan)) > 100 ? 0 : percent < 0 ? 0 : percent) +
                "%)";
            SetWindowText(tipWindow, rpmText.c_str());
        }

        BitBlt(hdc_r, 0, 0, graphZone.right - graphZone.left, graphZone.bottom - graphZone.top, hdc, 0, 0, SRCCOPY);

        // Free-up the off-screen DC
        SelectObject(hdc, hOld);

        DeleteObject(hbmMem);
        DeleteDC(hdc);
        ReleaseDC(fanWindow, hdc_r);
        DeleteDC(hdc_r);
    }
}

INT_PTR CALLBACK FanCurve(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    fan_block* cFan = fan_conf->FindFanBlock(fan_conf->FindSensor(fan_conf->lastSelectedSensor), fan_conf->lastSelectedFan);
    RECT cArea;
    GetClientRect(hDlg, &cArea);
    cArea.right -= 1;
    cArea.bottom -= 1;

    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);
        DrawFan();
        EndPaint(hDlg, &ps);
        return true;
    } break;
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam), y = GET_Y_LPARAM(lParam);

        if (lastFanPoint && wParam & MK_LBUTTON) {
            int temp = (100 * (GET_X_LPARAM(lParam) - cArea.left)) / (cArea.right - cArea.left),
                boost = (100 * (cArea.bottom - GET_Y_LPARAM(lParam))) / (cArea.bottom - cArea.top);
            lastFanPoint->temp = temp < 0 ? 0 : temp > 100 ? 100 : temp;
            lastFanPoint->boost = boost < 0 ? 0 : boost > 100 ? 100 : boost;
            DrawFan(1, x, y);
        }
        else
            DrawFan(2, x, y);
    } break;
    case WM_LBUTTONDOWN:
    {
        SetCapture(hDlg);
        if (cFan) {
            // check and add point
            int temp = (100 * (GET_X_LPARAM(lParam) - cArea.left)) / (cArea.right - cArea.left),
                boost = (100 * (cArea.bottom - GET_Y_LPARAM(lParam))) / (cArea.bottom - cArea.top);
            for (vector<fan_point>::iterator fPi = cFan->points.begin();
                fPi < cFan->points.end(); fPi++) {
                if (fPi->temp - DRAG_ZONE <= temp && fPi->temp + DRAG_ZONE >= temp) {
                    // Starting drag'n'drop...
                    lastFanPoint = &(*fPi);
                    break;
                }
                if (fPi->temp > temp) {
                    // Insert point here...
                    lastFanPoint = &(*cFan->points.insert(fPi, { (short)temp, (short)boost }));
                    break;
                }
            }
            DrawFan();
        }
    } break;
    case WM_LBUTTONUP: {
        ReleaseCapture();
        // re-sort and de-duplicate array.
        if (cFan) {
            for (vector<fan_point>::iterator fPi = cFan->points.begin();
                fPi < cFan->points.end() - 1; fPi++) {
                if (fPi->temp > (fPi + 1)->temp) {
                    fan_point t = *fPi;
                    *fPi = *(fPi + 1);
                    *(fPi + 1) = t;
                }
                if (fPi->temp == (fPi + 1)->temp && fPi->boost == (fPi + 1)->boost)
                    cFan->points.erase(fPi + 1);
            }
            cFan->points.front().temp = 0;
            cFan->points.back().temp = 100;
            DrawFan();
            fan_conf->Save();
        }
        SetFocus(GetParent(hDlg));
    } break;
    case WM_RBUTTONUP: {
        // remove point from curve...
        if (cFan && cFan->points.size() > 2) {
            // check and remove point
            int temp = (100 * (GET_X_LPARAM(lParam) - cArea.left)) / (cArea.right - cArea.left),
                boost = (100 * (cArea.bottom - GET_Y_LPARAM(lParam))) / (cArea.bottom - cArea.top);
            for (vector<fan_point>::iterator fPi = cFan->points.begin() + 1;
                fPi < cFan->points.end() - 1; fPi++)
                if (fPi->temp - DRAG_ZONE <= temp && fPi->temp + DRAG_ZONE >= temp) {
                    // Remove this element...
                    cFan->points.erase(fPi);
                    break;
                }
            DrawFan();
            fan_conf->Save();
        }
        SetFocus(GetParent(hDlg));
    } break;
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_ERASEBKGND:
        return true;
        break;
    }
    return DefWindowProc(hDlg, message, wParam, lParam);
}

//DWORD WINAPI UpdateFanUI(LPVOID lpParam) {
//    HWND tempList = GetDlgItem((HWND)lpParam, IDC_TEMP_LIST),
//        fanList = GetDlgItem((HWND)lpParam, IDC_FAN_LIST);
//
//    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
//    while (WaitForSingleObject(fuiEvent, 250) == WAIT_TIMEOUT) {
//        if (mon && IsWindowVisible((HWND)lpParam)) {
//            //DebugPrint("Fans UI update...\n");
//            for (int i = 0; i < mon->acpi->HowManySensors(); i++) {
//                string name = to_string(mon->senValues[i]) + " (" + to_string(mon->maxTemps[i]) + ")";
//                ListView_SetItemText(tempList, i, 0, (LPSTR)name.c_str());
//            }
//            for (int i = 0; i < mon->acpi->HowManyFans(); i++) {
//                string name = "Fan " + to_string(i + 1) + " (" + to_string(mon->fanValues[i]) + ")";
//                ListView_SetItemText(fanList, i, 0, (LPSTR)name.c_str());
//            }
//            SendMessage(fanWindow, WM_PAINT, 0, 0);
//        }
//    }
//    return 0;
//}