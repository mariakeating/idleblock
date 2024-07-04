#include <unistd.h>
#include <dbus/dbus.h>
#include <sys/mman.h>
#include <stdio.h>

#include <inttypes.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef s32 b32;

typedef float f32;
typedef double f64;

#define function static
#define global static

#define ArrayCount(array) (sizeof(array)/sizeof((array)[0]))

#define EmbedPreprocStr2(x) #x
#define EmbedPreprocStr(x) EmbedPreprocStr2(x)

#define EmbedFile(name, file) \
asm(".pushsection .rodata\n" \
".global Global" EmbedPreprocStr(name) "\n" \
".balign 16\n" \
"Global" EmbedPreprocStr(name) ":\n" \
".incbin \"" file "\"\n" \
".balign 1\n" \
".byte 0 \n" \
".global Global" EmbedPreprocStr(name) "Size\n" \
"Global" EmbedPreprocStr(name) "Size:\n" \
".quad . - Global" EmbedPreprocStr(name) " - 1\n" \
".popsection"); \
extern __attribute__((aligned(16))) char Global##name[]; \
extern u64 Global##name##Size

EmbedFile(OnIconMemory, "../data/on.bmp");
EmbedFile(OffIconMemory, "../data/off.bmp");

#define CAT2(a, b) a##b
#define CAT(a, b) CAT2(a,b)

#define IterAppendToBasicArray(type, data, count) IterAppendToBasicArray_(Current, type, (u64)data, count)
function void
IterAppendToBasicArray_(DBusMessageIter *Dest, int Type, u64 Data, int Count)
{
    dbus_message_iter_append_fixed_array(Dest, Type, &Data, Count);
}


#define IterAppendBasicArray(type, signature, data, count) AppendContainer(DBUS_TYPE_ARRAY, signature){IterAppendToBasicArray(type, data, count);}

#define AppendBasic(type, data) AppendBasic_(Current, type, (u64)data)
#define AppendVariant(signature, type, data) AppendContainer(DBUS_TYPE_VARIANT, signature){AppendBasic(type, data);}
function void
AppendBasic_(DBusMessageIter *Dest, int Type, u64 Data)
{
    dbus_message_iter_append_basic(Dest, Type, &Data);
}

#define AppendContainer(type, signature) for(int CAT(_i_, __LINE__) = (++Current, \
dbus_message_iter_open_container(Current-1, type, signature, Current), \
0); \
!CAT(_i_, __LINE__); \
++CAT(_i_, __LINE__), dbus_message_iter_close_container(Current-1, Current), --Current)

#define GetBasic(data) dbus_message_iter_get_basic(Current, data); dbus_message_iter_next(Current)

#define ScopedMethod(name, a, b, c, d) for(DBusMessage *name = dbus_message_new_method_call(a, b, c, d); \
name; \
dbus_message_unref(name), name = 0)
#define ScopedMethodResult(message, name) for(DBusMessage *name = dbus_connection_send_with_reply_and_block(Connection, message, DBUS_TIMEOUT_INFINITE, &Error); \
name; \
dbus_message_unref(name), name = 0)
#define ScopedReply(message, name) for(DBusMessage *name = dbus_message_new_method_return(message); \
name; \
dbus_message_unref(name), name = 0)

#define IterStack DBusMessageIter Stack[16]; DBusMessageIter *Current = Stack
#define InitAppendIter(message) dbus_message_iter_init_append(message, Current)
#define InitAppendIterWithStack(message) IterStack; InitAppendIter(message)
#define InitReadIter(message) dbus_message_iter_init(message, Current)
#define InitReadIterWithStack(message) IterStack; InitReadIter(message)

#define Str(raw) (string){(u8 *)(raw), StringLength(raw)}
#define StrLit(raw) (string){(u8 *)(raw), sizeof(raw) - 1}

typedef struct string string;
struct string
{
    u8 *Data;
    u64 Size;
};

function u64
StringLength(const char *A)
{
    char *B = (char *)A;
    for(;*B;++B);
    
    u64 Result = (u64)(B - A);
    return Result;
}

function b32
StringsAreEqual(string A, string B)
{
    b32 Result = false;
    
    if(A.Size == B.Size)
    {
        Result = true;
        
        for(u32 AIndex = 0;
            AIndex < A.Size;
            ++AIndex)
        {
            if(A.Data[AIndex] != B.Data[AIndex])
            {
                Result = false;
                break;
            }
        }
    }
    
    return Result;
}

#pragma pack(push, 1)
typedef struct bitmap_header bitmap_header;
struct bitmap_header
{
    u16 FileType;
    u32 FileSize;
    u32 Reserved;
    u32 BitmapOffset;
    u32 Size;
    s32 Width;
    s32 Height;
    u16 Planes;
    u16 BitsPerPixel;
    u32 Compression;
    u32 SizeOfBitmap;
    s32 HorizResolution;
    s32 VertResolution;
    u32 ColoursUsed;
    u32 ColoursImportant;
    u32 RedMask;
    u32 GreenMask;
    u32 BlueMask;
};
#pragma pack(pop)

typedef struct image32 image32;
struct image32
{
    u32 Width;
    u32 Height;
    u8 *Memory;
};

function image32
LoadBMP(void *Memory, u64 MemorySize)
{
    image32 Result = {};
    
    bitmap_header *Header = Memory;
    
    u8 *SourceMemory = (u8 *)Memory + Header->BitmapOffset;
    
    Result.Width = (u32)Header->Width;
    Result.Height = (u32)Header->Height;
    Result.Memory = mmap(0, Result.Width*Result.Height*4, PROT_READ|PROT_WRITE,
                         MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    u8 *SourceRow = SourceMemory + Result.Width*4 * (Result.Height - 1);
    u8 *DestRow = Result.Memory;
    for(u32 Y = 0;
        Y < Result.Height;
        ++Y)
    {
        u32 *SourcePixel = (u32 *)SourceRow;
        u32 *DestPixel = (u32 *)DestRow;
        for(u32 X = 0;
            X < Result.Width;
            ++X)
        {
            u32 Colour = *SourcePixel++;
            
            u8 Blue = (Colour >> 0) & 0xFF;
            u8 Green = (Colour >> 8) & 0xFF;
            u8 Red = (Colour >> 16) & 0xFF;
            u8 Alpha = (u8)((Colour >> 24) & 0xFF);
            
            u32 NewColour = (((u32)Blue << 24) | ((u32)Green << 16) | ((u32)Red << 8) | ((u32)Alpha));
            *DestPixel++ = NewColour;
        }
        
        SourceRow -= Result.Width*4;
        DestRow += Result.Width*4;
    }
    
    return Result;
}

enum menu_id
{
    MenuID_Invalid,
    MenuID_Root,
    MenuID_Quit,
};

typedef struct linux_state linux_state;
struct linux_state
{
    b32 Blocking;
    u32 BlockingCookie;
    image32 ActiveIcon;
    image32 InactiveIcon;
};

global b32 GlobalRunning;
global linux_state GlobalLinuxState;

#include<stdio.h>

typedef enum tray_property tray_property;
enum tray_property
{
    TrayProperty_Category,
    TrayProperty_ID,
    TrayProperty_Title,
    TrayProperty_Status,
    TrayProperty_Menu,
    TrayProperty_ItemIsMenu,
    TrayProperty_IconName,
    TrayProperty_IconThemePath,
    TrayProperty_IconPixmap,
    
    TrayProperty_Count,
};

global char *TrayPropertyNames[] =
{
    "Category",
    "Id",
    "Title",
    "Status",
    "Menu",
    "ItemIsMenu",
    "IconName",
    "IconThemePath",
    "IconPixmap",
};

_Static_assert(ArrayCount(TrayPropertyNames) == TrayProperty_Count, "Property name count not equal to amount of properties.");

#define AppendTrayProperty(property) AppendTrayProperty_(Current, property)
function void
AppendTrayProperty_(DBusMessageIter *Current, tray_property Property)
{
    linux_state *State = &GlobalLinuxState;
    
    switch(Property)
    {
        case TrayProperty_Category:
        {
            AppendVariant("s", DBUS_TYPE_STRING, "SystemServices");
        } break;
        
        case TrayProperty_ID:
        {
            AppendVariant("s", DBUS_TYPE_STRING, "IdleBlock");
        } break;
        
        case TrayProperty_Title:
        {
            AppendVariant("s", DBUS_TYPE_STRING, "IdleBlock");
        } break;
        
        case TrayProperty_Status:
        {
            AppendVariant("s", DBUS_TYPE_STRING, "Active");
        } break;
        
        case TrayProperty_Menu:
        {
            AppendVariant("o", DBUS_TYPE_OBJECT_PATH, "/TrayMenu");
        } break;
        
        case TrayProperty_ItemIsMenu:
        {
            AppendVariant("b", DBUS_TYPE_BOOLEAN, false);
        } break;
        
        case TrayProperty_IconName:
        {
            AppendVariant("s", DBUS_TYPE_STRING, "");
        } break;
        
        case TrayProperty_IconThemePath:
        {
            AppendVariant("s", DBUS_TYPE_STRING, "");
        } break;
        
        case TrayProperty_IconPixmap:
        {
            AppendContainer(DBUS_TYPE_VARIANT, "a(iiay)")
            {
                AppendContainer(DBUS_TYPE_ARRAY, "(iiay)")
                {
                    AppendContainer(DBUS_TYPE_STRUCT, 0)
                    {
                        image32 Icon = State->Blocking ? State->ActiveIcon : State->InactiveIcon;
                        
                        AppendBasic(DBUS_TYPE_INT32, Icon.Width);
                        AppendBasic(DBUS_TYPE_INT32, Icon.Height);
                        
                        IterAppendBasicArray(DBUS_TYPE_BYTE, "y", Icon.Memory, (int)(Icon.Width*Icon.Height*4));
                    }
                }
            }
        } break;
        
        default:{}break;
    };
}

int
main(void)
{
    linux_state *State = &GlobalLinuxState;
    
    State->ActiveIcon = LoadBMP(GlobalOnIconMemory, GlobalOnIconMemorySize);
    State->InactiveIcon = LoadBMP(GlobalOffIconMemory, GlobalOffIconMemorySize);
    
    DBusError Error = {};
    dbus_error_init(&Error);
    DBusConnection *Connection = dbus_bus_get(DBUS_BUS_SESSION, &Error);
    
    const char *UniqueName = dbus_bus_get_unique_name(Connection);
    
    ScopedMethod(Query, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                 "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem")
    {
        InitAppendIterWithStack(Query);
        
        char NotiName[64];
        snprintf(NotiName, sizeof(NotiName), "org.freedesktop.StatusNotifierItem-%d-1", getpid());
        
        // TODO(maria): check and retry
        dbus_bus_request_name(Connection, NotiName, 0, &Error);
        
        AppendBasic(DBUS_TYPE_STRING, NotiName);
        
        dbus_connection_send(Connection, Query, 0);
    }
    
    u32 MenuRevision = 0;
    GlobalRunning = true;
    while(GlobalRunning)
    {
        dbus_connection_read_write(Connection, -1);
        
        for(DBusMessage *Message = dbus_connection_pop_message(Connection);
            Message;
            dbus_message_unref(Message), Message = dbus_connection_pop_message(Connection))
        {
            if(dbus_message_is_method_call(Message, "org.kde.StatusNotifierItem", "Activate"))
            {
                if(!State->Blocking)
                {
                    ScopedMethod(Request, "org.freedesktop.ScreenSaver", "/ScreenSaver",
                                 "org.freedesktop.ScreenSaver", "Inhibit")
                    {
                        InitAppendIterWithStack(Request);
                        
                        AppendBasic(DBUS_TYPE_STRING, "IdleBlock");
                        AppendBasic(DBUS_TYPE_STRING, "Activated");
                        
                        ScopedMethodResult(Request, Result)
                        {
                            InitReadIter(Result);
                            GetBasic(&State->BlockingCookie);
                            State->Blocking = true;
                        }
                    }
                }
                else
                {
                    ScopedMethod(Request, "org.freedesktop.ScreenSaver", "/ScreenSaver",
                                 "org.freedesktop.ScreenSaver", "UnInhibit")
                    {
                        InitAppendIterWithStack(Request);
                        
                        AppendBasic(DBUS_TYPE_UINT32, State->BlockingCookie);
                        
                        ScopedMethodResult(Request, Result)
                        {
                            State->Blocking = false;
                        }
                    }
                }
                
                DBusMessage *Signal = dbus_message_new_signal("/StatusNotifierItem", "org.kde.StatusNotifierItem", "NewIcon");
                dbus_connection_send(Connection, Signal, 0);
                dbus_message_unref(Signal);
            }
            else if(dbus_message_is_method_call(Message, "org.kde.StatusNotifierItem", "ContextMenu"))
            {
                // NOTE(maria): sway compat
                GlobalRunning = false;
            }
            else if(dbus_message_is_method_call(Message, "com.canonical.dbusmenu", "GetLayout"))
            {
                ScopedReply(Message, Reply)
                {
                    InitAppendIterWithStack(Reply);
                    
                    AppendBasic(DBUS_TYPE_UINT32, MenuRevision++);
                    
                    AppendContainer(DBUS_TYPE_STRUCT, 0) // Root
                    {
                        AppendBasic(DBUS_TYPE_INT32, MenuID_Root);
                        
                        AppendContainer(DBUS_TYPE_ARRAY, "{sv}")
                        {
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, 0)
                            {
                                AppendBasic(DBUS_TYPE_STRING, "children-display");
                                AppendVariant("s", DBUS_TYPE_STRING, "submenu");
                            }
                        }
                        
                        AppendContainer(DBUS_TYPE_ARRAY, "v") // Children
                        {
                            // Quit
                            AppendContainer(DBUS_TYPE_VARIANT, "(ia{sv}av)")
                            {
                                AppendContainer(DBUS_TYPE_STRUCT, 0)
                                {
                                    AppendBasic(DBUS_TYPE_INT32, MenuID_Quit);
                                    
                                    AppendContainer(DBUS_TYPE_ARRAY, "{sv}")
                                    {
                                        AppendContainer(DBUS_TYPE_DICT_ENTRY, 0)
                                        {
                                            AppendBasic(DBUS_TYPE_STRING, "label");
                                            AppendVariant("s", DBUS_TYPE_STRING, "Quit");
                                        }
                                    }
                                    
                                    AppendContainer(DBUS_TYPE_ARRAY, "v");
                                }
                            }
                        }
                    }
                    
                    dbus_connection_send(Connection, Reply, 0);
                }
            }
            else if(dbus_message_is_method_call(Message, "com.canonical.dbusmenu", "AboutToShow"))
            {
                ScopedReply(Message, Reply)
                {
                    InitAppendIterWithStack(Reply);
                    
                    // should update
                    AppendBasic(DBUS_TYPE_BOOLEAN, false);
                    
                    dbus_connection_send(Connection, Reply, 0);
                }
            }
            else if(dbus_message_is_method_call(Message, "com.canonical.dbusmenu", "Event"))
            {
                InitReadIterWithStack(Message);
                
                int MenuID = 0;
                GetBasic(&MenuID);
                
                char *InteractionRaw = "";
                GetBasic(&InteractionRaw);
                string Interaction = Str(InteractionRaw);
                
                b32 Clicked = StringsAreEqual(Interaction, StrLit("clicked"));
                
                switch(MenuID)
                {
                    case MenuID_Quit:
                    {
                        GlobalRunning = false;
                    } break;
                }
            }
            else if(dbus_message_is_method_call(Message, "org.freedesktop.DBus.Properties", "Get"))
            {
                // NOTE(maria): do we need to handle icon change here?
                InitReadIterWithStack(Message);
                char *RequestedInterfaceRaw = "";
                GetBasic(&RequestedInterfaceRaw);
                string RequestedInterface = Str(RequestedInterfaceRaw);
                
                char *RequestedPropertyRaw = "";
                GetBasic(&RequestedPropertyRaw);
                string RequestedProperty = Str(RequestedPropertyRaw);
                
                ScopedReply(Message, Reply)
                {
                    InitAppendIter(Reply);
                    
                    if(StringsAreEqual(RequestedInterface, StrLit("org.kde.StatusNotifierItem")))
                    {
                        for(u32 PropertyIndex = 0;
                            PropertyIndex < TrayProperty_Count;
                            ++PropertyIndex)
                        {
                            if(StringsAreEqual(RequestedProperty, StrLit(TrayPropertyNames[PropertyIndex])))
                            {
                                AppendTrayProperty(PropertyIndex);
                            }
                        }
                    }
                    
                    dbus_connection_send(Connection, Reply, 0);
                }
            }
            else if(dbus_message_is_method_call(Message, "org.freedesktop.DBus.Properties", "GetAll"))
            {
                InitReadIterWithStack(Message);
                
                char *RequestedInterfaceRaw = "";
                GetBasic(&RequestedInterfaceRaw);
                string RequestedInterface = Str(RequestedInterfaceRaw);
                
                if(StringsAreEqual(RequestedInterface, StrLit("org.kde.StatusNotifierItem")))
                {
                    ScopedReply(Message, Reply)
                    {
                        InitAppendIter(Reply);
                        
                        AppendContainer(DBUS_TYPE_ARRAY, "{sv}")
                        {
                            for(u32 PropertyIndex = 0;
                                PropertyIndex < TrayProperty_Count;
                                ++PropertyIndex)
                            {
                                AppendContainer(DBUS_TYPE_DICT_ENTRY, 0)
                                {
                                    AppendBasic(DBUS_TYPE_STRING, TrayPropertyNames[PropertyIndex]);
                                    AppendTrayProperty(PropertyIndex);
                                }
                            }
                        }
                        
                        dbus_connection_send(Connection, Reply, 0);
                    }
                }
            }
        }
    }
    
    dbus_connection_unref(Connection);
    
    return 0;
}
