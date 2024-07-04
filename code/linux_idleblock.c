#include <dbus/dbus.h>

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

enum menu_id
{
    MenuID_Root,
    MenuID_Quit,
};

#include <stdio.h>

int
main(void)
{
    DBusError Error = {};
    dbus_error_init(&Error);
    DBusConnection *Connection = dbus_bus_get(DBUS_BUS_SESSION, &Error);
    
    const char *UniqueName = dbus_bus_get_unique_name(Connection);
    
    ScopedMethod(Query, "org.kde.StatusNotifierWatcher", "/StatusNotifierWatcher",
                 "org.kde.StatusNotifierWatcher", "RegisterStatusNotifierItem")
    {
        InitAppendIterWithStack(Query);
        AppendBasic(DBUS_TYPE_STRING, UniqueName);
        
        dbus_connection_send(Connection, Query, 0);
    }
    
    u32 ActiveColour = 0x00FF00FF;
    u32 InactiveColour = 0xc6c6c6FF;
    
    u32 MenuRevision = 0;
    u32 BlockingCookie = 0;
    b32 Blocking = false;
    b32 Running = true;
    while(Running)
    {
        dbus_connection_read_write(Connection, 0);
        
        for(DBusMessage *Message = dbus_connection_pop_message(Connection);
            Message;
            dbus_message_unref(Message), Message = dbus_connection_pop_message(Connection))
        {
            if(dbus_message_is_method_call(Message, "org.kde.StatusNotifierItem", "Activate"))
            {
                if(!Blocking)
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
                            GetBasic(&BlockingCookie);
                            Blocking = true;
                        }
                    }
                }
                else
                {
                    ScopedMethod(Request, "org.freedesktop.ScreenSaver", "/ScreenSaver",
                                 "org.freedesktop.ScreenSaver", "UnInhibit")
                    {
                        InitAppendIterWithStack(Request);
                        
                        AppendBasic(DBUS_TYPE_UINT32, BlockingCookie);
                        
                        ScopedMethodResult(Request, Result)
                        {
                            Blocking = false;
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
                Running = false;
            }
            else if(dbus_message_is_method_call(Message, "com.canonical.dbusmenu", "GetLayout"))
            {
                ScopedReply(Message, Reply)
                {
                    InitAppendIterWithStack(Reply);
                    
                    AppendBasic(DBUS_TYPE_UINT32, MenuRevision++);
                    
                    AppendContainer(DBUS_TYPE_STRUCT, "ia{sv}av") // Root
                    {
                        AppendBasic(DBUS_TYPE_INT32, MenuID_Root);
                        
                        AppendContainer(DBUS_TYPE_ARRAY, "{sv}")
                        {
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
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
                                AppendContainer(DBUS_TYPE_STRUCT, "ia{sv}av")
                                {
                                    AppendBasic(DBUS_TYPE_INT32, MenuID_Quit);
                                    
                                    AppendContainer(DBUS_TYPE_ARRAY, "{sv}")
                                    {
                                        AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
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
                        Running = false;
                    } break;
                }
            }
            else if(dbus_message_is_method_call(Message, "org.freedesktop.DBus.Properties", "Get"))
            {
                // NOTE(maria): do we need to handle icon change here?
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
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
                            {
                                AppendBasic(DBUS_TYPE_STRING, "Category");
                                AppendVariant("s", DBUS_TYPE_STRING, "ApplicationStatus");
                            }
                            
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
                            {
                                AppendBasic(DBUS_TYPE_STRING, "Id");
                                AppendVariant("s", DBUS_TYPE_STRING, "IdleBlock");
                            }
                            
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
                            {
                                AppendBasic(DBUS_TYPE_STRING, "Title");
                                AppendVariant("s", DBUS_TYPE_STRING, "IdleBlock");
                            }
                            
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
                            {
                                AppendBasic(DBUS_TYPE_STRING, "Status");
                                AppendVariant("s", DBUS_TYPE_STRING, "Active");
                            }
                            
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
                            {
                                AppendBasic(DBUS_TYPE_STRING, "Menu");
                                AppendVariant("o", DBUS_TYPE_STRING, "/TrayMenu");
                            }
                            
                            AppendContainer(DBUS_TYPE_DICT_ENTRY, "sv")
                            {
                                AppendBasic(DBUS_TYPE_STRING, "IconPixmap");
                                
                                AppendContainer(DBUS_TYPE_VARIANT, "a(iiay)")
                                {
                                    AppendContainer(DBUS_TYPE_ARRAY, "(iiay)")
                                    {
                                        AppendContainer(DBUS_TYPE_STRUCT, "iiay")
                                        {
                                            AppendBasic(DBUS_TYPE_INT32, 32);
                                            AppendBasic(DBUS_TYPE_INT32, 32);
                                            
                                            u32 ByteArray[32*32] = {};
                                            
                                            
                                            for(u32 y = 0;
                                                y < 32*32;
                                                ++y)
                                            {
                                                ByteArray[y] = Blocking ? ActiveColour : InactiveColour;
                                            }
                                            
                                            IterAppendBasicArray(DBUS_TYPE_BYTE, "y", ByteArray, 32*32*4);
                                        }
                                    }
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