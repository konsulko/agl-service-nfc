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

² Polling is stopped at detection bu no tag is exposed.
