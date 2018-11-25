# NFC Service

## Overview

NFC service uses neard package to detect the presence of NFC tags and signal clients via an event.
The NDEF data shall include a text record ('uid') to keep compatibility with the existing agl-service-identity-agent.

## Verbs

| Name               | Description                          | JSON Parameters                                                        |
|--------------------|:-------------------------------------|:-----------------------------------------------------------------------|
| subscribe          | subscribe to NFC events              | *Request:* {"value": "presence"}                                       |
| unsubscribe        | unsubscribe to NFC events            | *Request:* {"value": "presence"}                                       |

## Events
### neard response

| Name               | Description                          | JSON Response                                                          |
|--------------------|:-------------------------------------|:-----------------------------------------------------------------------|
| presence           | event that reports NFC tag presence  |  *Response:* {"status": "detected",                                    |
|                    |                                      |               "uid": "042eb3628e4981"},                                |
