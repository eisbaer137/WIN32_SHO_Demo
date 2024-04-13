#include "framework.h"
#include "resource.h"

#define	WIN_WIDTH	1024
#define	WIN_HEIGHT	650

#define	BOX_WIDTH	600
#define BOX_HEIGHT	400

#define BTN_START	101		// assigned for start button
#define BTN_STOP	102		// assigned for stop button

#define	ID_EDIT0	400		// assigned for mass m
#define	ID_EDIT1	401		// assigned for spring tension k
#define	ID_EDIT2	402		// assigned for initial velocity v_0
#define ID_EDIT3	403		// assigned for initial position x_0

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBit);
HINSTANCE g_hInst;
HWND hWndMain;
HWND hButton1;
HWND hButton2;
HWND hEdit0;
HWND hEdit1;
HWND hEdit2;
HWND hEdit3;
LPCTSTR lpszClass = TEXT("SHO_modeling");

HBITMAP hBit;
RECT brt;

HANDLE hThread;				// handle for bitmap drawing thread
bool init0 = false;

const int delta = 30;			// dt = 30 ms
unsigned int m0 = 1;
unsigned int k0 = 20;
double v0 = 0.0;
double x0 = 0.0;


class kinematicParameters
{
public:
	double position;
	double velocity;
	unsigned int mass;
	unsigned int tension;
	unsigned int omegaSq;
};

class SpringMass
{
public:
	POINT leftBar[2];
	POINT rightBar[2];
	POINT springDots[16];
	RECT mass;
	int radius;
	int barLength;
	int pitch;
	kinematicParameters springKin;

};

void OnTimerDraw();
void DrawBackGround(HDC& hdc);
void DrawSpringMass(HDC& hdc, SpringMass& sp_mass);

void UpdateSpringMass(SpringMass& sp_mass);
void EulerUpdateMotionEquation(SpringMass& sp_mass);
void InitSpringMass(SpringMass& sp_mass, unsigned int m, unsigned int k, double initVel, double initPos);

DWORD WINAPI ThreadDrawBitmap(LPVOID temp);

SpringMass spring0;

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdParam, int nCmdShow)
{
	HWND hWnd;
	MSG Message;
	WNDCLASS wdc;
	g_hInst = hInstance;

	wdc.cbClsExtra = 0;
	wdc.cbWndExtra = 0;
	wdc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wdc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wdc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	wdc.hInstance = hInstance;
	wdc.lpfnWndProc = WndProc;
	wdc.lpszClassName = lpszClass;
	wdc.lpszMenuName = nullptr;
	wdc.style = CS_HREDRAW | CS_VREDRAW;	// class style

	RegisterClass(&wdc);

	hWnd = CreateWindow(lpszClass, lpszClass, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, WIN_WIDTH, WIN_HEIGHT, nullptr,
		(HMENU)nullptr, hInstance, nullptr);

	ShowWindow(hWnd, nCmdShow);

	while (GetMessage(&Message, nullptr, 0, 0))
	{
		TranslateMessage(&Message);
		DispatchMessage(&Message);
	}
	return (int)Message.wParam;
}


void DrawBackGround(HDC& hdc)
{
	HPEN hPen, hOldPen;
	int i;
	hPen = CreatePen(PS_SOLID, 1, RGB(0, 128, 64));
	hOldPen = (HPEN)SelectObject(hdc, hPen);
	Rectangle(hdc, 0, 0, BOX_WIDTH, BOX_HEIGHT);

	// drawing lattice lines
	for (i = 0; i < BOX_WIDTH; i += 10)
	{
		MoveToEx(hdc, i, 0, nullptr);
		LineTo(hdc, i, BOX_HEIGHT);
	}
	for (i = 0; i < BOX_HEIGHT; i += 10)
	{
		MoveToEx(hdc, 0, i, nullptr);
		LineTo(hdc, BOX_WIDTH, i);
	}
	DeleteObject(SelectObject(hdc, hOldPen));
	hPen = CreatePen(PS_SOLID, 5, RGB(0, 0, 0));
	hOldPen = (HPEN)SelectObject(hdc, hPen);

	// left wall
	MoveToEx(hdc, 50, 100, nullptr);
	LineTo(hdc, 50, 300);

	// diagonal stripes
	for (i = 120; i <= 260; i += 15)
	{
		MoveToEx(hdc, 50, i, nullptr);
		LineTo(hdc, 30, i + 20);
	}

	// horizontal baseline
	MoveToEx(hdc, 50, 230, nullptr);
	LineTo(hdc, 570, 230);

	DeleteObject(SelectObject(hdc, hOldPen));
	DeleteObject(hPen);
}

void DrawSpringMass(HDC& hdc, SpringMass& sp_mass)
{
	HPEN hPen, hOldPen;
	HBRUSH hBrush, hOldBrush;
	int i;

	hPen = CreatePen(PS_SOLID, 5, RGB(128, 128, 0));
	hOldPen = (HPEN)SelectObject(hdc, hPen);

	// draw spring
	MoveToEx(hdc, sp_mass.leftBar[0].x, sp_mass.leftBar[0].y, nullptr);
	LineTo(hdc, sp_mass.leftBar[1].x, sp_mass.leftBar[1].y);

	for (i = 0; i < 15; i++)
	{
		MoveToEx(hdc, sp_mass.springDots[i].x, sp_mass.springDots[i].y, nullptr);
		LineTo(hdc, sp_mass.springDots[i + 1].x, sp_mass.springDots[i + 1].y);
	}

	MoveToEx(hdc, sp_mass.rightBar[0].x, sp_mass.rightBar[0].y, nullptr);
	LineTo(hdc, sp_mass.rightBar[1].x, sp_mass.rightBar[1].y);
	DeleteObject(SelectObject(hdc, hOldPen));

	// draw mass attached to the spring
	hPen = CreatePen(PS_INSIDEFRAME, 3, RGB(192, 128, 128));
	hOldPen = (HPEN)SelectObject(hdc, hPen);
	hBrush = CreateSolidBrush(RGB(0, 256, 64));
	hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);

	Rectangle(hdc, sp_mass.mass.left, sp_mass.mass.top, sp_mass.mass.right, sp_mass.mass.bottom);

	DeleteObject(SelectObject(hdc, hOldPen));
	DeleteObject(SelectObject(hdc, hOldBrush));

}

void OnTimerDraw()
{
	HDC hdc, hMemDC;
	HBITMAP hOldBit;

	brt.left = 0; brt.top = 0;
	brt.right = BOX_WIDTH;
	brt.bottom = BOX_HEIGHT;

	hdc = GetDC(hWndMain);

	if (hBit == nullptr)
	{
		hBit = CreateCompatibleBitmap(hdc, BOX_WIDTH, BOX_HEIGHT);
	}
	hMemDC = CreateCompatibleDC(hdc);
	hOldBit = (HBITMAP)SelectObject(hMemDC, hBit);

	FillRect(hMemDC, &brt, GetSysColorBrush(COLOR_WINDOW));

	DrawBackGround(hMemDC);
	DrawSpringMass(hMemDC, spring0);
	SelectObject(hMemDC, hOldBit);
	DeleteDC(hMemDC);

	ReleaseDC(hWndMain, hdc);
}

void UpdateSpringMass(SpringMass& sp_mass)
{
	int sgn = -1;

	// update the position of spring and mass
	sp_mass.pitch = (290 + (int)sp_mass.springKin.position) / 15;

	sp_mass.leftBar[0].x = 50; sp_mass.leftBar[0].y = BOX_HEIGHT / 2;
	sp_mass.leftBar[1].x = 50 + sp_mass.barLength; sp_mass.leftBar[1].y = BOX_HEIGHT / 2;
	sp_mass.springDots[0].x = sp_mass.leftBar[1].x;
	sp_mass.springDots[0].y = BOX_HEIGHT / 2;

	for (int i = 1; i < 15; i++)
	{
		sp_mass.springDots[i].x = 50 + sp_mass.barLength + i * sp_mass.pitch;
		sp_mass.springDots[i].y = BOX_HEIGHT / 2 + sgn * sp_mass.radius;
		sgn *= -1;
	}

	sp_mass.springDots[15].x = sp_mass.springDots[14].x + sp_mass.pitch;
	sp_mass.springDots[15].y = BOX_HEIGHT / 2;

	sp_mass.rightBar[0].x = sp_mass.springDots[15].x;
	sp_mass.rightBar[0].y = BOX_HEIGHT / 2;
	sp_mass.rightBar[1].x = sp_mass.rightBar[0].x + sp_mass.barLength;
	sp_mass.rightBar[1].y = BOX_HEIGHT / 2;

	// box mass
	sp_mass.mass.left = sp_mass.rightBar[1].x;
	sp_mass.mass.top = sp_mass.rightBar[1].y - 25;
	sp_mass.mass.right = sp_mass.mass.left + 50;
	sp_mass.mass.bottom = sp_mass.mass.top + 50;

}

void InitSpringMass(SpringMass& sp_mass, unsigned int m, unsigned int k, double initVel, double initPos) // mass, tension, initial velocity and position
{
	sp_mass.springKin.mass = m;
	sp_mass.springKin.tension = k;
	sp_mass.springKin.omegaSq = k / m;
	sp_mass.springKin.position = initPos;
	sp_mass.springKin.velocity = initVel;

	sp_mass.radius = 20;
	sp_mass.barLength = 30;
	int pitch = (290 + (int)initPos) / 15;
	UpdateSpringMass(sp_mass);
}

void EulerUpdateMotionEquation(SpringMass& sp_mass)
{
	sp_mass.springKin.velocity = sp_mass.springKin.velocity - (double)sp_mass.springKin.omegaSq * ((double)delta / 1000.0) * sp_mass.springKin.position;
	sp_mass.springKin.position = sp_mass.springKin.position + ((double)delta / 1000.0) * sp_mass.springKin.velocity;
}

DWORD WINAPI ThreadDrawBitmap(LPVOID temp)
{
	LPRECT rct = (RECT*)temp;
	while (true)
	{
		EulerUpdateMotionEquation(spring0);
		UpdateSpringMass(spring0);
		OnTimerDraw();
		InvalidateRect(hWndMain, rct, true);
		GdiFlush();
		Sleep(delta);
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	HDC hdc;
	PAINTSTRUCT ps;
	TCHAR desc[64];
	TCHAR temp[64];
	//TCHAR temp2[64];
	DWORD threadID;
	static RECT refresh;

	const static TCHAR* Mes = TEXT("Simple Harmonic Oscillator model using Euler Method");
	const static TCHAR* Mes2 = TEXT("One thread is allocated for drawing the oscillator.");

	switch (iMessage)
	{
	case WM_CREATE:
		hWndMain = hWnd;
		InitSpringMass(spring0, m0, k0, (int)v0, (int)x0);		// initialize a spring mass object, mass m:1, tension k:2, v_0=10, x_0=50
		SetWindowText(hWnd, TEXT("simple spring mass system demo using Win32 GDI"));

		// buttons
		hButton1=CreateWindow(TEXT("button"), TEXT("Start"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			700, 440, 100, 25, hWnd, (HMENU)BTN_START, g_hInst, nullptr);
		hButton2=CreateWindow(TEXT("button"), TEXT("Stop"), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
			850, 440, 100, 25, hWnd, (HMENU)BTN_STOP, g_hInst, nullptr);

		// edit windows
		hEdit0 = CreateWindow(TEXT("edit"), nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER,
			780, 150, 100, 25, hWnd, (HMENU)ID_EDIT0, g_hInst, nullptr);
		hEdit1 = CreateWindow(TEXT("edit"), nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER,
			780, 220, 100, 25, hWnd, (HMENU)ID_EDIT1, g_hInst, nullptr);
		hEdit2 = CreateWindow(TEXT("edit"), nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER,
			780, 290, 100, 25, hWnd, (HMENU)ID_EDIT2, g_hInst, nullptr);
		hEdit3 = CreateWindow(TEXT("edit"), nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER,
			780, 360, 100, 25, hWnd, (HMENU)ID_EDIT3, g_hInst, nullptr);

		// initial edit window values
		_stprintf_s(temp, TEXT("%d"), 1);
		SetWindowText(hEdit0, temp);

		_stprintf_s(temp, TEXT("%d"), 20);
		SetWindowText(hEdit1, temp);

		_stprintf_s(temp, TEXT("%f"), 0.0);
		SetWindowText(hEdit2, temp);

		_stprintf_s(temp, TEXT("%f"), 0.0);
		SetWindowText(hEdit3, temp);


		EulerUpdateMotionEquation(spring0);
		UpdateSpringMass(spring0);
		OnTimerDraw();
		//SetTimer(hWnd, 1, delta, nullptr);
		refresh.left = 20;
		refresh.top = 80;
		refresh.right = 670;
		refresh.bottom = 520;
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case BTN_START:
			MessageBeep(0);
			//SetTimer(hWnd, 1, delta, nullptr);
			if (init0 == false)
			{
				hThread = CreateThread(nullptr, 0, ThreadDrawBitmap, &refresh, 0, &threadID);
				init0 = true;
			}
			else
			{
				ResumeThread(hThread);
			}
			break;

		case BTN_STOP:
			MessageBeep(0);
			//KillTimer(hWnd, 1);
			SuspendThread(hThread);
			break;

		case ID_EDIT0:
			switch (HIWORD(wParam))
			{
			case EN_CHANGE:
				GetWindowText(hEdit0, temp, 64);
				m0 = (unsigned int)wcstod(temp, nullptr);
				InitSpringMass(spring0, m0, k0, (int)v0, (int)x0);
				break;
			}
		case ID_EDIT1:
			switch (HIWORD(wParam))
			{
			case EN_CHANGE:
				GetWindowText(hEdit1, temp, 64);
				k0 = (unsigned int)wcstod(temp, nullptr);
				InitSpringMass(spring0, m0, k0, (int)v0, (int)x0);
				break;
			}
		case ID_EDIT2:
			switch (HIWORD(wParam))
			{
			case EN_CHANGE:
				GetWindowText(hEdit2, temp, 64);
				v0 = (double)wcstod(temp, nullptr);
				InitSpringMass(spring0, m0, k0, (int)v0, (int)x0);
				break;
			}
		case ID_EDIT3:
			switch (HIWORD(wParam))
			{
			case EN_CHANGE:
				GetWindowText(hEdit3, temp, 64);
				x0 = (double)wcstod(temp, nullptr);
				InitSpringMass(spring0, m0, k0, (int)v0, (int)x0);
				break;
			}

		}
		return 0;


	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		TextOut(hdc, 170, 50, Mes, lstrlen(Mes));
		TextOut(hdc, 185, 75, Mes2, lstrlen(Mes2));
		LoadString(g_hInst, IDS_STRING1, desc, 64);
		TextOut(hdc, 790, 125, desc, lstrlen(desc));
		LoadString(g_hInst, IDS_STRING2, desc, 64);
		TextOut(hdc, 785, 195, desc, lstrlen(desc));
		LoadString(g_hInst, IDS_STRING3, desc, 64);
		TextOut(hdc, 740, 265, desc, lstrlen(desc));
		LoadString(g_hInst, IDS_STRING4, desc, 64);
		TextOut(hdc, 730, 335, desc, lstrlen(desc));

		if (hBit)
		{
			DrawBitmap(hdc, 50, 100, hBit);
		}
		EndPaint(hWnd, &ps);
		return 0;

	case WM_DESTROY:
		if (hBit)
		{
			DeleteObject(hBit);
		}
		CloseHandle(hThread);
		KillTimer(hWnd, 1);
		PostQuitMessage(0);
		return 0;
	}

	return (DefWindowProc(hWnd, iMessage, wParam, lParam));
}


void DrawBitmap(HDC hdc, int x, int y, HBITMAP hBit)
{
	HDC MemDC;
	HBITMAP OldBitmap;
	int bx, by;
	BITMAP bit;

	MemDC = CreateCompatibleDC(hdc);
	OldBitmap = (HBITMAP)SelectObject(MemDC, hBit);

	GetObject(hBit, sizeof(BITMAP), &bit);
	bx = bit.bmWidth;
	by = bit.bmHeight;

	BitBlt(hdc, x, y, bx, by, MemDC, 0, 0, SRCCOPY);
	//BitBlt(hdc, x, y, 600, 400, MemDC, 0, 0, SRCCOPY);

	SelectObject(MemDC, OldBitmap);
	DeleteDC(MemDC);
}
