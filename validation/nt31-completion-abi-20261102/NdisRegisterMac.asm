
/workspace/rltklant-vm/install/nt31-exports/ndis.sys:     file format pei-i386


Disassembly of section .text:

000126d0 <.text+0x22d0>:
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
   129bc:	8b                   	.byte 0x8b
   129bd:	8c                   	.byte 0x8c
