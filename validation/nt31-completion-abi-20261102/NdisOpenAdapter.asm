
/workspace/rltklant-vm/install/nt31-exports/ndis.sys:     file format pei-i386


Disassembly of section .text:

00012370 <.text+0x1f70>:
   12370:	83 ec 28             	sub    $0x28,%esp
   12373:	53                   	push   %ebx
   12374:	56                   	push   %esi
   12375:	57                   	push   %edi
   12376:	55                   	push   %ebp
   12377:	6a 48                	push   $0x48
   12379:	6a 00                	push   $0x0
   1237b:	e8 c0 61 00 00       	call   0x18540
   12380:	8b f0                	mov    %eax,%esi
   12382:	0b f6                	or     %esi,%esi
   12384:	75 14                	jne    0x1239a
   12386:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   1238a:	5d                   	pop    %ebp
   1238b:	c7 00 9a 00 00 c0    	movl   $0xc000009a,(%eax)
   12391:	5f                   	pop    %edi
   12392:	5e                   	pop    %esi
   12393:	5b                   	pop    %ebx
   12394:	83 c4 28             	add    $0x28,%esp
   12397:	c2 2c 00             	ret    $0x2c
   1239a:	6a 18                	push   $0x18
   1239c:	6a 00                	push   $0x0
   1239e:	e8 9d 61 00 00       	call   0x18540
   123a3:	8b f8                	mov    %eax,%edi
   123a5:	0b ff                	or     %edi,%edi
   123a7:	75 1a                	jne    0x123c3
   123a9:	56                   	push   %esi
   123aa:	e8 a1 61 00 00       	call   0x18550
   123af:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   123b3:	5d                   	pop    %ebp
   123b4:	c7 00 9a 00 00 c0    	movl   $0xc000009a,(%eax)
   123ba:	5f                   	pop    %edi
   123bb:	5e                   	pop    %esi
   123bc:	5b                   	pop    %ebx
   123bd:	83 c4 28             	add    $0x28,%esp
   123c0:	c2 2c 00             	ret    $0x2c
   123c3:	c7 07 00 00 00 00    	movl   $0x0,(%edi)
   123c9:	c6 47 04 00          	movb   $0x0,0x4(%edi)
   123cd:	c6 47 05 04          	movb   $0x4,0x5(%edi)
   123d1:	66 c7 47 06 08 00    	movw   $0x8,0x6(%edi)
   123d7:	8d 47 08             	lea    0x8(%edi),%eax
   123da:	6a 04                	push   $0x4
   123dc:	68 28 9e 01 00       	push   $0x19e28
   123e1:	50                   	push   %eax
   123e2:	e8 e1 65 00 00       	call   0x189c8
   123e7:	83 c4 0c             	add    $0xc,%esp
   123ea:	6a 08                	push   $0x8
   123ec:	68 30 9e 01 00       	push   $0x19e30
   123f1:	33 c0                	xor    %eax,%eax
   123f3:	8a 47 05             	mov    0x5(%edi),%al
   123f6:	03 c7                	add    %edi,%eax
   123f8:	83 c0 09             	add    $0x9,%eax
   123fb:	50                   	push   %eax
   123fc:	e8 c7 65 00 00       	call   0x189c8
   12401:	c7 44 24 2c 18 00 00 	movl   $0x18,0x2c(%esp)
   12408:	00 
   12409:	c7 44 24 30 00 00 00 	movl   $0x0,0x30(%esp)
   12410:	00 
   12411:	c7 44 24 38 40 00 00 	movl   $0x40,0x38(%esp)
   12418:	00 
   12419:	8b 44 24 68          	mov    0x68(%esp),%eax
   1241d:	89 44 24 34          	mov    %eax,0x34(%esp)
   12421:	c7 44 24 3c 00 00 00 	movl   $0x0,0x3c(%esp)
   12428:	00 
   12429:	c7 44 24 40 00 00 00 	movl   $0x0,0x40(%esp)
   12430:	00 
   12431:	83 c4 0c             	add    $0xc,%esp
   12434:	6a 18                	push   $0x18
   12436:	57                   	push   %edi
   12437:	6a 00                	push   $0x0
   12439:	6a 01                	push   $0x1
   1243b:	6a 03                	push   $0x3
   1243d:	6a 00                	push   $0x0
   1243f:	8d 44 24 30          	lea    0x30(%esp),%eax
   12443:	6a 00                	push   $0x0
   12445:	50                   	push   %eax
   12446:	8d 44 24 40          	lea    0x40(%esp),%eax
   1244a:	50                   	push   %eax
   1244b:	8d 44 24 38          	lea    0x38(%esp),%eax
   1244f:	6a 03                	push   $0x3
   12451:	50                   	push   %eax
   12452:	e8 39 65 00 00       	call   0x18990
   12457:	8b d8                	mov    %eax,%ebx
   12459:	57                   	push   %edi
   1245a:	e8 f1 60 00 00       	call   0x18550
   1245f:	0b db                	or     %ebx,%ebx
   12461:	74 1d                	je     0x12480
   12463:	56                   	push   %esi
   12464:	e8 e7 60 00 00       	call   0x18550
   12469:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   1246d:	5d                   	pop    %ebp
   1246e:	c7 00 06 00 01 c0    	movl   $0xc0010006,(%eax)
   12474:	5f                   	pop    %edi
   12475:	5e                   	pop    %esi
   12476:	5b                   	pop    %ebx
   12477:	83 c4 28             	add    $0x28,%esp
   1247a:	c2 2c 00             	ret    $0x2c
   1247d:	8d 49 00             	lea    0x0(%ecx),%ecx
   12480:	8d 44 24 10          	lea    0x10(%esp),%eax
   12484:	6a 00                	push   $0x0
   12486:	50                   	push   %eax
   12487:	6a 00                	push   $0x0
   12489:	6a 00                	push   $0x0
   1248b:	8b 44 24 24          	mov    0x24(%esp),%eax
   1248f:	6a 00                	push   $0x0
   12491:	50                   	push   %eax
   12492:	e8 29 64 00 00       	call   0x188c0
   12497:	8b 44 24 14          	mov    0x14(%esp),%eax
   1249b:	50                   	push   %eax
   1249c:	e8 df 64 00 00       	call   0x18980
   124a1:	8b 44 24 10          	mov    0x10(%esp),%eax
   124a5:	50                   	push   %eax
   124a6:	e8 15 62 00 00       	call   0x186c0
   124ab:	8b 78 28             	mov    0x28(%eax),%edi
   124ae:	8d 47 1c             	lea    0x1c(%edi),%eax
   124b1:	50                   	push   %eax
   124b2:	e8 d9 12 00 00       	call   0x13790
   124b7:	0a c0                	or     %al,%al
   124b9:	75 25                	jne    0x124e0
   124bb:	8b 44 24 10          	mov    0x10(%esp),%eax
   124bf:	50                   	push   %eax
   124c0:	e8 eb 63 00 00       	call   0x188b0
   124c5:	56                   	push   %esi
   124c6:	e8 85 60 00 00       	call   0x18550
   124cb:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   124cf:	5d                   	pop    %ebp
   124d0:	c7 00 02 00 01 c0    	movl   $0xc0010002,(%eax)
   124d6:	5f                   	pop    %edi
   124d7:	5e                   	pop    %esi
   124d8:	5b                   	pop    %ebx
   124d9:	83 c4 28             	add    $0x28,%esp
   124dc:	c2 2c 00             	ret    $0x2c
   124df:	90                   	nop
   124e0:	8b 5c 24 54          	mov    0x54(%esp),%ebx
   124e4:	8d 43 04             	lea    0x4(%ebx),%eax
   124e7:	50                   	push   %eax
   124e8:	e8 a3 12 00 00       	call   0x13790
   124ed:	0a c0                	or     %al,%al
   124ef:	75 2a                	jne    0x1251b
   124f1:	57                   	push   %edi
   124f2:	e8 99 1c 00 00       	call   0x14190
   124f7:	8b 44 24 10          	mov    0x10(%esp),%eax
   124fb:	50                   	push   %eax
   124fc:	e8 af 63 00 00       	call   0x188b0
   12501:	56                   	push   %esi
   12502:	e8 49 60 00 00       	call   0x18550
   12507:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   1250b:	5d                   	pop    %ebp
   1250c:	c7 00 02 00 01 c0    	movl   $0xc0010002,(%eax)
   12512:	5f                   	pop    %edi
   12513:	5e                   	pop    %esi
   12514:	5b                   	pop    %ebx
   12515:	83 c4 28             	add    $0x28,%esp
   12518:	c2 2c 00             	ret    $0x2c
   1251b:	8d 46 28             	lea    0x28(%esi),%eax
   1251e:	50                   	push   %eax
   1251f:	e8 2c 62 00 00       	call   0x18750
   12524:	c6 46 20 00          	movb   $0x0,0x20(%esi)
   12528:	89 7e 08             	mov    %edi,0x8(%esi)
   1252b:	89 5e 0c             	mov    %ebx,0xc(%esi)
   1252e:	8b 44 24 58          	mov    0x58(%esp),%eax
   12532:	89 46 10             	mov    %eax,0x10(%esi)
   12535:	8b 47 04             	mov    0x4(%edi),%eax
   12538:	89 06                	mov    %eax,(%esi)
   1253a:	8b 47 04             	mov    0x4(%edi),%eax
   1253d:	8b 40 28             	mov    0x28(%eax),%eax
   12540:	89 46 30             	mov    %eax,0x30(%esi)
   12543:	8b 47 04             	mov    0x4(%edi),%eax
   12546:	8b 40 2c             	mov    0x2c(%eax),%eax
   12549:	89 46 34             	mov    %eax,0x34(%esi)
   1254c:	8b 43 24             	mov    0x24(%ebx),%eax
   1254f:	89 46 38             	mov    %eax,0x38(%esi)
   12552:	8b 43 28             	mov    0x28(%ebx),%eax
   12555:	89 46 3c             	mov    %eax,0x3c(%esi)
   12558:	8b 43 34             	mov    0x34(%ebx),%eax
   1255b:	89 46 40             	mov    %eax,0x40(%esi)
   1255e:	8b 43 38             	mov    0x38(%ebx),%eax
   12561:	89 46 44             	mov    %eax,0x44(%esi)
   12564:	8b 44 24 10          	mov    0x10(%esp),%eax
   12568:	89 46 1c             	mov    %eax,0x1c(%esi)
   1256b:	8b 44 24 10          	mov    0x10(%esp),%eax
   1256f:	89 70 0c             	mov    %esi,0xc(%eax)
   12572:	8b 44 24 44          	mov    0x44(%esp),%eax
   12576:	89 30                	mov    %esi,(%eax)
   12578:	8b 44 24 64          	mov    0x64(%esp),%eax
   1257c:	50                   	push   %eax
   1257d:	8b 44 24 64          	mov    0x64(%esp),%eax
   12581:	50                   	push   %eax
   12582:	8b 47 08             	mov    0x8(%edi),%eax
   12585:	50                   	push   %eax
   12586:	8b 44 24 5c          	mov    0x5c(%esp),%eax
   1258a:	56                   	push   %esi
   1258b:	50                   	push   %eax
   1258c:	8b 44 24 60          	mov    0x60(%esp),%eax
   12590:	50                   	push   %eax
   12591:	8b 44 24 60          	mov    0x60(%esp),%eax
   12595:	50                   	push   %eax
   12596:	8d 46 04             	lea    0x4(%esi),%eax
   12599:	50                   	push   %eax
   1259a:	8b 44 24 60          	mov    0x60(%esp),%eax
   1259e:	50                   	push   %eax
   1259f:	8b 47 04             	mov    0x4(%edi),%eax
   125a2:	ff 50 20             	call   *0x20(%eax)
   125a5:	8b e8                	mov    %eax,%ebp
   125a7:	0b ed                	or     %ebp,%ebp
   125a9:	75 25                	jne    0x125d0
   125ab:	56                   	push   %esi
   125ac:	e8 5f 13 00 00       	call   0x13910
   125b1:	0a c0                	or     %al,%al
   125b3:	74 1b                	je     0x125d0
   125b5:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   125b9:	5d                   	pop    %ebp
   125ba:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
   125c0:	5f                   	pop    %edi
   125c1:	5e                   	pop    %esi
   125c2:	5b                   	pop    %ebx
   125c3:	83 c4 28             	add    $0x28,%esp
   125c6:	c2 2c 00             	ret    $0x2c
   125c9:	8d a4 24 00 00 00 00 	lea    0x0(%esp),%esp
   125d0:	81 fd 03 01 00 00    	cmp    $0x103,%ebp
   125d6:	75 18                	jne    0x125f0
   125d8:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   125dc:	5d                   	pop    %ebp
   125dd:	c7 00 03 01 00 00    	movl   $0x103,(%eax)
   125e3:	5f                   	pop    %edi
   125e4:	5e                   	pop    %esi
   125e5:	5b                   	pop    %ebx
   125e6:	83 c4 28             	add    $0x28,%esp
   125e9:	c2 2c 00             	ret    $0x2c
   125ec:	8d 64 24 00          	lea    0x0(%esp),%esp
   125f0:	8b 44 24 10          	mov    0x10(%esp),%eax
   125f4:	50                   	push   %eax
   125f5:	e8 b6 62 00 00       	call   0x188b0
   125fa:	57                   	push   %edi
   125fb:	e8 90 1b 00 00       	call   0x14190
   12600:	8d 43 04             	lea    0x4(%ebx),%eax
   12603:	50                   	push   %eax
   12604:	e8 c7 11 00 00       	call   0x137d0
   12609:	0a c0                	or     %al,%al
   1260b:	74 06                	je     0x12613
   1260d:	53                   	push   %ebx
   1260e:	e8 3d 5f 00 00       	call   0x18550
   12613:	56                   	push   %esi
   12614:	e8 37 5f 00 00       	call   0x18550
   12619:	8b 44 24 3c          	mov    0x3c(%esp),%eax
   1261d:	5d                   	pop    %ebp
   1261e:	c7 00 07 00 01 c0    	movl   $0xc0010007,(%eax)
   12624:	5f                   	pop    %edi
   12625:	5e                   	pop    %esi
   12626:	5b                   	pop    %ebx
   12627:	83 c4 28             	add    $0x28,%esp
   1262a:	c2 2c 00             	ret    $0x2c
   1262d:	8d 49 00             	lea    0x0(%ecx),%ecx
   12630:	56                   	push   %esi
   12631:	8b 44 24 0c          	mov    0xc(%esp),%eax
   12635:	50                   	push   %eax
   12636:	8b 70 1c             	mov    0x1c(%eax),%esi
   12639:	e8 b2 19 00 00       	call   0x13ff0
   1263e:	0a c0                	or     %al,%al
   12640:	74 14                	je     0x12656
   12642:	56                   	push   %esi
   12643:	e8 68 62 00 00       	call   0x188b0
   12648:	8b 44 24 08          	mov    0x8(%esp),%eax
   1264c:	5e                   	pop    %esi
   1264d:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
   12653:	c2 08 00             	ret    $0x8
   12656:	8b 44 24 08          	mov    0x8(%esp),%eax
   1265a:	5e                   	pop    %esi
   1265b:	c7 00 03 01 00 00    	movl   $0x103,(%eax)
   12661:	c2 08 00             	ret    $0x8
   12664:	05 00 00 00 00       	add    $0x0,%eax
   12669:	8d a4 24 00 00 00 00 	lea    0x0(%esp),%esp
   12670:	56                   	push   %esi
   12671:	a1 10 8e 01 00       	mov    0x18e10,%eax
   12676:	8b 0d 14 8e 01 00    	mov    0x18e14,%ecx
   1267c:	51                   	push   %ecx
   1267d:	50                   	push   %eax
   1267e:	6a 00                	push   $0x0
   12680:	8b 74 24 14          	mov    0x14(%esp),%esi
   12684:	6a 08                	push   $0x8
   12686:	56                   	push   %esi
   12687:	e8 84 eb ff ff       	call   0x11210
   1268c:	0b c0                	or     %eax,%eax
   1268e:	75 0f                	jne    0x1269f
   12690:	8b 0e                	mov    (%esi),%ecx
   12692:	8b 44 24 0c          	mov    0xc(%esp),%eax
   12696:	89 01                	mov    %eax,(%ecx)
   12698:	8b 44 24 10          	mov    0x10(%esp),%eax
   1269c:	89 41 04             	mov    %eax,0x4(%ecx)
   1269f:	5e                   	pop    %esi
   126a0:	c2 10 00             	ret    $0x10
   126a3:	8d 9b 00 00 00 00    	lea    0x0(%ebx),%ebx
   126a9:	8d a4 24 00 00 00 00 	lea    0x0(%esp),%esp
   126b0:	8b 44 24 04          	mov    0x4(%esp),%eax
   126b4:	0b c0                	or     %eax,%eax
   126b6:	74 0a                	je     0x126c2
   126b8:	6a 00                	push   $0x0
   126ba:	6a 08                	push   $0x8
   126bc:	50                   	push   %eax
   126bd:	e8 be eb ff ff       	call   0x11280
   126c2:	c2 08 00             	ret    $0x8
   126c5:	8d 64 24 00          	lea    0x0(%esp),%esp
   126c9:	8d a4 24 00 00 00 00 	lea    0x0(%esp),%esp
   126d0:	83 ec 08             	sub    $0x8,%esp
   126d3:	53                   	push   %ebx
   126d4:	56                   	push   %esi
   126d5:	57                   	push   %edi
   126d6:	55                   	push   %ebp
   126d7:	e8 34 dd ff ff       	call   0x10410
   126dc:	8b 44 24 20          	mov    0x20(%esp),%eax
   126e0:	8b 7c 24 24          	mov    0x24(%esp),%edi
   126e4:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
   126ea:	0b ff                	or     %edi,%edi
   126ec:	75 14                	jne    0x12702
   126ee:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   126f2:	5d                   	pop    %ebp
   126f3:	c7 00 01 00 00 c0    	movl   $0xc0000001,(%eax)
   126f9:	5f                   	pop    %edi
   126fa:	5e                   	pop    %esi
   126fb:	5b                   	pop    %ebx
   126fc:	83 c4 08             	add    $0x8,%esp
   126ff:	c2 18 00             	ret    $0x18
   12702:	8b 5c 24 2c          	mov    0x2c(%esp),%ebx
   12706:	80 3b 03             	cmpb   $0x3,(%ebx)
   12709:	0f 85 71 01 00 00    	jne    0x12880
   1270f:	80 7b 01 00          	cmpb   $0x0,0x1(%ebx)
   12713:	0f 85 67 01 00 00    	jne    0x12880
   12719:	8b 6c 24 30          	mov    0x30(%esp),%ebp
   1271d:	83 fd 38             	cmp    $0x38,%ebp
   12720:	73 14                	jae    0x12736
   12722:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   12726:	5d                   	pop    %ebp
   12727:	c7 00 05 00 01 c0    	movl   $0xc0010005,(%eax)
   1272d:	5f                   	pop    %edi
   1272e:	5e                   	pop    %esi
   1272f:	5b                   	pop    %ebx
   12730:	83 c4 08             	add    $0x8,%esp
   12733:	c2 18 00             	ret    $0x18
   12736:	33 c0                	xor    %eax,%eax
   12738:	66 8b 43 30          	mov    0x30(%ebx),%ax
   1273c:	03 e8                	add    %eax,%ebp
   1273e:	8d 45 34             	lea    0x34(%ebp),%eax
   12741:	89 44 24 14          	mov    %eax,0x14(%esp)
   12745:	50                   	push   %eax
   12746:	6a 00                	push   $0x0
   12748:	e8 f3 5d 00 00       	call   0x18540
   1274d:	8b f0                	mov    %eax,%esi
   1274f:	0b f6                	or     %esi,%esi
   12751:	75 14                	jne    0x12767
   12753:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   12757:	5d                   	pop    %ebp
   12758:	c7 00 9a 00 00 c0    	movl   $0xc000009a,(%eax)
   1275e:	5f                   	pop    %edi
   1275f:	5e                   	pop    %esi
   12760:	5b                   	pop    %ebx
   12761:	83 c4 08             	add    $0x8,%esp
   12764:	c2 18 00             	ret    $0x18
   12767:	8b 44 24 14          	mov    0x14(%esp),%eax
   1276b:	89 46 14             	mov    %eax,0x14(%esi)
   1276e:	55                   	push   %ebp
   1276f:	53                   	push   %ebx
   12770:	8d 46 18             	lea    0x18(%esi),%eax
   12773:	50                   	push   %eax
   12774:	e8 4f 62 00 00       	call   0x189c8
   12779:	83 c4 0c             	add    $0xc,%esp
   1277c:	8d 4e 50             	lea    0x50(%esi),%ecx
   1277f:	89 4e 4c             	mov    %ecx,0x4c(%esi)
   12782:	33 c0                	xor    %eax,%eax
   12784:	66 8b 43 30          	mov    0x30(%ebx),%ax
   12788:	50                   	push   %eax
   12789:	8b 43 34             	mov    0x34(%ebx),%eax
   1278c:	50                   	push   %eax
   1278d:	51                   	push   %ecx
   1278e:	e8 35 62 00 00       	call   0x189c8
   12793:	83 c4 0c             	add    $0xc,%esp
   12796:	c7 06 00 00 00 00    	movl   $0x0,(%esi)
   1279c:	8b 44 24 28          	mov    0x28(%esp),%eax
   127a0:	89 46 04             	mov    %eax,0x4(%esi)
   127a3:	8b 07                	mov    (%edi),%eax
   127a5:	c7 40 34 00 19 01 00 	movl   $0x11900,0x34(%eax)
   127ac:	8b 07                	mov    (%edi),%eax
   127ae:	c7 40 78 60 19 01 00 	movl   $0x11960,0x78(%eax)
   127b5:	8b 07                	mov    (%edi),%eax
   127b7:	c7 40 38 60 39 01 00 	movl   $0x13960,0x38(%eax)
   127be:	8b 07                	mov    (%edi),%eax
   127c0:	c7 40 70 f0 3b 01 00 	movl   $0x13bf0,0x70(%eax)
   127c7:	8b 07                	mov    (%edi),%eax
   127c9:	c7 80 80 00 00 00 a0 	movl   $0x13fa0,0x80(%eax)
   127d0:	3f 01 00 
   127d3:	8b 07                	mov    (%edi),%eax
   127d5:	c7 40 40 60 3f 01 00 	movl   $0x13f60,0x40(%eax)
   127dc:	8d 44 24 13          	lea    0x13(%esp),%eax
   127e0:	89 7e 50             	mov    %edi,0x50(%esi)
   127e3:	50                   	push   %eax
   127e4:	68 00 8c 01 00       	push   $0x18c00
   127e9:	e8 32 5f 00 00       	call   0x18720
   127ee:	a1 00 9e 01 00       	mov    0x19e00,%eax
   127f3:	89 46 54             	mov    %eax,0x54(%esi)
   127f6:	89 35 00 9e 01 00    	mov    %esi,0x19e00
   127fc:	8a 44 24 13          	mov    0x13(%esp),%al
   12800:	50                   	push   %eax
   12801:	68 00 8c 01 00       	push   $0x18c00
   12806:	e8 95 5f 00 00       	call   0x187a0
   1280b:	6a 00                	push   $0x0
   1280d:	6a 00                	push   $0x0
   1280f:	8d 46 58             	lea    0x58(%esi),%eax
   12812:	50                   	push   %eax
   12813:	e8 28 5f 00 00       	call   0x18740
   12818:	c6 46 68 00          	movb   $0x0,0x68(%esi)
   1281c:	8d 46 08             	lea    0x8(%esi),%eax
   1281f:	50                   	push   %eax
   12820:	e8 eb 0f 00 00       	call   0x13810
   12825:	8b 44 24 20          	mov    0x20(%esp),%eax
   12829:	89 30                	mov    %esi,(%eax)
   1282b:	83 7f 04 00          	cmpl   $0x0,0x4(%edi)
   1282f:	74 39                	je     0x1286a
   12831:	56                   	push   %esi
   12832:	e8 79 1c 00 00       	call   0x144b0
   12837:	0b c0                	or     %eax,%eax
   12839:	75 15                	jne    0x12850
   1283b:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   1283f:	5d                   	pop    %ebp
   12840:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
   12846:	5f                   	pop    %edi
   12847:	5e                   	pop    %esi
   12848:	5b                   	pop    %ebx
   12849:	83 c4 08             	add    $0x8,%esp
   1284c:	c2 18 00             	ret    $0x18
   1284f:	90                   	nop
   12850:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   12854:	56                   	push   %esi
   12855:	c7 00 01 00 00 c0    	movl   $0xc0000001,(%eax)
   1285b:	e8 90 1a 00 00       	call   0x142f0
   12860:	5d                   	pop    %ebp
   12861:	5f                   	pop    %edi
   12862:	5e                   	pop    %esi
   12863:	5b                   	pop    %ebx
   12864:	83 c4 08             	add    $0x8,%esp
   12867:	c2 18 00             	ret    $0x18
   1286a:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   1286e:	5d                   	pop    %ebp
   1286f:	c7 00 01 00 00 c0    	movl   $0xc0000001,(%eax)
   12875:	5f                   	pop    %edi
   12876:	5e                   	pop    %esi
   12877:	5b                   	pop    %ebx
   12878:	83 c4 08             	add    $0x8,%esp
   1287b:	c2 18 00             	ret    $0x18
   1287e:	8b ff                	mov    %edi,%edi
   12880:	8b 44 24 1c          	mov    0x1c(%esp),%eax
   12884:	5d                   	pop    %ebp
   12885:	c7 00 04 00 01 c0    	movl   $0xc0010004,(%eax)
   1288b:	5f                   	pop    %edi
   1288c:	5e                   	pop    %esi
   1288d:	5b                   	pop    %ebx
   1288e:	83 c4 08             	add    $0x8,%esp
   12891:	c2 18 00             	ret    $0x18
   12894:	05 00 00 00 00       	add    $0x0,%eax
   12899:	8d a4 24 00 00 00 00 	lea    0x0(%esp),%esp
   128a0:	8b 44 24 04          	mov    0x4(%esp),%eax
   128a4:	c7 00 00 00 00 00    	movl   $0x0,(%eax)
   128aa:	8b 44 24 08          	mov    0x8(%esp),%eax
   128ae:	0b c0                	or     %eax,%eax
   128b0:	74 09                	je     0x128bb
   128b2:	83 c0 08             	add    $0x8,%eax
   128b5:	50                   	push   %eax
   128b6:	e8 75 0f 00 00       	call   0x13830
   128bb:	c2 08 00             	ret    $0x8
   128be:	8b ff                	mov    %edi,%edi
   128c0:	56                   	push   %esi
   128c1:	8b 4c 24 14          	mov    0x14(%esp),%ecx
   128c5:	6a 00                	push   $0x0
   128c7:	8b 51 7c             	mov    0x7c(%ecx),%edx
   128ca:	8b 71 68             	mov    0x68(%ecx),%esi
   128cd:	83 c1 6c             	add    $0x6c,%ecx
   128d0:	8b 44 24 14          	mov    0x14(%esp),%eax
   128d4:	89 04 d6             	mov    %eax,(%esi,%edx,8)
   128d7:	6a 00                	push   $0x0
   128d9:	51                   	push   %ecx
   128da:	e8 e1 5e 00 00       	call   0x187c0
   128df:	b8 03 00 00 00       	mov    $0x3,%eax
   128e4:	5e                   	pop    %esi
   128e5:	c2 10 00             	ret    $0x10
   128e8:	8d 64 24 00          	lea    0x0(%esp),%esp
   128ec:	8d 64 24 00          	lea    0x0(%esp),%esp
   128f0:	81 ec 88 00 00 00    	sub    $0x88,%esp
   128f6:	53                   	push   %ebx
   128f7:	56                   	push   %esi
   128f8:	57                   	push   %edi
   128f9:	55                   	push   %ebp
   128fa:	c6 44 24 12 01       	movb   $0x1,0x12(%esp)
   128ff:	8b 84 24 a0 00 00 00 	mov    0xa0(%esp),%eax
   12906:	83 c0 08             	add    $0x8,%eax
   12909:	50                   	push   %eax
   1290a:	e8 81 0e 00 00       	call   0x13790
   1290f:	0a c0                	or     %al,%al
   12911:	0f 84 19 0c 00 00    	je     0x13530
   12917:	8b 9c 24 ac 00 00 00 	mov    0xac(%esp),%ebx
   1291e:	33 c0                	xor    %eax,%eax
   12920:	66 8b 43 02          	mov    0x2(%ebx),%ax
   12924:	50                   	push   %eax
   12925:	6a 00                	push   $0x0
   12927:	e8 14 5c 00 00       	call   0x18540
   1292c:	89 44 24 44          	mov    %eax,0x44(%esp)
   12930:	0b c0                	or     %eax,%eax
   12932:	0f 84 ee 0b 00 00    	je     0x13526
   12938:	66 8b 43 02          	mov    0x2(%ebx),%ax
   1293c:	66 89 44 24 42       	mov    %ax,0x42(%esp)
   12941:	66 8b 03             	mov    (%ebx),%ax
   12944:	66 89 44 24 40       	mov    %ax,0x40(%esp)
   12949:	33 c0                	xor    %eax,%eax
   1294b:	66 8b 43 02          	mov    0x2(%ebx),%ax
   1294f:	8b 7c 24 44          	mov    0x44(%esp),%edi
   12953:	8b 73 04             	mov    0x4(%ebx),%esi
   12956:	8b c8                	mov    %eax,%ecx
   12958:	c1 e9 02             	shr    $0x2,%ecx
   1295b:	f3 a5                	rep movsl %ds:(%esi),%es:(%edi)
   1295d:	8b c8                	mov    %eax,%ecx
   1295f:	83 e1 03             	and    $0x3,%ecx
   12962:	f3 a4                	rep movsb %ds:(%esi),%es:(%edi)
   12964:	8d 44 24 18          	lea    0x18(%esp),%eax
   12968:	50                   	push   %eax
   12969:	6a 00                	push   $0x0
   1296b:	6a 00                	push   $0x0
   1296d:	6a 17                	push   $0x17
   1296f:	53                   	push   %ebx
   12970:	8b 84 24 b4 00 00 00 	mov    0xb4(%esp),%eax
   12977:	68 84 00 00 00       	push   $0x84
   1297c:	8b 40 50             	mov    0x50(%eax),%eax
   1297f:	8b 00                	mov    (%eax),%eax
   12981:	50                   	push   %eax
   12982:	e8 d9 5c 00 00       	call   0x18660
   12987:	0b c0                	or     %eax,%eax
   12989:	0f 85 86 0b 00 00    	jne    0x13515
   1298f:	8b 44 24 18          	mov    0x18(%esp),%eax
   12993:	8b 58 28             	mov    0x28(%eax),%ebx
   12996:	89 03                	mov    %eax,(%ebx)
   12998:	8b 84 24 a0 00 00 00 	mov    0xa0(%esp),%eax
   1299f:	89 43 04             	mov    %eax,0x4(%ebx)
   129a2:	8b 84 24 a4 00 00 00 	mov    0xa4(%esp),%eax
   129a9:	89 43 08             	mov    %eax,0x8(%ebx)
   129ac:	8d 43 0c             	lea    0xc(%ebx),%eax
   129af:	8b 4c 24 40          	mov    0x40(%esp),%ecx
   129b3:	8b 54 24 44          	mov    0x44(%esp),%edx
   129b7:	89 08                	mov    %ecx,(%eax)
   129b9:	89 50 04             	mov    %edx,0x4(%eax)
   129bc:	8b 8c 24 a8 00 00 00 	mov    0xa8(%esp),%ecx
   129c3:	c7 43 14 00 00 00 00 	movl   $0x0,0x14(%ebx)
   129ca:	83 79 64 ff          	cmpl   $0xffffffff,0x64(%ecx)
   129ce:	75 11                	jne    0x129e1
   129d0:	c7 44 24 34 ff ff ff 	movl   $0xffffffff,0x34(%esp)
   129d7:	ff 
   129d8:	8b ac 24 b0 00 00 00 	mov    0xb0(%esp),%ebp
   129df:	eb 0e                	jmp    0x129ef
   129e1:	8b ac 24 b0 00 00 00 	mov    0xb0(%esp),%ebp
   129e8:	8b 45 0c             	mov    0xc(%ebp),%eax
   129eb:	89 44 24 34          	mov    %eax,0x34(%esp)
   129ef:	8b 41 6c             	mov    0x6c(%ecx),%eax
   129f2:	89 44 24 30          	mov    %eax,0x30(%esp)
   129f6:	83 7c 24 34 ff       	cmpl   $0xffffffff,0x34(%esp)
   129fb:	74 05                	je     0x12a02
   129fd:	83 f8 ff             	cmp    $0xffffffff,%eax
   12a00:	75 3e                	jne    0x12a40
   12a02:	0b ed                	or     %ebp,%ebp
   12a04:	74 3a                	je     0x12a40
   12a06:	83 7d 18 00          	cmpl   $0x0,0x18(%ebp)
   12a0a:	75 06                	jne    0x12a12
   12a0c:	80 7d 04 00          	cmpb   $0x0,0x4(%ebp)
   12a10:	74 2e                	je     0x12a40
   12a12:	8b 44 24 18          	mov    0x18(%esp),%eax
   12a16:	50                   	push   %eax
   12a17:	e8 54 5c 00 00       	call   0x18670
   12a1c:	8b 44 24 44          	mov    0x44(%esp),%eax
   12a20:	50                   	push   %eax
   12a21:	e8 2a 5b 00 00       	call   0x18550
   12a26:	8b 84 24 a0 00 00 00 	mov    0xa0(%esp),%eax
   12a2d:	50                   	push   %eax
   12a2e:	e8 bd 18 00 00       	call   0x142f0
   12a33:	b8 05 00 01 c0       	mov    $0xc0010005,%eax
   12a38:	e9 f8 0a 00 00       	jmp    0x13535
   12a3d:	8d 49 00             	lea    0x0(%ecx),%ecx
   12a40:	83 7c 24 34 ff       	cmpl   $0xffffffff,0x34(%esp)
   12a45:	0f 84 9a 01 00 00    	je     0x12be5
   12a4b:	83 7c 24 30 ff       	cmpl   $0xffffffff,0x30(%esp)
   12a50:	0f 84 8f 01 00 00    	je     0x12be5
   12a56:	0b ed                	or     %ebp,%ebp
   12a58:	0f 84 b2 02 00 00    	je     0x12d10
   12a5e:	80 7d 04 01          	cmpb   $0x1,0x4(%ebp)
   12a62:	75 0b                	jne    0x12a6f
   12a64:	83 7d 0c 01          	cmpl   $0x1,0xc(%ebp)
   12a68:	b8 01 00 00 00       	mov    $0x1,%eax
   12a6d:	74 02                	je     0x12a71
   12a6f:	2b c0                	sub    %eax,%eax
   12a71:	8b 4d 18             	mov    0x18(%ebp),%ecx
   12a74:	03 c8                	add    %eax,%ecx
   12a76:	c1 e1 04             	shl    $0x4,%ecx
   12a79:	83 c1 24             	add    $0x24,%ecx
   12a7c:	51                   	push   %ecx
   12a7d:	6a 00                	push   $0x0
   12a7f:	e8 bc 5a 00 00       	call   0x18540
   12a84:	89 44 24 14          	mov    %eax,0x14(%esp)
   12a88:	0b c0                	or     %eax,%eax
   12a8a:	75 2b                	jne    0x12ab7
   12a8c:	8b 44 24 18          	mov    0x18(%esp),%eax
   12a90:	50                   	push   %eax
   12a91:	e8 da 5b 00 00       	call   0x18670
   12a96:	8b 44 24 44          	mov    0x44(%esp),%eax
   12a9a:	50                   	push   %eax
   12a9b:	e8 b0 5a 00 00       	call   0x18550
   12aa0:	8b 84 24 a0 00 00 00 	mov    0xa0(%esp),%eax
   12aa7:	50                   	push   %eax
   12aa8:	e8 43 18 00 00       	call   0x142f0
   12aad:	b8 9a 00 00 c0       	mov    $0xc000009a,%eax
   12ab2:	e9 7e 0a 00 00       	jmp    0x13535
   12ab7:	8b 4c 24 14          	mov    0x14(%esp),%ecx
   12abb:	89 4b 2c             	mov    %ecx,0x2c(%ebx)
   12abe:	c7 01 01 00 00 00    	movl   $0x1,(%ecx)
   12ac4:	8b 45 0c             	mov    0xc(%ebp),%eax
   12ac7:	89 41 04             	mov    %eax,0x4(%ecx)
   12aca:	8b 44 24 30          	mov    0x30(%esp),%eax
   12ace:	89 41 08             	mov    %eax,0x8(%ecx)
   12ad1:	66 c7 41 0c 00 00    	movw   $0x0,0xc(%ecx)
   12ad7:	66 c7 41 0e 00 00    	movw   $0x0,0xe(%ecx)
   12add:	8b 45 18             	mov    0x18(%ebp),%eax
   12ae0:	89 41 10             	mov    %eax,0x10(%ecx)
   12ae3:	8b 44 24 30          	mov    0x30(%esp),%eax
   12ae7:	89 43 38             	mov    %eax,0x38(%ebx)
   12aea:	8b 44 24 34          	mov    0x34(%esp),%eax
   12aee:	89 43 3c             	mov    %eax,0x3c(%ebx)
   12af1:	8b 45 0c             	mov    0xc(%ebp),%eax
   12af4:	89 43 44             	mov    %eax,0x44(%ebx)
   12af7:	8a 45 04             	mov    0x4(%ebp),%al
   12afa:	88 43 48             	mov    %al,0x48(%ebx)
   12afd:	83 7d 18 00          	cmpl   $0x0,0x18(%ebp)
   12b01:	0f 86 fa 01 00 00    	jbe    0x12d01
   12b07:	c6 41 14 01          	movb   $0x1,0x14(%ecx)
   12b0b:	c6 41 15 01          	movb   $0x1,0x15(%ecx)
   12b0f:	83 7d 0c 01          	cmpl   $0x1,0xc(%ebp)
   12b13:	1b c0                	sbb    %eax,%eax
   12b15:	40                   	inc    %eax
   12b16:	66 89 41 16          	mov    %ax,0x16(%ecx)
   12b1a:	8b 45 1c             	mov    0x1c(%ebp),%eax
   12b1d:	50                   	push   %eax
   12b1e:	e8 dd 5d 00 00       	call   0x18900
   12b23:	8b 4c 24 14          	mov    0x14(%esp),%ecx
   12b27:	89 41 18             	mov    %eax,0x18(%ecx)
   12b2a:	89 51 1c             	mov    %edx,0x1c(%ecx)
   12b2d:	83 c1 18             	add    $0x18,%ecx
   12b30:	8b 4c 24 14          	mov    0x14(%esp),%ecx
   12b34:	8b 45 20             	mov    0x20(%ebp),%eax
   12b37:	89 41 20             	mov    %eax,0x20(%ecx)
   12b3a:	8b 45 1c             	mov    0x1c(%ebp),%eax
   12b3d:	89 44 24 20          	mov    %eax,0x20(%esp)
   12b41:	8b 45 20             	mov    0x20(%ebp),%eax
   12b44:	89 44 24 24          	mov    %eax,0x24(%esp)
   12b48:	03 44 24 20          	add    0x20(%esp),%eax
   12b4c:	89 44 24 24          	mov    %eax,0x24(%esp)
   12b50:	83 7d 24 00          	cmpl   $0x0,0x24(%ebp)
   12b54:	75 05                	jne    0x12b5b
   12b56:	c6 44 24 12 00       	movb   $0x0,0x12(%esp)
   12b5b:	c7 44 24 28 01 00 00 	movl   $0x1,0x28(%esp)
   12b62:	00 
   12b63:	83 7d 18 01          	cmpl   $0x1,0x18(%ebp)
   12b67:	76 6a                	jbe    0x12bd3
   12b69:	8d 7d 28             	lea    0x28(%ebp),%edi
   12b6c:	8b 74 24 14          	mov    0x14(%esp),%esi
   12b70:	83 c6 24             	add    $0x24,%esi
   12b73:	c6 06 01             	movb   $0x1,(%esi)
   12b76:	c6 46 01 01          	movb   $0x1,0x1(%esi)
   12b7a:	83 7d 0c 01          	cmpl   $0x1,0xc(%ebp)
   12b7e:	1b c0                	sbb    %eax,%eax
   12b80:	40                   	inc    %eax
   12b81:	66 89 46 02          	mov    %ax,0x2(%esi)
   12b85:	8b 07                	mov    (%edi),%eax
   12b87:	50                   	push   %eax
   12b88:	e8 73 5d 00 00       	call   0x18900
   12b8d:	89 46 04             	mov    %eax,0x4(%esi)
   12b90:	89 56 08             	mov    %edx,0x8(%esi)
   12b93:	8d 4e 04             	lea    0x4(%esi),%ecx
   12b96:	8b 47 04             	mov    0x4(%edi),%eax
   12b99:	89 46 0c             	mov    %eax,0xc(%esi)
   12b9c:	83 7f 08 00          	cmpl   $0x0,0x8(%edi)
   12ba0:	75 05                	jne    0x12ba7
   12ba2:	c6 44 24 12 00       	movb   $0x0,0x12(%esp)
   12ba7:	8b 07                	mov    (%edi),%eax
   12ba9:	39 44 24 20          	cmp    %eax,0x20(%esp)
   12bad:	76 04                	jbe    0x12bb3
   12baf:	89 44 24 20          	mov    %eax,0x20(%esp)
   12bb3:	03 47 04             	add    0x4(%edi),%eax
   12bb6:	3b 44 24 24          	cmp    0x24(%esp),%eax
   12bba:	76 04                	jbe    0x12bc0
   12bbc:	89 44 24 24          	mov    %eax,0x24(%esp)
   12bc0:	83 c7 0c             	add    $0xc,%edi
   12bc3:	83 c6 10             	add    $0x10,%esi
   12bc6:	ff 44 24 28          	incl   0x28(%esp)
   12bca:	8b 44 24 28          	mov    0x28(%esp),%eax
   12bce:	39 45 18             	cmp    %eax,0x18(%ebp)
   12bd1:	77 a0                	ja     0x12b73
   12bd3:	8b 44 24 20          	mov    0x20(%esp),%eax
   12bd7:	89 43 54             	mov    %eax,0x54(%ebx)
   12bda:	8b 44 24 24          	mov    0x24(%esp),%eax
   12bde:	2b 44 24 20          	sub    0x20(%esp),%eax
   12be2:	89 43 58             	mov    %eax,0x58(%ebx)
   12be5:	c6 43 28 00          	movb   $0x0,0x28(%ebx)
   12be9:	83 7c 24 34 ff       	cmpl   $0xffffffff,0x34(%esp)
   12bee:	0f 84 61 01 00 00    	je     0x12d55
   12bf4:	83 7c 24 30 ff       	cmpl   $0xffffffff,0x30(%esp)
   12bf9:	0f 84 56 01 00 00    	je     0x12d55
   12bff:	8d 44 24 13          	lea    0x13(%esp),%eax
   12c03:	50                   	push   %eax
   12c04:	6a 01                	push   $0x1
   12c06:	8b                   	.byte 0x8b
   12c07:	45                   	inc    %ebp
