#include <windows.h>

#include <stdio.h>

#define ALPHA 170
#define HOTKEY_ID 1

HDC hdc;
HDC darkHdc;

char isActive = 0;

RECT selection = {0};

char isDown = 0;

int x, y, width, height;

void displayError() {
  wchar_t *buffer;
  DWORD errorCode = GetLastError();
  FormatMessageW(
    FORMAT_MESSAGE_FROM_SYSTEM | 
    FORMAT_MESSAGE_ALLOCATE_BUFFER, 
    NULL, 
    errorCode, 
    0,
    (LPWSTR)&buffer,
    0,
    NULL
  );

  MessageBoxW(NULL, buffer, L"Error", MB_OK);

  LocalFree(buffer);
}

void darkenDC(HDC hdcDest, HDC hdcSrc, BYTE alpha) {
  BLENDFUNCTION bf;
  bf.BlendOp = AC_SRC_OVER;
  bf.BlendFlags = 0;
  bf.SourceConstantAlpha = alpha;
  bf.AlphaFormat = 0;

  HDC dc = CreateCompatibleDC(hdcSrc);
  HBITMAP map = CreateCompatibleBitmap(hdcSrc, width, height);
  HGDIOBJ oldMap = SelectObject(dc, map);

  HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
  RECT rect = {0, 0, width, height};
  FillRect(dc, &rect, brush);

  AlphaBlend(hdcDest, 0, 0, width, height, dc, 0, 0, width, height, bf);

  SelectObject(dc, oldMap);
  DeleteDC(dc);
  DeleteObject(brush);
  DeleteObject(map);
}

void takeScreenshot() {
  HDC dcScreen = GetDC(NULL);
  hdc = CreateCompatibleDC(dcScreen);
  darkHdc = CreateCompatibleDC(dcScreen);

  HBITMAP map = CreateCompatibleBitmap(dcScreen, width, height);
  HBITMAP oldMap = SelectObject(hdc, map);
  HBITMAP darkMap = CreateCompatibleBitmap(dcScreen, width, height);
  HBITMAP oldDarkMap = SelectObject(darkHdc, darkMap);

  BitBlt(hdc, 0, 0, width, height, dcScreen, 0, 0, SRCCOPY);
  BitBlt(darkHdc, 0, 0, width, height, hdc, 0, 0, SRCCOPY);

  darkenDC(darkHdc, hdc, ALPHA);

  DeleteObject(oldMap);
  DeleteObject(oldDarkMap);

}

void copyScreenshot(int x, int y, int cx, int cy) {
  if (!OpenClipboard(NULL)) {
    displayError();
    return;
  }

  if (!EmptyClipboard()) {
    displayError();
    return;
  }

  int w = abs(cx - x);
  int h = abs(cy - y);
  
  HDC dc = CreateCompatibleDC(hdc);
  HBITMAP map = CreateCompatibleBitmap(hdc, w, h);
  HBITMAP oldMap = SelectObject(dc, map);

  BitBlt(dc, 0, 0, w, h, hdc, x, y, SRCCOPY);

  SetClipboardData(CF_BITMAP, map);

  CloseClipboard();

  SelectObject(dc, oldMap);
  DeleteDC(dc);
  DeleteObject(map);

}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

  switch (uMsg) {

    case WM_CLOSE: {
      DestroyWindow(hwnd);
      return 0;
    }

    case WM_DESTROY: {
      PostQuitMessage(0);
      return 0;
    }

    case WM_HOTKEY: {
      if (wParam != HOTKEY_ID || isActive) return 0;

      takeScreenshot();
      ShowWindow(hwnd, SW_SHOWMAXIMIZED);
      SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
      isActive = 1;
      return 0;
    }

    case WM_KEYDOWN: {
      switch (wParam) {
        case VK_ESCAPE: {
          SetRectEmpty(&selection);
          ShowWindow(hwnd, SW_HIDE);
          isActive = 0;
          return 0;
        }

        case 'C': {
          if(GetKeyState(VK_CONTROL) >> 15) {
            int x = selection.left;
            int y = selection.top;
            int cx = selection.right;
            int cy = selection.bottom;
            copyScreenshot(x, y, cx, cy);
            SetRectEmpty(&selection);
            ShowWindow(hwnd, SW_HIDE);
            isActive = 0;
            return 0;
          };
        }
      }

      return 0;
    }

    case WM_LBUTTONDOWN: {
      isDown = 1;
      selection.left = LOWORD(lParam);
      selection.top = HIWORD(lParam);

      return 0;
    }

    case WM_LBUTTONUP: {
      isDown = 0;
    }

    case WM_MOUSEMOVE: {
      if (!isDown) return 0;

      selection.right = LOWORD(lParam);
      selection.bottom = HIWORD(lParam);

      RedrawWindow(
        hwnd, 
        NULL, NULL, 
        RDW_INVALIDATE | RDW_UPDATENOW | RDW_NOERASE
      );

      return 0;
    }
      
    case WM_ERASEBKGND: {
      return 1;
    }
    
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC dc = BeginPaint(hwnd, &ps);
      
      HBRUSH yellow = CreateSolidBrush(RGB(255, 255, 0));

      HDC memDc = CreateCompatibleDC(dc);
      HBITMAP map = CreateCompatibleBitmap(dc, width, height);
      HBITMAP oldMemMap = SelectObject(memDc, map);

      int x = selection.left;
      int y = selection.top;
      int sx = selection.right - x;
      int sy = selection.bottom - y;
      
      RECT frame;
      frame.left = min(x, selection.right);
      frame.top = min(y, selection.bottom);
      frame.right = max(x, selection.right);
      frame.bottom = max(y,selection.bottom);

      BitBlt(memDc, 0, 0, width, height, darkHdc, 0, 0, SRCCOPY);
      BitBlt(memDc, x, y, sx, sy, hdc, x, y, SRCCOPY);

      if ((frame.right - frame.left) != 0 && (frame.bottom - frame.top) != 0) {
        FrameRect(memDc, &frame, yellow);
      }

      BitBlt(dc, 0, 0, width, height, memDc, 0, 0, SRCCOPY);
      
      DeleteObject(yellow);
      SelectObject(memDc, oldMemMap);
      DeleteDC(memDc);
      DeleteObject(map);

      EndPaint(hwnd, &ps);
    }

    default: {
      break;
    }
  }

  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(
  HINSTANCE hInst, 
  HINSTANCE hPrevInst, 
  LPSTR lpCmdLine, 
  int nShowCmd
) {

  WNDCLASSW wc;
  wc.style = 0;
  wc.cbClsExtra = sizeof(WNDCLASSW);
  wc.cbWndExtra = 0;
  wc.hbrBackground = NULL;
  wc.hCursor = LoadCursor(NULL, IDC_CROSS);
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  wc.hInstance = hInst;
  wc.lpfnWndProc = WndProc;
  wc.lpszClassName = L"shot";
  wc.lpszMenuName = NULL;
  
  if (!RegisterClassW(&wc)) {
    displayError();
    return 1;
  }
  
  HWND hwnd = CreateWindowW(
    L"shot", 
    L"shot",
    WS_POPUPWINDOW |
    WS_CLIPCHILDREN,
    0, 0, 
    800, 600,
    NULL,
    NULL,
    hInst, 
    NULL
  );

  if (hwnd == NULL) {
    displayError();
    return 1;
  }

  if (!SetProcessDPIAware()) {
    displayError();
    return 1;
  }

  x = GetSystemMetrics(SM_XVIRTUALSCREEN);
  y = GetSystemMetrics(SM_YVIRTUALSCREEN);
  width = GetSystemMetrics(SM_CXVIRTUALSCREEN) - x;
  height = GetSystemMetrics(SM_CYVIRTUALSCREEN) - y;

  if (!RegisterHotKey(hwnd, HOTKEY_ID, MOD_WIN | MOD_SHIFT, 'S')) {
    displayError();
    return 1;
  }

  MSG msg;

  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  DeleteDC(hdc);
  DeleteDC(darkHdc);

  return 0;
}