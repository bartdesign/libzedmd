#pragma once

#ifndef __ANDROID__
#include "serialib/serialib.h"
#else
#include <jni.h>
#endif

#include <inttypes.h>

class SerialPort
{
public:
   void SetReadTimeout(int timeout);
   void SetWriteTimeout(int timeout);
   bool Open(const char *pDevice, int baudRate, int dataBits, int stopBits, int parity);
   bool IsOpen();
   void Close();
   int Available();
   void ClearDTR();
   void SetDTR();
   void ClearRTS();
   void SetRTS();
   int WriteBytes(uint8_t *pBytes, int size);
   int WriteChar(uint8_t byte);
   int ReadBytes(uint8_t *pBytes, int size);
   int ReadChar(uint8_t *pByte);
   uint8_t ReadByte();

private:
   int m_readTimeout = 0;
   int m_writeTimeout = 0;
#ifndef __ANDROID__
   serialib m_seriallib;
#endif
};
