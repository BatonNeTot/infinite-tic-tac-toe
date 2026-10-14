#ifndef INFINITE_NET_H
#define INFINITE_NET_H

#include "raylib.h"

#if defined(_WIN32) || defined(_WIN64)
/*
 * The following defines are meant to avoid conflicts between raylib and windows.h
 * https://github.com/raysan5/raylib/issues/857
 */

// If defined, the following flags inhibit definition of the indicated items
#define NOGDICAPMASKS     // CC_*, LC_*, PC_*, CP_*, TC_*, RC_
#define NOVIRTUALKEYCODES // VK_*
#define NOWINMESSAGES     // WM_*, EM_*, LB_*, CB_*
#define NOWINSTYLES       // WS_*, CS_*, ES_*, LBS_*, SBS_*, CBS_*
#define NOSYSMETRICS      // SM_*
#define NOMENUS           // MF_*
#define NOICONS           // IDI_*
#define NOKEYSTATES       // MK_*
#define NOSYSCOMMANDS     // SC_*
#define NORASTEROPS       // Binary and Tertiary raster ops
#define NOSHOWWINDOW      // SW_*
#define OEMRESOURCE       // OEM Resource values
#define NOATOM            // Atom Manager routines
#define NOCLIPBOARD       // Clipboard routines
#define NOCOLOR           // Screen colors
#define NOCTLMGR          // Control and Dialog routines
#define NODRAWTEXT        // DrawText() and DT_*
#define NOGDI             // All GDI defines and routines
#define NOKERNEL          // All KERNEL defines and routines
#define NOUSER            // All USER defines and routines
/*#define NONLS             // All NLS defines and routines*/
#define NOMB              // MB_* and MessageBox()
#define NOMEMMGR          // GMEM_*, LMEM_*, GHND, LHND, associated routines
#define NOMETAFILE        // typedef METAFILEPICT
#define NOMINMAX          // Macros min(a,b) and max(a,b)
#define NOMSG             // typedef MSG and associated routines
#define NOOPENFILE        // OpenFile(), OemToAnsi, AnsiToOem, and OF_*
#define NOSCROLL          // SB_* and scrolling routines
#define NOSERVICE         // All Service Controller routines, SERVICE_ equates, etc.
#define NOSOUND           // Sound driver routines
#define NOTEXTMETRIC      // typedef TEXTMETRIC and associated routines
#define NOWH              // SetWindowsHook and WH_*
#define NOWINOFFSETS      // GWL_*, GCL_*, associated routines
#define NOCOMM            // COMM driver routines
#define NOKANJI           // Kanji support stuff.
#define NOHELP            // Help engine interface.
#define NOPROFILER        // Profiler interface.
#define NODEFERWINDOWPOS  // DeferWindowPos routines
#define NOMCX             // Modem Configuration Extensions

// Type required before windows.h inclusion
typedef struct tagMSG *LPMSG;

#include <winsock2.h> // Has to be included before windows.h
#include <windows.h>

#endif // WINDOWS

#define NBN_LogInfo(...) TraceLog(LOG_INFO, __VA_ARGS__)

#define NBN_LogError(...) TraceLog(LOG_ERROR, __VA_ARGS__)
#define NBN_LogWarning(...) TraceLog(LOG_WARNING, __VA_ARGS__)
#define NBN_LogDebug(...) TraceLog(LOG_DEBUG, __VA_ARGS__)
#define NBN_LogTrace(...) TraceLog(LOG_TRACE, __VA_ARGS__)

#include "nbnet.h"
#include "net_drivers/udp.h"

#include "common.h"

typedef void (*NetOnConnect)(uint8_t *p_buffer, unsigned int size);
typedef void (*NetOnDisonnect)(void);
typedef void (*NetOnMessage)(void);

bool init_server(NetOnConnect on_connect, NetOnDisonnect on_disconnect, NetOnMessage on_message);
bool init_client(const char* p_ip, NetOnConnect on_connect, NetOnDisonnect on_disconnect, NetOnMessage on_message);

void reset_server(void);
void reset_client(void);

typedef void *(NetServerOnEvent)(int);

void process_server_messages(void);
void process_client_messages(void);

void send_server_move(int x, int y);
void send_client_move(int x, int y);

typedef enum {
    MESSAGE_MOVE_DONE,
    MESSAGE_GAME_END,
} MessageKind;

typedef enum {
    PLAYER_SERVER,
    PLAYER_CLIENT,
    PLAYER_TOTAL,
} Player;

typedef struct {
    int x;
    int y;
} MessageMove;

typedef struct {
    Player player;
    MessageMove winning_cells[WINNING_STROKE_SIZE];
} MessageGameEnd;

void send_server_game_end(Player player, MessageMove* winning_cells);

MessageMove *MessageMove_Create(void);
void MessageMove_Destroy(MessageMove *);
int MessageMove_Serialize(MessageMove *, NBN_Stream *);

MessageGameEnd *MessageGameEnd_Create(void);
void MessageGameEnd_Destroy(MessageGameEnd *);
int MessageGameEnd_Serialize(MessageGameEnd *, NBN_Stream *);

#endif // INFINITE_NET_H