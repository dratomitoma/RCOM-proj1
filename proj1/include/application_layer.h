// Application layer protocol header.
// NOTE: This file must not be changed.

#ifndef _APPLICATION_LAYER_H_
#define _APPLICATION_LAYER_H_

// Application layer main function.
// Arguments:
//   serialPort: Serial port name (e.g., /dev/ttyS0).
//   role: Application role {"tx", "rx"}.
//   baudrate: Baudrate of the serial port.
//   nTries: Maximum number of frame retries.
//   timeout: Frame timeout.
//   filename: Name of the file to send / receive.

//Função que implementa a camada de aplicação
void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename);
//Função que cria o pacote de controlo
unsigned char *createControlPacket(unsigned int c, int filesize, const char* filename,int* packSize);
//Função que cria o pacote de dados
unsigned char* createDataPacket(unsigned char *data, int dataSize, int* packetSize);
#endif // _APPLICATION_LAYER_H_
