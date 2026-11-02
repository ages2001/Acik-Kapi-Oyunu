
/workspace/rltklant-vm/install/nt31-exports/ndis.sys:     file format pei-i386


Disassembly of section .text:

00012280 <.text+0x1e80>:
   12280:	53                   	push   %ebx
   12281:	56                   	push   %esi
   12282:	57                   	push   %edi
   12283:	55                   	push   %ebp
   12284:	e8 87 e1 ff ff       	call   0x10410
   12289:	8b 7c 24 1c          	mov    0x1c(%esp),%edi
   1228d:	80 3f 03             	cmpb   $0x3,(%edi)
   12290:	75 79                	jne    0x1230b
   12292:	80 7f 01 00          	cmpb   $0x0,0x1(%edi)
   12296:	75 73                	jne    0x1230b
   12298:	8b 5c 24 20          	mov    0x20(%esp),%ebx
   1229c:	83 fb 38             	cmp    $0x38,%ebx
   1229f:	73 11                	jae    0x122b2
   122a1:	8b 44 24 14          	mov    0x14(%esp),%eax
   122a5:	5d                   	pop    %ebp
   122a6:	c7 00 05 00 01 c0    	movl   $0xc0010005,(%eax)
   122ac:	5f                   	pop    %edi
   122ad:	5e                   	pop    %esi
   122ae:	5b                   	pop    %ebx
   122af:	c2 10 00             	ret    $0x10
   122b2:	8d 6b 14             	lea    0x14(%ebx),%ebp
   122b5:	55                   	push   %ebp
   122b6:	6a 00                	push   $0x0
   122b8:	e8 83 62 00 00       	call   0x18540
   122bd:	8b f0                	mov    %eax,%esi
   122bf:	0b f6                	or     %esi,%esi
   122c1:	75 11                	jne    0x122d4
   122c3:	8b 44 24 14          	mov    0x14(%esp),%eax
   122c7:	5d                   	pop    %ebp
   122c8:	c7 00 9a 00 00 c0    	movl   $0xc000009a,(%eax)
   122ce:	5f                   	pop    %edi
   122cf:	5e                   	pop    %esi
   122d0:	5b                   	pop    %ebx
   122d1:	c2 10 00             	ret    $0x10
   122d4:	89 6e 10             	mov    %ebp,0x10(%esi)
   122d7:	53                   	push   %ebx
   122d8:	57                   	push   %edi
   122d9:	8d 46 14             	lea    0x14(%esi),%eax
   122dc:	50                   	push   %eax
   122dd:	e8 e6 66 00 00       	call   0x189c8
   122e2:	83 c4 0c             	add    $0xc,%esp
   122e5:	c7 06 00 00 00 00    	movl   $0x0,(%esi)
   122eb:	8d 46 04             	lea    0x4(%esi),%eax
   122ee:	50                   	push   %eax
   122ef:	e8 1c 15 00 00       	call   0x13810
   122f4:	8b 44 24 18          	mov    0x18(%esp),%eax
   122f8:	89 30                	mov    %esi,(%eax)
   122fa:	8b 44 24 14          	mov    0x14(%esp),%eax
   122fe:	5d                   	pop    %ebp
   122ff:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
   12305:	5f                   	pop    %edi
   12306:	5e                   	pop    %esi
   12307:	5b                   	pop    %ebx
   12308:	c2 10 00             	ret    $0x10
   1230b:	8b 44 24 14          	mov    0x14(%esp),%eax
   1230f:	5d                   	pop    %ebp
   12310:	c7 00 04 00 01 c0    	movl   $0xc0010004,(%eax)
   12316:	5f                   	pop    %edi
   12317:	5e                   	pop    %esi
   12318:	5b                   	pop    %ebx
   12319:	c2 10 00             	ret    $0x10
   1231c:	8d 64 24 00          	lea    0x0(%esp),%esp
   12320:	56                   	push   %esi
   12321:	57                   	push   %edi
   12322:	8b 7c 24 10          	mov    0x10(%esp),%edi
   12326:	8d 77 04             	lea    0x4(%edi),%esi
   12329:	56                   	push   %esi
