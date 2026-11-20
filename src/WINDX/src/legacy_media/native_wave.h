/* Native Windows WinMM adaptation for the Wine 1.2.3 DirectSound mixer. */
MMRESULT wx_wave_description(UINT device, BOOL capture, DSDRIVERDESC *desc);
void wx_choose_primary_format(UINT device, WAVEFORMATEX *format);
