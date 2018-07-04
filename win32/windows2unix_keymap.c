#include <string.h>
#include <windows.h>

#include "keymap.h"

static struct /* _win2unix_keymap_key_t */{
	unsigned short tag;
	unsigned short _size;
	unsigned long long value;
} _win2unix_keymap[22] = {
{/* "up",       */0xE048, 3, 0x1B5B41},
{/* "down",     */0xE050, 3, 0x1B5B42},
{/* "left",     */0xE04B, 3, 0x1B5B44},
{/* "right",    */0xE04D, 3, 0x1B5B43},
{/* "insert",   */0xE052, 4, 0x1B5B327E},
{/* "delete",   */0xE053, 4, 0x1B5B337E},
{/* "home",     */0xE047, 3, 0x1B5B48},
{/* "end",      */0xE04F, 3, 0x1B5B46},
{/* "pageup",   */0xE049, 4, 0x1B5B357E},
{/* "pagedown", */0xE051, 4, 0x1B5B367E},
{/* "F1",       */0x003B, 3, 0x1B4F50}, /* unknow */
{/* "F2",       */0x003C, 3, 0x1B4F51},
{/* "F3",       */0x003D, 3, 0x1B4F52},
{/* "F4",       */0x003E, 3, 0x1B4F53},
{/* "F5",       */0x003F, 5, 0x1B5B31357E},
{/* "F6",       */0x0040, 5, 0x1B5B31377E},
{/* "F7",       */0x0041, 5, 0x1B5B31387E},
{/* "F8",       */0x0042, 5, 0x1B5B31397E},
{/* "F9",       */0x0043, 5, 0x1B5B32307E},
{/* "F10",      */0x0044, 5, 0x0000000000}, /* unknow */
{/* "F11",      */0xE085, 5, 0x0000000000}, /* unknow */
{/* "F12",      */0xE086, 5, 0x1B5B32347E}
};

#include <stdio.h>
unsigned short keymap_find(unsigned int tag, char *value)
{
	int idx;
	int size;
	char *pv;

	for (idx = 0; idx < 22; ++idx) {
		if (tag == _win2unix_keymap[idx].tag)
			break;
	}
	if (idx >= 22)
		return 0;

	size = _win2unix_keymap[idx]._size;
	pv = (char *)&_win2unix_keymap[idx].value;
	*(unsigned long long *)value = 0;
	while (size-- > 0)
		value[size] = *pv++;

	return _win2unix_keymap[idx]._size;
}