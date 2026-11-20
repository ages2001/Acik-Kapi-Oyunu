from pathlib import Path
root=Path(__file__).resolve().parents[1]
src=root/'src'/'inf'
for family in ('NT3X','NT4','NT5X86','XPAMD64','WIN9X'):
    inf='WXPDM.INF'
    if family=='WIN9X':
        text=(src/'win9x.inf').read_text()
    elif family=='NT3X':
        text=(src/'nt3x.inf').read_text()
        inf='OEMSETUP.INF'
    else:
        text=(src/'nt.inf.in').read_text()
        x64=family=='XPAMD64'
        text=text.replace('@ARCH@','.NTamd64' if x64 else '').replace('@DECORATION@',',NTamd64' if x64 else '')
    (root/'bin'/family/inf).write_bytes(text.replace('\n','\r\n').encode('ascii'))
