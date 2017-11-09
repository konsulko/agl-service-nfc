# nfc-binding
Binding to handle NFC devices and tags.
It currently use the libnfc, but we would like to support neard.
Unfortunatly, neard seem not mature enough right now.

# Devices and Tags compatibilities

Those tags were tested with both SCL3711 (pn533) and ACS122U (pn532).
ACS122U is currently not working using neard and have also some issues with libnfc.
Expect random error, but seem to stay stable and working.

|Tag type                   |Protocol        |libnfc|neard|
|---------------------------|----------------|------|-----|
|Alien H3 + FM1108          |ISO/IEC 14443A  |yes   |no²  |
|FM1108                     |ISO/IEC 14443A  |yes   |no²  |
|Hitag2                     |ISO/IEC 14443A  |yes   |no   |
|Mifaire Ultralight         |ISO/IEC 14443A  |yes   |yes  |
|Mifare 1K S50              |ISO/IEC 14443A  |yes   |no²  |
|Mifare Plus S2K            |ISO/IEC 14443A  |yes   |no²  |
|Mifare Desfire D41         |ISO/IEC 14443A  |yes   |yes  |
|NTag 213                   |ISO/IEC 14443A  |yes   |yes  |
|Hellfest Cashless          |ISO/IEC 14443A  |yes   |no²  |
|French biometric passeport¹|ISO/IEC 14443A  |yes   |no²  |
|French Credit Card         |ISO/IEC 14443-4B|yes   |no²  |
|Alien H3                   |N/A             |no    |no   |
|Alien H3 9654              |N/A             |no    |no   |
|Alien H3 9662              |N/A             |no    |no   |
|Alien H3 + TK4100          |N/A             |no    |no   |
|EM4450                     |N/A             |no    |no   |
|ICODE SLI                  |N/A             |no    |no   |
|Picopass 2KS               |N/A             |no    |no   |
|SRI512                     |N/A             |no    |no   |
|Tag-it HF-I (TI2048)       |N/A             |no    |no   |
|TK4100                     |N/A             |no    |no   |

¹ UID is randomly generated so the tag is detected as a new one on each poll. 

² Polling is stopped at detection but no tag is exposed.

# UDev rules

You may want to add some UDev's rule for nfc device to make things easier.
You can put those lines into /etc/udev/rules.d/99-nfc.rules
```Shell
ACTION!="add", GOTO="nfc_pn53x_end"

# Advanced Card Systems, Ltd ACR122U
ATTRS{idVendor}=="072f", ATTRS{idProduct}=="2200", MODE="0666", SYMLINK+="nfc%n"

# SCM Microsystems, Inc. SCL3711-NFC&RW
ATTRS{idVendor}=="04e6", ATTRS{idProduct}=="5591", MODE="0666", SYMLINK+="nfc%n"

LABEL="nfc_pn53x_end"
```

# Kernel module
The SCL3711 is supported by the kernel and neard, so it takes the device and prevent the libnfc to use it.
If you have an error saying that the device is busy, it's probably the reason why.

You have to unload nfc kernel modules to get it work using libnfc.
```ShellSession
root@localhost# rmmod pn533_usb
root@localhost# rmmod pn533
root@localhost# rmmod nfc
```

To make it permanent you can add this file: /etc/modprobe.d/blacklist-nfc.conf
```
blacklist nfc
blacklist pn533
blacklist pn533_usb
```

# Compilation option

* USE_LIBNFC - Enable or disable the 'libnfc' usage. Default is ON.
    * LIBNFC_POLL_ALL Enable all modulation when polling. Default is OFF.
    * LIBNFC_POLL_NMT_ISO14443A - Enable ISO-14443A modulation when polling. Default is ON.
    * LIBNFC_POLL_NMT_ISOJEWEL - Enable ISO-JEWEL modulation when polling. Default is OFF.
    * LIBNFC_POLL_NMT_ISO14443B - Enable ISO-14443B modulation when polling. Default is OFF.
    * LIBNFC_POLL_NMT_ISO14443BI - Enable ISO-14443BI modulation when polling. Default is OFF.
    * LIBNFC_POLL_NMT_ISO14443B2SR - Enable ISO-14443B2SR modulation when polling. Default is OFF.
    * LIBNFC_POLL_NMT_ISO14443B2CT - Enable ISO-14443B2CT modulation when polling. Default is OFF.
    * LIBNFC_POLL_NMT_FELICA - Enable Felica modulation when polling. Default is OFF.