/*
 * dhcpnt31.c - standalone DHCP client for Windows NT 3.1.
 *
 * NT 3.1's TCP/IP-32 has no DHCP client of its own.  This small console
 * program performs a full DHCP handshake over a UDP broadcast socket
 * (Windows Sockets 1.1), then prints and optionally records the lease so
 * the RLTKLANT adapter can be configured on NT 3.1.
 *
 * Built freestanding (no C runtime): NT 3.1's KERNEL32.DLL predates
 * IsDBCSLeadByteEx and NT 3.1 has no MSVCRT.DLL, so the mingw CRT cannot
 * be used.  This file supplies its own entry point, console output and
 * string helpers and imports only KERNEL32 and WSOCK32.
 *
 * Copyright (C) 2026 ages2001.
 */
#include <winsock.h>
#include <windows.h>
#include <winreg.h>

#define DHCP_SERVER_PORT 67
#define DHCP_CLIENT_PORT 68
#define BOOTREQUEST      1
#define BOOTREPLY        2
#define DHCP_MAGIC       0x63825363UL

#define DHCPDISCOVER 1
#define DHCPOFFER    2
#define DHCPREQUEST  3
#define DHCPDECLINE  4
#define DHCPACK      5
#define DHCPNAK      6
#define DHCPRELEASE  7

#define OPT_PAD            0
#define OPT_SUBNET_MASK    1
#define OPT_ROUTER         3
#define OPT_DNS            6
#define OPT_HOSTNAME      12
#define OPT_REQUESTED_IP  50
#define OPT_LEASE_TIME    51
#define OPT_MSG_TYPE      53
#define OPT_SERVER_ID     54
#define OPT_PARAM_LIST    55
#define OPT_END          255

#pragma pack(push,1)
typedef struct _DHCP_MSG {
    unsigned char  op;
    unsigned char  htype;
    unsigned char  hlen;
    unsigned char  hops;
    unsigned long  xid;
    unsigned short secs;
    unsigned short flags;
    unsigned long  ciaddr;
    unsigned long  yiaddr;
    unsigned long  siaddr;
    unsigned long  giaddr;
    unsigned char  chaddr[16];
    unsigned char  sname[64];
    unsigned char  file[128];
    unsigned char  options[312];
} DHCP_MSG;
#pragma pack(pop)

/* ---- freestanding runtime helpers (no CRT) ---- */

static void *my_memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *dd = (unsigned char*)d; const unsigned char *ss = (const unsigned char*)s;
    while (n--) *dd++ = *ss++;
    return d;
}
static void *my_memset(void *d, int c, unsigned n)
{
    unsigned char *dd = (unsigned char*)d;
    while (n--) *dd++ = (unsigned char)c;
    return d;
}
/* gcc may emit calls to memcpy/memset for struct ops even with -ffreestanding */
void *memcpy(void *d, const void *s, unsigned n) { return my_memcpy(d,s,n); }
void *memset(void *d, int c, unsigned n)         { return my_memset(d,c,n); }

static unsigned my_strlen(const char *s){ unsigned n=0; while(s[n]) n++; return n; }

static void out(const char *s)
{
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD wr;
    WriteFile(h, s, my_strlen(s), &wr, NULL);
}

/* read one line from stdin; return first non-space char (0 on EOF) */
static char ask_char(const char *prompt)
{
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    char b[16]; DWORD rd = 0; int i;
    out(prompt);
    if (!ReadFile(h, b, sizeof(b)-1, &rd, NULL) || rd == 0) return 0;
    for (i = 0; i < (int)rd; i++)
        if (b[i] != ' ' && b[i] != '\t' && b[i] != '\r' && b[i] != '\n') return b[i];
    return 0;
}

/* append unsigned decimal to buf at *pos */
static void app_u(char *buf, int *pos, unsigned long v)
{
    char tmp[16]; int i = 0;
    if (v == 0) { buf[(*pos)++] = '0'; return; }
    while (v) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
    while (i) buf[(*pos)++] = tmp[--i];
}
static void app_s(char *buf, int *pos, const char *s){ while(*s) buf[(*pos)++]=*s++; }

/* net-order IPv4 -> "a.b.c.d" */
static void ipstr(unsigned long addr, char *buf)
{
    unsigned char *b = (unsigned char*)&addr; int p = 0;
    app_u(buf,&p,b[0]); buf[p++]='.'; app_u(buf,&p,b[1]); buf[p++]='.';
    app_u(buf,&p,b[2]); buf[p++]='.'; app_u(buf,&p,b[3]); buf[p]=0;
}

/* parse "aa-bb-cc-dd-ee-ff" (also accepts ':') into mac[6]; return 1 ok */
static int parse_mac(const char *s, unsigned char *mac)
{
    int i, j; int v;
    for (i = 0; i < 6; i++) {
        v = 0;
        for (j = 0; j < 2; j++) {
            char c = *s++;
            int d;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
            else return 0;
            v = (v << 4) | d;
        }
        mac[i] = (unsigned char)v;
        if (i < 5) { if (*s != '-' && *s != ':') return 0; s++; }
    }
    return 1;
}

static unsigned char *put_opt(unsigned char *p, unsigned char code,
                              unsigned char len, const void *val)
{
    *p++ = code; *p++ = len;
    my_memcpy(p, val, len);
    return p + len;
}

static const unsigned char *find_opt(const DHCP_MSG *m, unsigned received, unsigned char code,
                                     unsigned char *outlen)
{
    const unsigned char *p, *end;
    if (outlen) *outlen = 0;
    if (received < 240 || received > sizeof(*m)) return NULL;
    p = m->options + 4;          /* skip magic cookie */
    end = (const unsigned char *)m + received;
    while (p < end && *p != OPT_END) {
        unsigned char c = *p++;
        unsigned char l;
        if (c == OPT_PAD) continue;
        if (p == end) return NULL;
        l = *p++;
        if ((unsigned)(end - p) < l) return NULL;
        if (c == code) { if (outlen) *outlen = l; return p; }
        p += l;
    }
    return NULL;
}

/* Reject short, unrelated or malformed datagrams before reading any options.
 * recvfrom does not clear the unused tail of the receive structure. */
static int valid_reply(const DHCP_MSG *m, unsigned received,
                       unsigned long xid, const unsigned char *mac)
{
    const unsigned char *p, *end;
    unsigned i;
    if (received < 241 || received > sizeof(*m) || m->op != BOOTREPLY ||
        m->htype != 1 || m->hlen != 6 || m->xid != xid) return 0;
    for (i = 0; i < 6; i++) if (m->chaddr[i] != mac[i]) return 0;
    if (m->options[0] != 99 || m->options[1] != 130 ||
        m->options[2] != 83 || m->options[3] != 99) return 0;
    p = m->options + 4; end = (const unsigned char *)m + received;
    while (p < end) {
        unsigned char code = *p++, length;
        if (code == OPT_END) return 1;
        if (code == OPT_PAD) continue;
        if (p == end) return 0;
        length = *p++;
        if ((unsigned)(end - p) < length) return 0;
        p += length;
    }
    return 0; /* require an END marker within the received datagram */
}

static unsigned long build_msg(DHCP_MSG *m, unsigned char type,
                               const unsigned char *mac, unsigned long xid,
                               unsigned long reqip, unsigned long serverid)
{
    unsigned char *p;
    unsigned char plist[4];
    my_memset(m, 0, sizeof(*m));
    m->op = BOOTREQUEST; m->htype = 1; m->hlen = 6;
    m->xid = xid; m->flags = htons(0x8000);            /* broadcast reply */
    my_memcpy(m->chaddr, mac, 6);

    p = m->options;
    *(unsigned long*)p = htonl(DHCP_MAGIC); p += 4;
    p = put_opt(p, OPT_MSG_TYPE, 1, &type);
    if (reqip)    p = put_opt(p, OPT_REQUESTED_IP, 4, &reqip);
    if (serverid) p = put_opt(p, OPT_SERVER_ID, 4, &serverid);
    plist[0]=OPT_SUBNET_MASK; plist[1]=OPT_ROUTER; plist[2]=OPT_DNS; plist[3]=OPT_LEASE_TIME;
    p = put_opt(p, OPT_PARAM_LIST, 4, plist);
    *p++ = OPT_END;
    return (unsigned long)(p - (unsigned char*)m);
}

/* print "label" + ip + newline */
static void print_kv(const char *label, unsigned long netip)
{
    char line[64]; int p = 0; char ip[20];
    ipstr(netip, ip);
    app_s(line,&p,label); app_s(line,&p,ip); line[p++]='\n'; line[p]=0;
    out(line);
}

/* ---- registry: locate and update the RLTKLANT adapter's TCP/IP config ---- */

#define SVCBASE "SYSTEM\\CurrentControlSet\\Services"

/* TRUE if this Parameters\Tcpip key is a real, DHCP-configurable network
   adapter: it carries an IPAddress value (every NT 3.1 TCP/IP-32 adapter
   does), and it is not the software loopback (127.x), which must never be
   handed a DHCP lease.  This is what makes DHCPNT31 a *general* NT 3.1 DHCP
   client - it finds any bound physical adapter, not only RLTKLANT. */
static int is_tcpip_adapter(HKEY htcp)
{
    char buf[128]; DWORD bl = sizeof(buf), type = 0;
    if (RegQueryValueExA(htcp, "IPAddress", 0, &type, (LPBYTE)buf, &bl) != ERROR_SUCCESS)
        return 0;
    /* skip the loopback (IPAddress 127.x.x.x) */
    buf[bl < (DWORD)sizeof(buf) ? bl : sizeof(buf)-1] = 0;
    if (buf[0]=='1' && buf[1]=='2' && buf[2]=='7' && buf[3]=='.') return 0;
    return 1;
}
/* TRUE if a service name is the loopback (never configure it). */
static int is_loopback_name(const char *n)
{
    return (n[0]=='L'||n[0]=='l') && (n[1]=='o'||n[1]=='O') &&
           (n[2]=='o'||n[2]=='O') && (n[3]=='p'||n[3]=='P') &&
           (n[4]==0 || n[4]=='b' || n[4]=='B');   /* "Loop" or "Loopback" */
}

/* Open the Parameters\Tcpip key of the (want)-th TCP/IP-bound network adapter
 * (0-based), walking HKLM\...\Services in enumeration order.  Any service with
 * a Parameters\Tcpip key that has an IPAddress value counts, so a lease can be
 * applied to each card on a single- or multi-adapter machine, whatever driver
 * it uses.  Returns an open HKEY or NULL; writes the service name into svcout. */
static HKEY open_tcpip_n(int want, char *svcout)
{
    HKEY hsvc, htcp; char name[80]; char path[160]; DWORD i, nl; int p, k, seen = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, SVCBASE, 0, KEY_READ, &hsvc) != ERROR_SUCCESS)
        return NULL;
    for (i = 0; ; i++) {
        nl = sizeof(name);
        if (RegEnumKeyExA(hsvc, i, name, &nl, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
            break;
        if (is_loopback_name(name)) continue;          /* never the loopback */
        /* build "<name>\Parameters\Tcpip" */
        p = 0; for (k = 0; name[k]; k++) path[p++] = name[k];
        { const char *t = "\\Parameters\\Tcpip"; while (*t) path[p++] = *t++; }
        path[p] = 0;
        if (RegOpenKeyExA(hsvc, path, 0, KEY_READ|KEY_WRITE, &htcp) == ERROR_SUCCESS) {
            if (is_tcpip_adapter(htcp)) {
                if (seen == want) {
                    if (svcout) { for (k = 0; name[k] && k < 63; k++) svcout[k] = name[k]; svcout[k]=0; }
                    RegCloseKey(hsvc);
                    return htcp;
                }
                seen++;
            }
            RegCloseKey(htcp);          /* not the one we want; keep scanning */
        }
    }
    RegCloseKey(hsvc);
    return NULL;
}
static HKEY open_tcpip(char *svcout) { return open_tcpip_n(0, svcout); }
/* count of bound TCP/IP adapters */
static int count_adapters(void)
{
    HKEY h; char svc[64]; int n = 0;
    while ((h = open_tcpip_n(n, svc)) != NULL) { RegCloseKey(h); n++; if (n > 64) break; }
    return n;
}

typedef struct _DHCP_ROUTE { unsigned long ip, mask; } DHCP_ROUTE;

/* Multiple independent networks need an unambiguous directly connected route.
 * A wildcard limited broadcast can otherwise request the second NIC's lease
 * on the first NIC. Overlapping or unconfigured networks require a transport
 * with explicit interface selection; never guess and apply the wrong lease. */
static int select_destination(const DHCP_ROUTE *routes, int count, int idx,
                              unsigned long *destination)
{
    int i; unsigned long ip, mask, host;
    if (count < 1 || count > 64 || idx < 0 || idx >= count) return 0;
    if (count == 1) { *destination = INADDR_BROADCAST; return 1; }
    for (i = 0; i < count; i++) {
        ip = ntohl(routes[i].ip); mask = ntohl(routes[i].mask); host = ~mask;
        if (!mask || host < 3 || (host & (host + 1)) ||
            !(ip >> 24) || (ip >> 24) == 127 || (ip >> 24) >= 224 ||
            !(ip & host) || (ip & host) == host) return 0;
        if (i != idx) {
            unsigned long common = routes[i].mask & routes[idx].mask;
            if ((routes[i].ip & common) == (routes[idx].ip & common)) return 0;
        }
    }
    *destination = routes[idx].ip | ~routes[idx].mask;
    return 1;
}

static int read_network(int idx, DHCP_ROUTE *route)
{
    HKEY key = open_tcpip_n(idx, NULL); char text[64]; DWORD type, size;
    unsigned i; unsigned long *values[2];
    const char *names[2] = {"IPAddress", "SubnetMask"};
    if (!key) return 0;
    values[0] = &route->ip; values[1] = &route->mask;
    for (i = 0; i < 2; i++) {
        size = sizeof(text); type = 0;
        if (RegQueryValueExA(key, names[i], 0, &type, (LPBYTE)text, &size) != ERROR_SUCCESS ||
            type != REG_SZ || size < 2 || size > sizeof(text) || text[size-1] != 0) {
            RegCloseKey(key); return 0;
        }
        *values[i] = inet_addr(text);
    }
    RegCloseKey(key); return 1;
}

static int auto_enabled(int idx)
{
    HKEY key, tcp; char service[64], path[176]; int pos = 0;
    DWORD value = 0, type = 0, size = sizeof(value); LONG status;
    tcp = open_tcpip_n(idx, service);
    if (!tcp) return -1;
    RegCloseKey(tcp);
    app_s(path, &pos, SVCBASE "\\"); app_s(path, &pos, service);
    app_s(path, &pos, "\\Parameters"); path[pos] = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key) != ERROR_SUCCESS) return -1;
    status = RegQueryValueExA(key, "EnableDHCP", 0, &type, (LPBYTE)&value, &size);
    RegCloseKey(key);
    if (status == ERROR_FILE_NOT_FOUND) return 0;
    if (status != ERROR_SUCCESS || type != REG_DWORD || size != sizeof(value) || value > 1) return -1;
    return value == 1;
}

/* Read the idx-th bound adapter's burned-in MAC.  The RLTKLANT driver
 * publishes it as Services\<svc>\Parameters\CurrentAddress (REG_BINARY, 6
 * bytes) at init, so /auto needs no MAC on the command line.  Returns 1 and
 * fills mac[6] on success. */
static int adapter_mac_n(int idx, unsigned char *mac)
{
    HKEY htcp, hpar; char svc[64]; char path[176]; int p = 0, k;
    DWORD type = 0, bl;
    char hw[32];
    htcp = open_tcpip_n(idx, svc);
    if (!htcp) return 0;
    RegCloseKey(htcp);
    { const char *b = SVCBASE; while (*b) path[p++] = *b++; }
    path[p++] = '\\';
    for (k = 0; svc[k] && p < 160; k++) path[p++] = svc[k];
    { const char *t = "\\Parameters"; while (*t) path[p++] = *t++; }
    path[p] = 0;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &hpar) != ERROR_SUCCESS)
        return 0;
    /* the RLTKLANT driver publishes the burned-in MAC as a REG_SZ string
       "DhcpHwAddress" (e.g. "52:54:00:ab:cd:ef").  Fall back to a 6-byte
       REG_BINARY "CurrentAddress" if some driver writes that form instead. */
    bl = sizeof(hw);
    if (RegQueryValueExA(hpar, "DhcpHwAddress", 0, &type, (LPBYTE)hw, &bl) == ERROR_SUCCESS
        && type == REG_SZ && bl > 0) {
        hw[bl < sizeof(hw) ? bl : sizeof(hw) - 1] = 0;
        if (parse_mac(hw, mac)) { RegCloseKey(hpar); return 1; }
    }
    bl = 6; type = 0;
    if (RegQueryValueExA(hpar, "CurrentAddress", 0, &type, (LPBYTE)mac, &bl) == ERROR_SUCCESS
        && type == REG_BINARY && bl == 6) { RegCloseKey(hpar); return 1; }
    RegCloseKey(hpar);
    return 0;
}
static void mac_to_str(const unsigned char *mac, char *s)
{
    static const char hx[] = "0123456789abcdef";
    int i, p = 0;
    for (i = 0; i < 6; i++) {
        s[p++] = hx[mac[i] >> 4];
        s[p++] = hx[mac[i] & 15];
        if (i < 5) s[p++] = ':';
    }
    s[p] = 0;
}

/* NT 3.1 TCP/IP-32 stores IPAddress/SubnetMask/DefaultGateway as REG_SZ
 * (single strings), unlike NT 3.5+ which use REG_MULTI_SZ.                 */
static LONG set_sz(HKEY h, const char *name, const char *s)
{
    return RegSetValueExA(h, name, 0, REG_SZ, (const BYTE*)s, (DWORD)(my_strlen(s)+1));
}

/* NT3.1's resolver reads the shared Tcpip/Parameters NameServer value.
   Keep the per-adapter lease value too, but publish the distinct DNS servers
   acquired in this invocation in adapter order. Do not claim success when
   registry writes fail. The list is bounded, as is adapter enumeration. */
static char applied_dns[512];
static int apply_dns(const char *dns)
{
    HKEY h;int p=0,n=my_strlen(dns),i,start;LONG status;DWORD order=1,type=0,size=sizeof(order);
    if(!n)return 0;
    while(applied_dns[p]){
        start=p;while(applied_dns[p]&&applied_dns[p]!=' ')p++;
        if(p-start==n){for(i=0;i<n&&applied_dns[start+i]==dns[i];i++);if(i==n)return 0;}
        if(applied_dns[p])p++;
    }
    if(p+n+2>(int)sizeof(applied_dns)){out("apply: DNS server list is full\n");return 1;}
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,SVCBASE "\\Tcpip\\Parameters",0,KEY_SET_VALUE|KEY_QUERY_VALUE,&h)!=ERROR_SUCCESS){
        out("apply: cannot open the global TCP/IP DNS key\n");return 1;
    }
    start=p;if(p)applied_dns[p++]=' ';
    for(i=0;i<n;i++)applied_dns[p++]=dns[i];applied_dns[p]=0;
    status=set_sz(h,"NameServer",applied_dns);
    if(status==ERROR_SUCCESS){
        LONG query=RegQueryValueExA(h,"DNSLookupOrder",0,&type,(LPBYTE)&order,&size);
        /* NT3.1 value 4 disables DNS entirely. A leased NameServer must be
           usable; preserve existing DNS-enabled order preferences 1..3. */
        if(query==ERROR_FILE_NOT_FOUND||
           (query==ERROR_SUCCESS&&type==REG_DWORD&&size==4&&(order==0||order==4))){
            order=1;status=RegSetValueExA(h,"DNSLookupOrder",0,REG_DWORD,(const BYTE*)&order,sizeof(order));
        }else if(query!=ERROR_SUCCESS||type!=REG_DWORD||size!=4||order>4){
            status=ERROR_INVALID_DATA;
        }
    }
    RegCloseKey(h);
    if(status!=ERROR_SUCCESS){applied_dns[start]=0;out("apply: cannot write the global DNS server list\n");return 1;}
    return 0;
}

static int reg_show(void)
{
    HKEY h; char svc[64]; char val[128]; DWORD vl, type, i; char nm[64]; DWORD nl;
    char line[256]; int p, adp, any = 0;
    /* list every bound TCP/IP adapter, not just the first */
    for (adp = 0; (h = open_tcpip_n(adp, svc)) != NULL; adp++) {
        any = 1;
        p=0; app_s(line,&p,"Adapter #"); app_u(line,&p,(unsigned)(adp+1));
        app_s(line,&p,": Services\\"); app_s(line,&p,svc);
        app_s(line,&p,"\\Parameters\\Tcpip\n"); line[p]=0; out(line);
        for (i = 0; ; i++) {
            nl = sizeof(nm); vl = sizeof(val); type = 0;
            if (RegEnumValueA(h, i, nm, &nl, NULL, &type, (LPBYTE)val, &vl) != ERROR_SUCCESS) break;
            p=0; app_s(line,&p,"  "); app_s(line,&p,nm);
            app_s(line,&p," (type "); app_u(line,&p,type); app_s(line,&p,") = ");
            if (type == REG_SZ || type == REG_MULTI_SZ) {
                if (vl == 0) val[0]=0; else val[vl < (int)sizeof(val) ? vl : (int)sizeof(val)-1]=0;
                app_s(line,&p,val);
            } else if (type == REG_DWORD) {
                app_u(line,&p,*(DWORD*)val);
            }
            line[p++]='\n'; line[p]=0; out(line);
        }
        RegCloseKey(h);
    }
    if (!any) { out("No TCP/IP-bound network adapter found in the registry.\n"); return 1; }
    return 0;
}

/* write the DHCP lease into the idx-th RLTKLANT adapter's Tcpip key */
static int reg_apply_n(int idx, const char *ip, const char *mask, const char *gw, const char *dns)
{
    HKEY h; char svc[64]; char line[128]; int p,failed=0;
    h = open_tcpip_n(idx, svc);
    if (!h) { out("apply: no RLTKLANT TCP/IP key found\n"); return 1; }
    if(set_sz(h,"IPAddress",ip)!=ERROR_SUCCESS)failed=1;
    if(set_sz(h,"SubnetMask",mask)!=ERROR_SUCCESS)failed=1;
    if(gw&&gw[0]&&set_sz(h,"DefaultGateway",gw)!=ERROR_SUCCESS)failed=1;
    if(dns&&dns[0]&&set_sz(h,"NameServer",dns)!=ERROR_SUCCESS)failed=1;
    RegCloseKey(h);
    if(dns&&dns[0]&&apply_dns(dns))failed=1;
    if(failed){out("apply: one or more registry writes failed; configuration may be partial\n");return 1;}
    /* append a one-line record to C:\DHCPNT31.LOG (proof the client ran) */
    {
        HANDLE lf = CreateFileA("C:\\DHCPNT31.LOG", GENERIC_WRITE, FILE_SHARE_READ,
                                NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (lf != INVALID_HANDLE_VALUE) {
            char rec[80]; int rp = 0; DWORD wr;
            SetFilePointer(lf, 0, NULL, FILE_END);
            app_s(rec,&rp,"DHCP applied: IP="); app_s(rec,&rp,ip);
            app_s(rec,&rp," GW="); app_s(rec,&rp,(char*)(gw?gw:""));
            rec[rp++]='\r'; rec[rp++]='\n'; rec[rp]=0;
            WriteFile(lf, rec, rp, &wr, NULL);
            CloseHandle(lf);
        }
    }
    p=0; app_s(line,&p,"Applied DHCP lease to Services\\"); app_s(line,&p,svc);
    app_s(line,&p,"\\Parameters\\Tcpip\n"); line[p]=0; out(line);
    out("Reboot for the new address to take effect.\n");
    return 0;
}

static int run(const char *macarg, int apply, int idx)
{
    WSADATA wsa;
    SOCKET s;
    struct sockaddr_in local, dest, from;
    int fromlen, n, bcast = 1, tmo = 5000;
    DHCP_MSG msg, rep;
    unsigned long xid, offered = 0, serverid = 0, len;
    unsigned char mac[6];
    const unsigned char *o; unsigned char ol;
    char line[96]; int lp;
    char s_ip[20], s_mask[20], s_gw[20], s_dns[20];
    int tries, adapters, route_index, dont_route = 1;
    DHCP_ROUTE routes[64]; unsigned long destination;

    if (!macarg || !parse_mac(macarg, mac)) {
        out("DHCPNT31 - general DHCP client for Windows NT 3.1\n");
        out("usage: dhcpnt31 <mac1> [<mac2> ...] [/apply]\n");
        out("         acquire a lease per MAC and apply it to each bound\n");
        out("         TCP/IP adapter, in order (one MAC = single adapter)\n");
        out("       dhcpnt31 /auto [/apply]   discover each adapter's MAC from\n");
        out("         the registry (driver-published) - no MAC needed; this is\n");
        out("         what the setup DHCP checkbox runs at logon\n");
        out("       dhcpnt31 show   list every bound adapter's TCP/IP config\n");
        return 1;
    }

    adapters = count_adapters();
    my_memset(routes, 0, sizeof(routes));
    if (adapters > 1 && adapters <= 64) {
        for (route_index = 0; route_index < adapters; route_index++)
            if (!read_network(route_index, &routes[route_index])) {
                out("Cannot identify each adapter's saved IPv4 network; no lease was requested.\n"); return 1;
            }
    }
    if (!select_destination(routes, adapters, idx, &destination)) {
        out("Ambiguous, overlapping or unconfigured adapter networks; no lease was requested.\n"); return 1;
    }
    if (WSAStartup(0x0101, &wsa) != 0) { out("WSAStartup failed\n"); return 1; }
    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) { out("socket failed\n"); WSACleanup(); return 1; }
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, (char*)&bcast, sizeof(bcast)) ||
        setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char*)&tmo, sizeof(tmo)) ||
        (adapters > 1 && setsockopt(s, SOL_SOCKET, SO_DONTROUTE, (char*)&dont_route, sizeof(dont_route)))) {
        out("Required UDP socket options are unavailable; no lease was requested.\n");
        closesocket(s); WSACleanup(); return 1;
    }

    my_memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(DHCP_CLIENT_PORT);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(s, (struct sockaddr*)&local, sizeof(local)) == SOCKET_ERROR) {
        out("bind :68 failed (is another DHCP client running?)\n");
        closesocket(s); WSACleanup(); return 1;
    }

    my_memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(DHCP_SERVER_PORT);
    dest.sin_addr.s_addr = destination;

    xid = (unsigned long)GetTickCount();
    xid ^= ((unsigned long)mac[2] << 24) ^ ((unsigned long)mac[5] << 8);

    /* ---- DISCOVER / OFFER ---- */
    for (tries = 0; tries < 4 && !offered; tries++) {
        len = build_msg(&msg, DHCPDISCOVER, mac, xid, 0, 0);
        sendto(s, (char*)&msg, len, 0, (struct sockaddr*)&dest, sizeof(dest));
        lp=0; app_s(line,&lp,"DHCPDISCOVER sent (try "); app_u(line,&lp,tries+1);
        app_s(line,&lp,")\n"); line[lp]=0; out(line);
        fromlen = sizeof(from);
        n = recvfrom(s, (char*)&rep, sizeof(rep), 0, (struct sockaddr*)&from, &fromlen);
        if (n <= 0) continue;
        if (from.sin_port != htons(DHCP_SERVER_PORT) ||
            !valid_reply(&rep, (unsigned)n, xid, mac)) continue;
        o = find_opt(&rep, (unsigned)n, OPT_MSG_TYPE, &ol);
        if (!o || ol != 1 || *o != DHCPOFFER || !rep.yiaddr) continue;
        if (adapters > 1 && (rep.yiaddr & routes[idx].mask) != (routes[idx].ip & routes[idx].mask)) continue;
        o = find_opt(&rep, (unsigned)n, OPT_SERVER_ID, &ol);
        if (!o || ol != 4) continue;
        my_memcpy(&serverid, o, 4);
        if (!serverid) continue;
        offered = rep.yiaddr;
        print_kv("DHCPOFFER ", offered);
    }
    if (!offered) { out("no offer\n"); closesocket(s); WSACleanup(); return 2; }

    /* ---- REQUEST / ACK ---- */
    for (tries = 0; tries < 4; tries++) {
        len = build_msg(&msg, DHCPREQUEST, mac, xid, offered, serverid);
        sendto(s, (char*)&msg, len, 0, (struct sockaddr*)&dest, sizeof(dest));
        lp=0; app_s(line,&lp,"DHCPREQUEST sent (try "); app_u(line,&lp,tries+1);
        app_s(line,&lp,")\n"); line[lp]=0; out(line);
        fromlen = sizeof(from);
        n = recvfrom(s, (char*)&rep, sizeof(rep), 0, (struct sockaddr*)&from, &fromlen);
        if (n <= 0 || from.sin_port != htons(DHCP_SERVER_PORT) ||
            !valid_reply(&rep, (unsigned)n, xid, mac)) continue;
        o = find_opt(&rep, (unsigned)n, OPT_SERVER_ID, &ol);
        if (!o || ol != 4) continue;
        { unsigned long reply_server; my_memcpy(&reply_server, o, 4);
          if (reply_server != serverid) continue; }
        o = find_opt(&rep, (unsigned)n, OPT_MSG_TYPE, &ol);
        if (!o || ol != 1) continue;
        if (*o == DHCPNAK) { out("DHCPNAK - restarting\n"); offered = 0; break; }
        if (*o != DHCPACK || rep.yiaddr != offered) continue;
        o = find_opt(&rep, (unsigned)n, OPT_SUBNET_MASK, &ol);
        if (!o || ol != 4) continue;

        s_ip[0]=s_mask[0]=s_gw[0]=s_dns[0]=0;
        ipstr(rep.yiaddr, s_ip);
        out("\n=== LEASE ACQUIRED ===\n");
        print_kv("IP Address : ", rep.yiaddr);
        o = find_opt(&rep, (unsigned)n, OPT_SUBNET_MASK, &ol); if (o && ol==4){ ipstr(*(unsigned long*)o,s_mask); print_kv("Subnet Mask: ", *(unsigned long*)o);}
        o = find_opt(&rep, (unsigned)n, OPT_ROUTER, &ol);      if (o && ol>=4){ ipstr(*(unsigned long*)o,s_gw);   print_kv("Gateway    : ", *(unsigned long*)o);}
        o = find_opt(&rep, (unsigned)n, OPT_DNS, &ol);         if (o && ol>=4){ ipstr(*(unsigned long*)o,s_dns);  print_kv("DNS Server : ", *(unsigned long*)o);}
        o = find_opt(&rep, (unsigned)n, OPT_LEASE_TIME, &ol);  if (o && ol==4) {
            lp=0; app_s(line,&lp,"Lease Time : "); app_u(line,&lp,ntohl(*(unsigned long*)o));
            app_s(line,&lp," seconds\n"); line[lp]=0; out(line);
        }
        o = find_opt(&rep, (unsigned)n, OPT_SERVER_ID, &ol);   if (o && ol==4) print_kv("DHCP Server: ", *(unsigned long*)o);
        closesocket(s); WSACleanup();
        if (apply) {
            out("\n");
            return reg_apply_n(idx, s_ip, s_mask, s_gw, s_dns);
        }
        /* interactive: offer to apply the lease to the RLTKLANT adapter */
        {
            char c = ask_char("\nApply this lease to the RLTKLANT adapter? (Y/N): ");
            if (c == 'Y' || c == 'y') { out("\n"); return reg_apply_n(idx, s_ip, s_mask, s_gw, s_dns); }
        }
        out("Not applied.  (Run with /apply to write without prompting.)\n");
        return 0;
    }

    out("no ACK\n");
    closesocket(s); WSACleanup(); return 2;
}

/* ---- freestanding entry point ----
 * usage:  dhcpnt31 <mac> [/apply]     acquire a lease (and optionally write it)
 *         dhcpnt31 show               print the RLTKLANT adapter's TCP/IP config
 */
#define MAXTOK 10
static char tokbuf[MAXTOK][64];
static int tokenize(void)
{
    char *cl = GetCommandLineA();
    int n = 0, i;
    if (*cl == '"') { cl++; while (*cl && *cl != '"') cl++; if (*cl=='"') cl++; }
    else            { while (*cl && *cl != ' ' && *cl != '\t') cl++; }
    while (n < MAXTOK) {
        while (*cl == ' ' || *cl == '\t') cl++;
        if (!*cl) break;
        i = 0;
        while (*cl && *cl != ' ' && *cl != '\t' && i < 63) tokbuf[n][i++] = *cl++;
        tokbuf[n][i] = 0; n++;
    }
    return n;
}
static int streqi(const char *a, const char *b)
{
    while (*a && *b) {
        char ca=*a, cb=*b;
        if (ca>='A'&&ca<='Z') ca+=32; if (cb>='A'&&cb<='Z') cb+=32;
        if (ca!=cb) return 0; a++; b++;
    }
    return *a==*b;
}

void mainCRTStartup(void)
{
    int rc = 0, n = tokenize(), i, apply = 0, autom = 0, nmac = 0, idx;
    char *macs[MAXTOK];
    char line[64]; int lp;

    if (n >= 1 && streqi(tokbuf[0], "show")) { ExitProcess((UINT)reg_show()); }

    /* Collect MAC arguments, /apply, and /auto.  /auto discovers each bound
       adapter's MAC from the registry (published by the driver) and needs no
       MAC on the command line; it is how the installer's DHCP checkbox starts
       the client at logon ("DHCPNT31.EXE /auto /apply").  One MAC per adapter,
       in enumeration order; a single MAC (the common case) configures the
       single/first adapter. */
    for (i = 0; i < n; i++) {
        if (streqi(tokbuf[i], "/apply") || streqi(tokbuf[i], "-apply")) apply = 1;
        else if (streqi(tokbuf[i], "/auto") || streqi(tokbuf[i], "-auto")) autom = 1;
        else if (nmac < MAXTOK) macs[nmac++] = tokbuf[i];
    }

    if (autom) {
        int cnt = count_adapters();
        if (cnt <= 0) { out("DHCPNT31 /auto: no bound TCP/IP adapter found\n"); ExitProcess(1); }
        for (idx = 0; idx < cnt; idx++) {
            unsigned char m[6]; char ms[20];
            int enabled = auto_enabled(idx);
            if (!enabled) continue;
            if (enabled < 0) { out("Cannot read adapter DHCP opt-in; skipping.\n"); rc=1; continue; }
            lp = 0; app_s(line,&lp,"\n===== Adapter #"); app_u(line,&lp,(unsigned)(idx+1));
            app_s(line,&lp," ====="); line[lp++]='\n'; line[lp]=0; out(line);
            if (!adapter_mac_n(idx, m)) {
                out("  MAC not published by the driver (DhcpHwAddress/CurrentAddress missing) - skipping\n");
                rc=1;
                continue;
            }
            mac_to_str(m, ms);
            lp = 0; app_s(line,&lp,"  MAC "); app_s(line,&lp,ms); line[lp++]='\n'; line[lp]=0; out(line);
            if(run(ms,1,idx))rc=1;       /* /auto always applies the lease */
        }
        ExitProcess((UINT)rc);
    }

    if (nmac == 0) { ExitProcess((UINT)run((char*)0, 0, 0)); }   /* prints usage */

    /* one DHCP handshake + apply per MAC, to adapters in enumeration order */
    for (idx = 0; idx < nmac; idx++) {
        if (nmac > 1) {
            lp = 0; app_s(line,&lp,"\n===== Adapter #"); app_u(line,&lp,(unsigned)(idx+1));
            app_s(line,&lp," ====="); line[lp++]='\n'; line[lp]=0; out(line);
        }
        if(run(macs[idx],apply,idx))rc=1;
    }
    ExitProcess((UINT)rc);
}
