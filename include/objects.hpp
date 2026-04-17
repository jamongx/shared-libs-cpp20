
#ifndef _IVR_OBJECTS_H_
#define _IVR_OBJECTS_H_

#include <assert.h>
#include "base.hpp"
#include "list.hpp"
#include "vxmltypes.hpp"
#include "props.hpp"
#include "vars.hpp"
#include "logger.hpp"

#include "PYEngine.hpp"

namespace pdk {

#define CURRENT_VERSION 0x01000000
#define NUMBER_OF_DTMF 12  // 0, 1, ..., 9, *(10), #(11)

// Class Hierarchy
//class COject;
class CObjBase;
class CObjVxml;
class CObjProperty;
class CObjVar;
class CObjAssign;
class CObjPlay;
class CObjAudio;
class CObjPrompt;
class CObjChoice;
class CObjRecord;
class CObjDialog;
class CObjMenu;
class CObjForm;
class CObjEvent;
class CObjNoinput;
class CObjNomatch;
class CObjExit;
class CObjError;
//@@ Add 10/02/02
class CObjTransfer;
class CObjGoto;
//@@ Add 01/11/02
class CObjScript;
class CObjIf;
class CObjElseif;
class CObjElse;

typedef CList<int, int> CIdList;

class CVxmlObjects;

//////////////////////////////////////////////////////////////////////
// CObjBase

class CObjBase : public Object {
    DECLARE_SERIAL(CObjBase)

public:
    CObjBase();
    CObjBase(VXMLOBJTYPE nType, int nId, LPCSTR pszName = NULL);
    virtual ~CObjBase();

    void Serialize(Archive& ar);
    void operator=(const CObjBase& o);
    virtual CObjBase* Clone();

    inline void SetVersion(int v) { m_nVersion = v; }
    inline void set_type(VXMLOBJTYPE t) { type_ = t; }
    inline void SetName(LPCSTR pszName) { name_ = pszName; }
    inline void SetId(int i) { m_nId = i; }
    inline void SetParentId(int i) { m_nParentId = i; }
    inline void SetParentObject(CObjBase* pParent) { m_pParent = pParent; }
    //@@ Add 01/19/02
    inline void SetPyInterp(PyInterp* pPyIn) { m_pPyInterp = pPyIn; }

    inline int GetVersion() { return m_nVersion; }
    inline VXMLOBJTYPE type() { return type_; }
    inline const char* name() { return (LPCSTR)name_; }
    inline int GetId() { return m_nId; }
    inline int GetParentId() { return m_nParentId; }
    CObjBase* GetParentObject();

    inline const CIdList* GetDialogIdList() const { return m_pliDialogId; }
    inline const CIdList* GetPropIdList() const { return m_pliPropId; }
    inline const CIdList* GetPlayIdList() const { return m_pliPlayId; }
    inline const CIdList* GetChoiceIdList() const { return m_pliChoiceId; }
    inline const CIdList* GetEventIdList() const { return m_pliEventId; }
    inline const CIdList* GetVarIdList() const { return m_pliVarId; }
    inline const CIdList* GetAssignIdList() const { return m_pliAssignId; }
    //@@ Add 01/14/02
    inline const CIdList* GetIfIdList() const { return m_pliIfId; }
    //@@ added by eehd
    inline const CIdList* GetTransferIdList() const { return m_pliTransferId; }
    //@@ Added by eehd

    void AddDialogId(int v);
    void AddPropId(int v);
    void AddPlayId(int v);
    void AddChoiceId(int v);
    void AddEventId(int v);
    void AddVarId(int v);
    void AddAssignId(int v);

    //@@ Add 01/14/02
    void AddIfId(int v);
    //@@ Add by eehd
    void AddTransferId(int v);

    inline virtual int GetPropertyCount() { return 0; }
    inline virtual int GetVarCount() { return 0; }
    inline virtual int GetAssignCount() { return 0; }
    inline virtual int GetIfCount() { return 0; }

    inline virtual CObjProperty** GetProperties() { return NULL; }
    inline virtual CObjVar** GetVars() { return NULL; }
    inline virtual CObjAssign** GetAssigns() { return NULL; }
    inline virtual CObjIf** GetIfs() { return NULL; }
    //@@ Added by eehd
    inline virtual CObjTransfer** GetTransfers() { return NULL; }
    inline virtual int GetTransferCount() { return 0; }

    inline virtual int GetNumberOfPrompts() { return 0; }
    inline virtual CObjPlay** GetPrompts() { return NULL; }
    inline virtual void GetDtmfs(CString& dtmfs) { dtmfs.Empty(); }
    inline virtual void IncrPromptCount() {}

    // assigned by the tag, goto
    inline virtual void SetNextDialog(CObjDialog* pDialog) {}
    inline virtual CObjDialog* GetNextDialog() { return NULL; }
    inline virtual CObjDialog* GetNextDialog(char dtmf) { return NULL; }

    inline virtual int GetNumOfDtmfInput() { return 1; }
    inline virtual void GetValidDtmfs(CString& dtmfs) { dtmfs.Empty(); }

    inline void SetVxmlObjects(CVxmlObjects* p) { m_pVxmlObjects = p; }
    inline CVxmlObjects* GetVxmlObjects() { return m_pVxmlObjects; }
    inline virtual bool PrepareToRun() { return m_pVxmlObjects != NULL; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
    void DumpList(LPCSTR pszName, CDumpContext& dc, const CIdList* psrc) const;
#endif

protected:
    void CopyList(CIdList* pDest, const CIdList* pSrc);
    bool FindDialogsFromList(CObjDialog**& pDialogs, int& nNumOfDialogs);
    bool FindPlaysFromList(CObjPlay**& pPlays, int& nNumOfPlays);
    bool FindEventsFromList(CObjEvent**& pEvents, int& nNumOfEvents);
    bool FindObjectsFromList(const CRuntimeClass* pClass,
                             const CIdList* pSrcList,
                             CObjBase**& pObjects,
                             int& nNumOfObjects);

protected:
    // variables to write in the file
    int m_nVersion;
    VXMLOBJTYPE type_;
    CString name_;
    int m_nId;
    int m_nParentId;

    // variables for internal use
    CObjBase* m_pParent;
    CVxmlObjects* m_pVxmlObjects;

    Logger* logger_;
    //@@ Add 01/19/02
    PyInterp* m_pPyInterp;

private:
    // variables to write in the file
    CIdList* m_pliDialogId;
    CIdList* m_pliPlayId;
    CIdList* m_pliChoiceId;
    CIdList* m_pliEventId;

    CIdList* m_pliPropId;
    CIdList* m_pliVarId;
    CIdList* m_pliAssignId;

    //@@ Add 01/14/02
    CIdList* m_pliIfId;
    //@@ Added by eehd
    CIdList* m_pliTransferId;
};

//////////////////////////////////////////////////////////////////////
// CObjVxml

class CObjVxml : public CObjBase {
    DECLARE_SERIAL(CObjVxml)

public:
    CObjVxml();
    CObjVxml(int nId);
    virtual ~CObjVxml();

    void Serialize(Archive& ar);
    void operator=(const CObjVxml& o);

    inline void SetVxmlVersion(LPCSTR pszVer) { m_strVxmlVersion = pszVer; }
    inline LPCSTR GetVxmlVersion() { return (LPCSTR)m_strVxmlVersion; }

    bool PrepareToRun();

    inline virtual int GetPropertyCount() { return m_nNumOfProps; }
    inline virtual int GetVarCount() { return m_nNumOfVars; }
    inline virtual int GetAssignCount() { return m_nNumOfAssigns; }

    inline virtual CObjProperty** GetProperties() { return m_pProps; }
    inline virtual CObjVar** GetVars() { return m_pVars; }
    inline virtual CObjAssign** GetAssigns() { return m_pAssigns; }
    // Add 01/23/02
    inline virtual int GetIfCount() { return m_nNumOfIfs; }
    inline virtual CObjIf** GetIfs() { return m_pIfs; }
    //@@ Added by eehd
    inline virtual CObjTransfer** GetTransfers() { return m_pTransfers; }
    inline virtual int GetTransferCount() { return m_nNumOfTransfers; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strVxmlVersion;

    // variables for internal use
    int m_nNumOfDialogs;
    CObjDialog** m_pDialogs;
    int m_nNumOfProps;
    CObjProperty** m_pProps;
    int m_nNumOfVars;
    CObjVar** m_pVars;
    int m_nNumOfAssigns;
    CObjAssign** m_pAssigns;
    // Add 01/23/02
    int m_nNumOfIfs;
    CObjIf** m_pIfs;
    //@@ Added by eehd
    int m_nNumOfTransfers;
    CObjTransfer** m_pTransfers;
};

//////////////////////////////////////////////////////////////////////
// CObjProperty

class CObjProperty : public CObjBase {
    DECLARE_SERIAL(CObjProperty)

public:
    CObjProperty();
    CObjProperty(int nId);
    virtual ~CObjProperty();

    void Serialize(Archive& ar);
    void operator=(const CObjProperty& o);

    inline void SetPropName(LPCSTR pszName) { m_strPropName = pszName; }
    inline void SetPropValue(LPCSTR pszValue) { m_strPropValue = pszValue; }

    inline LPCSTR GetPropName() { return (LPCSTR)m_strPropName; }
    inline LPCSTR GetPropValue() { return (LPCSTR)m_strPropValue; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strPropName;
    CString m_strPropValue;

    // variables for internal use - nothing
};

//////////////////////////////////////////////////////////////////////
// CObjVar

class CObjVar : public CObjBase {
    DECLARE_SERIAL(CObjVar)

public:
    CObjVar();
    CObjVar(int nId);

    virtual ~CObjVar();

    void Serialize(Archive& ar);
    void operator=(const CObjVar& o);

    inline void SetVarName(LPCSTR pszName) { m_strVarName = pszName; }
    inline void SetVarValue(LPCSTR pszValue) { m_strVarValue = pszValue; }

    inline LPCSTR GetVarName() { return (LPCSTR)m_strVarName; }
    inline LPCSTR GetVarValue() { return (LPCSTR)m_strVarValue; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strVarName;
    CString m_strVarValue;

    // variables for internal use
};

//////////////////////////////////////////////////////////////////////
// CObjAssign

class CObjAssign : public CObjBase {
    DECLARE_SERIAL(CObjAssign)

public:
    CObjAssign();
    CObjAssign(int nId);

    virtual ~CObjAssign();

    void Serialize(Archive& ar);
    void operator=(const CObjAssign& o);

    inline void SetAssignName(LPCSTR pszName) { m_strAssignName = pszName; }
    inline void SetAssignValue(LPCSTR pszValue) { m_strAssignValue = pszValue; }

    inline LPCSTR GetAssignName() { return (LPCSTR)m_strAssignName; }
    inline LPCSTR GetAssignValue() { return (LPCSTR)m_strAssignValue; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strAssignName;
    CString m_strAssignValue;

    // variables for internal use
};

//////////////////////////////////////////////////////////////////////
// CObjPlay

class CObjPlay : public CObjBase {
    DECLARE_SERIAL(CObjPlay)

public:
    CObjPlay() : CObjBase(VXMLOBJ_PLAY, 0, NULL), m_nCount(1) {}
    CObjPlay(VXMLOBJTYPE nType, int nId, LPCSTR pszName = NULL)
        : CObjBase(nType, nId, pszName),
          m_nCount(1)
    {}
    virtual ~CObjPlay() {}

    void Serialize(Archive& ar);
    void operator=(const CObjPlay& o);

    inline void set_limit(int n) { m_nCount = n; }
    inline int count() { return m_nCount; }

    inline virtual LPCSTR GetPlayItem(bool bFirst) { return NULL; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    int m_nCount;
};

//////////////////////////////////////////////////////////////////////
// CObjAudio

class CObjAudio : public CObjPlay {
    DECLARE_SERIAL(CObjAudio)

public:
    CObjAudio();
    CObjAudio(int nId);
    virtual ~CObjAudio();

    void Serialize(Archive& ar);
    void operator=(const CObjAudio& o);

    inline void SetSource(LPCSTR pszSource) { m_strSource = pszSource; }

    inline LPCSTR GetSource() { return (LPCSTR)m_strSource; }

    bool PrepareToRun();

    LPCSTR GetPlayItem(bool bFirst);
    //@@ Add 02/04/02
    inline void SetExpr(LPCSTR pszExpr) { m_strExpr = pszExpr; }
    inline LPCSTR GetExpr() { return (LPCSTR)m_strExpr; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strSource;

    // variables for internal use
    bool m_bExist;
    //@@ Add 02/04/02
    CString m_strExpr;
    CString m_strExprResult;
};

//////////////////////////////////////////////////////////////////////
// CObjPrompt

class CObjPrompt : public CObjPlay {
    DECLARE_SERIAL(CObjPrompt)

public:
    CObjPrompt();
    CObjPrompt(int nId);
    virtual ~CObjPrompt();

    void Serialize(Archive& ar);
    void operator=(const CObjPrompt& o);

    inline void SetCondition(LPCSTR pszCond) { m_strCond = pszCond; }
    inline void SetTimeout(int t) { m_nTimeout = t; }

    inline LPCSTR GetCondition() { return (LPCSTR)m_strCond; }
    inline int GetTimeout() { return m_nTimeout; }

    bool PrepareToRun();

    LPCSTR GetPlayItem(bool bFirst);

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strCond;
    int m_nTimeout;  // unit: milliseconds

    // variables for internal use
    int m_nNumOfPlays;
    int m_nIndexOfPlays;
    CObjPlay** m_pPlays;
};

//////////////////////////////////////////////////////////////////////
// CObjChoice

class CObjChoice : public CObjBase {
    DECLARE_SERIAL(CObjChoice)

public:
    CObjChoice();
    CObjChoice(int nId);
    virtual ~CObjChoice();

    void Serialize(Archive& ar);
    void operator=(const CObjChoice& o);

    inline bool SetNext(LPCSTR pszNext);
    inline void SetEvent(LPCSTR pszEvent) { m_strEvent = pszEvent; }
    inline void SetDtmf(LPCSTR pszDtmf) { m_strDtmf = pszDtmf; }
    bool SetDtmf(char d);
    inline void SetNextId(int m) { m_nNextId = m; }

    inline LPCSTR GetNextDoc() { return (LPCSTR)m_strNextDoc; }
    inline LPCSTR GetNext() { return (LPCSTR)m_strNext; }
    inline LPCSTR GetEvent() { return (LPCSTR)m_strEvent; }
    inline LPCSTR GetDtmf() { return (LPCSTR)m_strDtmf; }
    inline int GetNextId() { return m_nNextId; }

    bool PrepareToRun();

    inline bool CheckDtmf(char dtmf) { return m_strDtmf.find(dtmf) >= 0; }
    //@@ Modify 02/04/02
    //   inline CObjDialog *GetNextDialog() { return m_pNextDialog; }
    CObjDialog* GetNextDialog();
    //@@ Add 01/30/02 - Jump File
    bool SetJumpDoc(LPCSTR pszNext);
    //@@ Add 02/04/02
    inline void SetExpr(LPCSTR pszExpr) { m_strExpr = pszExpr; }
    inline LPCSTR GetExpr() { return (LPCSTR)m_strExpr; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strDtmf;
    CString m_strNextDoc;
    CString m_strNext;
    CString m_strEvent;
    int m_nNextId;

    // variables for internal use
    CObjDialog* m_pNextDialog;
    //@@ Add 02/04/02
    CString m_strExpr;
};

//////////////////////////////////////////////////////////////////////
// CObjRecord

class CObjRecord : public CObjBase {
    DECLARE_SERIAL(CObjRecord)

public:
    CObjRecord();
    CObjRecord(int nId);
    virtual ~CObjRecord();

    void Serialize(Archive& ar);
    void operator=(const CObjRecord& o);

    inline void SetRecName(LPCSTR pszName) { name_ = pszName; }
    inline void SetMaxTime(int t) { m_nMaxTime = t; }
    inline void SetAudioType(AUDIOTYPE a) { m_nAudioType = a; }
    inline void SetBeep(bool b) { m_bBeep = b; }
    inline void SetDtmfTerm(bool b) { m_bDtmfTerm = b; }

    inline LPCSTR GetRecName() { return (LPCSTR)name_; }
    inline int GetMaxTime() { return m_nMaxTime; }
    inline AUDIOTYPE GetAudioType() { return m_nAudioType; }
    inline bool GetBeep() { return m_bBeep; }
    inline bool GetDtmfTerm() { return m_bDtmfTerm; }

    bool PrepareToRun();

    inline int GetNumberOfPrompts() { return m_nNumOfPlays; }
    inline virtual CObjPlay** GetPrompts() { return m_pPlays; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString name_;
    int m_nMaxTime;  // unit: milliseconds
    AUDIOTYPE m_nAudioType;
    bool m_bBeep;
    bool m_bDtmfTerm;

    // variables for internal use
    int m_nNumOfPlays;
    CObjPlay** m_pPlays;
};

//////////////////////////////////////////////////////////////////////
// CObjDialog

class CObjDialog : public CObjBase {
    DECLARE_SERIAL(CObjDialog)

public:
    CObjDialog();
    CObjDialog(VXMLOBJTYPE nType, int nId, LPCSTR pszName = NULL);
    virtual ~CObjDialog();

    void Serialize(Archive& ar)
    {
        CObjBase::Serialize(ar);
        if (ar.is_storing() == true)
            ar << m_strDialogName;
        else
            ar >> m_strDialogName;
    }
    void operator=(const CObjDialog& o)
    {
        CObjBase::operator=(o);
        m_strDialogName = o.m_strDialogName;
    }

    inline void SetDialogName(LPCSTR pszName) { m_strDialogName = pszName; }
    inline LPCSTR GetDialogName() { return (LPCSTR)m_strDialogName; }

    virtual bool PrepareToRun() { return m_pVxmlObjects != NULL; }

    // operations for the internal variables
    void ResetCount();
    inline void IncrPromptCount() { m_nPromptCount++; }
    // return the new count value
    int IncrEventCount(LPCSTR pszEventName);
    int GetEventCount(LPCSTR pszEventName);

    inline virtual bool GetWait() { return true; }
    inline virtual int GetNumberOfPrompts() { return 0; }
    inline virtual CObjPlay** GetPrompts() { return NULL; }
    inline virtual void GetDtmfs(CString& dtmfs) { dtmfs.Empty(); }
    inline virtual CObjEvent* GetEvent(const CRuntimeClass* pClass, int count) { return NULL; }
    inline virtual int GetNumOfDtmfInput() { return 1; }
    inline virtual void GetValidDtmfs(CString& dtmfs) { dtmfs.Empty(); }
    inline virtual CObjRecord* GetRecord() { return NULL; }

    inline virtual void SetNextDialog(CObjDialog* pDialog) {}
    inline virtual CObjDialog* GetNextDialog() { return NULL; }
    inline virtual CObjDialog* GetNextDialog(char dtmf) { return NULL; }

    inline virtual int GetNumberOfEvents() { return 0; }
    inline virtual CObjEvent** GetEvents() { return NULL; }

    inline virtual int GetAssignCount() { return 0; }
    inline virtual CObjAssign** GetAssigns() { return NULL; }

#ifdef DEBUG
    virtual void AssertValid() const
    {
        CObjBase::AssertValid();
        assert(this != NULL);
    }
    virtual void Dump(CDumpContext& dc) const
    {
        CObjBase::Dump(dc);
        dc << "\tDialogName=" << m_strDialogName << "\n";
    }
#endif

protected:
    // variables to write in the file
    CString m_strDialogName;

    // variables for internal use
    int m_nPromptCount;
    CMapStringToPtr m_mapEventCount;
};

//////////////////////////////////////////////////////////////////////
// CObjMenu

class CObjMenu : public CObjDialog {
    DECLARE_SERIAL(CObjMenu)

public:
    CObjMenu();
    CObjMenu(int nId);
    virtual ~CObjMenu();

    void Serialize(Archive& ar);
    void operator=(const CObjMenu& o);

    inline void SetDtmf(bool d) { m_bDtmf = d; }
    inline void SetWait(bool w) { m_bWait = w; }
    inline void SetNumOfDtmfInput(int n) { m_nNumOfDtmfInput = n; }
    inline void SetValidDtmfs(LPCSTR dtmfs) { m_strValidDtmfs = dtmfs; }
    inline void SetRecordId(int i) { m_nRecordId = i; }

    inline bool GetDtmf() { return m_bDtmf; }
    inline bool GetWait() { return m_bWait; }
    inline int GetNumOfDtmfInput() { return m_nNumOfDtmfInput; }
    inline void GetValidDtmfs(CString& dtmfs) { dtmfs = m_strValidDtmfs; }
    inline int GetRecordId() { return m_nRecordId; }

    bool PrepareToRun();

    inline int GetNumberOfPrompts() { return m_nNumOfPlays; }
    inline CObjPlay** GetPrompts() { return m_pPlays; }
    inline int GetAssignCount() { return m_nNumOfAssigns; }
    inline CObjAssign** GetAssigns() { return m_pAssigns; }
    inline int GetNumberOfEvents() { return m_nNumOfEvents; }
    inline CObjEvent** GetEvents() { return m_pEvents; }

    void GetDtmfs(CString& dtmfs);
    CObjEvent* GetEvent(const CRuntimeClass* pClass, int count);

    // assigned by the tag, goto
    inline void SetNextDialog(CObjDialog* pDialog) { m_pNextDialog = pDialog; }
    inline CObjDialog* GetNextDialog() { return m_pNextDialog; }
    CObjDialog* GetNextDialog(char dtmf);

    inline CObjRecord* GetRecord() { return m_pRecord; }
    //@@ Add 01/23/02
    inline virtual int GetIfCount() { return m_nNumOfIfs; }
    inline virtual CObjIf** GetIfs() { return m_pIfs; }
    inline virtual int GetChoiceCount() { return m_nNumOfChoices; }
    inline virtual CObjChoice** GetChoices() { return m_pChoices; }
    //@@ Added by eehd
    inline virtual CObjTransfer** GetTransfers() { return m_pTransfers; }
    inline virtual int GetTransferCount() { return m_nNumOfTransfers; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    bool m_bDtmf;
    bool m_bWait;
    // our own attribute <menu ... numdtmf="1" validdtmf="234">
    // if m_nNumOfDtmfInput is -1,
    // it means to get all the dtmfs until the term char is received.
    // default = 1
    int m_nNumOfDtmfInput;
    CString m_strValidDtmfs;  // default="0123456789*#"
    int m_nRecordId;

    // variables for internal use
    int m_nNumOfChoices;
    CObjChoice** m_pChoices;
    int m_nNumOfEvents;
    CObjEvent** m_pEvents;
    int m_nNumOfPlays;
    CObjPlay** m_pPlays;
    CObjRecord* m_pRecord;
    int m_nNumOfAssigns;
    CObjAssign** m_pAssigns;
    CObjDialog* m_pNextDialog;
    // Add 01/23/02
    int m_nNumOfIfs;
    CObjIf** m_pIfs;
    //@@ Added by eehd
    int m_nNumOfTransfers;
    CObjTransfer** m_pTransfers;
};

//////////////////////////////////////////////////////////////////////
// CObjForm

class CObjForm : public CObjDialog {
    DECLARE_SERIAL(CObjForm)

public:
    CObjForm();
    CObjForm(int nId);
    virtual ~CObjForm();

    void Serialize(Archive& ar);
    void operator=(const CObjForm& o);

    bool PrepareToRun();

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
};

//////////////////////////////////////////////////////////////////////
// CObjEvent

class CObjEvent : public CObjBase {
    DECLARE_SERIAL(CObjEvent)

public:
    CObjEvent();
    CObjEvent(VXMLOBJTYPE nType, int nId);
    virtual ~CObjEvent();

    void Serialize(Archive& ar);
    void operator=(const CObjEvent& o);

    inline void set_limit(int c) { m_nCount = c; }
    inline void SetEventName(LPCSTR pszName) { m_strEventName = pszName; }
    inline void SetCondition(LPCSTR pszCond) { m_strCond = pszCond; }

    inline int count() { return m_nCount; }
    inline LPCSTR GetEventName() { return (LPCSTR)m_strEventName; }
    inline LPCSTR GetCondition() { return (LPCSTR)m_strCond; }

    inline virtual int GetNumberOfPrompts() { return m_nNumOfPlays; }
    inline virtual CObjPlay** GetPrompts() { return m_pPlays; }

    inline virtual void SetNextDialog(CObjDialog* pDialog) {}
    inline virtual CObjDialog* GetNextDialog() { return NULL; }

    bool PrepareToRun();

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    int m_nCount;
    CString m_strEventName;
    CString m_strCond;

    // variables for internal use
    int m_nNumOfPlays;
    CObjPlay** m_pPlays;
};

//////////////////////////////////////////////////////////////////////
// CObjNoinput

class CObjNoinput : public CObjEvent {
    DECLARE_SERIAL(CObjNoinput)

public:
    CObjNoinput() : CObjEvent(VXMLOBJ_NOINPUT, 0) {}
    CObjNoinput(int nId) : CObjEvent(VXMLOBJ_NOINPUT, nId) {}
    virtual ~CObjNoinput() {}

    void Serialize(Archive& ar) { CObjEvent::Serialize(ar); }
    void operator=(const CObjNoinput& o) { CObjEvent::operator=(o); }

    inline void SetNextDialog(CObjDialog* pDialog) { m_pNextDialog = pDialog; }
    inline CObjDialog* GetNextDialog() { return m_pNextDialog; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file - nothing

    // variables for internal use
    CObjDialog* m_pNextDialog;
};

//////////////////////////////////////////////////////////////////////
// CObjNomatch

class CObjNomatch : public CObjEvent {
    DECLARE_SERIAL(CObjNomatch)

public:
    CObjNomatch() : CObjEvent(VXMLOBJ_NOMATCH, 0) {}
    CObjNomatch(int nId) : CObjEvent(VXMLOBJ_NOMATCH, nId) {}
    virtual ~CObjNomatch() {}

    void Serialize(Archive& ar) { CObjEvent::Serialize(ar); }
    void operator=(const CObjNomatch& o) { CObjEvent::operator=(o); }

    inline void SetNextDialog(CObjDialog* pDialog) { m_pNextDialog = pDialog; }
    inline CObjDialog* GetNextDialog() { return m_pNextDialog; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file - nothing

    // variables for internal use
    CObjDialog* m_pNextDialog;
};

//////////////////////////////////////////////////////////////////////
// CObjExit

class CObjExit : public CObjEvent {
    DECLARE_SERIAL(CObjExit)

public:
    CObjExit() : CObjEvent(VXMLOBJ_EXIT, 0) {}
    CObjExit(int nId) : CObjEvent(VXMLOBJ_EXIT, nId) {}
    virtual ~CObjExit() {}

    void Serialize(Archive& ar) { CObjEvent::Serialize(ar); }
    void operator=(const CObjExit& o) { CObjEvent::operator=(o); }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file - nothing
};

//////////////////////////////////////////////////////////////////////
// CObjError

class CObjError : public CObjEvent {
    DECLARE_SERIAL(CObjError)

public:
    CObjError() : CObjEvent(VXMLOBJ_ERROR, 0) {}
    CObjError(int nId) : CObjEvent(VXMLOBJ_ERROR, nId) {}
    virtual ~CObjError() {}

    void Serialize(Archive& ar) { CObjEvent::Serialize(ar); }
    void operator=(const CObjNomatch& o) { CObjEvent::operator=(o); }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file - nothing
};

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// CObjCatch

class CObjCatch : public CObjEvent {
    DECLARE_SERIAL(CObjCatch)

public:
    CObjCatch() : CObjEvent(VXMLOBJ_CATCH, 0) {}
    CObjCatch(int nId) : CObjEvent(VXMLOBJ_CATCH, nId) {}
    virtual ~CObjCatch() {}

    void Serialize(Archive& ar) { CObjEvent::Serialize(ar); }
    void operator=(const CObjNomatch& o) { CObjEvent::operator=(o); }

    inline void SetNextDialog(CObjDialog* pDialog) { m_pNextDialog = pDialog; }
    inline CObjDialog* GetNextDialog() { return m_pNextDialog; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file - nothing

    // variables for internal use
    CObjDialog* m_pNextDialog;
};

//////////////////////////////////////////////////////////////////////
// CObjGoto

class CObjGoto : public CObjBase {
    DECLARE_SERIAL(CObjGoto)

public:
    CObjGoto();
    CObjGoto(int nId);
    virtual ~CObjGoto();

    void Serialize(Archive& ar);
    void operator=(const CObjGoto& o);

    bool SetNext(LPCSTR pszNext);
    inline void SetNextItem(LPCSTR pszNextItem) { m_strNextItem = pszNextItem; }
    inline void SetNextId(int n) { m_nNextId = n; }
    inline void SetNextItemId(int n) { m_nNextItemId = n; }

    inline LPCSTR GetNextDoc() { return (LPCSTR)m_strNextDoc; }
    inline LPCSTR GetNext() { return (LPCSTR)m_strNext; }
    inline LPCSTR GetNextItem() { return (LPCSTR)m_strNextItem; }
    inline int GetNextId() { return m_nNextId; }
    inline int GetNextItemId() { return m_nNextItemId; }

    bool PrepareToRun();

    //@@ Modify 02/04/02
    //   inline CObjDialog *GetNextDialog() { return m_pNextDialog; }
    CObjDialog* GetNextDialog();
    //@@ Add 01/30/02 - Jump File
    bool SetJumpDoc(LPCSTR pszNext);
    //@@ Add 02/04/02
    inline void SetExpr(LPCSTR pszExpr) { m_strExpr = pszExpr; }
    inline LPCSTR GetExpr() { return (LPCSTR)m_strExpr; }
    inline void SetExprItem(LPCSTR pszExprItem) { m_strExprItem = pszExprItem; }
    inline LPCSTR GetExprItem() { return (LPCSTR)m_strExprItem; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file
    CString m_strNextDoc;
    CString m_strNext;
    CString m_strNextItem;
    int m_nNextId;
    int m_nNextItemId;

    // variables for internal use
    CObjDialog* m_pNextDialog;
    //@@ Add 02/04/02
    CString m_strExpr;
    CString m_strExprItem;
};

///////////////////////////////////////////////////////////////////////////
// CVxmlObjects

class QThread;

typedef CTypedPtrList<CObList, CObjBase*> CObjBaseList;

class CVxmlObjects : public Object {
    DECLARE_DYNAMIC(CVxmlObjects)

public:
    CObjBaseList m_liObjects;
    CVxmlObjects();
    virtual ~CVxmlObjects();

    inline void SetChannel(QThread* pChannel) { m_pChannel = pChannel; }
    inline QThread* GetChannel() { return m_pChannel; }
    static inline AUDIOTYPE GetAudioType() { return m_nAudioType; }
    static inline void SetAudioType(AUDIOTYPE type) { m_nAudioType = type; }

    void Serialize(Archive& ar);
    void operator=(const CVxmlObjects& o);

    inline void SetObjVxml(CObjVxml* pObjVxml) { m_pObjVxml = pObjVxml; }
    inline CObjVxml* GetObjVxml() { return m_pObjVxml; }

    CObjBase* new_prop(int nType);
    void DeleteLastObj();
    void AddObject(CObjBase* pObject);
    void AddObjectBefore(CObjBase* pObject, CObjBase* pInsert = NULL);
    void AddObjectAfter(CObjBase* pObject, CObjBase* pInsert = NULL);
    CObjBase* FindObject(const CRuntimeClass* pClass, LPCSTR pszName);
    CObjBase* FindObject(const CRuntimeClass* pClass, int id);
    CObjBase* FindObject(int id);
    CObjBase* FindObjectInParent(const CRuntimeClass* pClass, int pid);
    CObjDialog* FindDialogObject(LPCSTR pszName);
    CObjDialog* FindDialogObject(int id);
    CObjPlay* FindPlayObject(int id);
    CObjEvent* FindEventObject(int id);

    bool DoesPropExist(LPCSTR key);
    bool AddPropValue(LPCSTR key, LPCSTR value);
    bool UpdatePropValue(LPCSTR key, LPCSTR value);
    bool RestorePropValue(LPCSTR key);

    void remove_all();

    void GatherDialogObject();  // set CVxmlObjects in the CObjBase
    bool PrepareToRun();
    //@@ Modify 02/14/02
    void SetAni(LPCSTR pszAni);    // the number of the calling phone
    void SetDnis(LPCSTR pszDnis);  // the number that the caller dialed
    bool Run(VXMLEVENT nEvent, LPCSTR pValue = NULL, int nValueLen = 0);
    // Add 01/19/02
    inline void SetPyInterp(PyInterp* pPyIn) { m_pPyInterp = pPyIn; }

#ifdef __MEM_CACHE__
    PDK32U GetRefCount() { return m_nRefCount; }
    PDK32U IncRefCount() { return ++m_nRefCount; }
#endif

#ifdef DEBUG
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    void EventName(VXMLEVENT nEvent, char* pBuf, int buflen);
    PDK32U StringToDtmfValue(CString& dtmfs);
    void StartRunning();
    void CheckProperty(CObjBase* pBase);
    void CheckVar(CObjBase* pBase);
    void CheckAssign(CObjBase* pBase);
    //Add 01/23/02
    CObjDialog* CheckIf(CObjBase* pBase);
    //Added by eehd
    bool CheckTransfer(CObjBase* pBase);

    inline CObjDialog* GetTheFirstDialog() { return (CObjDialog*)m_liDialogObjects.GetHead(); }
    void InitDialog(CObjDialog* pDialog);
    VXMLREQ* QueuePrompt();
    VXMLREQ* QueuePrompt(CObjEvent* pEvent);
    VXMLREQ* QueueRecord();
    bool StartNewDialog(CObjDialog* pNewDialog);

protected:
    //   CObjBaseList m_liObjects;
    CObList m_liDialogObjects;
    Properties m_Props;
    CVariables m_Vars;

    CObjVxml* m_pObjVxml;
    CObjBase* m_pLastObj;

    // variables to run the document
    QThread* m_pChannel;
    CObjDialog* m_pCurDialog;
    bool m_bDoEvent;
    CObjEvent* m_pCurEvent;
    // Added by eehd
    CObjBase* m_pCurObject;
    CString m_strTranName;

#ifdef __MEM_CACHE__
    PDK32U m_nRefCount;
#endif
    VXMLREQ* m_pVxmlReq;
    Logger* logger_;
    //@@ Add 01/11/02
    PyInterp* m_pPyInterp;
    //@@ Add by eehd
    FarPtr FarSelf;

protected:
    static int m_nCurrentGenId;
    static int GetNewId();
    static AUDIOTYPE m_nAudioType;
};

//@~ Add 01/11/02
///////////////////////////////////////////////////////////////////////////
// CObjScript

class CObjScript : public CObjBase {
    DECLARE_SERIAL(CObjScript)

public:
    CObjScript();
    CObjScript(int nId);

    virtual ~CObjScript();

    void Serialize(Archive& ar);
    void operator=(const CObjScript& o);

    inline void SetSource(LPCSTR pszSource) { m_strSource = pszSource; }
    inline LPCSTR GetSource() { return (LPCSTR)m_strSource; }
    inline void SetCharSet(LPCSTR pszSource) { m_strCharset = pszSource; }
    inline LPCSTR GetCharSet() { return (LPCSTR)m_strCharset; }
    inline void SetBlock(LPCSTR pszSource) { m_strBlock = pszSource; }
    inline LPCSTR GetBlock() { return (LPCSTR)m_strBlock; }
    bool PrepareToRun();

protected:
    CString m_strSource;
    CString m_strCharset;
    CString m_strBlock;

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif
};
//~@

//@~ Add 01/14/02
///////////////////////////////////////////////////////////////////////////
// CObjIf

class CObjIf : public CObjBase {
    DECLARE_SERIAL(CObjIf)

public:
    CObjIf();
    CObjIf(int nId);
    CObjIf(VXMLOBJTYPE nType, int nId, LPCSTR pszName = NULL);

    virtual ~CObjIf();

    void Serialize(Archive& ar);
    void operator=(const CObjIf& o);
    bool PrepareToRun();
    inline void SetCondition(LPCSTR pszCond) { m_strCond = pszCond; }
    inline LPCSTR GetCondition() { return (LPCSTR)m_strCond; }
    CObjDialog* GetNextDialog();

    inline virtual int GetAssignCount() { return m_nNumOfAssigns; }
    inline virtual CObjAssign** GetAssigns() { return m_pAssigns; }

    inline virtual int GetIfCount() { return m_nNumOfIfs; }
    inline virtual CObjIf** GetIfs() { return m_pIfs; }

protected:
    CString m_strCond;

    int m_nNumOfAssigns;
    CObjAssign** m_pAssigns;

    int m_nNumOfIfs;
    CObjIf** m_pIfs;
    CObjDialog* m_pNextDialog;

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif
};

///////////////////////////////////////////////////////////////////////////
// CObjElseif

class CObjElseif : public CObjIf {
    DECLARE_SERIAL(CObjElseif)

public:
    CObjElseif();
    CObjElseif(int nId);

    virtual ~CObjElseif();

    void Serialize(Archive& ar);
    void operator=(const CObjElseif& o);
    bool PrepareToRun();

protected:
#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif
};

///////////////////////////////////////////////////////////////////////////
// CObjElse

class CObjElse : public CObjIf {
    DECLARE_SERIAL(CObjElse)

public:
    CObjElse();
    CObjElse(int nId);

    virtual ~CObjElse();

    void Serialize(Archive& ar);
    void operator=(const CObjElse& o);
    bool PrepareToRun();

protected:
#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif
};

//~@

//@@ Modified by eehd
//////////////////////////////////////////////////////////////////////
// CObjTransfer

class CObjTransfer : public CObjBase {
    DECLARE_SERIAL(CObjTransfer)

public:
    CObjTransfer();
    CObjTransfer(int nId);
    virtual ~CObjTransfer();

    void Serialize(Archive& ar);
    void operator=(const CObjTransfer& o);
    inline void SetDest(LPCSTR pszDestExpr) { m_strDestExpr = pszDestExpr; }
    inline LPCSTR GetDest() { return (LPCSTR)m_strDestExpr; }
    bool PrepareToRun();

    //@@ Add 04/07/2003 by eehd
    inline void SetConnectTimeout(int t) { m_nConnectTimeout = t; }
    inline int GetConnectTimeout() { return m_nConnectTimeout; }
    inline void SetName(LPCSTR pszName) { name_ = pszName; }
    inline LPCSTR name() { return static_cast<LPCSTR>(name_); }
    inline int GetIfCount() { return m_nNumOfIfs; }
    inline CObjIf** GetIfs() { return m_pIfs; }

#ifdef DEBUG
    virtual void AssertValid() const;
    virtual void Dump(CDumpContext& dc) const;
#endif

protected:
    // variables to write in the file - nothing
private:
    CString m_strDestExpr;

    //@@ Add 04/07/2003 by eehd
    CString name_;
    int m_nConnectTimeout;
    CObjIf** m_pIfs;
    int m_nNumOfIfs;
};
//~@

#endif

}  // namespace pdk
