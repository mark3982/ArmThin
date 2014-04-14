#ifndef STDTYPES_H
#define STDTYPES_H
#ifdef B64
typedef unsigned long long  uintptr;
#else
typedef unsigned int		uintptr;
#endif
typedef unsigned long long  uint64;
typedef unsigned int		uint32;
typedef unsigned char		uint8;
typedef unsigned short		uint16;
typedef int					int32;
#endif