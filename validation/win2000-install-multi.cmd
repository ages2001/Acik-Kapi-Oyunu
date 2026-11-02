@echo off
C:\RLTTEST\devprobe list > C:\RLTTEST\before-multi.txt
C:\RLTTEST\devprobe update C:\RLTTEST\netrtlk.inf "PCI\VEN_10EC&DEV_8169&SUBSYS_11001AF4&REV_10" >> C:\RLTTEST\install-multi.txt
C:\RLTTEST\devprobe update C:\RLTTEST\netrtlk.inf "PCI\VEN_10EC&DEV_8168&SUBSYS_11001AF4&REV_10" >> C:\RLTTEST\install-multi.txt
C:\RLTTEST\devprobe update C:\RLTTEST\netrtlk.inf "PCI\VEN_10EC&DEV_8125&SUBSYS_11001AF4&REV_10" >> C:\RLTTEST\install-multi.txt
C:\RLTTEST\devprobe update C:\RLTTEST\netrtlk.inf "PCI\VEN_10EC&DEV_8126&SUBSYS_11001AF4&REV_10" >> C:\RLTTEST\install-multi.txt
C:\RLTTEST\devprobe update C:\RLTTEST\netrtlk.inf "PCI\VEN_10EC&DEV_8127&SUBSYS_11001AF4&REV_10" >> C:\RLTTEST\install-multi.txt
C:\RLTTEST\devprobe list > C:\RLTTEST\after-multi.txt
