/**
 * @file ESPAsyncTCPbuffer.h
 * @date  22.01.2016
 * @author Markus Sattler
 *
 * Copyright (c) 2015 Markus Sattler. All rights reserved.
 * This file is part of the Asynv TCP for ESP.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef ESPASYNCTCPBUFFER_H_
#define ESPASYNCTCPBUFFER_H_

#define DEBUG_ASYNC_TCP(...)  while(((U0S >> USTXC) & 0x7F) != 0x00); os_printf( __VA_ARGS__ );

#ifndef DEBUG_ASYNC_TCP
#define DEBUG_ASYNC_TCP(...)
#endif

#include <Arduino.h>
#include <cbuf.h>

#include "ESPAsyncTCP.h"

typedef std::function<size_t(uint8_t * payload, size_t length)> AsyncTCPbufferDataCb;
typedef std::function<void(bool ok, void * ret)> AsyncTCPbufferDoneCb;

typedef enum {
    ATB_RX_MODE_NONE,
    ATB_RX_MODE_FREE,
    ATB_RX_MODE_READ_BYTES,
    ATB_RX_MODE_TERMINATOR,
    ATB_RX_MODE_TERMINATOR_STRING
} atbRxMode_t;

class AsyncTCPbuffer: public Print, public AsyncClient {

    public:

        AsyncTCPbuffer(AsyncClient* c);
        AsyncTCPbuffer(tcp_pcb* pcb = 0);
        ~AsyncTCPbuffer();

        size_t write(String data);
        size_t write(uint8_t data);
        size_t write(const char* data);
        size_t write(const char *data, size_t len);
        size_t write(const uint8_t *data, size_t len);

        void flush();

        void noCallback();

        void readStringUntil(char terminator, String * str, AsyncTCPbufferDoneCb done);

        // TODO implement read terminator non string
        //void readBytesUntil(char terminator, char *buffer, size_t length, AsyncTCPbufferDoneCb done);
        //void readBytesUntil(char terminator, uint8_t *buffer, size_t length, AsyncTCPbufferDoneCb done);

        void readBytes(char *buffer, size_t length, AsyncTCPbufferDoneCb done);
        void readBytes(uint8_t *buffer, size_t length, AsyncTCPbufferDoneCb done);

        void setTimeout(size_t timeout);

        void onData(AsyncTCPbufferDataCb cb);

    protected:
        cbuf * _TXbuffer;
        cbuf * _RXbuffer;
        atbRxMode_t _RXmode;
        size_t _rxSize;
        char _rxTerminator;
        uint8_t * _rxReadBytesPtr;
        String * _rxReadStringPtr;

        AsyncTCPbufferDataCb _cbRX;
        AsyncTCPbufferDoneCb _cbDone;

        void _attachCallbacks();
        void _sendBuffer();
        void _on_close();
        void _rxData(uint8_t *buf, size_t len);
        size_t _handleRxBuffer(uint8_t *buf, size_t len);

};

#endif /* ESPASYNCTCPBUFFER_H_ */
