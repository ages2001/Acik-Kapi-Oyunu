/* Poll position and buttons when the operating system has no low-level hook.
 * Feed the same data format and buffered-event queue as the hook backend. */
void wx_sample_mouse(IDirectInputDeviceImpl *device)
{
    static const unsigned vkeys[] = {VK_LBUTTON,VK_RBUTTON,VK_MBUTTON,VK_XBUTTON1,VK_XBUTTON2};
    SysMouseImpl *self = CONTAINING_RECORD(device, SysMouseImpl, base);
    POINT point;
    DWORD now=GetTickCount();
    EnterCriticalSection(&device->crit);
    if (!device->acquired || ((device->dwCoopLevel & DISCL_FOREGROUND) && GetForegroundWindow()!=device->win))
        goto done;
    if (GetCursorPos(&point)) {
        LONG delta[2]={point.x-self->sample_position.x,point.y-self->sample_position.y};
        BOOL absolute=(device->data_format.user_df->dwFlags & DIDF_ABSAXIS)!=0;
        if (self->have_sample_position) {
            if (absolute) { self->m_state.lX=point.x; self->m_state.lY=point.y; }
            else { self->m_state.lX+=delta[0]; self->m_state.lY+=delta[1]; }
            if (delta[0]) queue_event(&device->IDirectInputDevice8A_iface,
                    DIDFT_MAKEINSTANCE(WINE_MOUSE_X_AXIS_INSTANCE)|DIDFT_RELAXIS,absolute?point.x:delta[0],now,
                    InterlockedIncrement((LONG *)&device->dinput->evsequence));
            if (delta[1]) queue_event(&device->IDirectInputDevice8A_iface,
                    DIDFT_MAKEINSTANCE(WINE_MOUSE_Y_AXIS_INSTANCE)|DIDFT_RELAXIS,absolute?point.y:delta[1],now,
                    InterlockedIncrement((LONG *)&device->dinput->evsequence));
        }
        self->have_sample_position=TRUE;
        self->sample_position=point;
    }
    for (unsigned i=0;i<sizeof(vkeys)/sizeof(vkeys[0]);++i) {
        BYTE state=(GetAsyncKeyState(vkeys[i]) & 0x8000) ? 0x80 : 0;
        if (self->m_state.rgbButtons[i]==state) continue;
        self->m_state.rgbButtons[i]=state;
        queue_event(&device->IDirectInputDevice8A_iface,DIDFT_MAKEINSTANCE(WINE_MOUSE_BUTTONS_INSTANCE+i)|DIDFT_PSHBUTTON,
                    state,now,InterlockedIncrement((LONG *)&device->dinput->evsequence));
    }
done:
    LeaveCriticalSection(&device->crit);
}
