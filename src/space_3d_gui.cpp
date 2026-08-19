#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>
#include "space_3d_core.h"
#include "space_3d_runtime.h"

namespace {
enum { ID_ADD=100,ID_CLEAR,ID_PAUSE,ID_STEP,ID_COLLISION,ID_DROP,ID_FOUNTAIN,ID_IMPULSE,ID_SAVE,ID_LOAD,ID_BODY_RESET,ID_AUTOMATION,ID_TIMER=1 };
space3d::Space3DEngine engine;
space3d::HumanoidController body;
space3d::BodyRuntime body_runtime;
RECT viewport{16,60,920,710};
HWND info_box=nullptr;
std::uint32_t selected=0;
bool dragging=false;
bool jump_queued=false;
int last_x=0,last_y=0;

POINT Project(const space3d::Vector3& p,bool& visible){
 auto s=engine.GetCamera().Project(p,float(viewport.right-viewport.left),float(viewport.bottom-viewport.top));
 visible=s.visible;return{viewport.left+LONG(s.x),viewport.top+LONG(s.y)};
}
void Line3D(HDC dc,const space3d::Vector3&a,const space3d::Vector3&b){bool va=false,vb=false;POINT pa=Project(a,va),pb=Project(b,vb);if(va&&vb){MoveToEx(dc,pa.x,pa.y,nullptr);LineTo(dc,pb.x,pb.y);}}
void DrawSkeleton(HDC dc){
 const auto pose=body.BuildPose();
 HPEN bone=CreatePen(PS_SOLID,4,RGB(90,245,235));HGDIOBJ old=SelectObject(dc,bone);
 for(const auto& pair:space3d::HumanoidController::Bones()) Line3D(dc,pose.joints[static_cast<size_t>(pair.first)],pose.joints[static_cast<size_t>(pair.second)]);
 SelectObject(dc,old);DeleteObject(bone);
 HBRUSH joint=CreateSolidBrush(RGB(255,225,90));old=SelectObject(dc,joint);HPEN outline=CreatePen(PS_SOLID,1,RGB(20,30,35));HGDIOBJ oldpen=SelectObject(dc,outline);
 for(const auto&p:pose.joints){bool visible=false;POINT q=Project(p,visible);if(visible)Ellipse(dc,q.x-4,q.y-4,q.x+5,q.y+5);}
 SelectObject(dc,oldpen);SelectObject(dc,old);DeleteObject(outline);DeleteObject(joint);
}
void DrawScene(HDC dc){
 HBRUSH bg=CreateSolidBrush(RGB(10,14,24));FillRect(dc,&viewport,bg);DeleteObject(bg);
 HPEN grid=CreatePen(PS_SOLID,1,RGB(40,55,72));HGDIOBJ old_pen=SelectObject(dc,grid);float e=engine.GetSettings().half_extent,y=engine.GetSettings().ground_y;
 for(int i=-15;i<=15;i+=2){Line3D(dc,{float(i),y,-e},{float(i),y,e});Line3D(dc,{-e,y,float(i)},{e,y,float(i)});}DeleteObject(SelectObject(dc,old_pen));
 HPEN red=CreatePen(PS_SOLID,3,RGB(235,70,70));old_pen=SelectObject(dc,red);Line3D(dc,{0,y,0},{4,y,0});DeleteObject(SelectObject(dc,old_pen));
 HPEN green=CreatePen(PS_SOLID,3,RGB(70,235,110));old_pen=SelectObject(dc,green);Line3D(dc,{0,y,0},{0,y+4,0});DeleteObject(SelectObject(dc,old_pen));
 HPEN blue=CreatePen(PS_SOLID,3,RGB(80,130,255));old_pen=SelectObject(dc,blue);Line3D(dc,{0,y,0},{0,y,4});DeleteObject(SelectObject(dc,old_pen));
 struct Item{const space3d::Ball* ball;space3d::ScreenPoint point;};std::vector<Item> items;float w=float(viewport.right-viewport.left),h=float(viewport.bottom-viewport.top);
 for(const auto&b:engine.GetBalls()){auto p=engine.GetCamera().Project(b.position,w,h);if(p.visible)items.push_back({&b,p});}
 std::sort(items.begin(),items.end(),[](const Item&a,const Item&b){return a.point.depth>b.point.depth;});
 for(const auto&i:items){const auto&b=*i.ball;auto edge=engine.GetCamera().Project(b.position+engine.GetCamera().Right()*b.radius,w,h);int r=std::max(3,int(std::abs(edge.x-i.point.x)));int x=viewport.left+int(i.point.x),yy=viewport.top+int(i.point.y);int shade=std::max(40,220-int(i.point.depth*3));HBRUSH brush=CreateSolidBrush(RGB(shade,100+(b.id*37)%120,220));HPEN pen=CreatePen(PS_SOLID,b.id==selected?3:1,b.id==selected?RGB(255,230,70):RGB(230,240,255));HGDIOBJ ob=SelectObject(dc,brush),op=SelectObject(dc,pen);Ellipse(dc,x-r,yy-r,x+r,yy+r);SelectObject(dc,ob);SelectObject(dc,op);DeleteObject(brush);DeleteObject(pen);}
 DrawSkeleton(dc);
 SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(210,220,235));const wchar_t* help=L"Arrows/WASD: manual move   Shift: run   Space: jump   A: auto on/off   Drag: orbit";TextOutW(dc,viewport.left+12,viewport.bottom-24,help,lstrlenW(help));
}
void UpdateInfo(){
 auto d=engine.GetDiagnostics();const auto&s=body.GetState();std::wostringstream text;
 text<<L"SPACE 3D + TIMED BODY\r\nClock: "<<body_runtime.LocalClockText()<<L"\r\n\r\n"<<body_runtime.StatusText()<<L"\r\n\r\nSkeleton height: "<<space3d::HumanoidController::HeightMeters<<L" m\r\nSpeed: "<<int(s.horizontal_speed_mps*100)/100.0<<L" m/s\r\nBody mode: "<<(s.grounded?(s.running?L"RUN":(s.horizontal_speed_mps>.08f?L"WALK":L"STAND")):L"JUMP")<<L"\r\nPosition: ("<<int(s.position.x*10)/10.0<<L", "<<int(s.position.y*10)/10.0<<L", "<<int(s.position.z*10)/10.0<<L") m\r\n\r\nHuman-scale settings:\r\nWalk 1.4 m/s\r\nRun 3.5 m/s\r\nGravity 9.81 m/s²\r\n\r\nPhysics objects: "<<d.ball_count<<L"\r\nSteps: "<<d.simulation_steps<<L"\r\nCollisions: "<<d.collision_count<<L"\r\n\r\nRandom timed tasks loop continuously. Manual input temporarily takes control.";
 SetWindowTextW(info_box,text.str().c_str());
}
void AddButton(HWND hwnd,int id,const wchar_t*text,int x,int width=62){CreateWindowW(L"BUTTON",text,WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,x,16,width,28,hwnd,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),nullptr,nullptr);}
void SaveScene(HWND hwnd){std::ofstream f("space_3d_scene.txt",std::ios::binary);f<<engine.Serialize();MessageBoxW(hwnd,f?L"Scene saved.":L"Could not save scene.",L"Space 3D",f?MB_OK:MB_OK|MB_ICONERROR);}
void LoadScene(HWND hwnd){std::ifstream f("space_3d_scene.txt",std::ios::binary);std::string t((std::istreambuf_iterator<char>(f)),{}),error;if(!f||!engine.Deserialize(t,error)){std::wstring w(error.begin(),error.end());MessageBoxW(hwnd,w.empty()?L"Could not load scene.":w.c_str(),L"Space 3D",MB_OK|MB_ICONERROR);}selected=0;}
void SelectAt(int x,int y){float w=float(viewport.right-viewport.left),h=float(viewport.bottom-viewport.top);auto ray=engine.GetCamera().MakeRay(float(x-viewport.left),float(y-viewport.top),w,h);selected=engine.Raycast(ray).ball_id;}
void AddAt(int x,int y){float w=float(viewport.right-viewport.left),h=float(viewport.bottom-viewport.top);auto ray=engine.GetCamera().MakeRay(float(x-viewport.left),float(y-viewport.top),w,h);float plane=engine.GetSettings().ground_y+3;float t=std::abs(ray.direction.y)>1e-5f?(plane-ray.origin.y)/ray.direction.y:10.0f;if(t<1)t=10;auto p=ray.origin+ray.direction*t;selected=engine.AddBall(p,{ray.direction.x*4,6,ray.direction.z*4},.55f,1);}
bool Down(int key){return (GetAsyncKeyState(key)&0x8000)!=0;}
void UpdateBody(HWND hwnd){
 space3d::HumanoidInput input;
 bool manual=false;
 if(GetForegroundWindow()==hwnd){input.forward=(Down(VK_UP)||Down('W')?1.0f:0.0f)-(Down(VK_DOWN)||Down('S')?1.0f:0.0f);input.right=(Down(VK_RIGHT)||Down('D')?1.0f:0.0f)-(Down(VK_LEFT)||Down('A')?1.0f:0.0f);input.run=Down(VK_SHIFT);input.jump=jump_queued;manual=std::abs(input.forward)>.01f||std::abs(input.right)>.01f||input.jump;}
 jump_queued=false;
 const auto settings=engine.GetSettings();body_runtime.Update(.016f,body,input,manual,engine.GetCamera().Forward(),engine.GetCamera().Right(),settings.ground_y,settings.half_extent);
 const auto&p=body.GetState().position;engine.GetCamera().target={p.x,p.y+.9f,p.z};
}
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
 switch(msg){
 case WM_CREATE:AddButton(hwnd,ID_ADD,L"Add",16,48);AddButton(hwnd,ID_CLEAR,L"Clear",68,54);AddButton(hwnd,ID_PAUSE,L"Pause",126,58);AddButton(hwnd,ID_STEP,L"Step",188,52);AddButton(hwnd,ID_COLLISION,L"Collision",244,72);AddButton(hwnd,ID_DROP,L"Drop",320,50);AddButton(hwnd,ID_FOUNTAIN,L"Fountain",374,70);AddButton(hwnd,ID_IMPULSE,L"Impulse",448,66);AddButton(hwnd,ID_SAVE,L"Save",518,50);AddButton(hwnd,ID_LOAD,L"Load",572,50);AddButton(hwnd,ID_BODY_RESET,L"Reset Body",626,88);AddButton(hwnd,ID_AUTOMATION,L"Auto On/Off",718,90);info_box=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,936,64,280,646,hwnd,nullptr,nullptr,nullptr);body_runtime.Start();SetTimer(hwnd,ID_TIMER,16,nullptr);UpdateInfo();return 0;
 case WM_COMMAND:switch(LOWORD(wp)){case ID_ADD:selected=engine.AddBall({0,8,0},{4,2,-2},.6f,1);break;case ID_CLEAR:engine.Clear();selected=0;break;case ID_PAUSE:engine.SetPaused(!engine.IsPaused());break;case ID_STEP:engine.StepOnce();break;case ID_COLLISION:engine.Reset(space3d::Preset::CollisionLab);selected=0;break;case ID_DROP:engine.Reset(space3d::Preset::DropTower);selected=0;break;case ID_FOUNTAIN:engine.Reset(space3d::Preset::Fountain);selected=0;break;case ID_IMPULSE:engine.ApplyImpulse(selected,{0,8,4});break;case ID_SAVE:SaveScene(hwnd);break;case ID_LOAD:LoadScene(hwnd);break;case ID_BODY_RESET:body_runtime.Reset(body,engine.GetSettings().ground_y);break;case ID_AUTOMATION:body_runtime.ToggleAutomation();break;}UpdateInfo();InvalidateRect(hwnd,&viewport,FALSE);return 0;
 case WM_LBUTTONDOWN:if(PtInRect(&viewport,{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)})){dragging=true;last_x=GET_X_LPARAM(lp);last_y=GET_Y_LPARAM(lp);SetCapture(hwnd);SelectAt(last_x,last_y);UpdateInfo();}return 0;
 case WM_LBUTTONUP:dragging=false;ReleaseCapture();return 0;
 case WM_LBUTTONDBLCLK:AddAt(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));UpdateInfo();return 0;
 case WM_MOUSEMOVE:if(dragging){int x=GET_X_LPARAM(lp),y=GET_Y_LPARAM(lp);engine.GetCamera().Orbit((x-last_x)*.008f,(y-last_y)*.008f);last_x=x;last_y=y;InvalidateRect(hwnd,&viewport,FALSE);}return 0;
 case WM_MOUSEWHEEL:engine.GetCamera().Zoom(GET_WHEEL_DELTA_WPARAM(wp)>0?-2.0f:2.0f);InvalidateRect(hwnd,&viewport,FALSE);return 0;
 case WM_KEYDOWN:if(wp==VK_SPACE&&!((lp>>30)&1))jump_queued=true;else if(wp=='P')engine.SetPaused(!engine.IsPaused());else if(wp=='A'&&!((lp>>30)&1))body_runtime.ToggleAutomation();else if(wp==VK_DELETE&&selected){engine.RemoveBall(selected);selected=0;}else if(wp=='I')engine.ApplyImpulse(selected,{0,8,4});else if(wp=='R')body_runtime.Reset(body,engine.GetSettings().ground_y);UpdateInfo();return 0;
 case WM_TIMER:engine.Update(.016f);UpdateBody(hwnd);if(selected&&!engine.FindBall(selected))selected=0;UpdateInfo();InvalidateRect(hwnd,&viewport,FALSE);return 0;
 case WM_PAINT:{PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);DrawScene(dc);EndPaint(hwnd,&ps);return 0;}
 case WM_DESTROY:KillTimer(hwnd,ID_TIMER);PostQuitMessage(0);return 0;}
 return DefWindowProcW(hwnd,msg,wp,lp);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show){WNDCLASSW wc{};wc.lpfnWndProc=WndProc;wc.hInstance=instance;wc.lpszClassName=L"LocalDevAiSpace3D";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_BTNFACE+1);wc.style=CS_DBLCLKS;if(!RegisterClassW(&wc))return 1;HWND hwnd=CreateWindowW(wc.lpszClassName,L"Space 3D - Timed Random Skeleton + Real-Time Clock",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,1240,770,nullptr,nullptr,instance,nullptr);if(!hwnd)return 2;ShowWindow(hwnd,show);UpdateWindow(hwnd);MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return int(msg.wParam);}
