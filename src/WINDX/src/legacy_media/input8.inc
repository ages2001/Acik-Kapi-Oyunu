/*
 * DirectInput8Create entry point for the native build of Wine 2.0.5 dinput.
 * Based on Wine dlls/dinput8/dinput8_main.c, copyright 2002 TransGaming
 * Technologies Inc. and 2006 Roderick Colenbrander, LGPL-2.1-or-later.
 * The DirectInput 8 implementation and its COM factory reside in this DLL.
 */
HRESULT WINAPI DirectInput8Create(HINSTANCE app, DWORD version, REFIID iid,
                                  void **out, IUnknown *outer)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (!iid) return E_POINTER;
    if (!IsEqualGUID(iid, &IID_IDirectInput8A) &&
        !IsEqualGUID(iid, &IID_IDirectInput8W) &&
        !IsEqualGUID(iid, &IID_IUnknown)) return DIERR_NOINTERFACE;
    HRESULT hr = create_directinput_instance(iid, out, NULL);
    if (FAILED(hr) || outer || IsEqualGUID(iid, &IID_IUnknown)) return hr;
    /* The ANSI and Unicode vtables share the Initialize and Release ABI. */
    IDirectInput8A *input = *out;
    hr = IDirectInput8_Initialize(input, app, version);
    if (FAILED(hr)) {
        IDirectInput8_Release(input);
        *out = NULL;
    }
    return hr;
}
