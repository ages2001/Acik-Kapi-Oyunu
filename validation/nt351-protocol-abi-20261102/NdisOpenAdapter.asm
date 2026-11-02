
/workspace/rltklant-vm/install/nt351-standard/installed-ndis.sys:     file format pei-i386


Disassembly of section .text:

000127e1 <.text+0x2421>:
   127e1:	55                   	push   ebp
   127e2:	8b ec                	mov    ebp,esp
   127e4:	83 ec 34             	sub    esp,0x34
   127e7:	53                   	push   ebx
   127e8:	56                   	push   esi
   127e9:	57                   	push   edi
   127ea:	8b 35 94 55 01 00    	mov    esi,DWORD PTR ds:0x15594
   127f0:	68 4e 44 6f 62       	push   0x626f444e
   127f5:	6a 54                	push   0x54
   127f7:	6a 00                	push   0x0
   127f9:	ff d6                	call   esi
   127fb:	8b d8                	mov    ebx,eax
   127fd:	85 db                	test   ebx,ebx
   127ff:	75 12                	jne    0x12813
   12801:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12804:	c7 01 9a 00 00 c0    	mov    DWORD PTR [ecx],0xc000009a
   1280a:	5f                   	pop    edi
   1280b:	5e                   	pop    esi
   1280c:	5b                   	pop    ebx
   1280d:	8b e5                	mov    esp,ebp
   1280f:	5d                   	pop    ebp
   12810:	c2 2c 00             	ret    0x2c
   12813:	8b fb                	mov    edi,ebx
   12815:	33 c0                	xor    eax,eax
   12817:	b9 15 00 00 00       	mov    ecx,0x15
   1281c:	68 4e 44 20 20       	push   0x2020444e
   12821:	f3 ab                	rep stos DWORD PTR es:[edi],eax
   12823:	6a 18                	push   0x18
   12825:	50                   	push   eax
   12826:	ff d6                	call   esi
   12828:	89 45 fc             	mov    DWORD PTR [ebp-0x4],eax
   1282b:	85 c0                	test   eax,eax
   1282d:	75 12                	jne    0x12841
   1282f:	53                   	push   ebx
   12830:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   12836:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12839:	c7 01 9a 00 00 c0    	mov    DWORD PTR [ecx],0xc000009a
   1283f:	eb c9                	jmp    0x1280a
   12841:	8b 45 fc             	mov    eax,DWORD PTR [ebp-0x4]
   12844:	be 50 58 01 00       	mov    esi,0x15850
   12849:	8d 78 08             	lea    edi,[eax+0x8]
   1284c:	c7 00 00 00 00 00    	mov    DWORD PTR [eax],0x0
   12852:	c6 40 04 00          	mov    BYTE PTR [eax+0x4],0x0
   12856:	c6 40 05 04          	mov    BYTE PTR [eax+0x5],0x4
   1285a:	66 c7 40 06 08 00    	mov    WORD PTR [eax+0x6],0x8
   12860:	a5                   	movs   DWORD PTR es:[edi],DWORD PTR ds:[esi]
   12861:	be 58 58 01 00       	mov    esi,0x15858
   12866:	8b 45 fc             	mov    eax,DWORD PTR [ebp-0x4]
   12869:	0f b6 48 05          	movzx  ecx,BYTE PTR [eax+0x5]
   1286d:	8d 7c 01 09          	lea    edi,[ecx+eax*1+0x9]
   12871:	a5                   	movs   DWORD PTR es:[edi],DWORD PTR ds:[esi]
   12872:	a5                   	movs   DWORD PTR es:[edi],DWORD PTR ds:[esi]
   12873:	c7 45 cc 18 00 00 00 	mov    DWORD PTR [ebp-0x34],0x18
   1287a:	c7 45 d0 00 00 00 00 	mov    DWORD PTR [ebp-0x30],0x0
   12881:	8b 55 28             	mov    edx,DWORD PTR [ebp+0x28]
   12884:	6a 18                	push   0x18
   12886:	ff 75 fc             	push   DWORD PTR [ebp-0x4]
   12889:	6a 00                	push   0x0
   1288b:	6a 01                	push   0x1
   1288d:	89 55 d4             	mov    DWORD PTR [ebp-0x2c],edx
   12890:	6a 03                	push   0x3
   12892:	8d 4d e4             	lea    ecx,[ebp-0x1c]
   12895:	6a 00                	push   0x0
   12897:	8d 55 cc             	lea    edx,[ebp-0x34]
   1289a:	c7 45 d8 40 00 00 00 	mov    DWORD PTR [ebp-0x28],0x40
   128a1:	6a 00                	push   0x0
   128a3:	8d 45 ec             	lea    eax,[ebp-0x14]
   128a6:	c7 45 dc 00 00 00 00 	mov    DWORD PTR [ebp-0x24],0x0
   128ad:	c7 45 e0 00 00 00 00 	mov    DWORD PTR [ebp-0x20],0x0
   128b4:	51                   	push   ecx
   128b5:	52                   	push   edx
   128b6:	6a 03                	push   0x3
   128b8:	50                   	push   eax
   128b9:	ff 15 f0 55 01 00    	call   DWORD PTR ds:0x155f0
   128bf:	ff 75 fc             	push   DWORD PTR [ebp-0x4]
   128c2:	8b f8                	mov    edi,eax
   128c4:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   128ca:	85 ff                	test   edi,edi
   128cc:	74 15                	je     0x128e3
   128ce:	53                   	push   ebx
   128cf:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   128d5:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   128d8:	c7 01 06 00 01 c0    	mov    DWORD PTR [ecx],0xc0010006
   128de:	e9 27 ff ff ff       	jmp    0x1280a
   128e3:	6a 00                	push   0x0
   128e5:	8d 45 f0             	lea    eax,[ebp-0x10]
   128e8:	50                   	push   eax
   128e9:	6a 00                	push   0x0
   128eb:	6a 00                	push   0x0
   128ed:	6a 00                	push   0x0
   128ef:	ff 75 ec             	push   DWORD PTR [ebp-0x14]
   128f2:	ff 15 f4 55 01 00    	call   DWORD PTR ds:0x155f4
   128f8:	ff 75 ec             	push   DWORD PTR [ebp-0x14]
   128fb:	ff 15 f8 55 01 00    	call   DWORD PTR ds:0x155f8
   12901:	ff 75 f0             	push   DWORD PTR [ebp-0x10]
   12904:	ff 15 e4 55 01 00    	call   DWORD PTR ds:0x155e4
   1290a:	8b 70 28             	mov    esi,DWORD PTR [eax+0x28]
   1290d:	83 c6 50             	add    esi,0x50
   12910:	39 06                	cmp    DWORD PTR [esi],eax
   12912:	0f 84 c2 01 00 00    	je     0x12ada
   12918:	33 c9                	xor    ecx,ecx
   1291a:	8b 55 1c             	mov    edx,DWORD PTR [ebp+0x1c]
   1291d:	c6 45 fb 00          	mov    BYTE PTR [ebp-0x5],0x0
   12921:	3b ca                	cmp    ecx,edx
   12923:	74 19                	je     0x1293e
   12925:	8b 7d 18             	mov    edi,DWORD PTR [ebp+0x18]
   12928:	8b 86 e8 00 00 00    	mov    eax,DWORD PTR [esi+0xe8]
   1292e:	39 07                	cmp    DWORD PTR [edi],eax
   12930:	74 08                	je     0x1293a
   12932:	83 c7 04             	add    edi,0x4
   12935:	41                   	inc    ecx
   12936:	3b ca                	cmp    ecx,edx
   12938:	72 f4                	jb     0x1292e
   1293a:	3b ca                	cmp    ecx,edx
   1293c:	75 40                	jne    0x1297e
   1293e:	83 be e8 00 00 00 07 	cmp    DWORD PTR [esi+0xe8],0x7
   12945:	0f 85 a5 00 00 00    	jne    0x129f0
   1294b:	33 c9                	xor    ecx,ecx
   1294d:	85 d2                	test   edx,edx
   1294f:	74 10                	je     0x12961
   12951:	8b 45 18             	mov    eax,DWORD PTR [ebp+0x18]
   12954:	83 38 00             	cmp    DWORD PTR [eax],0x0
   12957:	74 08                	je     0x12961
   12959:	83 c0 04             	add    eax,0x4
   1295c:	41                   	inc    ecx
   1295d:	3b ca                	cmp    ecx,edx
   1295f:	72 f3                	jb     0x12954
   12961:	3b ca                	cmp    ecx,edx
   12963:	75 0e                	jne    0x12973
   12965:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12968:	c7 01 19 00 01 c0    	mov    DWORD PTR [ecx],0xc0010019
   1296e:	e9 97 fe ff ff       	jmp    0x1280a
   12973:	80 8e c8 01 00 00 06 	or     BYTE PTR [esi+0x1c8],0x6
   1297a:	c6 45 fb 01          	mov    BYTE PTR [ebp-0x5],0x1
   1297e:	8b 45 14             	mov    eax,DWORD PTR [ebp+0x14]
   12981:	89 08                	mov    DWORD PTR [eax],ecx
   12983:	b9 02 00 00 00       	mov    ecx,0x2
   12988:	ff 15 68 56 01 00    	call   DWORD PTR ds:0x15668
   1298e:	8d 4e 30             	lea    ecx,[esi+0x30]
   12991:	88 45 fa             	mov    BYTE PTR [ebp-0x6],al
   12994:	ff 15 9c 55 01 00    	call   DWORD PTR ds:0x1559c
   1299a:	80 7e 2e 00          	cmp    BYTE PTR [esi+0x2e],0x0
   1299e:	74 5e                	je     0x129fe
   129a0:	c6 45 fc 00          	mov    BYTE PTR [ebp-0x4],0x0
   129a4:	80 7d fc 00          	cmp    BYTE PTR [ebp-0x4],0x0
   129a8:	74 5e                	je     0x12a08
   129aa:	ff 75 fb             	push   DWORD PTR [ebp-0x5]
   129ad:	ff 75 f0             	push   DWORD PTR [ebp-0x10]
   129b0:	53                   	push   ebx
   129b1:	56                   	push   esi
   129b2:	ff 75 30             	push   DWORD PTR [ebp+0x30]
   129b5:	ff 75 2c             	push   DWORD PTR [ebp+0x2c]
   129b8:	ff 75 28             	push   DWORD PTR [ebp+0x28]
   129bb:	ff 75 24             	push   DWORD PTR [ebp+0x24]
   129be:	ff 75 20             	push   DWORD PTR [ebp+0x20]
   129c1:	ff 75 10             	push   DWORD PTR [ebp+0x10]
   129c4:	ff 75 0c             	push   DWORD PTR [ebp+0xc]
   129c7:	ff 75 08             	push   DWORD PTR [ebp+0x8]
   129ca:	e8 9d f9 ff ff       	call   0x1236c
   129cf:	80 7d fc 00          	cmp    BYTE PTR [ebp-0x4],0x0
   129d3:	74 04                	je     0x129d9
   129d5:	c6 46 2e 00          	mov    BYTE PTR [esi+0x2e],0x0
   129d9:	8d 4e 30             	lea    ecx,[esi+0x30]
   129dc:	ff 15 90 55 01 00    	call   DWORD PTR ds:0x15590
   129e2:	8b 4d fa             	mov    ecx,DWORD PTR [ebp-0x6]
   129e5:	ff 15 60 56 01 00    	call   DWORD PTR ds:0x15660
   129eb:	e9 1a fe ff ff       	jmp    0x1280a
   129f0:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   129f3:	c7 01 19 00 01 c0    	mov    DWORD PTR [ecx],0xc0010019
   129f9:	e9 0c fe ff ff       	jmp    0x1280a
   129fe:	c6 45 fc 01          	mov    BYTE PTR [ebp-0x4],0x1
   12a02:	c6 46 2e 01          	mov    BYTE PTR [esi+0x2e],0x1
   12a06:	eb 9c                	jmp    0x129a4
   12a08:	a1 80 56 01 00       	mov    eax,ds:0x15680
   12a0d:	8d 55 f4             	lea    edx,[ebp-0xc]
   12a10:	ff 35 84 56 01 00    	push   DWORD PTR ds:0x15684
   12a16:	50                   	push   eax
   12a17:	6a 00                	push   0x0
   12a19:	6a 2c                	push   0x2c
   12a1b:	52                   	push   edx
   12a1c:	e8 58 f8 ff ff       	call   0x12279
   12a21:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12a24:	85 c0                	test   eax,eax
   12a26:	89 01                	mov    DWORD PTR [ecx],eax
   12a28:	0f 85 97 00 00 00    	jne    0x12ac5
   12a2e:	8b 45 f4             	mov    eax,DWORD PTR [ebp-0xc]
   12a31:	8b 55 10             	mov    edx,DWORD PTR [ebp+0x10]
   12a34:	8b 7d 20             	mov    edi,DWORD PTR [ebp+0x20]
   12a37:	c7 00 00 00 00 00    	mov    DWORD PTR [eax],0x0
   12a3d:	8b 45 f4             	mov    eax,DWORD PTR [ebp-0xc]
   12a40:	89 50 04             	mov    DWORD PTR [eax+0x4],edx
   12a43:	8b 45 f4             	mov    eax,DWORD PTR [ebp-0xc]
   12a46:	8b 55 24             	mov    edx,DWORD PTR [ebp+0x24]
   12a49:	89 78 14             	mov    DWORD PTR [eax+0x14],edi
   12a4c:	8b 45 f4             	mov    eax,DWORD PTR [ebp-0xc]
   12a4f:	89 50 18             	mov    DWORD PTR [eax+0x18],edx
   12a52:	8b 45 28             	mov    eax,DWORD PTR [ebp+0x28]
   12a55:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a58:	8b 55 2c             	mov    edx,DWORD PTR [ebp+0x2c]
   12a5b:	89 47 1c             	mov    DWORD PTR [edi+0x1c],eax
   12a5e:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a61:	8b 45 30             	mov    eax,DWORD PTR [ebp+0x30]
   12a64:	89 57 20             	mov    DWORD PTR [edi+0x20],edx
   12a67:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a6a:	89 47 24             	mov    DWORD PTR [edi+0x24],eax
   12a6d:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a70:	89 77 08             	mov    DWORD PTR [edi+0x8],esi
   12a73:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a76:	8a 55 fb             	mov    dl,BYTE PTR [ebp-0x5]
   12a79:	89 5f 0c             	mov    DWORD PTR [edi+0xc],ebx
   12a7c:	8b 45 f0             	mov    eax,DWORD PTR [ebp-0x10]
   12a7f:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a82:	89 47 10             	mov    DWORD PTR [edi+0x10],eax
   12a85:	8b 7d f4             	mov    edi,DWORD PTR [ebp-0xc]
   12a88:	88 57 28             	mov    BYTE PTR [edi+0x28],dl
   12a8b:	83 be a8 04 00 00 00 	cmp    DWORD PTR [esi+0x4a8],0x0
   12a92:	75 24                	jne    0x12ab8
   12a94:	8b 45 f4             	mov    eax,DWORD PTR [ebp-0xc]
   12a97:	89 86 a8 04 00 00    	mov    DWORD PTR [esi+0x4a8],eax
   12a9d:	8b 45 f4             	mov    eax,DWORD PTR [ebp-0xc]
   12aa0:	89 86 ac 04 00 00    	mov    DWORD PTR [esi+0x4ac],eax
   12aa6:	c7 01 03 01 00 00    	mov    DWORD PTR [ecx],0x103
   12aac:	c6 86 d3 01 00 00 01 	mov    BYTE PTR [esi+0x1d3],0x1
   12ab3:	e9 17 ff ff ff       	jmp    0x129cf
   12ab8:	8b 55 f4             	mov    edx,DWORD PTR [ebp-0xc]
   12abb:	8b 86 ac 04 00 00    	mov    eax,DWORD PTR [esi+0x4ac]
   12ac1:	89 10                	mov    DWORD PTR [eax],edx
   12ac3:	eb d8                	jmp    0x12a9d
   12ac5:	8b 4d f0             	mov    ecx,DWORD PTR [ebp-0x10]
   12ac8:	ff 15 8c 55 01 00    	call   DWORD PTR ds:0x1558c
   12ace:	53                   	push   ebx
   12acf:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   12ad5:	e9 f5 fe ff ff       	jmp    0x129cf
   12ada:	8d 46 1c             	lea    eax,[esi+0x1c]
   12add:	50                   	push   eax
   12ade:	e8 10 03 00 00       	call   0x12df3
   12ae3:	84 c0                	test   al,al
   12ae5:	75 1e                	jne    0x12b05
   12ae7:	8b 4d f0             	mov    ecx,DWORD PTR [ebp-0x10]
   12aea:	ff 15 8c 55 01 00    	call   DWORD PTR ds:0x1558c
   12af0:	53                   	push   ebx
   12af1:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   12af7:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12afa:	c7 01 02 00 01 c0    	mov    DWORD PTR [ecx],0xc0010002
   12b00:	e9 05 fd ff ff       	jmp    0x1280a
   12b05:	8b 7d 20             	mov    edi,DWORD PTR [ebp+0x20]
   12b08:	8d 47 04             	lea    eax,[edi+0x4]
   12b0b:	50                   	push   eax
   12b0c:	e8 e2 02 00 00       	call   0x12df3
   12b11:	84 c0                	test   al,al
   12b13:	75 24                	jne    0x12b39
   12b15:	56                   	push   esi
   12b16:	e8 20 d1 00 00       	call   0x1fc3b
   12b1b:	8b 4d f0             	mov    ecx,DWORD PTR [ebp-0x10]
   12b1e:	ff 15 8c 55 01 00    	call   DWORD PTR ds:0x1558c
   12b24:	53                   	push   ebx
   12b25:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   12b2b:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12b2e:	c7 01 02 00 01 c0    	mov    DWORD PTR [ecx],0xc0010002
   12b34:	e9 d1 fc ff ff       	jmp    0x1280a
   12b39:	8d 43 28             	lea    eax,[ebx+0x28]
   12b3c:	50                   	push   eax
   12b3d:	ff 15 78 55 01 00    	call   DWORD PTR ds:0x15578
   12b43:	c6 43 20 00          	mov    BYTE PTR [ebx+0x20],0x0
   12b47:	89 73 08             	mov    DWORD PTR [ebx+0x8],esi
   12b4a:	89 7b 0c             	mov    DWORD PTR [ebx+0xc],edi
   12b4d:	8b 4e 04             	mov    ecx,DWORD PTR [esi+0x4]
   12b50:	89 0b                	mov    DWORD PTR [ebx],ecx
   12b52:	8b 4e 04             	mov    ecx,DWORD PTR [esi+0x4]
   12b55:	8b 41 28             	mov    eax,DWORD PTR [ecx+0x28]
   12b58:	89 43 30             	mov    DWORD PTR [ebx+0x30],eax
   12b5b:	8b 4e 04             	mov    ecx,DWORD PTR [esi+0x4]
   12b5e:	8b 51 2c             	mov    edx,DWORD PTR [ecx+0x2c]
   12b61:	89 53 34             	mov    DWORD PTR [ebx+0x34],edx
   12b64:	8b 55 24             	mov    edx,DWORD PTR [ebp+0x24]
   12b67:	8b 47 24             	mov    eax,DWORD PTR [edi+0x24]
   12b6a:	89 43 38             	mov    DWORD PTR [ebx+0x38],eax
   12b6d:	8b 4f 28             	mov    ecx,DWORD PTR [edi+0x28]
   12b70:	89 4b 3c             	mov    DWORD PTR [ebx+0x3c],ecx
   12b73:	89 53 10             	mov    DWORD PTR [ebx+0x10],edx
   12b76:	8b 47 34             	mov    eax,DWORD PTR [edi+0x34]
   12b79:	89 43 48             	mov    DWORD PTR [ebx+0x48],eax
   12b7c:	8b 4f 38             	mov    ecx,DWORD PTR [edi+0x38]
   12b7f:	89 4b 4c             	mov    DWORD PTR [ebx+0x4c],ecx
   12b82:	b9 e8 57 01 00       	mov    ecx,0x157e8
   12b87:	c7 43 40 62 f5 01 00 	mov    DWORD PTR [ebx+0x40],0x1f562
   12b8e:	c7 43 44 a5 f5 01 00 	mov    DWORD PTR [ebx+0x44],0x1f5a5
   12b95:	ff 15 74 56 01 00    	call   DWORD PTR ds:0x15674
   12b9b:	8b 0d e0 57 01 00    	mov    ecx,DWORD PTR ds:0x157e0
   12ba1:	a2 ec 57 01 00       	mov    ds:0x157ec,al
   12ba6:	89 4b 50             	mov    DWORD PTR [ebx+0x50],ecx
   12ba9:	b9 e8 57 01 00       	mov    ecx,0x157e8
   12bae:	a0 ec 57 01 00       	mov    al,ds:0x157ec
   12bb3:	89 1d e0 57 01 00    	mov    DWORD PTR ds:0x157e0,ebx
   12bb9:	8b d0                	mov    edx,eax
   12bbb:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12bc1:	8b 4d f0             	mov    ecx,DWORD PTR [ebp-0x10]
   12bc4:	8b 55 10             	mov    edx,DWORD PTR [ebp+0x10]
   12bc7:	ff 75 30             	push   DWORD PTR [ebp+0x30]
   12bca:	89 4b 1c             	mov    DWORD PTR [ebx+0x1c],ecx
   12bcd:	8b 4d f0             	mov    ecx,DWORD PTR [ebp-0x10]
   12bd0:	ff 75 2c             	push   DWORD PTR [ebp+0x2c]
   12bd3:	89 59 0c             	mov    DWORD PTR [ecx+0xc],ebx
   12bd6:	8d 4b 04             	lea    ecx,[ebx+0x4]
   12bd9:	89 1a                	mov    DWORD PTR [edx],ebx
   12bdb:	ff 76 08             	push   DWORD PTR [esi+0x8]
   12bde:	53                   	push   ebx
   12bdf:	ff 75 1c             	push   DWORD PTR [ebp+0x1c]
   12be2:	ff 75 18             	push   DWORD PTR [ebp+0x18]
   12be5:	ff 75 14             	push   DWORD PTR [ebp+0x14]
   12be8:	51                   	push   ecx
   12be9:	8b 46 04             	mov    eax,DWORD PTR [esi+0x4]
   12bec:	ff 75 0c             	push   DWORD PTR [ebp+0xc]
   12bef:	ff 50 20             	call   DWORD PTR [eax+0x20]
   12bf2:	89 45 fc             	mov    DWORD PTR [ebp-0x4],eax
   12bf5:	85 c0                	test   eax,eax
   12bf7:	75 18                	jne    0x12c11
   12bf9:	53                   	push   ebx
   12bfa:	e8 ac cb 00 00       	call   0x1f7ab
   12bff:	84 c0                	test   al,al
   12c01:	74 0e                	je     0x12c11
   12c03:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12c06:	c7 01 00 00 00 00    	mov    DWORD PTR [ecx],0x0
   12c0c:	e9 f9 fb ff ff       	jmp    0x1280a
   12c11:	81 7d fc 03 01 00 00 	cmp    DWORD PTR [ebp-0x4],0x103
   12c18:	75 0e                	jne    0x12c28
   12c1a:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12c1d:	c7 01 03 01 00 00    	mov    DWORD PTR [ecx],0x103
   12c23:	e9 e2 fb ff ff       	jmp    0x1280a
   12c28:	b9 e8 57 01 00       	mov    ecx,0x157e8
   12c2d:	ff 15 74 56 01 00    	call   DWORD PTR ds:0x15674
   12c33:	a2 ec 57 01 00       	mov    ds:0x157ec,al
   12c38:	3b 1d e0 57 01 00    	cmp    ebx,DWORD PTR ds:0x157e0
   12c3e:	75 51                	jne    0x12c91
   12c40:	8b 43 50             	mov    eax,DWORD PTR [ebx+0x50]
   12c43:	a3 e0 57 01 00       	mov    ds:0x157e0,eax
   12c48:	8a 15 ec 57 01 00    	mov    dl,BYTE PTR ds:0x157ec
   12c4e:	b9 e8 57 01 00       	mov    ecx,0x157e8
   12c53:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12c59:	8b 4d f0             	mov    ecx,DWORD PTR [ebp-0x10]
   12c5c:	ff 15 8c 55 01 00    	call   DWORD PTR ds:0x1558c
   12c62:	56                   	push   esi
   12c63:	e8 d3 cf 00 00       	call   0x1fc3b
   12c68:	8d 47 04             	lea    eax,[edi+0x4]
   12c6b:	50                   	push   eax
   12c6c:	e8 fe 1a 00 00       	call   0x1476f
   12c71:	84 c0                	test   al,al
   12c73:	74 07                	je     0x12c7c
   12c75:	57                   	push   edi
   12c76:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   12c7c:	53                   	push   ebx
   12c7d:	ff 15 88 55 01 00    	call   DWORD PTR ds:0x15588
   12c83:	8b 4d 08             	mov    ecx,DWORD PTR [ebp+0x8]
   12c86:	c7 01 07 00 01 c0    	mov    DWORD PTR [ecx],0xc0010007
   12c8c:	e9 79 fb ff ff       	jmp    0x1280a
   12c91:	a1 e0 57 01 00       	mov    eax,ds:0x157e0
   12c96:	39 58 50             	cmp    DWORD PTR [eax+0x50],ebx
   12c99:	74 08                	je     0x12ca3
   12c9b:	8b 40 50             	mov    eax,DWORD PTR [eax+0x50]
   12c9e:	39 58 50             	cmp    DWORD PTR [eax+0x50],ebx
   12ca1:	75 f8                	jne    0x12c9b
   12ca3:	8b 4b 50             	mov    ecx,DWORD PTR [ebx+0x50]
   12ca6:	89 48 50             	mov    DWORD PTR [eax+0x50],ecx
   12ca9:	eb 9d                	jmp    0x12c48
   12cab:	c2 04 00             	ret    0x4
   12cae:	68 b8 58 01 00       	push   0x158b8
   12cb3:	ff 15 78 55 01 00    	call   DWORD PTR ds:0x15578
   12cb9:	6a 00                	push   0x0
   12cbb:	6a 00                	push   0x0
   12cbd:	68 c0 58 01 00       	push   0x158c0
   12cc2:	ff 15 68 55 01 00    	call   DWORD PTR ds:0x15568
   12cc8:	c3                   	ret
   12cc9:	56                   	push   esi
   12cca:	8b 74 24 14          	mov    esi,DWORD PTR [esp+0x14]
   12cce:	83 3e 00             	cmp    DWORD PTR [esi],0x0
   12cd1:	75 32                	jne    0x12d05
   12cd3:	8b 86 30 01 00 00    	mov    eax,DWORD PTR [esi+0x130]
   12cd9:	8b 8e 14 01 00 00    	mov    ecx,DWORD PTR [esi+0x114]
   12cdf:	8b 54 24 10          	mov    edx,DWORD PTR [esp+0x10]
   12ce3:	89 14 c1             	mov    DWORD PTR [ecx+eax*8],edx
   12ce6:	83 3e 00             	cmp    DWORD PTR [esi],0x0
   12ce9:	75 22                	jne    0x12d0d
   12ceb:	81 c6 20 01 00 00    	add    esi,0x120
   12cf1:	6a 00                	push   0x0
   12cf3:	6a 00                	push   0x0
   12cf5:	56                   	push   esi
   12cf6:	ff 15 74 55 01 00    	call   DWORD PTR ds:0x15574
   12cfc:	b8 03 00 00 00       	mov    eax,0x3
   12d01:	5e                   	pop    esi
   12d02:	c2 10 00             	ret    0x10
   12d05:	8b 46 7c             	mov    eax,DWORD PTR [esi+0x7c]
   12d08:	8b 4e 68             	mov    ecx,DWORD PTR [esi+0x68]
   12d0b:	eb d2                	jmp    0x12cdf
   12d0d:	83 c6 6c             	add    esi,0x6c
   12d10:	eb df                	jmp    0x12cf1
   12d12:	b9 f8 57 01 00       	mov    ecx,0x157f8
   12d17:	ff 15 74 56 01 00    	call   DWORD PTR ds:0x15674
   12d1d:	a2 fc 57 01 00       	mov    ds:0x157fc,al
   12d22:	a1 10 58 01 00       	mov    eax,ds:0x15810
   12d27:	40                   	inc    eax
   12d28:	a3 10 58 01 00       	mov    ds:0x15810,eax
   12d2d:	83 f8 01             	cmp    eax,0x1
   12d30:	75 3c                	jne    0x12d6e
   12d32:	68 00 58 01 00       	push   0x15800
   12d37:	ff 15 7c 55 01 00    	call   DWORD PTR ds:0x1557c
   12d3d:	8a 15 fc 57 01 00    	mov    dl,BYTE PTR ds:0x157fc
   12d43:	b9 f8 57 01 00       	mov    ecx,0x157f8
   12d48:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12d4e:	68 00 f4 01 00       	push   0x1f400
   12d53:	ff 15 84 55 01 00    	call   DWORD PTR ds:0x15584
   12d59:	6a 00                	push   0x0
   12d5b:	a3 14 58 01 00       	mov    ds:0x15814,eax
   12d60:	6a 00                	push   0x0
   12d62:	68 00 58 01 00       	push   0x15800
   12d67:	ff 15 74 55 01 00    	call   DWORD PTR ds:0x15574
   12d6d:	c3                   	ret
   12d6e:	8a 15 fc 57 01 00    	mov    dl,BYTE PTR ds:0x157fc
   12d74:	b9 f8 57 01 00       	mov    ecx,0x157f8
   12d79:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12d7f:	6a 00                	push   0x0
   12d81:	6a 01                	push   0x1
   12d83:	6a 00                	push   0x0
   12d85:	6a 00                	push   0x0
   12d87:	68 00 58 01 00       	push   0x15800
   12d8c:	ff 15 70 55 01 00    	call   DWORD PTR ds:0x15570
   12d92:	eb d9                	jmp    0x12d6d
   12d94:	68 70 57 01 00       	push   0x15770
   12d99:	ff 15 78 55 01 00    	call   DWORD PTR ds:0x15578
   12d9f:	6a 00                	push   0x0
   12da1:	6a 00                	push   0x0
   12da3:	68 78 57 01 00       	push   0x15778
   12da8:	ff 15 68 55 01 00    	call   DWORD PTR ds:0x15568
   12dae:	c3                   	ret
   12daf:	56                   	push   esi
   12db0:	8b 74 24 08          	mov    esi,DWORD PTR [esp+0x8]
   12db4:	8b 46 04             	mov    eax,DWORD PTR [esi+0x4]
   12db7:	85 c0                	test   eax,eax
   12db9:	74 0f                	je     0x12dca
   12dbb:	6a 00                	push   0x0
   12dbd:	8b 4e 20             	mov    ecx,DWORD PTR [esi+0x20]
   12dc0:	6b c9 0c             	imul   ecx,ecx,0xc
   12dc3:	51                   	push   ecx
   12dc4:	50                   	push   eax
   12dc5:	e8 10 f5 ff ff       	call   0x122da
   12dca:	8b 46 08             	mov    eax,DWORD PTR [esi+0x8]
   12dcd:	85 c0                	test   eax,eax
   12dcf:	74 0f                	je     0x12de0
   12dd1:	6a 00                	push   0x0
   12dd3:	8b 4e 20             	mov    ecx,DWORD PTR [esi+0x20]
   12dd6:	c1 e1 03             	shl    ecx,0x3
   12dd9:	51                   	push   ecx
   12dda:	50                   	push   eax
   12ddb:	e8 fa f4 ff ff       	call   0x122da
   12de0:	6a 00                	push   0x0
   12de2:	6a 34                	push   0x34
   12de4:	56                   	push   esi
   12de5:	e8 f0 f4 ff ff       	call   0x122da
   12dea:	e8 a3 ea ff ff       	call   0x11892
   12def:	5e                   	pop    esi
   12df0:	c2 04 00             	ret    0x4
   12df3:	56                   	push   esi
   12df4:	8b 74 24 08          	mov    esi,DWORD PTR [esp+0x8]
   12df8:	8b ce                	mov    ecx,esi
   12dfa:	ff 15 74 56 01 00    	call   DWORD PTR ds:0x15674
   12e00:	80 7e 0a 00          	cmp    BYTE PTR [esi+0xa],0x0
   12e04:	88 46 04             	mov    BYTE PTR [esi+0x4],al
   12e07:	74 10                	je     0x12e19
   12e09:	8a d0                	mov    dl,al
   12e0b:	8b ce                	mov    ecx,esi
   12e0d:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12e13:	32 c0                	xor    al,al
   12e15:	5e                   	pop    esi
   12e16:	c2 04 00             	ret    0x4
   12e19:	66 ff 46 08          	inc    WORD PTR [esi+0x8]
   12e1d:	8a 56 04             	mov    dl,BYTE PTR [esi+0x4]
   12e20:	8b ce                	mov    ecx,esi
   12e22:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12e28:	b0 01                	mov    al,0x1
   12e2a:	eb e9                	jmp    0x12e15
   12e2c:	8b 44 24 04          	mov    eax,DWORD PTR [esp+0x4]
   12e30:	66 c7 40 08 01 00    	mov    WORD PTR [eax+0x8],0x1
   12e36:	c6 40 0a 00          	mov    BYTE PTR [eax+0xa],0x0
   12e3a:	50                   	push   eax
   12e3b:	ff 15 78 55 01 00    	call   DWORD PTR ds:0x15578
   12e41:	c2 04 00             	ret    0x4
   12e44:	ff 25 34 55 01 00    	jmp    DWORD PTR ds:0x15534
   12e4a:	56                   	push   esi
   12e4b:	57                   	push   edi
   12e4c:	8b 74 24 10          	mov    esi,DWORD PTR [esp+0x10]
   12e50:	8d 7e 04             	lea    edi,[esi+0x4]
   12e53:	8b cf                	mov    ecx,edi
   12e55:	ff 15 74 56 01 00    	call   DWORD PTR ds:0x15674
   12e5b:	80 7e 0e 00          	cmp    BYTE PTR [esi+0xe],0x0
   12e5f:	88 46 08             	mov    BYTE PTR [esi+0x8],al
   12e62:	74 11                	je     0x12e75
   12e64:	8a d0                	mov    dl,al
   12e66:	8b cf                	mov    ecx,edi
   12e68:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12e6e:	32 c0                	xor    al,al
   12e70:	5f                   	pop    edi
   12e71:	5e                   	pop    esi
   12e72:	c2 08 00             	ret    0x8
   12e75:	8b 0e                	mov    ecx,DWORD PTR [esi]
   12e77:	8b 44 24 0c          	mov    eax,DWORD PTR [esp+0xc]
   12e7b:	89 48 18             	mov    DWORD PTR [eax+0x18],ecx
   12e7e:	8b cf                	mov    ecx,edi
   12e80:	8a 56 08             	mov    dl,BYTE PTR [esi+0x8]
   12e83:	89 06                	mov    DWORD PTR [esi],eax
   12e85:	ff 15 6c 56 01 00    	call   DWORD PTR ds:0x1566c
   12e8b:	b0 01                	mov    al,0x1
   12e8d:	eb e1                	jmp    0x12e70
   12e8f:	8b 44 24 04          	mov    eax,DWORD PTR [esp+0x4]
   12e93:	83 ec 08             	sub    esp,0x8
   12e96:	53                   	push   ebx
   12e97:	8b 40 28             	mov    eax,DWORD PTR [eax+0x28]
   12e9a:	56                   	push   esi
   12e9b:	83 c0 50             	add    eax,0x50
   12e9e:	89 44 24 0c          	mov    DWORD PTR [esp+0xc],eax
   12ea2:	57                   	push   edi
   12ea3:	55                   	push   ebp
   12ea4:	33 ff                	xor    edi,edi
   12ea6:	8b 74 24 20          	mov    esi,DWORD PTR [esp+0x20]
   12eaa:	83 38 01             	cmp    DWORD PTR [eax],0x1
   12ead:	1a c0                	sbb    al,al
   12eaf:	8b 5e 60             	mov    ebx,DWORD PTR [esi+0x60]
   12eb2:	f6 d8                	neg    al
   12eb4:	8b 6e 0c             	mov    ebp,DWORD PTR [esi+0xc]
   12eb7:	88 44 24 13          	mov    BYTE PTR [esp+0x13],al
   12ebb:	85 ed                	test   ebp,ebp
   12ebd:	75 7e                	jne    0x12f3d
   12ebf:	68 4e 44 20 20       	push   0x2020444e
   12ec4:	6a 10                	push   0x10
   12ec6:	57                   	push   edi
   12ec7:	ff 15 94 55 01 00    	call   DWORD PTR ds:0x15594
   12ecd:	8b e8                	mov    ebp,eax
   12ecf:	85 ed                	test   ebp,ebp
   12ed1:	75 21                	jne    0x12ef4
   12ed3:	bf 9a 00 00 c0       	mov    edi,0xc000009a
   12ed8:	ba 02 00 00 00       	mov    edx,0x2
   12edd:	8b ce                	mov    ecx,esi
   12edf:	89 7e 18             	mov    DWORD PTR [esi+0x18],edi
   12ee2:	ff 15 a0 55 01 00    	call   DWORD PTR ds:0x155a0
   12ee8:	8b c7                	mov    eax,edi
   12eea:	5d                   	pop    ebp
   12eeb:	5f                   	pop    edi
   12eec:	5e                   	pop    esi
   12eed:	5b                   	pop    ebx
   12eee:	83 c4 08             	add    esp,0x8
   12ef1:	c2 08 00             	ret    0x8
   12ef4:	8b 44 24 1c          	mov    eax,DWORD PTR [esp+0x1c]
   12ef8:	8b 4c 24 14          	mov    ecx,DWORD PTR [esp+0x14]
   12efc:	89 45 00             	mov    DWORD PTR [ebp+0x0],eax
   12eff:	33                   	.byte 0x33
