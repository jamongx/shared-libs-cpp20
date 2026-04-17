
namespace pdk {
/*
	HURIM Copy Right

	File : alarm.h

	1. Declare Alarm

	Create Date : 2001.  by Jeong
	Updated     : 2002.1 by WindJun - VMS
*/

#ifndef __ALARM_H__
#define __ALARM_H__

typedef enum { ALARM_CLEARED, ALARM_MINOR, ALARM_MAJOR, ALARM_CRITICAL } ALARM_LEVEL, *PALARM_LEVEL;

typedef enum {
    communicationFail,
    unixSocketError,
    inetSocketError,
    databaseError,
    ipcShmError,
    ipcMsgqError,
    ipcSemError,
    tcpCLosedError,
    processError,
    configError,
    signalError,
    processKilled,
    cpuOverflow,
    diskOverflow,
    memoryOverflow,
    swapOverflow,
    e1boardError,
    e1portError,
    ss7SystemError,
    ss7BoardError,
    ss7LinkError,
    nowHacking,
    /* only vms snmp agent use */
    DTMFNotify,
    statusChange,
    cpuUtilChange,
    memUtilChange,
    diskUtilChange,
    procShutdown,
    versionNotify,
    threadKilled,  //new
    queueFull,     //new
    fileError      //new
} ALARM_TYPE,
    *PALARM_TYPE;

#endif  // __ALARM_H__

}  // namespace pdk
