"""Close the exact v3 installed regression, preserving earlier unresolved findings."""
from pathlib import Path
import datetime, hashlib, json, re

v=Path(__file__).resolve().parent
snapshot='usbnt-controller-sleep-v3-20261102'
def read(name): return json.loads((v/name).read_text())
def sha(name): return hashlib.sha256((v/name).read_bytes()).hexdigest()
usb=read('frozen-builds/'+snapshot+'/manifest.json')['files']
specs=[
 ('nt31','legacy','nt31-usbsetup-sleep-v3-rtl8117-8127','ndisnt31'),
 ('nt4','legacy','nt4-ndis-dual-sleep-v3-8127','ndis3'),
 ('nt351','legacy','nt351-usbsetup-sleep-v3-rtl8139-8127','ndis3'),
 ('win2000','i386','win2000-standard-usbnet-mixed-sleep-v3-8127','ndis5'),
 ('xp64','amd64','xp64-usbnet-mixed-sleep-v3-8127','ndis5x64'),
]
rows=[]
for os,arch,guest,rlt in specs:
    prefix=os+'-sleep-v3-'
    offline=prefix+'offline-20261102'
    evidence=prefix+'evidence-20261102'
    names=[offline+'/manifest.json',evidence+'/manifest.json']
    files={r['file']:r for r in read(names[0])['files'].values()}
    assert read(names[0])['guest']==read(names[1])['guest']==guest
    for r in files.values(): assert sha(offline+'/'+r['file'])==r['sha256']
    assert files['USBNT.SYS']['sha256']==usb[arch]['sha256']
    assert files['RLTKLANT.SYS']['sha256']==sha('frozen-builds/rlt-init-diagnostics-v3-20261102/'+rlt+'.sys')
    assert read(evidence+'/process-exit.json')['exit_code']==0
    debug=(v/evidence/'debug.log').read_text(errors='replace')
    assert 'INIT FAIL' not in debug
    row=dict(os=os,guest=guest,result='PASS_BOUNDED_QEMU_REGRESSION',drivers=files)
    if os in ('nt31','nt4','nt351'):
        name=guest+'-sleep-v3-20261102-sibling-matrix.json';names.append(name)
        matrix=read(name); expected={'nt31':104,'nt4':332,'nt351':68}[os]
        assert matrix['result']=='PASS' and matrix['full_payload_replies']==expected
        assert len(matrix['rows'])==(8 if os=='nt4' else 4)
        for item in matrix['rows']:
            assert item['result']=='PASS'
            for report in item['reports']: assert sha(report['file'])==report['sha256']
        row.update(usb_hotplug_cycles=len(matrix['rows']),carrier_cycles=len(matrix['rows']),full_payload_replies=expected)
        if os=='nt351':
            name='nt351-sleep-v3-20261102-registered-reset.json';names.append(name)
            assert read(name)['result']=='PASS' and read(name)['trace'].count('N35TEST reset-complete=00000000')==3
            name='nt351-sleep-v3-tcp-wire-proof-20261102.json';names.append(name)
            assert read(name)['result']=='PASS' and len(read(name)['captures'][0]['connections'])==3
            assert 'RTL8139 RX watchdog' not in debug
            assert 'PASS TCP TX 1048576 RX 1048576 ALL BYTES MATCH' in (v/offline/'TCP35.LOG').read_text()
            for phase in ('baseline','reset','cable'):
                name='nt351-sleep-v3-20261102-tcp-'+phase; names += [name+'.json',name+'-screen.png']
                data=read(name+'.json'); assert data['result']=='PASS_HOST_PAYLOAD' and len(data['rounds'])==16
                assert sum(r['rx_bytes'] for r in data['rounds'])==1048576
                assert sum(r['tx_bytes'] for r in data['rounds'])==1048576
            row.update(tcp_bytes_each_direction=3145728,registered_mac_resets=3,rx_watchdog_events=0,
                       result='PASS_WITH_PRIOR_ATTACHMENT_LATENCY_FINDING_OPEN',
                       limits=['PCI-only TCP/IP binding; no USB TCP/IP claimed for this NT3.51 fixture.',
                               'Two older RTL8159 30-second attachment failures remain unexplained; this pass does not prove a timing fix.'])
    else:
        for helper in ('HCPOWER.EXE','HCPOWER.SYS'):
            assert files[helper]['sha256']==sha('usbnt-hc-power-single-'+arch+'-20261102/'+helper)
        for helper in ('SLPTEST.EXE','SLPTEST.SYS'):
            assert files[helper]['sha256']==sha('usbnt-sleep-probe-'+arch+'-20261102/'+helper)
        assert files['MKEY2.EXE']['sha256']==sha('ftp-fixture/MKEY2.EXE')
        if os=='xp64':
            assert files['MRESET64.EXE']['sha256']==sha('ftp-fixture/MRESET64.EXE')
        name=guest+'-sleep-v3-20261102-retirement-matrix.json';names.append(name)
        matrix=read(name);assert matrix['result']=='PASS' and len(matrix['models'])==4
        assert matrix['full_mtu_replies']==64 and matrix['function_power_pairs']==12
        for item in matrix['models']:
            assert item['result']=='PASS'
            for key in ('stalled_removal','function_power','port_reset','carrier'):
                assert sha(item[key]['file'])==item[key]['sha256']
        name=os+'-mixed-sleep-v3-cycles-20261102.json';names.append(name)
        power=read(name);assert power['result']=='PASS_LIVE_PENDING_STOPPED_PDO_READBACK' and len(power['rows'])==6
        tokens=[]
        for item in power['rows']:
            assert 'RLTK RtlInitialize' not in item['trace'] and 'INIT FAIL' not in item['trace']
            for key in ('network','keyboard'):
                rec=item[key];assert sha(rec['file'])==rec['sha256'];names.append(rec['file'])
            network=read(item['network']['file'])['report']
            assert 'PROBLEM 0 ' in network and 'ICMP_REPLIES 4 PAYLOAD 1472' in network and network.endswith('PASS\r\n')
            key=read(item['keyboard']['file']);assert key['result']=='PASS_GUEST_CONSOLE_EVENTS';tokens.append(key['token'])
        sleep=(v/offline/'SLPDONE.TXT').read_text()
        sleeps=re.findall(r'sleep: device (\d+) gen (\d+) PDO ([a-fA-F0-9]+) retained',sleep)
        wakes=re.findall(r'resume: device (\d+) gen (\d+) PDO ([a-fA-F0-9]+) restored',sleep)
        assert len(sleeps)==7 and len(wakes)==6 and len(set(sleeps+wakes))==1
        assert sleep.count('device 0: removed')==1
        assert sleep.index('device 0: removed')>sleep.rindex('retained')
        assert sleep.count('PnP: device 0 (0bda:8153) function')==2
        assert 'device 0: keyboard' in sleep and 'interfaces 0..0' in sleep
        for operation in ('query','unplug-down','unplug-up'):
            name=os+'-mixed-sleep-v3-20261102-'+operation+'.json';names.append(name);assert read(name)['result']=='PASS'
        name=os+'-mixed-sleep-v3-20261102-unplug-host.json';names.append(name)
        assert read(name)['result']=='HOST_DEVICE_REMOVED_WHILE_CONTROLLER_D3'
        for phase in ('baseline','after-unplug'):
            name=guest+'-sleep-v3-'+phase+'-20261102.json';names.append(name)
            report=read(name)['report'];assert 'ICMP_REPLIES 4 PAYLOAD 1472' in report and report.endswith('PASS\r\n')
        name=os+'-mixed-sleep-v3-after-unplug-key-20261102.json';names.append(name)
        key=read(name);assert key['result']=='PASS_GUEST_CONSOLE_EVENTS';tokens.append(key['token'])
        name=os+'-mixed-reset-v3-20261102.json';names.append(name)
        resets=read(name);assert resets['result']=='PASS_LIVE_PENDING_STOPPED_READBACK' and len(resets['rows'])==3
        for item in resets['rows']:
            assert item['physical_resets']>=1
            for k in ('network','keyboard'): assert sha(item[k]['file'])==item[k]['sha256'];names.append(item[k]['file'])
            cycles=read(item['network']['file'])['cycles'];assert len(cycles)==1
            assert 'RESETTEST ioctl=00000000' in cycles[0]['reset_trace']
            key=read(item['keyboard']['file']);assert key['result']=='PASS_GUEST_CONSOLE_EVENTS';tokens.append(key['token'])
        keys=(v/offline/'KEY.LOG').read_text()
        for token in tokens:
            blocks=[b for b in keys.split('RUN ') if b and b.splitlines()[0].endswith(' '+token)]
            assert len(blocks)==1 and 'FAIL' not in blocks[0]
            assert all(s in blocks[0] for s in ('KEY B DOWN SCAN 30','KEY B UP SCAN 30','PASS WINDOWS COMPOSITE HID INPUT'))
        row.update(controller_device_power_pairs=3,controller_system_irp_pairs=3,unplug_during_d3=1,
                   retained_pdo=sleeps[0],mixed_physical_resets=3,windows_key_checks=10,
                   model_function_power_pairs=12,model_stalled_removals=4,model_port_resets=4,
                   model_carrier_cycles=4,full_payload_network_replies=108,
                   limits=['Fictional mixed NIC/HID fixture is distinct from ordinary Realtek profiles.',
                           'Injected system-power IRPs do not establish actual platform ACPI S3.'])
        if os=='xp64':
            for phase in ('query','down'):
                name='xp64-sleep-v3-hub-'+phase+'-status-20261102.json';names.append(name)
                data=read(name);assert data['result']=='PASS' and data['expected']=='80000011'
            name='xp64-sleep-v3-after-hub-query-20261102.json';names.append(name)
            assert read(name)['result']=='PASS' and read(name)['expected']=='00000000'
            name=guest+'-sleep-v3-hub-rejection-20261102.json';names.append(name)
            assert 'ICMP_REPLIES 4 PAYLOAD 1472' in read(name)['report']
            row.update(unsupported_hub_preflight_checks=2,full_payload_network_replies=112)
    row['evidence']={n:sha(n) for n in names}
    rows.append(row)

out=v/'usbnt-controller-sleep-v3-five-os-closed-proof-20261102.json'
assert not out.exists()
out.write_text(json.dumps(dict(utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),
    result='PASS_BOUNDED_FIVE_OS_REGRESSION_WITH_LIMITS',snapshot=snapshot,rows=rows,
    scope='Exact installed candidate after normal shutdown and stopped-disk readback. Windows 2000 AHCI; all other guests IDE.',
    limits=['USBNT 1.3 remains unreleased; product version is 1.2.',
            'Only QEMU is available; physical silicon, analog PHY and real MCU execution are unverified.',
            'Hub, unknown owner, retained generic-client state and active EHCI companion sleep are rejected before mutation.',
            'The prior NT3.51 attachment-latency finding remains open.']),indent=2)+'\n')
print(out.name+': PASS bounded exact-build regression for five OS fixtures')
