/* Included by the Wine DxDiag provider after its container-property helpers. */
#include "wx_display.h"

static HRESULT wx_display_property(IDxDiagContainerImpl_Container *node, const char *name, const char *value)
{
    WCHAR key[64];
    if (!MultiByteToWideChar(CP_ACP,0,name,-1,key,sizeof(key)/sizeof(*key))) return E_FAIL;
    return add_hardware_text(node,key,value);
}

static HRESULT wx_display_bool(IDxDiagContainerImpl_Container *node, const char *name, BOOL value)
{
    WCHAR key[64];
    if (!MultiByteToWideChar(CP_ACP,0,name,-1,key,sizeof(key)/sizeof(*key))) return E_FAIL;
    return add_bool_property(node,key,value);
}

static HRESULT wx_fill_measured_display(IDxDiagContainerImpl_Container *node, IDirect3D9 *api, UINT index)
{
    struct wx_display_inventory d;
    wx_display_inventory_read(index, NULL, &d);
    if (!d.active_windx) return S_FALSE;
    const struct {const WCHAR *property; const char *value;} fields[] = {
        {szDescription,d.description},{szDeviceName,d.device},{szDriverName,d.driver},
        {szDriverVersion,d.driver_version},{szManufacturer,d.manufacturer},{szChipType,d.chip},
        {szDACType,d.dac},{szKeyDeviceKey,d.native_key},{szKeyDeviceID,d.native_device_id},
        {szDisplayMemoryLocalized,d.memory},{szDisplayMemoryEnglish,d.memory},{szRevision,""}
    };
    HRESULT hr;
    for (unsigned i=0;i<sizeof(fields)/sizeof(*fields);++i) {
        hr=add_hardware_text(node,fields[i].property,fields[i].value);
        if (FAILED(hr)) return hr;
    }
    const struct {const WCHAR *property; DWORD value;} mode[] = {
        {dwWidth,d.width},{dwHeight,d.height},{dwBpp,d.bits},{dwRefreshRate,d.hz}
    };
    for (unsigned i=0;i<sizeof(mode)/sizeof(*mode);++i) {
        hr=add_ui4_property(node,mode[i].property,mode[i].value);
        if (FAILED(hr)) return hr;
    }
    hr=wx_display_property(node,"szDisplayModeEnglish",d.mode);
    if (SUCCEEDED(hr)) hr=wx_display_property(node,"szDisplayModeLocalized",d.mode);
    if (SUCCEEDED(hr)) hr=wx_display_property(node,"szWINDXGLRenderer",d.renderer);
    if (FAILED(hr)) return hr;

    /* Keep identifiers returned by the API available to applications. Its
     * adapter description can name an emulated GPU on a CPU renderer. */
    char notes[356] = "";
    D3DADAPTER_IDENTIFIER9 id;
    if (api && SUCCEEDED(IDirect3D9_GetAdapterIdentifier(api,index,0,&id))) {
        char vendor[16], device[16], subsystem[16], revision[16];
        WCHAR guid[39];
        wsprintfA(vendor,"0x%04lx",id.VendorId);
        wsprintfA(device,"0x%04lx",id.DeviceId);
        wsprintfA(subsystem,"0x%08lx",id.SubSysId);
        wsprintfA(revision,"0x%04lx",id.Revision);
        hr=add_hardware_text(node,szVendorId,vendor);
        if (SUCCEEDED(hr)) hr=add_hardware_text(node,szDeviceId,device);
        if (SUCCEEDED(hr)) hr=add_hardware_text(node,szSubSysId,subsystem);
        if (SUCCEEDED(hr)) hr=add_hardware_text(node,szRevisionId,revision);
        if (SUCCEEDED(hr) && StringFromGUID2(&id.DeviceIdentifier,guid,39))
            hr=add_bstr_property(node,szDeviceIdentifier,guid);
        if (FAILED(hr)) return hr;
        wsprintfA(notes,"Direct3D adapter profile IDs: vendor %s, device %s.",vendor,device);
        if (d.software) lstrcatA(notes," Direct3D renders on the CPU.");
    } else if (d.software) lstrcpyA(notes,"The active renderer runs on the CPU.");
    hr=wx_display_property(node,"szNotesEnglish",notes);
    if (SUCCEEDED(hr)) hr=wx_display_property(node,"szNotesLocalized",notes);
    if (SUCCEEDED(hr) && d.software) hr=wx_display_bool(node,"b3DAccelerationExists",FALSE);
    if (SUCCEEDED(hr) && d.software) hr=wx_display_bool(node,"b3DAccelerationEnabled",FALSE);
    if (SUCCEEDED(hr) && d.software) hr=wx_display_bool(node,"bDDAccelerationEnabled",FALSE);
    if (SUCCEEDED(hr) && d.width && d.height) hr=wx_display_bool(node,"bNoHardware",FALSE);
    return hr;
}
