#include <Windows.h>
#include "resource.h"
#include "paint.h"
#include <CommDlg.h>
#include <vector>
#include <cmath>

using namespace std;

typedef struct 
{
	int x;
	int y;
}Point;


typedef struct 
{
	Point point;
	int penIndex;
}Trace;

#define  UNTITLED TEXT("(untitled)")

//#define WM_ColorChange1 (WM_USER + 100)

//#define WM_ColorChange2 (WM_USER + 50)

struct Record 
{
	POINT beg_end[20];
	COLORREF color;        //外边线颜色
	COLORREF full_color;   //填充颜色
	int tool_kind;         //图形类别
	int n;                 //多边形的边数
	BOOL ifDelete;         //是否删除
	BOOL ifchoosed;        //是否被选择，用于图形移动前的判断
	BOOL finish;           //是否结束
	int line_wide;         //线条粗细
	BOOL move;             //是否需要移动
};


static RECT rcClient;    // 客户区矩形

//以下为全局变量
HPEN hpen;                         //画笔句柄          
HBRUSH hbrush;                     //画刷句柄
COLORREF setColor=RGB(0,0,0);      //当前线条颜色
COLORREF setColor_Fule=RGB(0,0,0); //当前填充颜色
static OPENFILENAME ofn;           //文件信息
vector<Record> shapes;             
Record alldata[100];
COPYDATASTRUCT MyCDS;
PCOPYDATASTRUCT pMyCDS;
int Length;                       //长度

BOOL mouse=FALSE;

//以下为函数的声明

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM) ;     //主窗口消息处理函数

COLORREF ColorUserDef();   //用户自定义颜色
void DoCaption(HWND hwnd,TCHAR * szTitleName);
BOOL PaintFileOpenDlg(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName);
BOOL PaintFileSaveDlg(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName);
void PaintFileInitialize(HWND hwnd);

//判断选中的函数
BOOL in_Line(POINT *beg_end,POINT cur_pt);//是否在直线上
BOOL in_Ellipse(POINT *beg_end,POINT cur_pt);//是否在椭圆内
BOOL in_Ellipse1(POINT *beg_end,POINT cur_pt);//是否在填充的椭圆内
BOOL in_Rect(POINT *beg_end,POINT cur_pt);//是否在矩形内
BOOL in_Rect1(POINT *beg_end,POINT cur_pt);//是否在被填充的矩形内
BOOL in_Polygon(POINT *beg_end,POINT cur_pt,int n);//是否在多边形内
void GetPoint(HWND hwnd,LPARAM lParam,POINT &pt,SIZE &Canvas,const POINT &ScrollPos,int Scale,int PixelPerScroll);
void SetScrollSize(HWND hwnd,const SIZE &Canvas,const SIZE &Client,POINT &ScrollPos,int Scale,int PixelPerScroll);
void if_ctrl(Record &data,POINT &end,int kind);

void SendData1(HWND hwnd,UINT message, WPARAM wParam, LPARAM lParam);//通信函数

TCHAR szAppName[] = TEXT("Paint");

//主函数，程序入口
int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
					PSTR szCmdLine, int iCmdShow)
{
	HWND     hwnd ;     //窗口句柄
	MSG      msg ;      //消息
	WNDCLASS wndclass ; //

	wndclass.style         = CS_HREDRAW | CS_VREDRAW ;    //窗口类型
	wndclass.lpfnWndProc   = WndProc ;                    //处理
	wndclass.cbClsExtra    = 0 ;                  
	wndclass.cbWndExtra    = 0 ;
	wndclass.hInstance     = hInstance ;
	wndclass.hIcon         = LoadIcon (hInstance,MAKEINTRESOURCE(IDI_TITLE)) ;//图标
	wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;                   //游标
	wndclass.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH) ;          //背景画刷
	wndclass.lpszMenuName  = szAppName;                                       //窗口名字
	wndclass.lpszClassName = szAppName ;

	if (!RegisterClass (&wndclass))
	{
		MessageBox (NULL, TEXT ("This program requires Windows NT!"),
			szAppName, MB_ICONERROR) ;
		return 0 ;
	}
	
	//创建窗口
	hwnd = CreateWindow (szAppName, szAppName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, CW_USEDEFAULT,
		NULL, NULL, hInstance, NULL) ;

	ShowWindow (hwnd, iCmdShow) ;   //显示窗口
	UpdateWindow (hwnd) ;           //更新窗口


	while (GetMessage (&msg, NULL, 0, 0))   //消息队列
	{
		TranslateMessage (&msg) ;
		DispatchMessage (&msg) ;
	}
	return (int)msg.wParam ;    //分发消息，将消息传给系统，系统再传给WndProc窗口过程
}

LRESULT CALLBACK WndProc (
                          HWND hwnd,     //窗口句柄
                          UINT message,  //消息标识符
                          WPARAM wParam, //消息的第一个参数
                          LPARAM lParam  //消息的第二个参数
                          )
{

	HMENU hMenu;             //菜单句柄
	HDC hdc;                 //设备上下文句柄
	PAINTSTRUCT ps;          //

	static Point prePoint;     //前加一个点
	//static HPEN hpen[3];       //画笔
	static int index;
	static vector<Trace>trace;     //记录轨迹以重绘

	static int iSelection=0,iSelection1=IDM_WIDE_ONE;
	static BOOL Saved=TRUE,choose=FALSE,finish=TRUE,deleted=FALSE,Move=FALSE;
	static Record data;
	static POINT begin,end;
	static POINT move_beg,move_end;
	static int n=0,size=0,i=0,kind=-1,j=0,m=0;
	static int Answer=-1;
	static TCHAR szFileName[MAX_PATH];
	static TCHAR szTitleName[MAX_PATH];
	static DWORD dwByteRead;
	static Record temp;
	static int wide=1;

	static SIZE Canvas;
	static POINT ScrollPos;
	static int PixelPerScroll = 10;
	static SIZE Client;
	static int Scale = 4;
	SCROLLINFO ScrInfo;

	static HDC hdcMem;  // 内存设备描述表
	static HBITMAP hBitmap; // 缓冲位图
	HGDIOBJ hOldObject; // 内存设备描述表原位图


	switch(message)
	{
	case WM_CREATE:  //窗口创建时收到此消息
		/*
		hpen[0] = CreatePen(PS_SOLID, 4, RGB(255,0,0));   //创建画笔
		hpen[1] = CreatePen(PS_SOLID, 6, RGB(0,255,0));
		hpen[2] = CreatePen(PS_SOLID, 8, RGB(0,0,255)); 
		*/

		SendData1(hwnd,message,wParam,lParam);   //通信函数
		hMenu = GetMenu(hwnd);                   //获得菜单句柄
		hdc=GetDC(hwnd);                         //获得设备上下文句柄
		GetClientRect(hwnd,&rcClient);           //获得图形客户区
		hdcMem = CreateCompatibleDC(hdc);
		hBitmap = CreateCompatibleBitmap(hdc,rcClient.right-rcClient.left,rcClient.bottom-rcClient.top);  //位图句柄
		hOldObject = SelectObject(hdcMem,hBitmap); //该函数选择一对象到指定的设备上下文环境中，该新对象替换先前的相同类型的对象。
		CheckMenuItem(hMenu,iSelection1,MF_CHECKED);
		EnableMenuItem(hMenu,IDM_EDIT_DELETE,MF_GRAYED);   

		PaintFileInitialize(hwnd);
	//	DoCaption(hwnd,szTitleName);
		return 0;
	case WM_SIZE: //设置窗口大小
		SendData1(hwnd,message,wParam,lParam);
		Client.cx = static_cast<short>(LOWORD(lParam));
		Client.cy = static_cast<short>(HIWORD(lParam));
		SetScrollSize(hwnd,Canvas,Client,ScrollPos,Scale,PixelPerScroll);
		break;

	case WM_KEYDOWN: //处理按键
		SendData1(hwnd,message,wParam,lParam);
		hMenu = GetMenu(hwnd);
		switch (wParam)
		{
		case VK_DELETE:  //删除
			SendData1(hwnd,message,wParam,lParam);
			SendMessage(hwnd,WM_COMMAND,IDM_EDIT_DELETE,0);  
			break;
		case VK_ADD: //加粗
			SendData1(hwnd,message,wParam,lParam);
			if (Scale<=8)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMOUT,MF_ENABLED);
				Scale *= 2;
				SetScrollSize(hwnd,Canvas,Client,ScrollPos,Scale,PixelPerScroll);
			}
			if (Scale>8)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMIN,MF_GRAYED);
			}
			break;
		case VK_SUBTRACT:
			SendData1(hwnd,message,wParam,lParam);
			if (Scale>=2)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMIN,MF_ENABLED);
				Scale /= 2;
				SetScrollSize(hwnd,Canvas,Client,ScrollPos,Scale,PixelPerScroll);
			}
			if (Scale<2)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMOUT,MF_GRAYED);
			}
			break;
		default:
			break;
		}
		break;


	case WM_HSCROLL:  //水平滚动
		SendData1(hwnd,message,wParam,lParam);
		ScrInfo.cbSize = sizeof(SCROLLINFO);
		ScrInfo.fMask = SIF_ALL;
		GetScrollInfo(hwnd,SB_HORZ,&ScrInfo);
		ScrollPos.x = ScrInfo.nPos;
		switch(LOWORD(wParam))  //wParam的低位数，其代表了鼠标在滚动条上的动作，也被称为“通知码”
		{
		case SB_LINELEFT:  //SB_代表滚动条
			ScrInfo.nPos -= 1;
			break;
		case SB_LINERIGHT:
			ScrInfo.nPos += 1;
			break;
		case SB_PAGELEFT:
			ScrInfo.nPos -= ScrInfo.nPage;
			break;
		case SB_PAGERIGHT:
			ScrInfo.nPos += ScrInfo.nPage;
			break;
		case SB_LEFT:
			ScrInfo.nPos = ScrInfo.nMin;
			break;
		case SB_RIGHT:
			ScrInfo.nPos = ScrInfo.nMax;
			break;
		case SB_THUMBTRACK:
			ScrInfo.nPos = ScrInfo.nTrackPos;
		default:
			break;
		}
		ScrInfo.fMask = SIF_POS;
		SetScrollInfo(hwnd,SB_HORZ,&ScrInfo,TRUE);
		GetScrollInfo(hwnd,SB_HORZ,&ScrInfo);
		if (ScrollPos.x!=ScrInfo.nPos)
		{
			ScrollWindow(hwnd,PixelPerScroll*(ScrollPos.x-ScrInfo.nPos),0,NULL,NULL);
			ScrollPos.x = ScrInfo.nPos;
			InvalidateRect(hwnd,NULL,TRUE);
			UpdateWindow(hwnd);
		}
		break;
	case WM_VSCROLL:  //竖直滚动
		SendData1(hwnd,message,wParam,lParam);
		ScrInfo.cbSize = sizeof(SCROLLINFO);
		ScrInfo.fMask = SIF_ALL;
		GetScrollInfo(hwnd,SB_VERT,&ScrInfo);
		ScrollPos.y = ScrInfo.nPos;
		switch(LOWORD(wParam))
		{
		case SB_LINEUP:
			ScrInfo.nPos -= 1;
			break;
		case SB_LINEDOWN:
			ScrInfo.nPos += 1;
			break;
		case SB_PAGEUP:
			ScrInfo.nPos -= ScrInfo.nPage;
			break;
		case SB_PAGEDOWN:
			ScrInfo.nPos += ScrInfo.nPage;
			break;
		case SB_TOP:
			ScrInfo.nPos = ScrInfo.nMin;
			break;
		case SB_BOTTOM:
			ScrInfo.nPos = ScrInfo.nMax;
			break;
		case SB_THUMBTRACK:
			ScrInfo.nPos = ScrInfo.nTrackPos;
		default:
			break;
		}
		ScrInfo.fMask = SIF_POS;
		SetScrollInfo(hwnd,SB_VERT,&ScrInfo,TRUE);
		GetScrollInfo(hwnd,SB_VERT,&ScrInfo);
		if (ScrollPos.y!=ScrInfo.nPos)
		{
			ScrollWindow(hwnd,0,PixelPerScroll*(ScrollPos.y-ScrInfo.nPos),NULL,NULL);
			ScrollPos.y = ScrInfo.nPos;
			InvalidateRect(hwnd,NULL,TRUE);
			UpdateWindow(hwnd);
		}
		break;


	case WM_COMMAND: //命令输入，菜单项被选中时，或者按钮被单击
		SendData1(hwnd,message,wParam,lParam);

		hMenu = GetMenu(hwnd);
		switch(LOWORD(wParam))
		{
		case IDM_FILE_NEW:  //新建文件
			
			if(Saved == FALSE)
			{
				//首先判断之前的文档是否已经保存
				Answer = MessageBox(hwnd,TEXT("文档没有保存,要保存吗?"),TEXT("提示:"),
					MB_YESNOCANCEL|MB_APPLMODAL|MB_ICONQUESTION);
				if(Answer == IDYES)  //需要保存时
				{
					SendMessage (hwnd, WM_COMMAND,IDM_FILE_SAVE, 0);
					shapes.clear();
					InvalidateRect(hwnd,NULL,true);
					return 0;
				}
				if(Answer == IDNO)  //不需要保存时
				{
					shapes.clear();
					InvalidateRect(hwnd,NULL,true);
					Saved = TRUE;
				}
			}
			else
			{
				shapes.clear();
				InvalidateRect(hwnd,NULL,true);
			}
			SendData1(hwnd,message,wParam,lParam);
			return 0;
			break;
		case IDM_FILE_OPEN : //打开文件
			HANDLE hFile;      //文件句柄
			if(Saved == FALSE) //和新建文件前的判断一样
			{
				Answer = MessageBox(hwnd,TEXT("文档没有保存,要保存吗?"),TEXT("提示:"),
					MB_YESNOCANCEL|MB_APPLMODAL|MB_ICONQUESTION);
				if(Answer == IDYES)
				{

					SendMessage (hwnd, WM_COMMAND,IDM_FILE_SAVE, 0);
				}
				if(Answer == IDNO)
				{
					Saved = TRUE;
				}
				if(Answer == IDCANCEL) //取消，不会有任何操作
					return 0;
			}
			if(PaintFileOpenDlg(hwnd,szFileName,szTitleName))
			{

				ofn.Flags = OFN_HIDEREADONLY | OFN_CREATEPROMPT ;
				{
					hFile=CreateFile(ofn.lpstrFile,GENERIC_READ,0,NULL,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
					shapes.clear();
					while(ReadFile(hFile,&temp,sizeof(Record),&dwByteRead,NULL)!=0&&dwByteRead!=0)
						shapes.push_back(temp);
					CloseHandle(hFile);
					InvalidateRect(hwnd,NULL,TRUE);
				}
			}
			//DoCaption(hwnd,szTitleName);
			Saved=TRUE;
				SendData1(hwnd,message,wParam,lParam);
			return 0;
			break;

		case IDM_FILE_SAVE  : //保存文件
			SendData1(hwnd,message,wParam,lParam);
			if (szFileName[0])
			{
				vector<Record>::iterator it;
				DWORD dwByteWritten;
				HANDLE hFile;
				ofn.Flags= OFN_OVERWRITEPROMPT ;
				{
					hFile=CreateFile(ofn.lpstrFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
					for(it=shapes.begin();it!=shapes.end();it++)
					{
						temp=*it;
						WriteFile(hFile,&temp,sizeof(Record),&dwByteWritten,NULL);
					}
					Saved = TRUE;
					CloseHandle(hFile);
				}
			}
			else if (PaintFileSaveDlg(hwnd,szFileName,szTitleName))
			{
				//DoCaption(hwnd,szTitleName);
				vector<Record>::iterator it;
				Record temp;
				DWORD dwByteWritten;
				HANDLE hFile;
				ofn.Flags= OFN_OVERWRITEPROMPT ;
				{
					hFile=CreateFile(ofn.lpstrFile,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_FLAG_SEQUENTIAL_SCAN,NULL);
					for(it=shapes.begin();it!=shapes.end();it++)
					{
						temp=*it;
						WriteFile(hFile,&temp,sizeof(Record),&dwByteWritten,NULL);
					}
					Saved = TRUE;
					CloseHandle(hFile);
				}
			}
			Saved=TRUE;
			return 0;

		case IDM_APP_EXIT: //退出
			SendData1(hwnd,message,wParam,lParam);
			SendMessage(hwnd,WM_CLOSE,0,0);
			return 0 ;

		case IDM_EDIT_CHOOSE  :	//选择
			Move=FALSE;
			if (choose==TRUE)
			{
				choose=FALSE;
				kind=-1;
				CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			}
			else
			{
				choose=TRUE;
				CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
				iSelection=LOWORD(wParam);
				CheckMenuItem(hMenu,iSelection,MF_CHECKED);
				SetCursor (LoadCursor (NULL, IDC_ARROW));
			}
			//SendData1(hwnd,message,wParam,lParam);
			return 0;
		case IDM_EDIT_MOVE:  //移动图形
			choose=FALSE;
			if (Move==TRUE)
			{
				Move=FALSE;
				kind=-1;
				CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			}
			else
			{
				Move=TRUE;
				CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
				iSelection=LOWORD(wParam);
				CheckMenuItem(hMenu,iSelection,MF_CHECKED);
				SetCursor (LoadCursor (NULL, IDC_ARROW));
			}
			//SendData1(hwnd,message,wParam,lParam);
			return 0;
		case IDM_EDIT_DELETE  :  //删除
			SendData1(hwnd,message,wParam,lParam);
			for (i=0;i<shapes.size();i++)
				if (shapes[i].ifchoosed)
				{
					shapes[i].ifDelete=TRUE;
					Saved=FALSE;
					deleted=TRUE;
				}
				EnableMenuItem(hMenu,IDM_EDIT_DELETE,MF_GRAYED);
				InvalidateRect(hwnd,NULL,TRUE);
				return 0;
		case IDM_WIDE_ONE: //线条粗细：1
			SendData1(hwnd,message,wParam,lParam);
			wide=1;
			CheckMenuItem(hMenu,iSelection1,MF_UNCHECKED);
			iSelection1=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection1,MF_CHECKED);
			return 0;
		case IDM_WIDE_TWO://线条粗细：2
			SendData1(hwnd,message,wParam,lParam);
			wide=2;
			CheckMenuItem(hMenu,iSelection1,MF_UNCHECKED);
			iSelection1=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection1,MF_CHECKED);
			return 0;
		case IDM_WIDE_THREE: //线条粗细：3
			SendData1(hwnd,message,wParam,lParam);
			wide=3;
			CheckMenuItem(hMenu,iSelection1,MF_UNCHECKED);
			iSelection1=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection1,MF_CHECKED);
			return 0;

		case IDM_EDIT_ZOOMIN  : //放大
		//	SendData1(hwnd,message,wParam,lParam);
			if (Scale<=8)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMOUT,MF_ENABLED);
				Scale *= 2;
				SetScrollSize(hwnd,Canvas,Client,ScrollPos,Scale,PixelPerScroll);

			}
			if (Scale>8)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMIN,MF_GRAYED);
			}
			break;
		case IDM_EDIT_ZOOMOUT : //缩小
			//SendData1(hwnd,message,wParam,lParam);
			if (Scale>=2)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMIN,MF_ENABLED);
				Scale /= 2;
				SetScrollSize(hwnd,Canvas,Client,ScrollPos,Scale,PixelPerScroll);
			}
			if (Scale<2)
			{
				EnableMenuItem(hMenu,IDM_EDIT_ZOOMOUT,MF_GRAYED);
			}
			return 0;
		case IDM_EDIT_SETCOLOR://设置颜色
		//	SendData1(hwnd,message,wParam,lParam);
		//	SendMessage(hwnd,message,WM_ColorChange1,lParam);
			setColor=ColorUserDef();
			return 0;
		case IDM_EDIT_SETCOLOR_FULL: //设置填充颜色
	//		SendData1(hwnd,message,wParam,lParam);
			setColor_Fule=ColorUserDef();
			return 0;
		case IDM_TOOL_LINE: //直线
			SendData1(hwnd,message,wParam,lParam);
			Move=FALSE;
			choose=FALSE;
			kind=IDM_TOOL_LINE;
			CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			iSelection=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection,MF_CHECKED);
			return 0;
		case IDM_TOOL_ELLIPSE: //选择不填充的椭圆  
			SendData1(hwnd,message,wParam,lParam);
			Move=FALSE;
			choose=FALSE;
			kind=IDM_TOOL_ELLIPSE;
			CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			iSelection=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection,MF_CHECKED);
			break;

		case IDM_TOOL_ELLIPSE1: //选择填充的椭圆
			SendData1(hwnd,message,wParam,lParam);
			Move=FALSE;
			choose=FALSE;
			kind=IDM_TOOL_ELLIPSE1;
			CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			iSelection=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection,MF_CHECKED);
			break;
		case IDM_TOOL_RECT :  //选择不填充的矩形
			SendData1(hwnd,message,wParam,lParam);
			Move=FALSE;
			choose=FALSE;
			kind=IDM_TOOL_RECT;
			CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			iSelection=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection,MF_CHECKED);
			break;
		case IDM_TOOL_RECT1 :  //选择填充的颜色
			SendData1(hwnd,message,wParam,lParam);
			Move=FALSE;
			choose=FALSE;
			kind=IDM_TOOL_RECT1;
			CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			iSelection=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection,MF_CHECKED);
			break;
		case IDM_TOOL_POLYGON: //选择多边形
			SendData1(hwnd,message,wParam,lParam);
				Move=FALSE;
			choose=FALSE;
			kind=IDM_TOOL_POLYGON;
			CheckMenuItem(hMenu,iSelection,MF_UNCHECKED);
			iSelection=LOWORD(wParam);
			CheckMenuItem(hMenu,iSelection,MF_CHECKED);
			return 0;

		case IDM_HELP_ABOUT: //在帮助菜单中显示“关于”
			SendData1(hwnd,message,wParam,lParam);
			MessageBox (hwnd, TEXT ("")
				TEXT ("画板V1！\n"),
				szAppName, MB_ICONINFORMATION | MB_OK) ;
			return 0 ;
		}
		break;

	case WM_LBUTTONDOWN: //鼠标左键
		SendData1(hwnd,message,wParam,lParam);
		data.finish=FALSE;
		hMenu = GetMenu(hwnd);

		prePoint.x = LOWORD(lParam);
		prePoint.y= HIWORD(lParam);
		Trace temp;
		temp.point = prePoint;
		temp.penIndex = 1;
		trace.push_back(temp);         //向trace中插入temp点

		if (Move==FALSE&&choose==FALSE&&(kind==IDM_TOOL_LINE||kind==IDM_TOOL_ELLIPSE||kind==IDM_TOOL_RECT||kind==IDM_TOOL_ELLIPSE1||kind==IDM_TOOL_RECT1)) //未被选中，不移动
		{
			Saved=FALSE;
			SetCursor (LoadCursor (NULL, IDC_CROSS));
			begin.x=LOWORD(lParam);
			begin.y=HIWORD(lParam);
			GetPoint(hwnd,lParam,begin,Canvas,ScrollPos,Scale,PixelPerScroll);
			data.beg_end[0].x=begin.x;
			data.beg_end[0].y=begin.y;
			data.line_wide=wide;
			data.tool_kind=kind;
			data.ifchoosed=FALSE;
			data.move=FALSE;
			data.color=setColor;
			data.full_color=setColor_Fule;
			data.ifDelete=FALSE;
			mouse=TRUE;
		}
		else if (Move==FALSE&&choose==FALSE&&kind==IDM_TOOL_POLYGON) //未被选中
		{
			Saved=FALSE;
			if (n==0)
			{
				begin.x=LOWORD(lParam);
				begin.y=HIWORD(lParam);
				GetPoint(hwnd,lParam,begin,Canvas,ScrollPos,Scale,PixelPerScroll);
				data.beg_end[0].x=begin.x;
				data.beg_end[0].y=begin.y;
				data.tool_kind=kind;
				data.line_wide=wide;
				data.color=setColor;
				data.full_color=setColor_Fule;
				data.ifchoosed=FALSE;
				data.move=FALSE;
				data.ifDelete=FALSE;
				mouse=TRUE;
			}
			else
			{
				size=++n;
				begin.x=LOWORD(lParam);
				begin.y=HIWORD(lParam);
				GetPoint(hwnd,lParam,begin,Canvas,ScrollPos,Scale,PixelPerScroll);
				data.beg_end[size].x=begin.x;
				data.beg_end[size].y=begin.y;
				mouse=TRUE;

				hdc=GetDC(hwnd);
				
				//BitBlt函数对指定的源设备环境区域中的像素进行位块（bit_block）转换，以传送到目标设备环境
				BitBlt(hdc,rcClient.left,rcClient.top,rcClient.right-rcClient.left,rcClient.bottom-rcClient.top,hdcMem,0,0,SRCCOPY);
				hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
				SelectObject(hdc,hpen);
				MoveToEx(hdc,data.beg_end[n-1].x,data.beg_end[n-1].y,NULL);  //功能是将当前绘图位置移动到某个具体的点，同时也可获得之前位置的坐标。
				LineTo(hdc,data.beg_end[n].x,data.beg_end[n].y); //用当前画笔画一条线，从当前位置连到一个指定的点。这个函数调用完毕，当前位置变成x,y
			}
		}
		else if (choose==TRUE&&Move==FALSE) //被选中，但不移动
		{
			begin.x=LOWORD(lParam);
			begin.y=HIWORD(lParam);
			//GetPoint(hwnd,lParam,begin,Canvas,ScrollPos,Scale,PixelPerScroll);
			for (i=0;i<shapes.size();i++)
			{
				switch(shapes[i].tool_kind)
				{
				case IDM_TOOL_LINE:
					if(in_Line(shapes[i].beg_end,begin)) //如果光标在直线上，选中直线
					{
						if(shapes[i].ifchoosed==FALSE)
							shapes[i].ifchoosed=TRUE;
						else
							shapes[i].ifchoosed=FALSE;
					}
					break;
				case IDM_TOOL_ELLIPSE: //如果光标在椭圆内，选中椭圆
					if(in_Ellipse(shapes[i].beg_end,begin))
					{
						if(shapes[i].ifchoosed==FALSE)
							shapes[i].ifchoosed=TRUE;
						else
							shapes[i].ifchoosed=FALSE;
					}
					break;
				case IDM_TOOL_ELLIPSE1:
					if (in_Ellipse1(shapes[i].beg_end,begin))
					{
						if(shapes[i].ifchoosed==FALSE)
							shapes[i].ifchoosed=TRUE;
						else
							shapes[i].ifchoosed=FALSE;
					}
					break;
				case IDM_TOOL_RECT: //如果光标在矩形内，选中矩形
					if(in_Rect(shapes[i].beg_end,begin))
					{
						if(shapes[i].ifchoosed==FALSE)
							shapes[i].ifchoosed=TRUE;
						else
							shapes[i].ifchoosed=FALSE;
					}
					break;
				case IDM_TOOL_RECT1:
					if(in_Rect1(shapes[i].beg_end,begin))
					{
						if(shapes[i].ifchoosed==FALSE)
							shapes[i].ifchoosed=TRUE;
						else
							shapes[i].ifchoosed=FALSE;
					}
					break;
				case IDM_TOOL_POLYGON: //如果光标在多边形内，选中多边形
					if(in_Polygon(shapes[i].beg_end,begin,shapes[i].n))
					{
						if(shapes[i].ifchoosed==FALSE)
							shapes[i].ifchoosed=TRUE;
						else
							shapes[i].ifchoosed=FALSE;
					}
					break;
				default:
					break;
				}
			}
			for (i=0;i<shapes.size();i++)
			{
				if (shapes[i].ifchoosed==TRUE)
				{
					EnableMenuItem(hMenu,IDM_EDIT_DELETE,MF_ENABLED);
					break;
				}
			}
			InvalidateRect(hwnd,NULL,TRUE);
		}
		else if (Move==TRUE)  //被选中且要移动，move=true
		{
			finish=FALSE;
			begin.x=LOWORD(lParam);
			begin.y=HIWORD(lParam);
			for (i=0;i<shapes.size();i++)
			{
				switch(shapes[i].tool_kind)
				{
				case IDM_TOOL_LINE:
					if(in_Line(shapes[i].beg_end,begin))
					{
							shapes[i].move=TRUE;
					}
					break;
				case IDM_TOOL_ELLIPSE:
					if(in_Ellipse(shapes[i].beg_end,begin))
					{
						shapes[i].move=TRUE;
					}
					break;
				case IDM_TOOL_ELLIPSE1:
					if (in_Ellipse1(shapes[i].beg_end,begin))
					{
						shapes[i].move=TRUE;
					}
					break;
				case IDM_TOOL_RECT:
					if(in_Rect(shapes[i].beg_end,begin))
					{
						shapes[i].move=TRUE;
					}
					break;
				case IDM_TOOL_RECT1:
					if(in_Rect1(shapes[i].beg_end,begin))
					{
						shapes[i].move=TRUE;
					}
					break;
				case IDM_TOOL_POLYGON:
					if(in_Polygon(shapes[i].beg_end,begin,shapes[i].n))
					{
						shapes[i].move=TRUE;
					}
					break;
				default:
					break;
				}
			}
		}
		return 0;
	case WM_MOUSEMOVE: //在鼠标移动过程中动态显示要画的图形
		SendData1(hwnd,message,wParam,lParam);
		if(MK_LBUTTON&wParam)     //位运算取值定位，判断鼠标左键是否被按下
			{
				Point point;
				point.x = LOWORD(lParam);
				point.y = HIWORD(lParam);
				hdc = GetDC(hwnd);     ///返回设备描述表句柄
				
				hpen = CreatePen(PS_SOLID, 1,data.color);
				SelectObject(hdc,hpen);	
				MoveToEx(hdc, prePoint.x, prePoint.y, NULL);
				LineTo(hdc, point.x, point.y);

				Trace temp;
				temp.point = point;
				temp.penIndex  = 1;
				trace.push_back(temp);

				ReleaseDC(hwnd, hdc);
				prePoint = point;
			}

		if(mouse) 
			SetCursor(LoadCursor(NULL,IDC_CROSS));

		hdc=GetDC(hwnd);
		if (choose==FALSE&&data.finish==FALSE&&Move==FALSE) //没有被选中，且……
		{
			SetMapMode(hdc,MM_ANISOTROPIC);
			SetWindowExtEx(hdc,4,4,NULL);
			SetViewportExtEx(hdc,Scale,Scale,NULL);
			SetWindowOrgEx(hdc,ScrollPos.x*PixelPerScroll,ScrollPos.y*PixelPerScroll,NULL);
			if (data.tool_kind==IDM_TOOL_LINE||data.tool_kind==IDM_TOOL_ELLIPSE||data.tool_kind==IDM_TOOL_RECT||data.tool_kind==IDM_TOOL_ELLIPSE1||data.tool_kind==IDM_TOOL_RECT1)
			{
				end.x=LOWORD(lParam);
				end.y=HIWORD(lParam);

				if(wParam&MK_CONTROL)
				{
					if_ctrl(data,end,data.tool_kind);
				}
				else
				{
					data.beg_end[1].x=end.x;
					data.beg_end[1].y=end.y;
				}

				BitBlt(hdc,rcClient.left,rcClient.top,rcClient.right-rcClient.left,rcClient.bottom-rcClient.top,hdcMem,0,0,SRCCOPY);

				switch(data.tool_kind)
				{
				case IDM_TOOL_LINE:
					hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
					SelectObject(hdc,hpen);
					MoveToEx(hdc,data.beg_end[0].x,data.beg_end[0].y,NULL);
					LineTo(hdc,data.beg_end[1].x,data.beg_end[1].y);      //随着鼠标移动，画直线

					break;
				case IDM_TOOL_ELLIPSE:
					hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
					SelectObject(hdc,hpen);
					SelectObject (hdc,GetStockObject(NULL_BRUSH));
					Ellipse(hdc,data.beg_end[0].x,data.beg_end[0].y,
						data.beg_end[1].x,data.beg_end[1].y);            //随着鼠标移动，画椭圆
					break;
				case IDM_TOOL_RECT:
					hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
					SelectObject(hdc,hpen);
					SelectObject (hdc,GetStockObject(NULL_BRUSH));
					Rectangle(hdc,data.beg_end[0].x,data.beg_end[0].y,
						data.beg_end[1].x,data.beg_end[1].y);            //随着鼠标移动，画矩形
					break;
				case IDM_TOOL_RECT1:
					hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
					hbrush=CreateSolidBrush(data.full_color);
					SelectObject(hdc,hpen);
					SelectObject (hdc,hbrush);
					Rectangle(hdc,data.beg_end[0].x,data.beg_end[0].y,
						data.beg_end[1].x,data.beg_end[1].y);         //画填充矩形
					break;
				case IDM_TOOL_ELLIPSE1:                
					hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
					hbrush=CreateSolidBrush(data.full_color);
					SelectObject(hdc,hpen);
					SelectObject (hdc,hbrush);
					Ellipse(hdc,data.beg_end[0].x,data.beg_end[0].y,
						data.beg_end[1].x,data.beg_end[1].y);          //画填充椭圆
					break;
				default:
					break;
				}
				//InvalidateRect(hwnd,NULL,TRUE);
			}
			else if (kind==IDM_TOOL_POLYGON )    //画任意多边形
			{
				end.x=LOWORD(lParam);
				end.y=HIWORD(lParam);
				
				BitBlt(hdc,rcClient.left,rcClient.top,rcClient.right-rcClient.left,rcClient.bottom-rcClient.top,hdcMem,0,0,SRCCOPY);
				if(n==0)   //多边形的边数为零
				{
					if(mouse)
					{
						data.beg_end[1].x=end.x;
						data.beg_end[1].y=end.y;
						hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
						SelectObject(hdc,hpen);
						MoveToEx(hdc,data.beg_end[0].x,data.beg_end[0].y,NULL);
						LineTo(hdc,data.beg_end[1].x,data.beg_end[1].y);    //多边形等价为很短的线段
					}

				}
				else  //边数不为零
				{
					data.beg_end[n].x=end.x;
					data.beg_end[n].y=end.y;
					hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
					SelectObject(hdc,hpen);
					MoveToEx(hdc,data.beg_end[n-1].x,data.beg_end[n-1].y,NULL);  //将点移动到鼠标的位置
					LineTo(hdc,data.beg_end[n].x,data.beg_end[n].y);     //画多边形
				}

			}
		}
		
		else if(Move==TRUE&&finish==FALSE)        //移动图形
		{
			end.x=LOWORD(lParam);
			end.y=HIWORD(lParam);
			for (i=0;i<shapes.size();i++)
			{
				if (shapes[i].move&&shapes[i].tool_kind!=IDM_TOOL_POLYGON)    //非任意多边形
				{	
					shapes[i].beg_end[0].x=shapes[i].beg_end[0].x+end.x-begin.x;
					shapes[i].beg_end[0].y=shapes[i].beg_end[0].y+end.y-begin.y;
					shapes[i].beg_end[1].x=shapes[i].beg_end[1].x+end.x-begin.x;
					shapes[i].beg_end[1].y=shapes[i].beg_end[1].y+end.y-begin.y;
					begin=end;
					break;
				}
				else if(shapes[i].move&&shapes[i].tool_kind==IDM_TOOL_POLYGON)  //任意多边形
				{
					for (j=0;j<shapes[i].n;j++)
					{
						shapes[i].beg_end[j].x=shapes[i].beg_end[j].x+end.x-begin.x;
						shapes[i].beg_end[j].y=shapes[i].beg_end[j].y+end.y-begin.y;
					
					}
					begin=end;
					break;
				}
			}
			InvalidateRect(hwnd,NULL,TRUE);
			//BitBlt(hdc,rcClient.left,rcClient.top,rcClient.right-rcClient.left,rcClient.bottom-rcClient.top,hdcMem,0,0,SRCCOPY);
		}
	
		return 0;
	case WM_ERASEBKGND: //擦除背景
		//SendData1(hwnd,message,wParam,lParam);
		return TRUE;
	case WM_LBUTTONUP: //鼠标左键被释放
		SendData1(hwnd,message,wParam,lParam);
		finish=TRUE;      //过程结束
		
	
		data.finish=TRUE;
		if(mouse&&data.finish==TRUE&&Move==FALSE)
		{
			Saved=FALSE;
		end.x = LOWORD(lParam);
		end.y = HIWORD(lParam);
		GetPoint(hwnd,lParam,end,Canvas,ScrollPos,Scale,PixelPerScroll);
		switch(kind)
		{
		case IDM_TOOL_LINE:
			data.beg_end[1].x=end.x;
			data.beg_end[1].y=end.y;
			shapes.push_back(data);
			InvalidateRect(hwnd,NULL,TRUE);
			break;
		case IDM_TOOL_ELLIPSE:
			data.beg_end[1].x=end.x;
			data.beg_end[1].y=end.y;
			shapes.push_back(data);    //在shapes尾部加入data数据
			InvalidateRect(hwnd,NULL,TRUE);
			break;
		case IDM_TOOL_ELLIPSE1:
			data.beg_end[1].x=end.x;
			data.beg_end[1].y=end.y;
			shapes.push_back(data);   //在shapes尾部加入data数据
			InvalidateRect(hwnd,NULL,TRUE);
			break;
		case IDM_TOOL_RECT:
			data.beg_end[1].x=end.x;
			data.beg_end[1].y=end.y;
			shapes.push_back(data);   //在shapes尾部加入data数据
			InvalidateRect(hwnd,NULL,TRUE);
			break;
		case IDM_TOOL_RECT1:
			data.beg_end[1].x=end.x;
			data.beg_end[1].y=end.y;
			shapes.push_back(data);  //在shapes尾部加入data数据
			InvalidateRect(hwnd,NULL,TRUE);
			break;
		case IDM_TOOL_POLYGON:
			if(n==0)  //多边形的边数为零
			{
				size=++n;
				data.beg_end[size].x=end.x;
				data.beg_end[size].y=end.y;
				InvalidateRect(hwnd,NULL,TRUE);
			}	
			else     //多边形的边数不为零
			{
				size=n;
				data.beg_end[size].x=end.x;
				data.beg_end[size].y=end.y;
				InvalidateRect(hwnd,NULL,TRUE);
			}
			break;
		default:
			break;
		}
	}
	else if (Move)  //需要移动时
	{
		for (i=0;i<shapes.size();i++)
		{
			if (shapes[i].move)
			{
				shapes[i].move=FALSE;
				shapes[i].ifchoosed=FALSE;
			}
		}
		InvalidateRect(hwnd,NULL,TRUE);
	}
	mouse=FALSE;
	return 0;

case WM_RBUTTONDOWN: //鼠标右键被点击时，结束多边形的绘制
	SendData1(hwnd,message,wParam,lParam);
	if (kind==IDM_TOOL_POLYGON)  //只对多边形有效
	{
		data.n=n+1;       
		end.x = LOWORD(lParam);
		end.y = HIWORD(lParam);
		GetPoint(hwnd,lParam,end,Canvas,ScrollPos,Scale,PixelPerScroll);
		shapes.push_back(data);
		n=0;
		InvalidateRect(hwnd,NULL,TRUE);
	}

case WM_PAINT: //窗口被绘制时收到此消息
	hdc=BeginPaint(hwnd,&ps);  //开始绘图
	RECT rect;
	GetClientRect(hwnd, &rect);     //窗口客户区矩形

	DrawText(hdc, L"提示：左键绘画，右键换笔", -1,&rect, DT_SINGLELINE | DT_TOP |DT_CENTER);
		//绘图区域
		Point prePoint;
		Point point;
		if(trace.size()>0)
		{
			prePoint = trace[0].point;
		}
		for(int i=1; i<trace.size();i++)
		{
			point = trace[i].point;
			hpen = CreatePen(PS_SOLID, 1,data.color);
			SelectObject(hdc,hpen);
			MoveToEx(hdc, prePoint.x, prePoint.y, NULL);
			LineTo(hdc, point.x, point.y);
			prePoint=point;
		}

	SetMapMode(hdc,MM_ANISOTROPIC);   //设置指定设备环境的映射方式
	SetWindowExtEx(hdc,4,4,NULL);     //以指定的值为设备环境设置窗口的水平的和垂直的范围，NULL表示返回值为空
	SetViewportExtEx(hdc,Scale,Scale,NULL); //用指定的值来设置指定设备环境坐标的X轴、Y轴范围。，NULL表示返回值为空
	SetWindowOrgEx(hdc,ScrollPos.x*PixelPerScroll,ScrollPos.y*PixelPerScroll,NULL);  //设置窗口原点

	FillRect(hdcMem,&rcClient,(HBRUSH) GetStockObject(WHITE_BRUSH)); //用指定的画刷填充矩形


	for (i=0;i<shapes.size();i++)
	{
		if(shapes[i].ifDelete==FALSE)  //未被删除
		{
			switch(shapes[i].tool_kind)
			{
			case IDM_TOOL_LINE:
				if (shapes[i].ifchoosed==TRUE)
					hpen=CreatePen(PS_DASHDOTDOT,1,shapes[i].color);
				else
				hpen=CreatePen(PS_SOLID,shapes[i].line_wide,shapes[i].color);
				SelectObject(hdcMem,hpen);
				MoveToEx(hdcMem,shapes[i].beg_end[0].x,shapes[i].beg_end[0].y,NULL);
				LineTo(hdcMem,shapes[i].beg_end[1].x,shapes[i].beg_end[1].y);  //用指定画笔画直线
				break;
			case IDM_TOOL_ELLIPSE:
				if (shapes[i].ifchoosed==TRUE)   //如果被选中，则用点-点-划线
					hpen=CreatePen(PS_DASHDOTDOT,1,shapes[i].color);  //点-点-划线
				else
					hpen=CreatePen(PS_SOLID,shapes[i].line_wide,shapes[i].color);  //如未被选中，则用实线
				SelectObject(hdcMem,hpen);
				SelectObject (hdcMem,GetStockObject(NULL_BRUSH));
				Ellipse(hdcMem,shapes[i].beg_end[0].x,shapes[i].beg_end[0].y,
					shapes[i].beg_end[1].x,shapes[i].beg_end[1].y);   //用指定画笔画椭圆
				break;
			case IDM_TOOL_ELLIPSE1:
				if(shapes[i].ifchoosed==TRUE)
					hbrush=CreateHatchBrush(HS_BDIAGONAL,shapes[i].full_color);  //画刷
				else
					hbrush=CreateSolidBrush(shapes[i].full_color); //原画刷
				hpen=CreatePen(PS_SOLID,shapes[i].line_wide,shapes[i].color);
				SelectObject(hdcMem,hpen);
				SelectObject (hdcMem,hbrush);
				Ellipse(hdcMem,shapes[i].beg_end[0].x,shapes[i].beg_end[0].y,
					shapes[i].beg_end[1].x,shapes[i].beg_end[1].y);  //用指定画刷画椭圆
				break; 
			case IDM_TOOL_RECT:
				if (shapes[i].ifchoosed==TRUE)
					hpen=CreatePen(PS_DASHDOTDOT,1,shapes[i].color);
				else
					hpen=CreatePen(PS_SOLID,shapes[i].line_wide,shapes[i].color);
				SelectObject(hdcMem,hpen);
				SelectObject (hdcMem,GetStockObject(NULL_BRUSH));
				Rectangle(hdcMem,shapes[i].beg_end[0].x,shapes[i].beg_end[0].y,
					shapes[i].beg_end[1].x,shapes[i].beg_end[1].y);  //用指定画笔画矩形
				break;
			case IDM_TOOL_RECT1:
				if(shapes[i].ifchoosed==TRUE)
					hbrush=CreateHatchBrush(HS_BDIAGONAL,shapes[i].full_color);
				else
					hbrush=CreateSolidBrush(shapes[i].full_color);
				hpen=CreatePen(PS_SOLID,shapes[i].line_wide,shapes[i].color);
				SelectObject(hdcMem,hpen);
				SelectObject (hdcMem,hbrush);
				Rectangle(hdcMem,shapes[i].beg_end[0].x,shapes[i].beg_end[0].y,
					shapes[i].beg_end[1].x,shapes[i].beg_end[1].y);  //用指定画刷画填充椭圆
				break;
			case IDM_TOOL_POLYGON :
				if (shapes[i].ifchoosed==TRUE)
					hpen=CreatePen(PS_DASHDOTDOT,1,shapes[i].color);
				else
					hpen=CreatePen(PS_SOLID,shapes[i].line_wide,shapes[i].color);
				SelectObject(hdcMem,hpen);
				SelectObject (hdcMem,GetStockObject(NULL_BRUSH));
				Polygon(hdcMem,shapes[i].beg_end,shapes[i].n);   ////用指定画笔画多边形
				break;
			default:
				break;
			}
		}
	}
	if (kind==IDM_TOOL_POLYGON)
	{
		if(data.finish==FALSE)
		{
			for (i=0;i<n;i++)
			{
				hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
				SelectObject(hdcMem,hpen); //该函数选择一对象到指定的设备上下文环境中，该新对象替换先前的相同类型的对象。
				MoveToEx(hdcMem,data.beg_end[i].x,data.beg_end[i].y,NULL); //功能是将当前绘图位置移动到某个具体的点，同时也可获得之前位置的坐标
				LineTo(hdcMem,data.beg_end[i+1].x,data.beg_end[i+1].y); 
			}
		}
		else
		{
			for (i=0;i<n;i++)
			{
				hpen=CreatePen(PS_SOLID,data.line_wide,data.color);
				SelectObject(hdcMem,hpen);
				MoveToEx(hdcMem,data.beg_end[i].x,data.beg_end[i].y,NULL);
				LineTo(hdcMem,data.beg_end[i+1].x,data.beg_end[i+1].y);
			}
		}
	}

	BitBlt(hdc,rcClient.left,rcClient.top,rcClient.right-rcClient.left,rcClient.bottom-rcClient.top,hdcMem,0,0,SRCCOPY);
	ReleaseDC(hwnd,hdc);
	DeleteObject(hpen);
	EndPaint(hwnd,&ps);
	return 0;
case WM_CLOSE:  //关闭文件
	SendData1(hwnd,message,wParam,lParam);
	if(Saved == FALSE)
		Answer = MessageBox(hwnd,TEXT("文档没有保存,要保存吗?"),TEXT("提示:"),
		MB_YESNOCANCEL|MB_APPLMODAL|MB_ICONQUESTION);
	if(Answer == IDYES)
	{
		SendMessage (hwnd, WM_COMMAND,IDM_FILE_SAVE, 0);
		return 0;
	}
	if(Answer == IDNO)
	{
		DestroyWindow(hwnd);
		return 0;
	}
	if(Answer == IDCANCEL)		
		return 0;

case WM_DESTROY: //窗口销毁
	SendData1(hwnd,message,wParam,lParam);
	PostQuitMessage (0); //该函数向系统表明有个线程有终止请求。通常用来响应WM_DESTROY消息。
	return 0 ;
}

//有很多消息未被处理，需要由默认窗口消息处理函数来处理
return DefWindowProc (hwnd, message, wParam, lParam);
}


COLORREF ColorUserDef()              //颜色自定义（颜色值）
{
	static CHOOSECOLOR cc ;    //该函数创建一个能使用户从中选择颜色的通用颜色对话框。
	static COLORREF    crCustColors[16] ;
	cc.lStructSize    = sizeof (CHOOSECOLOR) ;
	cc.hwndOwner      = NULL ;
	cc.hInstance      = NULL ;
	cc.rgbResult      = RGB (0x80, 0x80, 0x80) ;
	cc.lpCustColors   = crCustColors ;
	cc.Flags          = CC_RGBINIT | CC_FULLOPEN ;
	cc.lCustData      = 0 ;
	cc.lpfnHook       = NULL ;
	cc.lpTemplateName = NULL ;
	ChooseColor(&cc);
	return cc.rgbResult;
}

void DoCaption(HWND hwnd,TCHAR * szTitleName)  //窗口标题栏内容
{
	TCHAR szCaption [64+MAX_PATH];

	wsprintf(szCaption,TEXT("%s -%s"),szAppName,szTitleName[0]?szTitleName:UNTITLED);
	SetWindowText(hwnd,szCaption);
}

BOOL PaintFileOpenDlg(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName)  //打开文件对话框
{
	ofn.hwndOwner         = hwnd ;       //拥有句柄者
	ofn.lpstrFile         = pstrFileName ;  //存放用户选择文件的路径及文件名缓冲区
	ofn.lpstrFileTitle    = pstrTitleName ; //选择文件对话框标题
	ofn.Flags             = OFN_HIDEREADONLY | OFN_CREATEPROMPT ;

	return GetOpenFileName (&ofn) ;  //获取文件名称
}

BOOL PaintFileSaveDlg(HWND hwnd, PTSTR pstrFileName, PTSTR pstrTitleName)  //保存文件对话框
{
	ofn.hwndOwner         = hwnd ;  //拥有句柄者
	ofn.lpstrFile         = pstrFileName ; //存放用户选择文本的路径及文件名、缓存区
	ofn.lpstrFileTitle    = pstrTitleName ;
	ofn.Flags             = OFN_OVERWRITEPROMPT ;

	return GetSaveFileName(&ofn) ;  //获取文件名称
}
/*
初始化文件，把初始化文件作为一个函数，在创建窗口使调用，
这样才不用再保存参数时再写一次
*/

void PaintFileInitialize(HWND hwnd)
{
	static TCHAR szFilter[] = TEXT ("Gif Files (*.GIF)\0*.gif\0")  \
		TEXT ("ASCII Files (*.ASC)\0*.asc\0") \
		TEXT ("All Files (*.*)\0*.*\0\0");
		
  /*关于每个参数的说明及设置详见：http://blog.csdn.net/whoami021/article/details/17156359*/
	ofn.lStructSize       = sizeof (OPENFILENAME) ; 
	ofn.hwndOwner         = hwnd ;    
	ofn.hInstance         = NULL ;
	ofn.lpstrFilter       = szFilter;
	ofn.lpstrCustomFilter = NULL ;
	ofn.nMaxCustFilter    = 0 ;
	ofn.nFilterIndex      = 0 ;
	ofn.lpstrFile         = NULL ;          // Set in Open and Close functions
	ofn.nMaxFile          = MAX_PATH ;
	ofn.lpstrFileTitle    = NULL ;          // Set in Open and Close functions
	ofn.nMaxFileTitle     = MAX_PATH ;
	ofn.lpstrInitialDir   = NULL ;
	ofn.lpstrTitle        = NULL ;
	ofn.Flags             = 0 ;             // Set in Open and Close functions
	ofn.nFileOffset       = 0 ;
	ofn.nFileExtension    = 0 ;
	ofn.lpstrDefExt       = TEXT ("gif") ;
	ofn.lCustData         = 0L ;
	ofn.lpfnHook          = NULL ;
	ofn.lpTemplateName    = NULL ;
}

//判断choose的函数们

BOOL in_Line(POINT beg_end[],POINT cur_pt)  //判断点是否在直线上
{

	double a,b,d,t;
	if (cur_pt.x<min(beg_end[0].x,beg_end[1].x)||cur_pt.x>max(beg_end[0].x,beg_end[1].x)) //如果其坐标超出边界，则不在直线上
	{
		return FALSE;
	}
	else if (cur_pt.y<min(beg_end[0].y,beg_end[1].y)||cur_pt.y>max(beg_end[0].y,beg_end[1].y))
	{
		return FALSE;
	}
	else
	{
		t=(double)(beg_end[1].y-beg_end[0].y)/(double)(beg_end[1].x-beg_end[0].x);
		a=fabs((double)(t*cur_pt.x+(-1)*cur_pt.y+(beg_end[1].y-(t*beg_end[1].x))));
		b=sqrt((double)(1+(t*t)));
		d=a/b;
		if(d<5) return TRUE; 
		else return FALSE;
	}
}

////判断点是否在多边形内
BOOL in_Polygon(POINT beg_end[],POINT cur_pt,int n)
{
	double a,b,d,t;
	int m;
	for(m=0;m<n-1;m++)  //处理多条边，每条表的处理过程类似于直线的处理
	{
		if (cur_pt.x<min(beg_end[m].x,beg_end[m+1].x)||cur_pt.x>max(beg_end[m].x,beg_end[m+1].x))
		{
			continue;
		}
		else if (cur_pt.y<min(beg_end[m].y,beg_end[m+1].y)||cur_pt.y>max(beg_end[m].y,beg_end[m+1].y))
		{
			continue;
		}
		else
		{
			t=(double)(beg_end[m+1].y-beg_end[m].y)/(double)(beg_end[m+1].x-beg_end[m].x);
			a=fabs((double)(t*cur_pt.x+(-1)*cur_pt.y+(beg_end[m+1].y-(t*beg_end[m+1].x))));
			b=sqrt((double)(1+(t*t)));
			d=a/b;
			if(d<5) 
				return TRUE;
			else 
				continue;
		}
	}

	if (cur_pt.x<min(beg_end[0].x,beg_end[m].x)||cur_pt.x>max(beg_end[0].x,beg_end[m].x))
	{
		return FALSE;
	}
	else if (cur_pt.y<min(beg_end[0].y,beg_end[m].y)||cur_pt.y>max(beg_end[0].y,beg_end[m].y))
	{
		return FALSE;
	}
	else
	{
		t=(double)(beg_end[m].y-beg_end[0].y)/(double)(beg_end[m].x-beg_end[0].x);
		a=fabs((double)(t*cur_pt.x+(-1)*cur_pt.y+(beg_end[m].y-(t*beg_end[m].x))));
		b=sqrt((double)(1+(t*t)));
		d=a/b;
		if(d<5) 
			return TRUE;
		else 
			return FALSE;
	}


}

//判断点是否在椭圆内
BOOL in_Ellipse(POINT *beg_end,POINT cur_pt)
{
	if (cur_pt.x<min(beg_end[0].x,beg_end[1].x)||cur_pt.x>max(beg_end[0].x,beg_end[1].x))//超出边界
	{
		return FALSE;
	}
	else if (cur_pt.y<min(beg_end[0].y,beg_end[1].y)||cur_pt.y>max(beg_end[0].y,beg_end[1].y)) //超出边界
	{
		return FALSE;
	}
	else
	{
		double a,b,c,temp;
		double d,t1,t2;
		double f1_x,f2_x,f_y;
		a=abs(beg_end[0].x-beg_end[1].x)/2;//==2a 椭圆的长轴
		b=abs(beg_end[0].y-beg_end[1].y)/2;//==2b 椭圆的短轴
		if(a>=b)
		{
			c=sqrt(a*a-b*b);
			f1_x=min(beg_end[0].x,beg_end[1].x)+a-c;
			f2_x=f1_x+2*c;
			f_y=min(beg_end[0].y,beg_end[1].y)+b;
			t1=(cur_pt.x-f1_x)*(cur_pt.x-f1_x)+(cur_pt.y-f_y)*(cur_pt.y-f_y);
			t2=(cur_pt.x-f2_x)*(cur_pt.x-f2_x)+(cur_pt.y-f_y)*(cur_pt.y-f_y);
			d=sqrt(t1)+sqrt(t2);
		}
		else
		{
			temp=a;
			a=b;
			b=temp;
			c=sqrt(a*a-b*b);
			f1_x=min(beg_end[0].y,beg_end[1].y)+a-c;
			f2_x=f1_x+2*c;
			f_y=min(beg_end[0].x,beg_end[1].x)+b;
			t1=(cur_pt.y-f1_x)*(cur_pt.y-f1_x)+(cur_pt.x-f_y)*(cur_pt.x-f_y);
			t2=(cur_pt.y-f2_x)*(cur_pt.y-f2_x)+(cur_pt.x-f_y)*(cur_pt.x-f_y);
			d=sqrt(t1)+sqrt(t2);
		}
		if(fabs(d-2*a)<5)
			return TRUE;
		else return FALSE;
	}
	return 0;
}

//同上
BOOL in_Ellipse1(POINT *beg_end,POINT cur_pt)
{
	if (cur_pt.x<min(beg_end[0].x,beg_end[1].x)||cur_pt.x>max(beg_end[0].x,beg_end[1].x))
	{
		return FALSE;
	}
	else if (cur_pt.y<min(beg_end[0].y,beg_end[1].y)||cur_pt.y>max(beg_end[0].y,beg_end[1].y))
	{
		return FALSE;
	}
	else
	{
		double a,b,c,temp;
		double d,t1,t2;
		double f1_x,f2_x,f_y;
		a=abs(beg_end[0].x-beg_end[1].x)/2;//==2a
		b=abs(beg_end[0].y-beg_end[1].y)/2;
		if(a>=b)
		{
			c=sqrt(a*a-b*b);
			f1_x=min(beg_end[0].x,beg_end[1].x)+a-c;
			f2_x=f1_x+2*c;
			f_y=min(beg_end[0].y,beg_end[1].y)+b;
			t1=(cur_pt.x-f1_x)*(cur_pt.x-f1_x)+(cur_pt.y-f_y)*(cur_pt.y-f_y);
			t2=(cur_pt.x-f2_x)*(cur_pt.x-f2_x)+(cur_pt.y-f_y)*(cur_pt.y-f_y);
			d=sqrt(t1)+sqrt(t2);
		}
		else
		{
			temp=a;
			a=b;
			b=temp;
			c=sqrt(a*a-b*b);
			f1_x=min(beg_end[0].y,beg_end[1].y)+a-c;
			f2_x=f1_x+2*c;
			f_y=min(beg_end[0].x,beg_end[1].x)+b;
			t1=(cur_pt.y-f1_x)*(cur_pt.y-f1_x)+(cur_pt.x-f_y)*(cur_pt.x-f_y);
			t2=(cur_pt.y-f2_x)*(cur_pt.y-f2_x)+(cur_pt.x-f_y)*(cur_pt.x-f_y);
			d=sqrt(t1)+sqrt(t2);
		}
		if(d<=2*a)
			return TRUE;
		else return FALSE;
	}
	return 0;
}

////判断点是否在矩形内
BOOL in_Rect(POINT *beg_end,POINT cur_pt)
{
	POINT out_1,out_2;
	POINT in_1,in_2;
	out_1.x=min(beg_end[0].x,beg_end[1].x)-3;
	out_1.y=min(beg_end[0].y,beg_end[1].y)-3;
	in_1.x=out_1.x+6;
	in_1.y=out_1.y+6;
	out_2.x=max(beg_end[0].x,beg_end[1].x)+3;
	out_2.y=max(beg_end[0].y,beg_end[1].y)+3;
	in_2.x=out_2.x-6;
	in_2.y=out_2.y-6;
	if (out_1.x<cur_pt.x&&cur_pt.x<out_2.x&&out_1.y<cur_pt.y&&cur_pt.y<out_2.y)
	{
		if (in_1.x<cur_pt.x&&cur_pt.x<in_2.x&&in_1.y<cur_pt.y&&cur_pt.y<in_2.y)
		{
			return FALSE;
		}
		else
			return TRUE;
	}
	else
		return FALSE;

}

//判断点是否在矩形内
BOOL in_Rect1(POINT *beg_end,POINT cur_pt)
{
	POINT out_1,out_2;
	out_1.x=min(beg_end[0].x,beg_end[1].x);
	out_1.y=min(beg_end[0].y,beg_end[1].y);
	out_2.x=max(beg_end[0].x,beg_end[1].x);
	out_2.y=max(beg_end[0].y,beg_end[1].y);
	if (cur_pt.x>=out_1.x&&cur_pt.x<=out_2.x&&cur_pt.y>=out_1.y&&cur_pt.y<=out_2.y)
		return TRUE;
	else 
		return FALSE;
}

//设置滚动尺寸
void SetScrollSize(HWND hwnd,const SIZE &Canvas,const SIZE &Client,POINT &ScrollPos,int Scale,int PixelPerScroll)
{
	ScrollPos.x = 0;
	ScrollPos.y = 0;
	SCROLLINFO ScrInfo;
	ScrInfo.cbSize = sizeof(SCROLLINFO);
	ScrInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	ScrInfo.nMin = 0;
	ScrInfo.nPos = 0;

	ScrInfo.nMax = Canvas.cx*Scale/4/PixelPerScroll;
	ScrInfo.nPage = Client.cx/PixelPerScroll;
	SetScrollInfo(hwnd,SB_HORZ,&ScrInfo,TRUE);

	ScrInfo.nMax = Canvas.cy*Scale/4/PixelPerScroll;
	ScrInfo.nPage = Client.cy/PixelPerScroll;
	SetScrollInfo(hwnd,SB_VERT,&ScrInfo,TRUE);
	InvalidateRect(hwnd,NULL,TRUE);
}

//获取点的坐标
void GetPoint(HWND hwnd,LPARAM lParam,POINT &pt,SIZE &Canvas,const POINT &ScrollPos,int Scale,int PixelPerScroll)
{
	HDC hdc = GetDC(hwnd);
	SetMapMode(hdc,MM_ANISOTROPIC);
	SetWindowExtEx(hdc,4,4,NULL);
	SetViewportExtEx(hdc,Scale,Scale,NULL);
	SetWindowOrgEx(hdc,ScrollPos.x*PixelPerScroll,ScrollPos.y*PixelPerScroll,NULL);
	DPtoLP(hdc,&pt,1);
	ReleaseDC(hwnd,hdc);
	if (pt.x>Canvas.cx)
		Canvas.cx = pt.x;
	if (pt.y>Canvas.cy)
		Canvas.cy = pt.y;
}

//
void if_ctrl(Record &data,POINT &end,int kind)
{
	int i,j,m;
	double d=0,n=0;
	i=data.beg_end[0].x-end.x;
	j=data.beg_end[0].y-end.y;
	m=min(abs(i),abs(j));
	if (data.beg_end[0].x<end.x&&data.beg_end[0].y>=end.y)
	{
		data.beg_end[1].x=data.beg_end[0].x+m;
		data.beg_end[1].y=data.beg_end[0].y-m;
	}
	else if(data.beg_end[0].x<end.x&&data.beg_end[0].y<=end.y)
	{
		data.beg_end[1].x=data.beg_end[0].x+m;
		data.beg_end[1].y=data.beg_end[0].y+m;
	}
	else if (data.beg_end[0].x>=end.x&&data.beg_end[0].y<end.y)
	{
		data.beg_end[1].x=data.beg_end[0].x-m;
		data.beg_end[1].y=data.beg_end[0].y+m;
	}
	else if (data.beg_end[0].x>=end.x&&data.beg_end[0].y>end.y)
	{
		data.beg_end[1].x=data.beg_end[0].x-m;
		data.beg_end[1].y=data.beg_end[0].y-m;
	}
	if (kind==IDM_TOOL_LINE)
	{
		if (abs(j)<abs(i)/2)
		{
			if (data.beg_end[0].x>end.x)
			{
				data.beg_end[1].x=data.beg_end[0].x-abs(i);
				data.beg_end[1].y=data.beg_end[0].y;
			}
			else
			{
				data.beg_end[1].x=data.beg_end[0].x+abs(i);
				data.beg_end[1].y=data.beg_end[0].y;
			}
		}
		if (abs(j)/2>abs(i))
		{
			if (data.beg_end[0].y>end.y)
			{
				data.beg_end[1].x=data.beg_end[0].x;
				data.beg_end[1].y=data.beg_end[0].y-abs(j);
			}
			else
			{
				data.beg_end[1].x=data.beg_end[0].x;
				data.beg_end[1].y=data.beg_end[0].y+abs(j);
			}
		}
	}
}


void SendData1(HWND hwnd,UINT message, WPARAM wParam, LPARAM lParam)
{
	HWND hwndTarget ;
	hwndTarget= FindWindowEx(NULL,hwnd,NULL,szAppName); //在窗口列表中寻找与指定条件相符的第一个子窗口 。
	if (hwndTarget!=NULL)
		SendMessage(hwndTarget,message,wParam,lParam);  //此函数为指定的窗口调用窗口程序，直到窗口程序处理完消息再返回。
}
