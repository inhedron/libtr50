/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 ILS Technology, LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <string.h>
#include <stdio.h>

#if defined(_AT_SIMULATOR)
void SYGetModel(char *tmp_str) {
	strcpy(tmp_str, "HE 910");
}

int BASGetImei(char *imei) {
	int i;
	for (i = 0; i < 8; ++i) {
		imei[i] = 10 + i;
	}
	return 1;
}

void SYGetSWVersion(char *tmp_str, unsigned short tmp_str_size, int CTcall, int OTAFormat) {
	strcpy(tmp_str, "VERSION.ABC.DEF");
}
#else
#include <atsim.h>
#endif

const char *_tr50_at_info_imei(char *buffer32) {
	char imei[8];
	BASGetImei(imei);
	sprintf(buffer32, "%02X%02X%02X%02X%02X%02X%02X%d",
			imei[0], imei[1], imei[2],
			imei[3], imei[4], imei[5],
			imei[6], imei[7]);
	return buffer32;
}

const char *_tr50_at_info_ccid(char *buffer32) {
#if defined(_AT_SIMULATOR)
	strcpy(buffer32, "CCID2255334411");
#else
	if (SIMParam.ICCID[10] == 0x00) { //  valid value
		UINT8 theData[10];
		UINT8 i = 0, j, tmp_data;

		SYmemcpy(theData, SIMParam.ICCID, sizeof(UINT8) * 10);

		for (j = 0; j <= 9; j++) {
			if (theData[j] == 0xFF) {
				break;
			}
			tmp_data = theData[j] & 0x0F;
			if (tmp_data < 0x0A) { // bug6731
				buffer32[i++] = tmp_data + 0x30;
			} else {
				buffer32[i++] = tmp_data + 0x37;
			}

			if ((theData[j] & 0xF0) == 0xF0) {
				break;
			}
			tmp_data = (theData[j] & 0xF0) >> 4;
			if (tmp_data < 0x0A) { // bug6731
				buffer32[i++] = tmp_data + 0x30;
			} else {
				buffer32[i++] = tmp_data + 0x37;
			}
		}
		buffer32[i] = 0;
	} else {
		strcpy(buffer32, "CCID: Not Valid");
	}
#endif
	return buffer32;
}

const char *_tr50_at_info_model(char *buffer32) {
	SYGetModel(buffer32);
	return buffer32;
}

const char *_tr50_at_info_version(char *buffer64) {
	SYGetSWVersion(buffer64, 64, 0, 0);
	return buffer64;
}

