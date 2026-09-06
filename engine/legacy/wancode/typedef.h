#ifndef _TYPEDEF_H
#define _TYPEDEF_H

// Macro and Definition for Type
typedef char					  int8;
typedef unsigned char   uint8;
typedef short           int16;
typedef unsigned short  uint16;
typedef long  					int32;
typedef unsigned long   uint32;

typedef uint8 BYTE;
typedef uint16 BYTE16;
typedef uint16 WORD;
typedef uint32 DWORD;
typedef uint8  BOOL;

#define MAKEDWORD(a, b)     ((DWORD)(((WORD)(a)) | (((DWORD)((WORD)(b))) << 16)))
#define MAKEWORD(a, b)      ((WORD)(((BYTE)(a))  | (((WORD)((BYTE)(b)))  << 8)))
#define LOWORD(l)           ((WORD)(l))
#define HIWORD(l)           ((WORD)(((DWORD)(l) >> 16) & 0xFFFF))
#define LOBYTE(w)           ((BYTE)(w))
#define HIBYTE(w)           ((BYTE)(((WORD)(w) >> 8) & 0xFF))

/*=====================================================*/
#define	TRUE	1
#define	FALSE	0
/*=====================================================*/
#endif