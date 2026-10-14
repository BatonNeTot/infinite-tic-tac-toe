
#define NBNET_IMPL 
#include "net.h"

#include "common.h"

static NBN_ConnectionHandle client_handle = 0;

static NetOnConnect net_on_connect = NULL;
static NetOnDisonnect net_on_disconnect = NULL;
static NetOnMessage net_on_message = NULL;

void reset_server(void) {
    fprintf(stderr, "reset_server\n");
    client_handle = 0;
    NBN_GameServer_Stop();
}
void reset_client(void) {
    NBN_GameClient_Stop();
}

bool init_server(NetOnConnect on_connect, NetOnDisonnect on_disconnect, NetOnMessage on_message) {
    net_on_connect = on_connect;
    net_on_disconnect = on_disconnect;
    net_on_message = on_message;

    if (NBN_GameServer_StartEx(GAME_PROTOCOL_NAME, GAME_PORT) < 0) {
        TraceLog(LOG_ERROR, "Game server failed to start. Exit");
        return false;
    }

    NBN_GameServer_RegisterMessage(
            MESSAGE_MOVE_DONE,
            (NBN_MessageBuilder)MessageMove_Create,
            (NBN_MessageDestructor)MessageMove_Destroy,
            (NBN_MessageSerializer)MessageMove_Serialize);
    NBN_GameServer_RegisterMessage(
            MESSAGE_GAME_END,
            (NBN_MessageBuilder)MessageGameEnd_Create,
            (NBN_MessageDestructor)MessageGameEnd_Destroy,
            (NBN_MessageSerializer)MessageGameEnd_Serialize);

    return true;
}

bool init_client(const char* p_ip, NetOnConnect on_connect, NetOnDisonnect on_disconnect, NetOnMessage on_message) {
    net_on_connect = on_connect;
    net_on_disconnect = on_disconnect;
    net_on_message = on_message;

    if (NBN_GameClient_StartEx(GAME_PROTOCOL_NAME, p_ip, GAME_PORT, NULL, 0) < 0) {
        TraceLog(LOG_WARNING, "Game client failed to start. Exit");
        return false;
    }

    NBN_GameClient_RegisterMessage(
            MESSAGE_MOVE_DONE,
            (NBN_MessageBuilder)MessageMove_Create,
            (NBN_MessageDestructor)MessageMove_Destroy,
            (NBN_MessageSerializer)MessageMove_Serialize);
    NBN_GameClient_RegisterMessage(
            MESSAGE_GAME_END,
            (NBN_MessageBuilder)MessageGameEnd_Create,
            (NBN_MessageDestructor)MessageGameEnd_Destroy,
            (NBN_MessageSerializer)MessageGameEnd_Serialize);

    return true;
}

void process_server_messages(void) {
    int ev;
    while ((ev = NBN_GameServer_Poll()) != NBN_NO_EVENT) {
        if (ev < 0) {
            TraceLog(LOG_ERROR, "An occured while polling network events. Exit");
            break;
        }

        switch (ev) {
            case NBN_NEW_CONNECTION: {
                if (client_handle != 0) {
                    NBN_GameServer_RejectIncomingConnectionWithCode(-1);
                    break;
                }

                client_handle = NBN_GameServer_GetIncomingConnection();
                
                uint8_t data[32];
                net_on_connect(data, sizeof(data) / sizeof(data[0]));
                NBN_GameServer_AcceptIncomingConnectionWithData(data, sizeof(data));
                break;
            }
            case NBN_CLIENT_DISCONNECTED:
                net_on_disconnect();
                return;

            case NBN_CLIENT_MESSAGE_RECEIVED:
                net_on_message();
                break;
        }
    }

    if (NBN_GameServer_SendPackets() < 0)
    {
        TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");
        return;
    }
}
void process_client_messages(void) {
    int ev;
    while ((ev = NBN_GameClient_Poll()) != NBN_NO_EVENT) {
        if (ev < 0) {
            TraceLog(LOG_WARNING, "An occured while polling network events. Exit");
            break;
        }

        switch (ev) {
            case NBN_CONNECTED: {
                uint8_t data[32];
                unsigned int data_len = NBN_GameClient_ReadServerData(data);
                net_on_connect(data, data_len);
                break;
            }

            case NBN_DISCONNECTED:
                net_on_disconnect();
                return;

            case NBN_MESSAGE_RECEIVED:
                net_on_message();
                break;
        }
    }

    if (NBN_GameClient_SendPackets() < 0) {
        TraceLog(LOG_ERROR, "An occured while flushing the send queue. Exit");
        return;
    }
}

void send_server_move(int x, int y) {
    MessageMove *msg = MessageMove_Create();

    msg->x = x;
    msg->y = y;

    NBN_GameServer_SendReliableMessageTo(client_handle, MESSAGE_MOVE_DONE, msg);
}
void send_client_move(int x, int y) {
    MessageMove *msg = MessageMove_Create();

    msg->x = x;
    msg->y = y;

    NBN_GameClient_SendReliableMessage(MESSAGE_MOVE_DONE, msg);
}

void send_server_game_end(Player player, MessageMove* winning_cells) {
    fprintf(stderr, "send_server_game_end %u\n", client_handle);
    MessageGameEnd *msg = MessageGameEnd_Create();

    msg->player = player;
    memcpy(&msg->winning_cells[0], winning_cells, WINNING_STROKE_SIZE * sizeof(MessageMove));

    NBN_GameServer_SendReliableMessageTo(client_handle, MESSAGE_GAME_END, msg);
}

MessageMove *MessageMove_Create(void) {
    return malloc(sizeof(MessageMove));
}
void MessageMove_Destroy(MessageMove *msg) {
    free(msg);
}
int MessageMove_Serialize(MessageMove *msg, NBN_Stream *stream) {
    NBN_SerializeInt(stream, msg->x, INT_MIN, INT_MAX);
    NBN_SerializeInt(stream, msg->y, INT_MIN, INT_MAX);

    return 0;
}

MessageGameEnd *MessageGameEnd_Create(void) {
    return malloc(sizeof(MessageGameEnd));
}
void MessageGameEnd_Destroy(MessageGameEnd *msg) {
    free(msg);
}
int MessageGameEnd_Serialize(MessageGameEnd *msg, NBN_Stream *stream) {
    NBN_SerializeUInt(stream, msg->player, 0, PLAYER_TOTAL - 1);

    for (int i = 0; i < WINNING_STROKE_SIZE; ++i) {
        MessageMove_Serialize(&msg->winning_cells[i], stream);
    }

    return 0;
}
