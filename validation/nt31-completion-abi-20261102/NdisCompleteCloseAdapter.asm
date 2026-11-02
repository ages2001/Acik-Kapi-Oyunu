
/workspace/rltklant-vm/install/nt31-exports/ndis.sys:     file format pei-i386


Disassembly of section .text:

00013730 <.text+0x3330>:
   13730:	56                   	push   %esi
   13731:	8b 74 24 08          	mov    0x8(%esp),%esi
   13735:	8b 44 24 0c          	mov    0xc(%esp),%eax
   13739:	50                   	push   %eax
   1373a:	8b 46 10             	mov    0x10(%esi),%eax
   1373d:	50                   	push   %eax
   1373e:	8b 46 0c             	mov    0xc(%esi),%eax
   13741:	ff 50 20             	call   *0x20(%eax)
   13744:	8b 46 08             	mov    0x8(%esi),%eax
   13747:	50                   	push   %eax
   13748:	56                   	push   %esi
   13749:	e8 52 0b 00 00       	call   0x142a0
   1374e:	8b 46 0c             	mov    0xc(%esi),%eax
   13751:	50                   	push   %eax
   13752:	56                   	push   %esi
   13753:	e8 68 01 00 00       	call   0x138c0
   13758:	8b 46 0c             	mov    0xc(%esi),%eax
   1375b:	83 c0 04             	add    $0x4,%eax
   1375e:	50                   	push   %eax
   1375f:	e8 6c 00 00 00       	call   0x137d0
   13764:	0a c0                	or     %al,%al
   13766:	74 09                	je     0x13771
   13768:	8b 46 0c             	mov    0xc(%esi),%eax
   1376b:	50                   	push   %eax
   1376c:	e8 df 4d 00 00       	call   0x18550
   13771:	8b 46 08             	mov    0x8(%esi),%eax
   13774:	50                   	push   %eax
   13775:	e8 16 0a 00 00       	call   0x14190
   1377a:	8b 46 1c             	mov    0x1c(%esi),%eax
   1377d:	50                   	push   %eax
   1377e:	e8 2d 51 00 00       	call   0x188b0
   13783:	56                   	push   %esi
   13784:	e8 c7 4d 00 00       	call   0x18550
   13789:	5e                   	pop    %esi
   1378a:	c2 08 00             	ret    $0x8
   1378d:	8d 49 00             	lea    0x0(%ecx),%ecx
   13790:	56                   	push   %esi
   13791:	57                   	push   %edi
   13792:	8b 7c 24 0c          	mov    0xc(%esp),%edi
   13796:	8d 77 04             	lea    0x4(%edi),%esi
   13799:	56                   	push   %esi
   1379a:	57                   	push   %edi
   1379b:	e8 80 4f 00 00       	call   0x18720
   137a0:	80 7f 0a 00          	cmpb   $0x0,0xa(%edi)
   137a4:	74 10                	je     0x137b6
   137a6:	8a 06                	mov    (%esi),%al
   137a8:	50                   	push   %eax
   137a9:	57                   	push   %edi
   137aa:	e8 f1 4f 00 00       	call   0x187a0
   137af:	2a c0                	sub    %al,%al
   137b1:	5f                   	pop    %edi
   137b2:	5e                   	pop    %esi
   137b3:	c2 04 00             	ret    $0x4
   137b6:	66 ff 47 08          	incw   0x8(%edi)
   137ba:	8a 06                	mov    (%esi),%al
   137bc:	50                   	push   %eax
   137bd:	57                   	push   %edi
   137be:	e8 dd 4f 00 00       	call   0x187a0
   137c3:	b0 01                	mov    $0x1,%al
   137c5:	5f                   	pop    %edi
   137c6:	5e                   	pop    %esi
   137c7:	c2 04 00             	ret    $0x4
   137ca:	8d 9b 00 00 00 00    	lea    0x0(%ebx),%ebx
   137d0:	56                   	push   %esi
   137d1:	57                   	push   %edi
   137d2:	8b 74 24 0c          	mov    0xc(%esp),%esi
   137d6:	8d 7e 04             	lea    0x4(%esi),%edi
   137d9:	57                   	push   %edi
