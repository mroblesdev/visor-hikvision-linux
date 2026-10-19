#pragma once

// funciones de libPlayCtrl.so; el plaympeg4.h que trae el SDK es la versión de Windows
extern "C" {
int PlayM4_GetPort(int *nPort);
int PlayM4_FreePort(int nPort);
int PlayM4_SetStreamOpenMode(int nPort, unsigned int nMode);
int PlayM4_OpenStream(int nPort, unsigned char *pFileHeadBuf, unsigned int nSize, unsigned int nBufPoolSize);
int PlayM4_InputData(int nPort, unsigned char *pBuf, unsigned int nSize);
int PlayM4_SetSecretKey(int nPort, int lKeyType, char *pSecretKey, int lKeyLen);
int PlayM4_Play(int nPort, unsigned long hWnd);
int PlayM4_Stop(int nPort);
int PlayM4_CloseStream(int nPort);
unsigned int PlayM4_GetLastError(int nPort);
int PlayM4_GetPictureSize(int nPort, int *pWidth, int *pHeight);
int PlayM4_GetJPEG(int nPort, unsigned char *pJpeg, unsigned int nBufSize, unsigned int *pJpegSize);
}

#define PLAYM4_STREAM_REALTIME 0
