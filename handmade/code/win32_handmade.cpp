#include <windows.h>
#include <cstdint>
#include <Xinput.h>

#define internal        static
#define local_persist   static
#define global_variable static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void* Memory;
    int Width;
    int Height;
    int BytesPerPixel;
    int Pitch;
};

//TODO: This shouldn't be a global.
global_variable bool Running;
global_variable win32_offscreen_buffer GlobalBackbuffer;

struct win32_window_dimension
{
    int Width;
    int Height;
};

// =====================================================================================================================


//NOTE: If we try to call the XInput API directly, we would get an access violation, since we are not linking with the
//  necessary lib file. However, we're getting around this by creating stubs for these function calls, and we can
//  load the function definitions later.
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

// =====================================================================================================================

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    int Width = Buffer->Width;
    int Height = Buffer->Height;

    int Pitch = Width * Buffer->BytesPerPixel;
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y) 
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            // Pixel Layout in Memory = BB GG RR xx
            // Register value = xx RR GG BB
            uint8 B = (X + BlueOffset);
            uint8 G = (Y + GreenOffset);


            *Pixel++ = ((G << 8) | B); // Blue assigned to smallest 2 bytes (uint8), Green moved left to its position.  
        }
        Row += Pitch;
    }
}

// =====================================================================================================================

internal void Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if (!XInputLibrary)
    {
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState) {XInputGetState = XInputGetStateStub;}
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState) {XInputSetState = XInputSetStateStub;}
    }
}

// =====================================================================================================================

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return (Result);
}

// =====================================================================================================================

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    //TODO: Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;
    Buffer->BytesPerPixel = 4;

    //NOTE: Assigning biHeight as negative is the clue to Windows to treat the bitmap as top-down, not bottom up.
    //  So the first pixel is going to be the top left, and the last is going to be the bottom right, and it will go by
    //  row.
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32; // RR + GG + BB + padding
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
//    RenderWeirdGradient(128, 0);
    Buffer->Pitch = Width * Buffer->BytesPerPixel;
}

// =====================================================================================================================

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, 
        int WindowWidth, int WindowHeight)
{
    //TODO: Aspect-ratio correction
    StretchDIBits(
            DeviceContext,
            /*X, Y, Width, Height,
            X, Y, Width, Height,*/
            0, 0, WindowWidth, WindowHeight,
            0, 0, Buffer->Width, Buffer->Height,
            Buffer->Memory,
            &Buffer->Info,
            DIB_RGB_COLORS, SRCCOPY);
}

// =====================================================================================================================

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window,
                            UINT Message,
                            WPARAM WParam,
                            LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_SIZE:
        {
        } break;

        case WM_DESTROY:
        {
            //TODO: Handle this as an error - recreate window?
            Running = false;
        } break;

        case WM_CLOSE:
        {
            //TODO: Handle this with a message to the user?
            Running = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0); // bit 30 says whether the key was down or not. 1 is yes, 0 is no.
            bool IsDown = ((LParam & (1 << 31)) == 0); // bit 31 says whether the key is currently down or not.

            if (WasDown != IsDown)
            {
                if (VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_UP)
                {
                }
                else if (VKCode == VK_LEFT)
                {
                }
                else if (VKCode == VK_RIGHT)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                }
                else if (VKCode == VK_SPACE)
                {
                }
            }
            bool AltKeyWasDown = ((LParam & (1 << 29)) != 0);
            if ((VKCode == VK_F4) && AltKeyWasDown)
            {
                Running = false;
            }
        } break;
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);

            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            LONG Width = Paint.rcPaint.right - Paint.rcPaint.left;
            LONG Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return(Result);
}

// =====================================================================================================================

int CALLBACK WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int ShowCode)
{
    Win32LoadXInput(); // This is where we load the XInput functions we're using to get around linking with the lib.
    
    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);

    //NOTE: I can specify CS_OWNDC. What that does is allow us to do is get the device context once and just own it
    //  forever, meaning the HDC DeviceContext that shows up later does not have to be released.
    //NOTE: Since the Windows kernel will handle memory cleanup when the application closes, we don't need to worry
    //  about releasing the device context ever.
    WindowClass.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClass(&WindowClass)) 
    {
        
        HWND Window = CreateWindowEx(
                0, WindowClass.lpszClassName, "Handmade Hero",
                WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                CW_USEDEFAULT, CW_USEDEFAULT,
                CW_USEDEFAULT, CW_USEDEFAULT,
                0, 0, Instance, 0);
        if (Window)
        {
            Running = true;
            
            HDC DeviceContext = GetDC(Window);
            int XOffset = 0;
            int YOffset = 0;
            while(Running)
            {
                MSG Message;
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        Running = false;
                    }
                    TranslateMessage(&Message);
                    DispatchMessage(&Message);
                }
                
                //TODO: Should we poll this more frequently??? It would make sense that we might want to poll more
                //  often than once per frame.
                for(DWORD ControllerIndex = 0;
                        ControllerIndex < XUSER_MAX_COUNT;
                        ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        //NOTE: This controller is available
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;
                    }
                    else 
                    {
                        //NOTE: This controller is NOT available
                    }
                }

                RenderWeirdGradient(&GlobalBackbuffer, XOffset, YOffset);
                win32_window_dimension Dimension = Win32GetWindowDimension(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
                ++XOffset;
            }
        }
        else
        {
            //TODO: Logging
        }
    }
    else 
    {
        //TODO: Logging
    }


    return(0);
}
