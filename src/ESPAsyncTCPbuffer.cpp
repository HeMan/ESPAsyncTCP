/**
 * @file  ESPAsyncTCPbuffer.cpp
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


#include <Arduino.h>
#include <debug.h>

#include "ESPAsyncTCPbuffer.h"

AsyncTCPbuffer::AsyncTCPbuffer(AsyncClient* c) :
        AsyncTCPbuffer(c->_pcb) {
}

AsyncTCPbuffer::AsyncTCPbuffer(tcp_pcb* pcb) :
        AsyncClient(pcb) {
    _TXbuffer = new cbuf(1460);
    _RXbuffer = new cbuf(100);
    _RXmode = ATB_RX_MODE_FREE;
    _rxSize = 0;
    _rxTerminator = 0x00;
    _rxReadBytesPtr = NULL;
    _rxReadStringPtr = NULL;

    _cbRX = NULL;
    _cbDone = NULL;
    _attachCallbacks();
}

AsyncTCPbuffer::~AsyncTCPbuffer() {
    AsyncClient::close();
    if(_RXbuffer) {
        delete _RXbuffer;
        _RXbuffer = NULL;
    }
    if(_TXbuffer) {
        delete _TXbuffer;
        _TXbuffer = NULL;
    }
}

size_t AsyncTCPbuffer::write(String data) {
    return write(data.c_str(), data.length());
}

size_t AsyncTCPbuffer::write(uint8_t data) {
    return write(&data, 1);
}

size_t AsyncTCPbuffer::write(const char* data) {
    return write((const uint8_t *) data, strlen(data));
}

size_t AsyncTCPbuffer::write(const char *data, size_t len) {
    return write((const uint8_t *) data, len);
}

/**
 * write data in to buffer and try to send the data
 * @param data
 * @param len
 * @return
 */
size_t AsyncTCPbuffer::write(const uint8_t *data, size_t len) {
    if(_TXbuffer == NULL || !AsyncClient::connected() || data == NULL || len == 0) {
        return 0;
    }

    size_t free = _TXbuffer->room();

    while(free < len) {

        size_t w = (len - free);
        w = _TXbuffer->write((const char*) data, w);
        len -= w;
        data += w;

        while(!AsyncClient::canSend()) {
            delay(0);
        }
        _sendBuffer();
        free = _TXbuffer->room();
    }

    _TXbuffer->write((const char*) data, len);
    _sendBuffer();
    return len;
}

/**
 * wait until all data has send out
 */
void AsyncTCPbuffer::flush() {
    while(!_TXbuffer->empty()) {
        while(!AsyncClient::canSend()) {
            delay(0);
        }
        _sendBuffer();
    }
}

void AsyncTCPbuffer::noCallback() {
    _RXmode = ATB_RX_MODE_NONE;
}

void AsyncTCPbuffer::readStringUntil(char terminator, String * str, AsyncTCPbufferDoneCb done) {
    DEBUG_ASYNC_TCP("[A-TCP] readStringUntil terminator: %02X\n", terminator);
    _RXmode = ATB_RX_MODE_NONE;
    _cbDone = done;
    _rxReadStringPtr = str;
    _rxTerminator = terminator;
    _rxSize = 0;
    _RXmode = ATB_RX_MODE_TERMINATOR_STRING;
}

/*
 void AsyncTCPbuffer::readBytesUntil(char terminator, char *buffer, size_t length, AsyncTCPbufferDoneCb done) {
 _RXmode = ATB_RX_MODE_NONE;
 _cbDone = done;
 _rxReadBytesPtr = (uint8_t *) buffer;
 _rxTerminator = terminator;
 _rxSize = length;
 _RXmode = ATB_RX_MODE_TERMINATOR;
 _handleRxBuffer(NULL, 0);
 }

 void AsyncTCPbuffer::readBytesUntil(char terminator, uint8_t *buffer, size_t length, AsyncTCPbufferDoneCb done) {
 readBytesUntil(terminator, (char *) buffer, length, done);
 }
 */

void AsyncTCPbuffer::readBytes(char *buffer, size_t length, AsyncTCPbufferDoneCb done) {
    DEBUG_ASYNC_TCP("[A-TCP] readBytes length: %d\n", length);
    _RXmode = ATB_RX_MODE_NONE;
    _cbDone = done;
    _rxReadBytesPtr = (uint8_t *) buffer;
    _rxSize = length;
    _RXmode = ATB_RX_MODE_READ_BYTES;
}

void AsyncTCPbuffer::readBytes(uint8_t *buffer, size_t length, AsyncTCPbufferDoneCb done) {
    readBytes((char *) buffer, length, done);
}

void AsyncTCPbuffer::onData(AsyncTCPbufferDataCb cb) {
    DEBUG_ASYNC_TCP("[A-TCP] onData\n");
    _RXmode = ATB_RX_MODE_NONE;
    _cbDone = NULL;
    _cbRX = cb;
    _RXmode = ATB_RX_MODE_FREE;
}

///--------------------------------

/**
 * attachCallbacks to AsyncClient class
 */
void AsyncTCPbuffer::_attachCallbacks() {
    DEBUG_ASYNC_TCP("[A-TCP] attachCallbacks\n");

    AsyncClient::onPoll([](void *obj, AsyncClient* c) {
        AsyncTCPbuffer* b = ((AsyncTCPbuffer*)(obj));
        if(!b->_TXbuffer->empty()) {
            b->_sendBuffer();
        }
        //    if(!b->_RXbuffer->empty()) {
        //       b->_handleRxBuffer(NULL, 0);
        //   }
    }, this);

    AsyncClient::onAck([](void *obj, AsyncClient* c, size_t len, uint32_t time) {
        DEBUG_ASYNC_TCP("[A-TCP] onAck\n");
        ((AsyncTCPbuffer*)(obj))->_sendBuffer();
    }, this);

    AsyncClient::onDisconnect([](void *obj, AsyncClient* c) {
        DEBUG_ASYNC_TCP("[A-TCP] onDisconnect\n");
        ((AsyncTCPbuffer*)(obj))->_on_close();
        c->free();
        delete c;
    }, this);

    AsyncClient::onData([](void *obj, AsyncClient* c, void *buf, size_t len) {
        AsyncTCPbuffer* b = ((AsyncTCPbuffer*)(obj));
        b->_rxData((uint8_t *)buf, len);
    }, this);

    DEBUG_ASYNC_TCP("[A-TCP] attachCallbacks Done.\n");
}

/**
 * send TX buffer if possible
 */
void AsyncTCPbuffer::_sendBuffer() {
    //DEBUG_ASYNC_TCP("[A-TCP] _sendBuffer...\n");
    size_t available = _TXbuffer->getSize();
    if(available == 0 || !AsyncClient::connected() || !AsyncClient::canSend()) {
        return;
    }

    if(available > space()) {
        available = space();
    }

    char *out = new char[available];
    if(out == NULL) {
        DEBUG_ASYNC_TCP("[A-TCP] to less heap\n");
        return;
    }
    _TXbuffer->read(out, available);

    size_t send = AsyncClient::write((const char*) out, available);
    if(send != available) {
        DEBUG_ASYNC_TCP("[A-TCP] write failed\n");
    }

    delete out;
}

/**
 * called on Disconnect
 */
void AsyncTCPbuffer::_on_close() {
    DEBUG_ASYNC_TCP("[A-TCP] _on_close\n");

    if(_cbDone) {
        switch(_RXmode) {
            case ATB_RX_MODE_READ_BYTES:
            case ATB_RX_MODE_TERMINATOR:
            case ATB_RX_MODE_TERMINATOR_STRING:
                _RXmode = ATB_RX_MODE_NONE;
                _cbDone(false, NULL);
                break;
        }
    }

    if(_TXbuffer) {
        cbuf *b = _TXbuffer;
        _TXbuffer = NULL;
        delete b;
    }
}

/**
 * called on incoming data
 * @param buf
 * @param len
 */
void AsyncTCPbuffer::_rxData(uint8_t *buf, size_t len) {
    if(!AsyncClient::connected()) {
        return;
    }
    DEBUG_ASYNC_TCP("[A-TCP] _rxData len: %d RXmode: %d\n", len, _RXmode);

    size_t handled = 0;

    if(_RXmode != ATB_RX_MODE_NONE) {
        handled = _handleRxBuffer((uint8_t *) buf, len);
        buf += handled;
        len -= handled;

        // handle as much as possible before using the buffer
        if(_RXbuffer->empty()) {
            while(_RXmode != ATB_RX_MODE_NONE && handled != 0 && len > 0) {
                handled = _handleRxBuffer(buf, len);
                buf += handled;
                len -= handled;
            }
        }
    }

    if(len > 0) {

        if(_RXbuffer->room() < len) {
            // to less space
            DEBUG_ASYNC_TCP("[A-TCP] _rxData buffer full try resize\n");
            _RXbuffer->resizeAdd((len + _RXbuffer->room()));

            if(_RXbuffer->room() < len) {
                DEBUG_ASYNC_TCP("[A-TCP] _rxData buffer to full can only handle %d!!!\n", _RXbuffer->room());
            }
        }

        _RXbuffer->write((const char *) (buf), len);
    }

    if(!_RXbuffer->empty() && _RXmode != ATB_RX_MODE_NONE) {
        // handle as much as possible data in buffer
        handled = _handleRxBuffer(NULL, 0);
        while(_RXmode != ATB_RX_MODE_NONE && handled != 0) {
            handled = _handleRxBuffer(NULL, 0);
        }
    }

    // clean up ram
    if(_RXbuffer->empty() && _RXbuffer->room() != 100) {
        _RXbuffer->resize(100);
    }

}

/**
 *
 */
size_t AsyncTCPbuffer::_handleRxBuffer(uint8_t *buf, size_t len) {
    if(!AsyncClient::connected()) {
        return 0;
    }

    DEBUG_ASYNC_TCP("[A-TCP] _handleRxBuffer len: %d RXmode: %d\n", len, _RXmode);

    size_t BufferAvailable = _RXbuffer->getSize();
    size_t r = 0;

    if(_RXmode == ATB_RX_MODE_NONE) {
        return 0;
    } else if(_RXmode == ATB_RX_MODE_FREE) {
        if(_cbRX == NULL) {
            return 0;
        }

        if(BufferAvailable > 0) {
            uint8_t * b = new uint8_t[BufferAvailable];
            _RXbuffer->peek((char *) b, BufferAvailable);
            r = _cbRX(b, BufferAvailable);
            _RXbuffer->remove(r);
        }

        if(r == BufferAvailable && buf && (len > 0)) {
            return _cbRX(buf, len);
        } else {
            return 0;
        }

    } else if(_RXmode == ATB_RX_MODE_READ_BYTES) {
        if(_rxReadBytesPtr == NULL || _cbDone == NULL) {
            return 0;
        }

        size_t newReadCount = 0;

        if(BufferAvailable) {
            r = _RXbuffer->read((char *) _rxReadBytesPtr, _rxSize);
            _rxSize -= r;
            _rxReadBytesPtr += r;
        }

        if(_RXbuffer->empty() && (len > 0) && buf) {
            r = len;
            if(r > _rxSize) {
                r = _rxSize;
            }
            memcpy(_rxReadBytesPtr, buf, r);
            _rxReadBytesPtr += r;
            _rxSize -= r;
            newReadCount += r;
        }

        if(_rxSize == 0) {
            _RXmode = ATB_RX_MODE_NONE;
            _cbDone(true, NULL);
        }

        // add left over bytes to Buffer
        return newReadCount;

    } else if(_RXmode == ATB_RX_MODE_TERMINATOR) {
        // TODO implement read terminator non string

    } else if(_RXmode == ATB_RX_MODE_TERMINATOR_STRING) {
        if(_rxReadStringPtr == NULL || _cbDone == NULL) {
            return 0;
        }

        // handle Buffer
        if(BufferAvailable > 0) {
            while(!_RXbuffer->empty()) {
                char c = _RXbuffer->read();
                if(c == _rxTerminator || c == 0x00) {
                    _RXmode = ATB_RX_MODE_NONE;
                    _cbDone(true, _rxReadStringPtr);
                    return 0;
                } else {
                    (*_rxReadStringPtr) += c;
                }
            }
        }

        if(_RXbuffer->empty() && (len > 0) && buf) {
            size_t newReadCount = 0;
            while(newReadCount < len) {
                char c = (char) *buf;
                buf++;
                newReadCount++;
                if(c == _rxTerminator || c == 0x00) {
                    _RXmode = ATB_RX_MODE_NONE;
                    _cbDone(true, _rxReadStringPtr);
                    return newReadCount;
                } else {
                    (*_rxReadStringPtr) += c;
                }
            }
            return newReadCount;
        }
    }

    return 0;
}

