#pragma once

#include <cstdint>
#include "jthread.hpp"

namespace pdk {

// ── Event packet sizes ─────────────────────────────────────────────────────────
#define MAX_EVENTINFO_SIZE (4 * 1024)
#define EVENTINFO_HEADER_LENGTH (sizeof(XTMMSGHDR) + sizeof(ARSLINE))
#define MAX_VALUE_SIZE (MAX_EVENTINFO_SIZE - EVENTINFO_HEADER_LENGTH)

// ── Network address structs ────────────────────────────────────────────────────
struct XTMADDR {
    uint16_t usZoneId;
    uint16_t usSysId;
    uint16_t usParentId;
    uint16_t usChildId;
    uint32_t ulTmpPid;
};
using PXTMADDR = XTMADDR*;

struct XTMMSGHDR {
    unsigned int nMagicCookie;
    int nMsgLen;
    XTMADDR stReceiver;
    XTMADDR stSender;
    short snHopCnt;
    unsigned short usTransactionId;
    short snSerNo;
    unsigned short usType;
    unsigned short usSubType;
    unsigned short _reserved;
};
using PXTMMSGHDR = XTMMSGHDR*;

struct ARSLINE {
    uint16_t usAreaId;
    uint16_t usSystemId;
    uint16_t usBoardId;
    uint16_t usTrunkId;
    uint16_t usChannelId;
    uint16_t usReserved[3];
};
using PARSLINE = ARSLINE*;

struct EVENTINFO {
    XTMMSGHDR h;
    ARSLINE line;
    char Value[MAX_VALUE_SIZE];
};
using PEVENTINFO = EVENTINFO*;

// ── Accessor macros ────────────────────────────────────────────────────────────
#define EVENTINFO_LENGTH(e) ((e)->h.nMsgLen)
#define EVENTINFO_VALUE_LENGTH(e) ((e)->h.nMsgLen - (int)EVENTINFO_HEADER_LENGTH)
#define EVENTINFO_RECEIVER(e) ((e)->h.stReceiver.usSysId)
#define EVENTINFO_SENDER(e) ((e)->h.stSender.usSysId)
#define EVENTINFO_RECV_PARENTID(e) ((e)->h.stReceiver.usParentId)
#define EVENTINFO_TYPE(e) ((e)->h.usType)
#define EVENTINFO_SUBTYPE(e) ((e)->h.usSubType)
#define EVENTINFO_AREAID(e) ((e)->line.usAreaId)
#define EVENTINFO_SYSTEMID(e) ((e)->line.usSystemId)
#define EVENTINFO_CLIENTID(e) ((e)->line.usBoardId)
#define EVENTINFO_BOARD(e) ((e)->line.usBoardId)
#define EVENTINFO_TRUNK(e) ((e)->line.usTrunkId)
#define EVENTINFO_CHANNEL(e) ((e)->line.usChannelId)

// ── Event type enumeration ────────────────────────────────────────────────────
enum EVENTTYPE {
    CI_QUIT = 0x7fff,
    CI_NONE = 0x8000,
    CI_ACK = 0x8001,
    CI_LOGIN = 0x8002,
    CI_LOGOUT = 0x8003,
    CI_TIMEOUT = 0x8004,
    CI_ARS_START = 0x8005,
    CI_ARS_STOP = 0x8006,
    CI_SS7_CALL = 0x8007,
    CI_SS7_HANGUP = 0x8008,
    CI_SS7_ALERT = 0x8009,
    CI_SS7_ANSWER = 0x800a,
    CI_SS7_RECV = 0x800b,
    CI_SS7_RELEASE = 0x800c,
    CI_SS7_REJECT = 0x800d,
    CI_TTS_REQUEST = 0x800e,
    CI_TTS_REPLY = 0x800f,
    CI_PLAY_DONE = 0x9001,
    CI_RECORD_DONE,
    CI_DTMF_SINGLE,
    CI_DTMF_MULTI,
    CI_NOINPUT,
    CI_NOMATCH,
    CI_NMS_EVENT = 0xf000,
    CI_VXML_REQUEST,
    CI_VXML_DONE,
    CI_QUIT_THREAD,
    CI_START_SIM,
    CI_STOP_SIM,
    CI_START_SIM_HELPER,
    CI_GEN_DTMF,
    CI_PLAY_FILE,
    CI_VXML_RESPONSE,
    CI_APP_REQUEST,
    CI_SS7_REQUEST,
    CI_SS7_OG_REQ,
    CI_SS7_CH_STATUS,
    CI_SS7_CH_STATUS_ACK,
    CI_JUMP_DOC,
    CI_USER_CERT,
    CI_SS7_REL_COMPLETE,
    CI_SS7_RESET,
    CI_SS7_REATEMPT,
    CI_OG_MESSAGE,
    CI_OG_RELEASE,
    CI_OG_CALL_RES,
    CI_OG_CALL_RECV,
    CI_OG_CALL_ACCEPT,
    CI_MGR_MESSAGE,
    CI_MGR_CALL,
    CI_MGR_CALL_ACCEPT,
    CI_MGR_RECALL,
    CI_MGR_REL,
    CI_MGR_FREECH,
    CI_OG_REQUEST,
    CI_CTRL_MESSAGE,
    CI_SS7_G_DISTRIBUTE_RESET,
    CI_START_VXML,
    CI_DTMF_TONE,
    CI_OG_STATUS_REQ,
    CI_OG_STATUS_RES
};  // enum EVENTTYPE

#define DEF_EVENT_QUEUE_LENGTH 32
#define BOARD_EVENT_QUEUE_LENGTH (DEF_EVENT_QUEUE_LENGTH * 4)
#define APP_EVENT_QUEUE_LENGTH BOARD_EVENT_QUEUE_LENGTH
#define EVENT_RETRY_COUNT 1
#define EVENT_RETRY_DELAY 100

// EventQueue: bounded queue of EVENTINFO (via Queue<PEVENTINFO,EVENTINFO>)
using EventQueue = Queue<PEVENTINFO, EVENTINFO>;

}  // namespace pdk
