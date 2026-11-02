
/workspace/rltklant-vm/install/nt31-exports/ndis.sys:     file format pei-i386


Disassembly of section .text:

000136f0 <.text+0x32f0>:
   136f0:	56                   	push   %esi
   136f1:	57                   	push   %edi
   136f2:	8b 74 24 10          	mov    0x10(%esp),%esi
   136f6:	0b f6                	or     %esi,%esi
   136f8:	75 16                	jne    0x13710
   136fa:	8b 7c 24 0c          	mov    0xc(%esp),%edi
   136fe:	57                   	push   %edi
   136ff:	e8 0c 02 00 00       	call   0x13910
   13704:	0a c0                	or     %al,%al
   13706:	75 0c                	jne    0x13714
   13708:	be 02 00 01 c0       	mov    $0xc0010002,%esi
   1370d:	eb 05                	jmp    0x13714
   1370f:	90                   	nop
   13710:	8b 7c 24 0c          	mov    0xc(%esp),%edi
   13714:	8b 44 24 14          	mov    0x14(%esp),%eax
   13718:	50                   	push   %eax
   13719:	56                   	push   %esi
   1371a:	8b 47 10             	mov    0x10(%edi),%eax
   1371d:	50                   	push   %eax
   1371e:	8b 47 0c             	mov    0xc(%edi),%eax
   13721:	ff 50 1c             	call   *0x1c(%eax)
   13724:	5f                   	pop    %edi
   13725:	5e                   	pop    %esi
   13726:	c2 0c 00             	ret    $0xc
   13729:	8d a4 24 00 00 00 00 	lea    0x0(%esp),%esp
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
