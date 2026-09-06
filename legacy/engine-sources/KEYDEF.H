#ifndef _KEYDEF_H_
#define _KEYDEF_H_

#include "typedef.h"
//////////////////
//  ASCII CODE  //
//////////////////

#ifndef NUL
    #define NUL '\x00'
#endif
#define SOH '\x01'
#define STX '\x02'
#define ETX '\x03'
#define EOT '\x04'
#define ENQ '\x05'
#define ACK '\x06'
#define BEL '\x07'
#define BS  '\x08'
#define HT  '\x09'
#define LF  '\x0a'
#define VT  '\x0b'
#define FF  '\x0c'
#define CR  '\x0d'
#define SO  '\x0e'
#define SI  '\x0f'
#define DLE '\x10'
#define DC1 '\x11'
#define DC2 '\x12'
#define DC3 '\x13'
#define DC4 '\x14'
#define NAK '\x15'
#define SYN '\x16'
#define ETB '\x17'
#define CAN '\x18'
#define EM  '\x19'
#define SUB '\x1a'
#define ESC '\x1b'
#define FS  '\x1c'
#define GS  '\x1d'
#define RS  '\x1e'
#define US  '\x1f'
#define DEL '\x7f'

#define CTRL_A 0x01
#define CTRL_B 0x02
#define CTRL_C 0x03
#define CTRL_D 0x04
#define CTRL_E 0x05
#define CTRL_F 0x06
#define CTRL_G 0x07
#define CTRL_H 0x08
#define CTRL_I 0x09
#define CTRL_J 0x0a
#define CTRL_K 0x0b
#define CTRL_L 0x0c
#define CTRL_M 0x0d
#define CTRL_N 0x0e
#define CTRL_O 0x0f
#define CTRL_P 0x10
#define CTRL_Q 0x11
#define CTRL_R 0x12
#define CTRL_S 0x13
#define CTRL_T 0x14
#define CTRL_U 0x15
#define CTRL_V 0x16
#define CTRL_W 0x17
#define CTRL_X 0x18
#define CTRL_Y 0x19
#define CTRL_Z 0x1a

#define ENTER CR
#define TAB   HT

/////////////////
//  SCAN CODE  //
/////////////////

#define F1  0x3b00
#define F2  0x3c00
#define F3  0x3d00
#define F4  0x3e00
#define F5  0x3f00
#define F6  0x4000
#define F7  0x4100
#define F8  0x4200
#define F9  0x4300
#define F10 0x4400
#define F11 ((int16)0x8500)
#define F12 ((int16)0x8600)

#define SHIFT_F1  0x5400
#define SHIFT_F2  0x5500
#define SHIFT_F3  0x5600
#define SHIFT_F4  0x5700
#define SHIFT_F5  0x5800
#define SHIFT_F6  0x5900
#define SHIFT_F7  0x5a00
#define SHIFT_F8  0x5b00
#define SHIFT_F9  0x5c00
#define SHIFT_F10 0x5d00
#define SHIFT_F11 ((int16)0x8700)
#define SHIFT_F12 ((int16)0x8800)

#define CTRL_F1  0x5e00
#define CTRL_F2  0x5f00
#define CTRL_F3  0x6000
#define CTRL_F4  0x6100
#define CTRL_F5  0x6200
#define CTRL_F6  0x6300
#define CTRL_F7  0x6400
#define CTRL_F8  0x6500
#define CTRL_F9  0x6600
#define CTRL_F10 0x6700
#define CTRL_F11 ((int16)0x8900)
#define CTRL_F12 ((int16)0x8a00)

#define ALT_F1  0x6800
#define ALT_F2  0x6900
#define ALT_F3  0x6a00
#define ALT_F4  0x6b00
#define ALT_F5  0x6c00
#define ALT_F6  0x6d00
#define ALT_F7  0x6e00
#define ALT_F8  0x6f00
#define ALT_F9  0x7000
#define ALT_F10 0x7100
#define ALT_F11 ((int16)0x8b00)
#define ALT_F12 ((int16)0x8c00)

#define ALT_1 0x7800
#define ALT_2 0x7900
#define ALT_3 0x7a00
#define ALT_4 0x7b00
#define ALT_5 0x7c00
#define ALT_6 0x7d00
#define ALT_7 0x7e00
#define ALT_8 0x7f00
#define ALT_9 ((int16)0x8000)
#define ALT_0 ((int16)0x8100)

#define ALT_MINUS ((int16)0x8200)
#define ALT_EQUAL ((int16)0x8300)

#define ALT_Q 0x1000
#define ALT_W 0x1100
#define ALT_E 0x1200
#define ALT_R 0x1300
#define ALT_T 0x1400
#define ALT_Y 0x1500
#define ALT_U 0x1600
#define ALT_I 0x1700
#define ALT_O 0x1800
#define ALT_P 0x1900

#define ALT_A 0x1e00
#define ALT_S 0x1f00
#define ALT_D 0x2000
#define ALT_F 0x2100
#define ALT_G 0x2200
#define ALT_H 0x2300
#define ALT_J 0x2400
#define ALT_K 0x2500
#define ALT_L 0x2600

#define ALT_Z 0x2c00
#define ALT_X 0x2d00
#define ALT_C 0x2e00
#define ALT_V 0x2f00
#define ALT_B 0x3000
#define ALT_N 0x3100
#define ALT_M 0x3200

#define NULLKEY 0x0300  /* <Ctrl-Shift-@> */

#define SHIFT_TAB 0x0f00

#define LEFTARROW  0x4b00
#define RIGHTARROW 0x4d00
#define UPARROW    0x4800
#define DOWNARROW  0x5000

#define HOMEKEY 0x4700
#define ENDKEY  0x4f00
#define PGUPKEY 0x4900
#define PGDNKEY 0x5100

#define INSKEY 0x5200
#define DELKEY 0x5300

#define CTRL_LEFT  0x7300
#define CTRL_RIGHT 0x7400

#define CTRL_HOME  0x7700
#define CTRL_END   0x7500
#define CTRL_PGUP  ((int16)0x8400)
#define CTRL_PGDN  0x7600

#define CTRL_BREAK ((int16)0xffff)

#define HANKEY   0x5700
#define HANJAKEY 0x5600

#define H1_FC   0x80  /*  FC  */
#define H1_G    0x81  /*  àA  */
#define H1_GG   0x82  /*  åA  */
#define H1_N    0x83  /*  êA  */
#define H1_D    0x84  /*  îA  */
#define H1_DD   0x85  /*  òA  */
#define H1_R    0x86  /*  úA  */
#define H1_M    0x87  /*  †A  */
#define H1_B    0x88  /*  §A  */
#define H1_BB   0x89  /*  ®A  */
#define H1_S    0x8a  /*  ¨A  */
#define H1_SS   0x8b  /*  ∞A  */
#define H1_O    0x8c  /*  ¥A  */
#define H1_J    0x8d  /*  ∏A  */
#define H1_JJ   0x8e  /*  ºA  */
#define H1_C    0x8f  /*  ¿A  */
#define H1_K    0x90  /*  ƒA  */
#define H1_T    0x91  /*  »A  */
#define H1_P    0x92  /*  ÃA  */
#define H1_H    0x93  /*  –A  */

#define H2_FC   0xa0  /*  FC  */
#define H2_A    0xa1  /*  Ña  */
#define H2_AE   0xa2  /*  ÑÅ  */
#define H2_YA   0xa3  /*  Ñ°  */
#define H2_YAE  0xa4  /*  Ñ¡  */
#define H2_EO   0xa5  /*  Ñ·  */
#define H2_E    0xa6  /*  ÖA  */
#define H2_YEO  0xa7  /*  Öa  */
#define H2_YE   0xa8  /*  ÖÅ  */
#define H2_O    0xa9  /*  Ö°  */
#define H2_WA   0xaa  /*  Ö¡  */
#define H2_WAE  0xab  /*  Ö·  */
#define H2_OI   0xac  /*  ÜA  */
#define H2_YO   0xad  /*  Üa  */
#define H2_U    0xae  /*  ÜÅ  */
#define H2_WEO  0xaf  /*  Ü°  */
#define H2_WE   0xb0  /*  Ü¡  */
#define H2_WI   0xb1  /*  Ü·  */
#define H2_YU   0xb2  /*  áA  */
#define H2_EU   0xb3  /*  áa  */
#define H2_EUI  0xb4  /*  áÅ  */
#define H2_I    0xb5  /*  á°  */

#define H3_FC   0xc0  /*  FC  */
#define H3_G    0xc1  /*  ÑB  */
#define H3_GG   0xc2  /*  ÑC  */
#define H3_GS   0xc3  /*  ÑD  */
#define H3_N    0xc4  /*  ÑE  */
#define H3_NJ   0xc5  /*  ÑF  */
#define H3_NH   0xc6  /*  ÑG  */
#define H3_D    0xc7  /*  ÑH  */
#define H3_R    0xc8  /*  ÑI  */
#define H3_RG   0xc9  /*  ÑJ  */
#define H3_RM   0xca  /*  ÑK  */
#define H3_RB   0xcb  /*  ÑL  */
#define H3_RS   0xcc  /*  ÑM  */
#define H3_RT   0xcd  /*  ÑN  */
#define H3_RP   0xce  /*  ÑO  */
#define H3_RH   0xcf  /*  ÑP  */
#define H3_M    0xd0  /*  ÑQ  */
#define H3_B    0xd1  /*  ÑS  */
#define H3_BS   0xd2  /*  ÑT  */
#define H3_S    0xd3  /*  ÑU  */
#define H3_SS   0xd4  /*  ÑV  */
#define H3_NG   0xd5  /*  ÑW  */
#define H3_J    0xd6  /*  ÑX  */
#define H3_C    0xd7  /*  ÑY  */
#define H3_K    0xd8  /*  ÑZ  */
#define H3_T    0xd9  /*  Ñ[  */
#define H3_P    0xda  /*  Ñ\  */
#define H3_H    0xdb  /*  Ñ]  */

#endif
