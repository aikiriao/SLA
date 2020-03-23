#ifndef SLA_BYTEARRAY_H_INCLUDED
#define SLA_BYTEARRAY_H_INCLUDED

#include "SLAStdint.h"

/* 1バイト読み出し */
#define SLAByteArray_ReadUint8(p_array)   \
  (uint8_t)((p_array)[0])

/* 2バイト読み出し */
#define SLAByteArray_ReadUint16(p_array)  \
  (uint16_t)(                             \
   (((uint16_t)((p_array)[0])) << 8) |    \
   (((uint16_t)((p_array)[1])) << 0)      \
  )

/* 4バイト読み出し */
#define SLAByteArray_ReadUint32(p_array)  \
  (uint32_t)(                             \
   (((uint32_t)((p_array)[0])) << 24) |   \
   (((uint32_t)((p_array)[1])) << 16) |   \
   (((uint32_t)((p_array)[2])) <<  8) |   \
   (((uint32_t)((p_array)[3])) <<  0)     \
  )

/* 1バイト取得 */
#define SLAByteArray_GetUint8(p_array, p_u8val) {     \
  (*(p_u8val)) = SLAByteArray_ReadUint8(p_array);     \
  (p_array) += 1;                                     \
}

/* 2バイト取得 */
#define SLAByteArray_GetUint16(p_array, p_u16val) {   \
  (*(p_u16val)) = SLAByteArray_ReadUint16(p_array);   \
  (p_array) += 2;                                     \
}

/* 4バイト取得 */
#define SLAByteArray_GetUint32(p_array, p_u32val) {   \
  (*(p_u32val)) = SLAByteArray_ReadUint32(p_array);   \
  (p_array) += 4;                                     \
}

/* 1バイト書き出し */
#define SLAByteArray_WriteUint8(p_array, u8val)   {   \
  ((p_array)[0]) = (uint8_t)(u8val);                  \
}

/* 2バイト書き出し */
#define SLAByteArray_WriteUint16(p_array, u16val) {     \
  ((p_array)[0]) = (uint8_t)(((u16val) >> 8) & 0xFF);   \
  ((p_array)[1]) = (uint8_t)(((u16val) >> 0) & 0xFF);   \
}

/* 4バイト書き出し */
#define SLAByteArray_WriteUint32(p_array, u32val) {     \
  ((p_array)[0]) = (uint8_t)(((u32val) >> 24) & 0xFF);  \
  ((p_array)[1]) = (uint8_t)(((u32val) >> 16) & 0xFF);  \
  ((p_array)[2]) = (uint8_t)(((u32val) >>  8) & 0xFF);  \
  ((p_array)[3]) = (uint8_t)(((u32val) >>  0) & 0xFF);  \
}

/* 1バイト出力 */
#define SLAByteArray_PutUint8(p_array, u8val) {       \
  SLAByteArray_WriteUint8(p_array, u8val);            \
  (p_array) += 1;                                     \
}

/* 2バイト出力 */
#define SLAByteArray_PutUint16(p_array, u16val) {     \
  SLAByteArray_WriteUint16(p_array, u16val);          \
  (p_array) += 2;                                     \
}

/* 4バイト出力 */
#define SLAByteArray_PutUint32(p_array, u32val) {     \
  SLAByteArray_WriteUint32(p_array, u32val);          \
  (p_array) += 4;                                     \
}

#endif /* SLA_BYTEARRAY_H_INCLUDED */
