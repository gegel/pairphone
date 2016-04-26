# PairPhone
The effort for P2P private talk over GSM-FR compressed voice channel

PairPhone is the crossplatform testing software provides p2p speech 
encryption duplexes over GSM compressed voice call. The modern 
cryptography with about 128-bit protection level is use Triple Diffie-
Hellman initial key exchange with elliptic curve 25519 and SHA3 Keccak 
1600/576 Sponge Duplexing based cipher, hash, MAC, SPRNG. Each call's 
setup provide PSF for encryption and ID’s protecting, UKS and KCI 
resistance. Primary authentication using shared password and comparing 
of secret's fingerprint, hidden notification of enforcement are 
available. Devices can be paired during first ‘guest’ call, certificate 
for this contact will be added to the address book.
PairPhone uses special ‘military’ style of cryptographic solutions 
suitable  for low data rate with permanent BER such as error spreading 
resistance, probabilistic flow authentication and short exchanged 
values. 
The goal of the project is specially designed pseudo speech modem with 
small distortion of baseband signal passing over tandem of GSM FR 
codecs uses in real GSM voice communication. Modem achieves 1200 bps 
data rate with less then 1% bit error rate and quick on-the-fly 
synchronizing  after less than 200 ms of streaming. Using the voice 
calls instead CSD avoid the user profiling and prevents the targeted 
collection of metadata.
The app is maked by GCC (*nix) or mingw (Windows) (just follow ‘make’) 
and does not require libraries except libasound2-dev. The binary file 
for Linux and Windows is statically linked, fully portable, can be run 
from removable media or TrueCrypt-container and write to address book 
file in the working folder only during pairing.
Source code have low-level C style and ready to embedding into hardware 
(at least 50 MIPS and floating points arithmetic requires , i.e. Cortex 
F4). The testing application can be run on a PC (1GHz Celeron) and 
requires 2 hardware audio devices (e.g., onboard audio plus installed 
USB audio or bluetooth headset) and uses an analog audio interface to 
connect with GSM link. We used evaluation boards for GSM-modules 
Quectel M66 providing the lock the GSM FR codec (just one that must be 
supported in any cells) and advanced audio settings via the engineering 
menu. If digital audio bus will be used the modem can work at 8KHz 
samples rate instead of 48 kHz thus reduce the computational complexity 
of about 5 times.

Web page: http://torfone.org/pairphone

Docs: http://torfone.org/download/pp1a_doc.pdf

Binaries(Windows, Linux): http://torfone.org/download/pp1a_bin.zip

Contact:  «Van Gegel» <torfone@ukr.net>


