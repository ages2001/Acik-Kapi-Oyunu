
/workspace/rltklant-vm/install/xp64-kernel/ndis.sys:     file format pei-x86-64


Disassembly of section PAGENPNP:

0000000000038b30 <PAGENPNP+0x7b30>:
   38b30:	48 83 ec 28          	sub    $0x28,%rsp
   38b34:	4c 8d 4c 24 48       	lea    0x48(%rsp),%r9
   38b39:	e8 12 00 00 00       	call   0x38b50
   38b3e:	48 83 c4 28          	add    $0x28,%rsp
   38b42:	c3                   	ret
   38b43:	90                   	nop
   38b44:	90                   	nop
   38b45:	90                   	nop
   38b46:	90                   	nop
   38b47:	90                   	nop
   38b48:	90                   	nop
   38b49:	90                   	nop
   38b4a:	90                   	nop
   38b4b:	90                   	nop
   38b4c:	90                   	nop
   38b4d:	90                   	nop
   38b4e:	90                   	nop
   38b4f:	90                   	nop
   38b50:	4c 8b dc             	mov    %rsp,%r11
   38b53:	48 83 ec 38          	sub    $0x38,%rsp
   38b57:	48 85 c9             	test   %rcx,%rcx
   38b5a:	49 89 5b 08          	mov    %rbx,0x8(%r11)
   38b5e:	49 89 6b 10          	mov    %rbp,0x10(%r11)
   38b62:	49 89 7b 20          	mov    %rdi,0x20(%r11)
   38b66:	49 8b e9             	mov    %r9,%rbp
   38b69:	48 8b da             	mov    %rdx,%rbx
   38b6c:	48 8b f9             	mov    %rcx,%rdi
   38b6f:	49 c7 43 e8 00 00 00 	movq   $0x0,-0x18(%r11)
   38b76:	00 
   38b77:	75 16                	jne    0x38b8f
   38b79:	b8 01 00 00 c0       	mov    $0xc0000001,%eax
   38b7e:	49 8b 7b 20          	mov    0x20(%r11),%rdi
   38b82:	49 8b 6b 10          	mov    0x10(%r11),%rbp
   38b86:	49 8b 5b 08          	mov    0x8(%r11),%rbx
   38b8a:	48 83 c4 38          	add    $0x38,%rsp
   38b8e:	c3                   	ret
   38b8f:	0f b6 52 01          	movzbl 0x1(%rdx),%edx
   38b93:	48 89 74 24 50       	mov    %rsi,0x50(%rsp)
   38b98:	84 d2                	test   %dl,%dl
   38b9a:	0f 85 12 09 00 00    	jne    0x394b2
   38ba0:	0f b6 0b             	movzbl (%rbx),%ecx
   38ba3:	80 f9 03             	cmp    $0x3,%cl
   38ba6:	0f 84 97 90 00 00    	je     0x41c43
   38bac:	80 f9 04             	cmp    $0x4,%cl
   38baf:	0f 84 17 10 00 00    	je     0x39bcc
   38bb5:	80 f9 05             	cmp    $0x5,%cl
   38bb8:	0f 85 8e 90 00 00    	jne    0x41c4c
   38bbe:	66 be b8 00          	mov    $0xb8,%si
   38bc2:	eb 00                	jmp    0x38bc4
   38bc4:	0f b7 c6             	movzwl %si,%eax
   38bc7:	44 3b c0             	cmp    %eax,%r8d
   38bca:	0f 82 fa 01 00 00    	jb     0x38dca
   38bd0:	80 f9 05             	cmp    $0x5,%cl
   38bd3:	75 21                	jne    0x38bf6
   38bd5:	48 83 bb a8 00 00 00 	cmpq   $0x0,0xa8(%rbx)
   38bdc:	00 
   38bdd:	74 0e                	je     0x38bed
   38bdf:	48 83 bb b0 00 00 00 	cmpq   $0x0,0xb0(%rbx)
   38be6:	00 
   38be7:	0f 84 dd 01 00 00    	je     0x38dca
   38bed:	80 fa 01             	cmp    $0x1,%dl
   38bf0:	0f 83 c6 01 00 00    	jae    0x38dbc
   38bf6:	48 8b 0f             	mov    (%rdi),%rcx
   38bf9:	4c 8d 4c 24 20       	lea    0x20(%rsp),%r9
   38bfe:	ba 44 49 4d 4e       	mov    $0x4e4d4944,%edx
   38c03:	41 b8 a0 01 00 00    	mov    $0x1a0,%r8d
   38c09:	ff 15 31 17 fe ff    	call   *-0x1e8cf(%rip)        # 0x1a340
   38c0f:	85 c0                	test   %eax,%eax
   38c11:	0f 88 3f 90 00 00    	js     0x41c56
   38c17:	48 8b 4c 24 20       	mov    0x20(%rsp),%rcx
   38c1c:	33 d2                	xor    %edx,%edx
   38c1e:	41 b8 a0 01 00 00    	mov    $0x1a0,%r8d
   38c24:	e8 b7 84 fd ff       	call   0x110e0
   38c29:	48 8b 4c 24 20       	mov    0x20(%rsp),%rcx
   38c2e:	44 0f b7 c6          	movzwl %si,%r8d
   38c32:	48 83 c1 40          	add    $0x40,%rcx
   38c36:	48 8b d3             	mov    %rbx,%rdx
   38c39:	e8 c2 8b fd ff       	call   0x11800
   38c3e:	48 8b 0f             	mov    (%rdi),%rcx
   38c41:	ff 15 01 17 fe ff    	call   *-0x1e8ff(%rip)        # 0x1a348
   38c47:	85 c0                	test   %eax,%eax
   38c49:	0f 85 11 90 00 00    	jne    0x41c60
   38c4f:	48 8b 4c 24 20       	mov    0x20(%rsp),%rcx
   38c54:	48 c7 41 08 00 00 00 	movq   $0x0,0x8(%rcx)
   38c5b:	00 
   38c5c:	b9 70 00 00 00       	mov    $0x70,%ecx
   38c61:	4c 8d 05 18 8e ff ff 	lea    -0x71e8(%rip),%r8        # 0x31a80
   38c68:	8d 51 ac             	lea    -0x54(%rcx),%edx
   38c6b:	66 90                	xchg   %ax,%ax
   38c6d:	48 8b 07             	mov    (%rdi),%rax
   38c70:	48 83 c1 08          	add    $0x8,%rcx
   38c74:	48 ff ca             	dec    %rdx
   38c77:	4c 89 44 01 f8       	mov    %r8,-0x8(%rcx,%rax,1)
   38c7c:	75 ef                	jne    0x38c6d
   38c7e:	48 8b 07             	mov    (%rdi),%rax
   38c81:	45 33 c0             	xor    %r8d,%r8d
   38c84:	48 8b 48 30          	mov    0x30(%rax),%rcx
   38c88:	48 8d 05 61 01 00 00 	lea    0x161(%rip),%rax        # 0x38df0
   38c8f:	48 89 41 08          	mov    %rax,0x8(%rcx)
   38c93:	48 8b 07             	mov    (%rdi),%rax
   38c96:	48 8d 0d e3 68 00 00 	lea    0x68e3(%rip),%rcx        # 0x3f580
   38c9d:	48 89 48 68          	mov    %rcx,0x68(%rax)
   38ca1:	48 8b 07             	mov    (%rdi),%rax
   38ca4:	48 8d 0d 65 99 fd ff 	lea    -0x2669b(%rip),%rcx        # 0x12610
   38cab:	48 89 48 70          	mov    %rcx,0x70(%rax)
   38caf:	48 8b 07             	mov    (%rdi),%rax
   38cb2:	48 8d 0d 47 8b ff ff 	lea    -0x74b9(%rip),%rcx        # 0x31800
   38cb9:	48 89 88 e0 00 00 00 	mov    %rcx,0xe0(%rax)
   38cc0:	48 8b 07             	mov    (%rdi),%rax
   38cc3:	48 8d 0d 16 98 fd ff 	lea    -0x267ea(%rip),%rcx        # 0x124e0
   38cca:	48 89 88 80 00 00 00 	mov    %rcx,0x80(%rax)
   38cd1:	48 8b 07             	mov    (%rdi),%rax
   38cd4:	48 8d 0d 95 8f ff ff 	lea    -0x706b(%rip),%rcx        # 0x31c70
   38cdb:	48 89 88 48 01 00 00 	mov    %rcx,0x148(%rax)
   38ce2:	48 8b 07             	mov    (%rdi),%rax
   38ce5:	48 8d 0d d4 c1 00 00 	lea    0xc1d4(%rip),%rcx        # 0x44ec0
   38cec:	48 89 88 20 01 00 00 	mov    %rcx,0x120(%rax)
   38cf3:	48 8b 07             	mov    (%rdi),%rax
   38cf6:	48 8d 0d a3 95 ff ff 	lea    -0x6a5d(%rip),%rcx        # 0x322a0
   38cfd:	48 89 88 28 01 00 00 	mov    %rcx,0x128(%rax)
   38d04:	48 8b 4c 24 20       	mov    0x20(%rsp),%rcx
   38d09:	48 81 c1 30 01 00 00 	add    $0x130,%rcx
   38d10:	ff 15 d2 13 fe ff    	call   *-0x1ec2e(%rip)        # 0x1a0e8
   38d16:	4c 8b 5c 24 20       	mov    0x20(%rsp),%r11
   38d1b:	48 8d 0d be d6 fe ff 	lea    -0x12942(%rip),%rcx        # 0x263e0
   38d22:	49 89 7b 10          	mov    %rdi,0x10(%r11)
   38d26:	48 8b 44 24 20       	mov    0x20(%rsp),%rax
   38d2b:	48 83 c0 20          	add    $0x20,%rax
   38d2f:	48 89 40 08          	mov    %rax,0x8(%rax)
   38d33:	48 89 00             	mov    %rax,(%rax)
   38d36:	48 8b 44 24 20       	mov    0x20(%rsp),%rax
   38d3b:	c6 80 52 01 00 00 00 	movb   $0x0,0x152(%rax)
   38d42:	66 c7 80 50 01 00 00 	movw   $0x1,0x150(%rax)
   38d49:	01 00 
   38d4b:	48 c7 80 48 01 00 00 	movq   $0x0,0x148(%rax)
   38d52:	00 00 00 00 
   38d56:	e8 a5 96 fd ff       	call   0x12400
   38d5b:	48 8d 0d 8e e6 fe ff 	lea    -0x11972(%rip),%rcx        # 0x273f0
   38d62:	ff 15 60 13 fe ff    	call   *-0x1eca0(%rip)        # 0x1a0c8
   38d68:	48 8b 15 31 d7 fe ff 	mov    -0x128cf(%rip),%rdx        # 0x264a0
   38d6f:	4c 8b 44 24 20       	mov    0x20(%rsp),%r8
   38d74:	48 8b 0d fd d7 fe ff 	mov    -0x12803(%rip),%rcx        # 0x26578
   38d7b:	49 89 10             	mov    %rdx,(%r8)
   38d7e:	48 8b 54 24 20       	mov    0x20(%rsp),%rdx
   38d83:	48 89 15 16 d7 fe ff 	mov    %rdx,-0x128ea(%rip)        # 0x264a0
   38d8a:	0f b6 d8             	movzbl %al,%ebx
   38d8d:	ff 15 fd 12 fe ff    	call   *-0x1ed03(%rip)        # 0x1a090
   38d93:	48 8d 0d 56 e6 fe ff 	lea    -0x119aa(%rip),%rcx        # 0x273f0
   38d9a:	0f b6 d3             	movzbl %bl,%edx
   38d9d:	ff 15 2d 13 fe ff    	call   *-0x1ecd3(%rip)        # 0x1a0d0
   38da3:	48 8d 0d 36 d6 fe ff 	lea    -0x129ca(%rip),%rcx        # 0x263e0
   38daa:	e8 81 96 fd ff       	call   0x12430
   38daf:	4c 8b 5c 24 20       	mov    0x20(%rsp),%r11
   38db4:	33 c0                	xor    %eax,%eax
   38db6:	4c 89 5d 00          	mov    %r11,0x0(%rbp)
   38dba:	eb 15                	jmp    0x38dd1
   38dbc:	48 83 bb c8 00 00 00 	cmpq   $0x0,0xc8(%rbx)
   38dc3:	00 
   38dc4:	0f 85 2c fe ff ff    	jne    0x38bf6
   38dca:	b8 05 00 01 c0       	mov    $0xc0010005,%eax
   38dcf:	eb 00                	jmp    0x38dd1
   38dd1:	48 8b 74 24 50       	mov    0x50(%rsp),%rsi
   38dd6:	48 8b 7c 24 58       	mov    0x58(%rsp),%rdi
   38ddb:	48 8b 6c 24 48       	mov    0x48(%rsp),%rbp
   38de0:	48 8b 5c 24 40       	mov    0x40(%rsp),%rbx
   38de5:	48 83 c4 38          	add    $0x38,%rsp
   38de9:	c3                   	ret
   38dea:	90                   	nop
   38deb:	90                   	nop
   38dec:	90                   	nop
   38ded:	90                   	nop
   38dee:	90                   	nop
   38def:	90                   	nop
   38df0:	48 8b c4             	mov    %rsp,%rax
   38df3:	48 81 ec 18 01 00 00 	sub    $0x118,%rsp
   38dfa:	48 89 58 08          	mov    %rbx,0x8(%rax)
   38dfe:	48 89 68 10          	mov    %rbp,0x10(%rax)
   38e02:	33 db                	xor    %ebx,%ebx
   38e04:	48 89 70 18          	mov    %rsi,0x18(%rax)
   38e08:	48 8b f2             	mov    %rdx,%rsi
   38e0b:	48 89 78 20          	mov    %rdi,0x20(%rax)
   38e0f:	48 8b e9             	mov    %rcx,%rbp
   38e12:	8d 53 02             	lea    0x2(%rbx),%edx
   38e15:	4c 8d 4c 24 38       	lea    0x38(%rsp),%r9
   38e1a:	41 b8 00 00 00 82    	mov    $0x82000000,%r8d
   38e20:	48 8b ce             	mov    %rsi,%rcx
   38e23:	4c 89 60 f8          	mov    %r12,-0x8(%rax)
   38e27:	48 89 5c 24 30       	mov    %rbx,0x30(%rsp)
   38e2c:	48 89 5c 24 38       	mov    %rbx,0x38(%rsp)
   38e31:	48 89 5c 24 40       	mov    %rbx,0x40(%rsp)
   38e36:	bf 01 00 00 c0       	mov    $0xc0000001,%edi
   38e3b:	ff 15 f7 12 fe ff    	call   *-0x1ed09(%rip)        # 0x1a138
   38e41:	85 c0                	test   %eax,%eax
   38e43:	0f 88 de 99 00 00    	js     0x42827
   38e49:	48 8b 54 24 38       	mov    0x38(%rsp),%rdx
   38e4e:	48 8d 05 eb 1c 00 00 	lea    0x1ceb(%rip),%rax        # 0x3ab40
   38e55:	4c 8d 25 54 98 ff ff 	lea    -0x67ac(%rip),%r12        # 0x326b0
   38e5c:	48 89 44 24 70       	mov    %rax,0x70(%rsp)
   38e61:	48 8d 05 e8 1c 00 00 	lea    0x1ce8(%rip),%rax        # 0x3ab50
   38e68:	4c 8d 44 24 60       	lea    0x60(%rsp),%r8
   38e6d:	48 89 84 24 a8 00 00 	mov    %rax,0xa8(%rsp)
   38e74:	00 
   38e75:	48 8d 44 24 30       	lea    0x30(%rsp),%rax
   38e7a:	45 33 c9             	xor    %r9d,%r9d
   38e7d:	b9 00 00 00 40       	mov    $0x40000000,%ecx
   38e82:	48 89 5c 24 60       	mov    %rbx,0x60(%rsp)
   38e87:	c7 44 24 68 01 00 00 	movl   $0x1,0x68(%rsp)
   38e8e:	00 
   38e8f:	48 89 84 24 b0 00 00 	mov    %rax,0xb0(%rsp)
   38e96:	00 
   38e97:	4c 89 a4 24 98 00 00 	mov    %r12,0x98(%rsp)
   38e9e:	00 
   38e9f:	c7 84 24 a0 00 00 00 	movl   $0x14,0xa0(%rsp)
   38ea6:	14 00 00 00 
   38eaa:	89 9c 24 b8 00 00 00 	mov    %ebx,0xb8(%rsp)
   38eb1:	48 89 9c 24 d0 00 00 	mov    %rbx,0xd0(%rsp)
   38eb8:	00 
   38eb9:	89 9c 24 d8 00 00 00 	mov    %ebx,0xd8(%rsp)
   38ec0:	48 89 9c 24 e0 00 00 	mov    %rbx,0xe0(%rsp)
   38ec7:	00 
   38ec8:	48 89 5c 24 20       	mov    %rbx,0x20(%rsp)
   38ecd:	ff 15 a5 11 fe ff    	call   *-0x1ee5b(%rip)        # 0x1a078
   38ed3:	85 c0                	test   %eax,%eax
   38ed5:	0f 88 4c 99 00 00    	js     0x42827
   38edb:	48 8b 4c 24 30       	mov    0x30(%rsp),%rcx
   38ee0:	48 85 c9             	test   %rcx,%rcx
   38ee3:	0f 84 97 00 00 00    	je     0x38f80
   38ee9:	48 8b d1             	mov    %rcx,%rdx
   38eec:	48 8d 4c 24 50       	lea    0x50(%rsp),%rcx
   38ef1:	ff 15 41 16 fe ff    	call   *-0x1e9bf(%rip)        # 0x1a538
   38ef7:	48 8b 54 24 38       	mov    0x38(%rsp),%rdx
   38efc:	48 8d 05 ed 1f 00 00 	lea    0x1fed(%rip),%rax        # 0x3aef0
   38f03:	48 89 44 24 70       	mov    %rax,0x70(%rsp)
   38f08:	48 8d 44 24 40       	lea    0x40(%rsp),%rax
   38f0d:	4c 8d 4c 24 48       	lea    0x48(%rsp),%r9
   38f12:	4c 8d 44 24 60       	lea    0x60(%rsp),%r8
   38f17:	b9 00 00 00 40       	mov    $0x40000000,%ecx
   38f1c:	4c 89 64 24 60       	mov    %r12,0x60(%rsp)
   38f21:	48 89 44 24 78       	mov    %rax,0x78(%rsp)
   38f26:	c7 44 24 68 10 00 00 	movl   $0x10,0x68(%rsp)
   38f2d:	00 
   38f2e:	89 9c 24 80 00 00 00 	mov    %ebx,0x80(%rsp)
   38f35:	48 89 9c 24 98 00 00 	mov    %rbx,0x98(%rsp)
   38f3c:	00 
   38f3d:	89 9c 24 a0 00 00 00 	mov    %ebx,0xa0(%rsp)
   38f44:	48 89 9c 24 a8 00 00 	mov    %rbx,0xa8(%rsp)
   38f4b:	00 
   38f4c:	48 89 5c 24 20       	mov    %rbx,0x20(%rsp)
   38f51:	ff 15 21 11 fe ff    	call   *-0x1eedf(%rip)        # 0x1a078
   38f57:	4c 8b 5c 24 40       	mov    0x40(%rsp),%r11
   38f5c:	4d 85 db             	test   %r11,%r11
   38f5f:	74 03                	je     0x38f64
   38f61:	41 8b 1b             	mov    (%r11),%ebx
   38f64:	48 8d 54 24 50       	lea    0x50(%rsp),%rdx
   38f69:	44 8b cb             	mov    %ebx,%r9d
   38f6c:	4c 8b c6             	mov    %rsi,%r8
   38f6f:	48 8b cd             	mov    %rbp,%rcx
   38f72:	e8 79 00 00 00       	call   0x38ff0
   38f77:	48 8b 4c 24 30       	mov    0x30(%rsp),%rcx
   38f7c:	8b f8                	mov    %eax,%edi
   38f7e:	eb 00                	jmp    0x38f80
   38f80:	48 8b 44 24 38       	mov    0x38(%rsp),%rax
   38f85:	4c 8b a4 24 10 01 00 	mov    0x110(%rsp),%r12
   38f8c:	00 
   38f8d:	48 8b b4 24 30 01 00 	mov    0x130(%rsp),%rsi
   38f94:	00 
   38f95:	48 85 c0             	test   %rax,%rax
   38f98:	48 8b ac 24 28 01 00 	mov    0x128(%rsp),%rbp
   38f9f:	00 
   38fa0:	48 8b 9c 24 20 01 00 	mov    0x120(%rsp),%rbx
   38fa7:	00 
   38fa8:	74 0e                	je     0x38fb8
   38faa:	48 8b c8             	mov    %rax,%rcx
   38fad:	ff 15 9d 11 fe ff    	call   *-0x1ee63(%rip)        # 0x1a150
   38fb3:	48 8b 4c 24 30       	mov    0x30(%rsp),%rcx
   38fb8:	48 85 c9             	test   %rcx,%rcx
   38fbb:	74 08                	je     0x38fc5
   38fbd:	33 d2                	xor    %edx,%edx
   38fbf:	ff 15 c3 10 fe ff    	call   *-0x1ef3d(%rip)        # 0x1a088
   38fc5:	48 8b 4c 24 40       	mov    0x40(%rsp),%rcx
   38fca:	48 85 c9             	test   %rcx,%rcx
   38fcd:	74 08                	je     0x38fd7
   38fcf:	33 d2                	xor    %edx,%edx
   38fd1:	ff 15 b1 10 fe ff    	call   *-0x1ef4f(%rip)        # 0x1a088
   38fd7:	8b c7                	mov    %edi,%eax
   38fd9:	48 8b bc 24 38 01 00 	mov    0x138(%rsp),%rdi
   38fe0:	00 
   38fe1:	48 81 c4 18 01 00 00 	add    $0x118,%rsp
   38fe8:	c3                   	ret
   38fe9:	90                   	nop
   38fea:	90                   	nop
   38feb:	90                   	nop
   38fec:	90                   	nop
   38fed:	90                   	nop
   38fee:	90                   	nop
   38fef:	90                   	nop
   38ff0:	48 8b c4             	mov    %rsp,%rax
   38ff3:	48 81 ec 98 00 00 00 	sub    $0x98,%rsp
   38ffa:	48 89 68 10          	mov    %rbp,0x10(%rax)
   38ffe:	48 89 70 18          	mov    %rsi,0x18(%rax)
   39002:	48 89 78 20          	mov    %rdi,0x20(%rax)
   39006:	4c 89 68 f0          	mov    %r13,-0x10(%rax)
   3900a:	4c 89 70 e8          	mov    %r14,-0x18(%rax)
   3900e:	48 8b f9             	mov    %rcx,%rdi
   39011:	4c 89 78 e0          	mov    %r15,-0x20(%rax)
   39015:	48 8d 0d c4 d3 fe ff 	lea    -0x12c3c(%rip),%rcx        # 0x263e0
   3901c:	45 8b e9             	mov    %r9d,%r13d
   3901f:	4d 8b f0             	mov    %r8,%r14
   39022:	4c 8b fa             	mov    %rdx,%r15
   39025:	be 01 00 00 c0       	mov    $0xc0000001,%esi
   3902a:	48 c7 40 a8 00 00 00 	movq   $0x0,-0x58(%rax)
   39031:	00 
   39032:	e8 c9 93 fd ff       	call   0x12400
   39037:	ba 44 49 4d 4e       	mov    $0x4e4d4944,%edx
   3903c:	48 8b cf             	mov    %rdi,%rcx
   3903f:	ff 15 3b 12 fe ff    	call   *-0x1edc5(%rip)        # 0x1a280
   39045:	48 85 c0             	test   %rax,%rax
   39048:	48 8b e8             	mov    %rax,%rbp
   3904b:	0f 84 89 02 00 00    	je     0x392da
   39051:	48 8d 0d 98 e3 fe ff 	lea    -0x11c68(%rip),%rcx        # 0x273f0
   39058:	48 89 9c 24 a0 00 00 	mov    %rbx,0xa0(%rsp)
   3905f:	00 
   39060:	ff 15 62 10 fe ff    	call   *-0x1ef9e(%rip)        # 0x1a0c8
   39066:	48 8b 1d 33 d4 fe ff 	mov    -0x12bcd(%rip),%rbx        # 0x264a0
   3906d:	48 85 db             	test   %rbx,%rbx
   39070:	74 09                	je     0x3907b
   39072:	48 3b dd             	cmp    %rbp,%rbx
   39075:	0f 85 c8 97 00 00    	jne    0x42843
   3907b:	48 8d 0d 6e e3 fe ff 	lea    -0x11c92(%rip),%rcx        # 0x273f0
   39082:	0f b6 d0             	movzbl %al,%edx
   39085:	ff 15 45 10 fe ff    	call   *-0x1efbb(%rip)        # 0x1a0d0
   3908b:	48 3b dd             	cmp    %rbp,%rbx
   3908e:	0f 85 3e 02 00 00    	jne    0x392d2
   39094:	41 0f b7 1f          	movzwl (%r15),%ebx
   39098:	48 8d 44 24 40       	lea    0x40(%rsp),%rax
   3909d:	41 b9 17 00 00 00    	mov    $0x17,%r9d
   390a3:	48 89 44 24 30       	mov    %rax,0x30(%rsp)
   390a8:	81 c3 8c 09 00 00    	add    $0x98c,%ebx
   390ae:	4d 8b c7             	mov    %r15,%r8
   390b1:	8b d3                	mov    %ebx,%edx
   390b3:	48 8b cf             	mov    %rdi,%rcx
   390b6:	c6 44 24 28 00       	movb   $0x0,0x28(%rsp)
   390bb:	c7 44 24 20 00 01 00 	movl   $0x100,0x20(%rsp)
   390c2:	00 
   390c3:	ff 15 47 0f fe ff    	call   *-0x1f0b9(%rip)        # 0x1a010
   390c9:	85 c0                	test   %eax,%eax
   390cb:	0f 88 01 02 00 00    	js     0x392d2
   390d1:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
   390d6:	49 8b d6             	mov    %r14,%rdx
   390d9:	4c 89 a4 24 90 00 00 	mov    %r12,0x90(%rsp)
   390e0:	00 
   390e1:	81 60 30 7f ff ff ff 	andl   $0xffffff7f,0x30(%rax)
   390e8:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
   390ed:	83 48 30 10          	orl    $0x10,0x30(%rax)
   390f1:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
   390f6:	41 81 66 30 7f ff ff 	andl   $0xffffff7f,0x30(%r14)
   390fd:	ff 
   390fe:	81 48 30 00 20 00 00 	orl    $0x2000,0x30(%rax)
   39105:	48 8b 4c 24 40       	mov    0x40(%rsp),%rcx
   3910a:	ff 15 88 12 fe ff    	call   *-0x1ed78(%rip)        # 0x1a398
   39110:	48 85 c0             	test   %rax,%rax
   39113:	4c 8b e0             	mov    %rax,%r12
   39116:	0f 84 7f 97 00 00    	je     0x4289b
   3911c:	48 8b 4c 24 40       	mov    0x40(%rsp),%rcx
   39121:	4c 63 c3             	movslq %ebx,%r8
   39124:	33 d2                	xor    %edx,%edx
   39126:	48 8b 49 40          	mov    0x40(%rcx),%rcx
   3912a:	e8 b1 7f fd ff       	call   0x110e0
   3912f:	4c 8b 5c 24 40       	mov    0x40(%rsp),%r11
   39134:	49 8b 7b 40          	mov    0x40(%r11),%rdi
   39138:	48 c7 47 78 4e 44 4d 	movq   $0x504d444e,0x78(%rdi)
   3913f:	50 
   39140:	48 83 c7 78          	add    $0x78,%rdi
   39144:	48 8d 8f d0 04 00 00 	lea    0x4d0(%rdi),%rcx
   3914b:	48 89 6f 10          	mov    %rbp,0x10(%rdi)
   3914f:	e8 fc 90 fd ff       	call   0x12250
   39154:	48 8b 47 10          	mov    0x10(%rdi),%rax
   39158:	33 d2                	xor    %edx,%edx
   3915a:	48 89 57 60          	mov    %rdx,0x60(%rdi)
   3915e:	f6 80 58 01 00 00 02 	testb  $0x2,0x158(%rax)
   39165:	0f 85 e9 96 00 00    	jne    0x42854
   3916b:	48 89 bf b0 00 00 00 	mov    %rdi,0xb0(%rdi)
   39172:	89 97 e8 04 00 00    	mov    %edx,0x4e8(%rdi)
   39178:	4c 89 b7 e8 01 00 00 	mov    %r14,0x1e8(%rdi)
   3917f:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
   39184:	4c 89 a7 f0 01 00 00 	mov    %r12,0x1f0(%rdi)
   3918b:	48 89 87 e0 01 00 00 	mov    %rax,0x1e0(%rdi)
   39192:	48 8b 44 24 40       	mov    0x40(%rsp),%rax
   39197:	48 8b 48 40          	mov    0x40(%rax),%rcx
   3919b:	48 8d 87 80 00 00 00 	lea    0x80(%rdi),%rax
   391a2:	48 89 8f b8 00 00 00 	mov    %rcx,0xb8(%rdi)
   391a9:	48 8d 8f 00 09 00 00 	lea    0x900(%rdi),%rcx
   391b0:	48 89 40 08          	mov    %rax,0x8(%rax)
   391b4:	48 89 00             	mov    %rax,(%rax)
   391b7:	e8 74 ad fd ff       	call   0x13f30
   391bc:	41 f6 c5 08          	test   $0x8,%r13b
   391c0:	89 97 08 09 00 00    	mov    %edx,0x908(%rdi)
   391c6:	74 07                	je     0x391cf
   391c8:	81 4f 7c 00 10 00 00 	orl    $0x1000,0x7c(%rdi)
   391cf:	41 83 e5 02          	and    $0x2,%r13d
   391d3:	0f 85 87 96 00 00    	jne    0x42860
   391d9:	48 8d 87 10 09 00 00 	lea    0x910(%rdi),%rax
   391e0:	48 8d 4f 20          	lea    0x20(%rdi),%rcx
   391e4:	45 33 c0             	xor    %r8d,%r8d
   391e7:	48 89 47 28          	mov    %rax,0x28(%rdi)
   391eb:	41 0f b7 07          	movzwl (%r15),%eax
   391ef:	49 8b d7             	mov    %r15,%rdx
   391f2:	66 89 47 20          	mov    %ax,0x20(%rdi)
   391f6:	66 83 c0 02          	add    $0x2,%ax
   391fa:	66 89 47 22          	mov    %ax,0x22(%rdi)
   391fe:	ff 15 ec 0e fe ff    	call   *-0x1f114(%rip)        # 0x1a0f0
   39204:	48 8b 47 28          	mov    0x28(%rdi),%rax
   39208:	44 0f b7 1d 78 d1 fe 	movzwl -0x12e88(%rip),%r11d        # 0x26388
   3920f:	ff 
   39210:	49 8b d6             	mov    %r14,%rdx
   39213:	49 d1 eb             	shr    $1,%r11
   39216:	4a 8d 0c 58          	lea    (%rax,%r11,2),%rcx
   3921a:	48 89 8f 60 01 00 00 	mov    %rcx,0x160(%rdi)
   39221:	0f b7 47 20          	movzwl 0x20(%rdi),%eax
   39225:	48 8d 8f 28 02 00 00 	lea    0x228(%rdi),%rcx
   3922c:	66 2b 05 55 d1 fe ff 	sub    -0x12eab(%rip),%ax        # 0x26388
   39233:	66                   	data16
   39234:	89                   	.byte 0x89
   39235:	87 58 01             	xchg   %ebx,0x1(%rax)
