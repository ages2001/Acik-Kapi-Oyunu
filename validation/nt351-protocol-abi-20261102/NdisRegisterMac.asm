
/workspace/rltklant-vm/install/nt351-standard/installed-ndis.sys:     file format pei-i386


Disassembly of section .text:

000143f3 <.text+0x4033>:
   143f3:	53                   	push   ebx
   143f4:	56                   	push   esi
   143f5:	8b 5c 24 14          	mov    ebx,DWORD PTR [esp+0x14]
   143f9:	57                   	push   edi
   143fa:	55                   	push   ebp
   143fb:	ff 33                	push   DWORD PTR [ebx]
   143fd:	e8 11 df ff ff       	call   0x12313
   14402:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   14406:	85 c0                	test   eax,eax
   14408:	89 01                	mov    DWORD PTR [ecx],eax
   1440a:	7c 14                	jl     0x14420
   1440c:	8b 44 24 18          	mov    eax,DWORD PTR [esp+0x18]
   14410:	85 db                	test   ebx,ebx
   14412:	c7 00 00 00 00 00    	mov    DWORD PTR [eax],0x0
   14418:	75 0d                	jne    0x14427
   1441a:	c7 01 01 00 00 c0    	mov    DWORD PTR [ecx],0xc0000001
   14420:	5d                   	pop    ebp
   14421:	5f                   	pop    edi
   14422:	5e                   	pop    esi
   14423:	5b                   	pop    ebx
   14424:	c2 18 00             	ret    0x18
   14427:	8b 44 24 24          	mov    eax,DWORD PTR [esp+0x24]
   1442b:	80 38 03             	cmp    BYTE PTR [eax],0x3
   1442e:	0f 85 62 01 00 00    	jne    0x14596
   14434:	80 78 01 00          	cmp    BYTE PTR [eax+0x1],0x0
   14438:	0f 85 58 01 00 00    	jne    0x14596
   1443e:	83 7c 24 28 38       	cmp    DWORD PTR [esp+0x28],0x38
   14443:	73 0c                	jae    0x14451
   14445:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   14449:	c7 01 05 00 01 c0    	mov    DWORD PTR [ecx],0xc0010005
   1444f:	eb cf                	jmp    0x14420
   14451:	8b 44 24 24          	mov    eax,DWORD PTR [esp+0x24]
   14455:	68 4e 44 6d 62       	push   0x626d444e
   1445a:	0f b7 70 30          	movzx  esi,WORD PTR [eax+0x30]
   1445e:	83 c6 70             	add    esi,0x70
   14461:	56                   	push   esi
   14462:	6a 00                	push   0x0
   14464:	ff 15 94 55 01 00    	call   DWORD PTR ds:0x15594
   1446a:	8b e8                	mov    ebp,eax
   1446c:	85 ed                	test   ebp,ebp
   1446e:	75 0c                	jne    0x1447c
   14470:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   14474:	c7 01 9a 00 00 c0    	mov    DWORD PTR [ecx],0xc000009a
   1447a:	eb a4                	jmp    0x14420
   1447c:	8b fd                	mov    edi,ebp
   1447e:	33 c0                	xor    eax,eax
   14480:	b9 1c 00 00 00       	mov    ecx,0x1c
   14485:	f3 ab                	rep stos DWORD PTR es:[edi],eax
   14487:	8d 7d 18             	lea    edi,[ebp+0x18]
   1448a:	b9 0e 00 00 00       	mov    ecx,0xe
   1448f:	89 75 14             	mov    DWORD PTR [ebp+0x14],esi
   14492:	8b 74 24 24          	mov    esi,DWORD PTR [esp+0x24]
   14496:	f3 a5                	rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]
   14498:	8d 7d 70             	lea    edi,[ebp+0x70]
   1449b:	8b 44 24 24          	mov    eax,DWORD PTR [esp+0x24]
   1449f:	89 7d 4c             	mov    DWORD PTR [ebp+0x4c],edi
   144a2:	0f b7 50 30          	movzx  edx,WORD PTR [eax+0x30]
   144a6:	8b 70 34             	mov    esi,DWORD PTR [eax+0x34]
   144a9:	8b ca                	mov    ecx,edx
   144ab:	c1 e9 02             	shr    ecx,0x2
   144ae:	f3 a5                	rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]
   144b0:	8b ca                	mov    ecx,edx
   144b2:	83 e1 03             	and    ecx,0x3
   144b5:	f3 a4                	rep movs BYTE PTR es:[edi],BYTE PTR ds:[esi]
   144b7:	8b 4c 24 20          	mov    ecx,DWORD PTR [esp+0x20]
   144bb:	c7 45 00 00 00 00 00 	mov    DWORD PTR [ebp+0x0],0x0
   144c2:	89 4d 04             	mov    DWORD PTR [ebp+0x4],ecx
   144c5:	8b 13                	mov    edx,DWORD PTR [ebx]
   144c7:	c7 42 34 a7 f4 01 00 	mov    DWORD PTR [edx+0x34],0x1f4a7
   144ce:	8b 13                	mov    edx,DWORD PTR [ebx]
   144d0:	c7 42 78 35 f5 01 00 	mov    DWORD PTR [edx+0x78],0x1f535
   144d7:	8b 13                	mov    edx,DWORD PTR [ebx]
   144d9:	c7 42 38 8f 2e 01 00 	mov    DWORD PTR [edx+0x38],0x12e8f
   144e0:	8b 13                	mov    edx,DWORD PTR [ebx]
   144e2:	b9 98 57 01 00       	mov    ecx,0x15798
   144e7:	c7 42 70 7a 48 01 00 	mov    DWORD PTR [edx+0x70],0x1487a
   144ee:	8b 13                	mov    edx,DWORD PTR [ebx]
   144f0:	c7 82 80 00 00 00 9f 	mov    DWORD PTR [edx+0x80],0x12f9f
   144f7:	2f 01 00 
   144fa:	8b 13                	mov    edx,DWORD PTR [ebx]
   144fc:	c7 42 40 f8 f9 01 00 	mov    DWORD PTR [edx+0x40],0x1f9f8
   14503:	89 5d 50             	mov    DWORD PTR [ebp+0x50],ebx
   14506:	ff 15 74 56 01 00    	call   DWORD PTR ds:0x15674
   1450c:	8b 0d 90 57 01 00    	mov    ecx,DWORD PTR ds:0x15790
   14512:	a2 9c 57 01 00       	mov    ds:0x1579c,al
   14517:	89 4d 54             	mov    DWORD PTR [ebp+0x54],ecx
   1451a:	b9 98 57 01 00       	mov    ecx,0x15798
   1451f:	a0 9c 57 01 00       	mov    al,ds:0x1579c
   14524:	89 2d 90 57 01 00    	mov    DWORD PTR ds:0x15790,ebp
   1452a:	8b d0                	mov    edx,eax
   1452c:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   14532:	6a 00                	push   0x0
   14534:	8d 4d 58             	lea    ecx,[ebp+0x58]
   14537:	6a 00                	push   0x0
   14539:	51                   	push   ecx
   1453a:	ff 15 68 55 01 00    	call   DWORD PTR ds:0x15568
   14540:	c6 45 68 00          	mov    BYTE PTR [ebp+0x68],0x0
   14544:	8d 4d 08             	lea    ecx,[ebp+0x8]
   14547:	51                   	push   ecx
   14548:	e8 df e8 ff ff       	call   0x12e2c
   1454d:	8b 4c 24 18          	mov    ecx,DWORD PTR [esp+0x18]
   14551:	89 29                	mov    DWORD PTR [ecx],ebp
   14553:	e8 14 d5 ff ff       	call   0x11a6c
   14558:	83 7b 04 00          	cmp    DWORD PTR [ebx+0x4],0x0
   1455c:	74 2c                	je     0x1458a
   1455e:	55                   	push   ebp
   1455f:	e8 a2 50 00 00       	call   0x19606
   14564:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   14568:	85 c0                	test   eax,eax
   1456a:	75 10                	jne    0x1457c
   1456c:	c7 01 00 00 00 00    	mov    DWORD PTR [ecx],0x0
   14572:	e8 79 d3 ff ff       	call   0x118f0
   14577:	e9 a4 fe ff ff       	jmp    0x14420
   1457c:	55                   	push   ebp
   1457d:	c7 01 01 00 00 c0    	mov    DWORD PTR [ecx],0xc0000001
   14583:	e8 73 b7 00 00       	call   0x1fcfb
   14588:	eb e8                	jmp    0x14572
   1458a:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   1458e:	c7 01 01 00 00 c0    	mov    DWORD PTR [ecx],0xc0000001
   14594:	eb dc                	jmp    0x14572
   14596:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   1459a:	c7 01 04 00 01 c0    	mov    DWORD PTR [ecx],0xc0010004
   145a0:	e9 7b fe ff ff       	jmp    0x14420
   145a5:	8b 44 24 04          	mov    eax,DWORD PTR [esp+0x4]
   145a9:	c7 00 00 00 00 00    	mov    DWORD PTR [eax],0x0
   145af:	8b 44 24 08          	mov    eax,DWORD PTR [esp+0x8]
   145b3:	85 c0                	test   eax,eax
   145b5:	74 09                	je     0x145c0
   145b7:	83 c0 08             	add    eax,0x8
   145ba:	50                   	push   eax
   145bb:	e8 e6 01 00 00       	call   0x147a6
   145c0:	c2 08 00             	ret    0x8
   145c3:	56                   	push   esi
   145c4:	8b 74 24 08          	mov    esi,DWORD PTR [esp+0x8]
   145c8:	56                   	push   esi
   145c9:	e8 34 b6 00 00       	call   0x1fc02
   145ce:	56                   	push   esi
   145cf:	e8 67 b6 00 00       	call   0x1fc3b
   145d4:	e8 76 d2 ff ff       	call   0x1184f
   145d9:	33 c0                	xor    eax,eax
   145db:	5e                   	pop    esi
   145dc:	c2 04 00             	ret    0x4
   145df:	8b 44 24 04          	mov    eax,DWORD PTR [esp+0x4]
   145e3:	56                   	push   esi
   145e4:	8b 70 34             	mov    esi,DWORD PTR [eax+0x34]
   145e7:	83 7e 04 00          	cmp    DWORD PTR [esi+0x4],0x0
   145eb:	74 19                	je     0x14606
   145ed:	c7 46 04 00 00 00 00 	mov    DWORD PTR [esi+0x4],0x0
   145f4:	ff 30                	push   DWORD PTR [eax]
   145f6:	ff 15 a8 55 01 00    	call   DWORD PTR ds:0x155a8
   145fc:	83 c6 08             	add    esi,0x8
   145ff:	56                   	push   esi
