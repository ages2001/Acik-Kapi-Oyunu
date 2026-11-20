/* Called while the application's DirectInput keyboard is acquired.
 * NT 3.1 has no low-level keyboard hook. Sample the native key state, map
 * keyboard scan codes to DirectInput offsets, and feed the normal event queue.
 */
void wx_sample_keyboard(IDirectInputDeviceImpl *device)
{
    SysKeyboardImpl *self = CONTAINING_RECORD(device, SysKeyboardImpl, base);
    BYTE keys[256] = {0};
    unsigned vk;
    EnterCriticalSection(&device->crit);
    if (!device->acquired || ((device->dwCoopLevel & DISCL_FOREGROUND) && GetForegroundWindow()!=device->win))
        goto done;
    for (vk=VK_BACK; vk<256; ++vk) {
        if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
        unsigned code = MapVirtualKeyA(vk, MAPVK_VK_TO_VSC) & 0xff;
        /* Extended navigation keys and right modifiers share scan codes with
         * their unextended keypad and left-hand counterparts. */
        switch (vk) {
        case VK_RMENU: code=DIK_RMENU; break;
        case VK_RCONTROL: code=DIK_RCONTROL; break;
        case VK_RSHIFT: code=DIK_RSHIFT; break;
        case VK_LMENU: code=DIK_LMENU; break;
        case VK_LCONTROL: code=DIK_LCONTROL; break;
        case VK_LSHIFT: code=DIK_LSHIFT; break;
        case VK_SHIFT: if (GetAsyncKeyState(VK_RSHIFT)&0x8000) continue; code=DIK_LSHIFT; break;
        case VK_CONTROL: if (GetAsyncKeyState(VK_RCONTROL)&0x8000) continue; code=DIK_LCONTROL; break;
        case VK_MENU: if (GetAsyncKeyState(VK_RMENU)&0x8000) continue; code=DIK_LMENU; break;
        case VK_PRIOR: case VK_NEXT: case VK_END: case VK_HOME:
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
        case VK_INSERT: case VK_DELETE: case VK_DIVIDE: case VK_LWIN: case VK_RWIN: case VK_APPS:
            if (code) code |= 0x80;
            break;
        case VK_SNAPSHOT: code=DIK_SYSRQ; break;
        case VK_PAUSE: code=DIK_PAUSE; break;
        }
        if (code < sizeof(keys) && code) keys[code]=0x80;
    }
    for (unsigned code=0;code<sizeof(keys);++code) {
        if (keys[code] == self->DInputKeyState[code]) continue;
        self->DInputKeyState[code]=keys[code];
        queue_event(&device->IDirectInputDevice8A_iface,DIDFT_MAKEINSTANCE(code)|DIDFT_PSHBUTTON,
                    keys[code],GetTickCount(),InterlockedIncrement((LONG *)&device->dinput->evsequence));
    }
done:
    LeaveCriticalSection(&device->crit);
}
