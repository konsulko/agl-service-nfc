# NFC Service

## Overview

NFC service uses the respective libnfc package to detect the presence of NFC tags and singal via an event

## Verbs

| Name               | Description                          | JSON Parameters                                                        |
|--------------------|:-------------------------------------|:-----------------------------------------------------------------------|
| subscribe          | subscribe to NFC events              | *Request:* {"value": "presence"}                                       |
| unsubscribe        | unsubscribe to NFC events            | *Request:* {"value": "presence"}                                       |

## Events

| Name               | Description                          | JSON Response                                                          |
|--------------------|:-------------------------------------|:------------------------------------------ ----------------------------|
| presence           | event that reports NFC tag presence  |  *Response:* {"status": "detected", "uid": "042eb3628e4981"}           |
